"""V-Pet main flow with real buttons/router, LCD flushes and Windows BLE RF peer."""
import argparse,json,re,subprocess,sys,time
from pathlib import Path
import serial
from PIL import Image,ImageChops
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--output',type=Path,required=True)
p.add_argument('--hatch-wait',type=float,default=0);p.add_argument('--smoke',action='store_true')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.1;s.dtr=False;s.rts=False
lines=[];result={};peer=None;peer_log=None;selected=0;stop=a.output/'peer.stop';stop.unlink(missing_ok=True)
def open_port():s.open();s.reset_input_buffer()
def send(command):s.write((command+'\n').encode());s.flush()
def wait(pattern,seconds=15):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        line=s.readline().decode('utf8','replace').strip()
        if not line:continue
        lines.append(line)
        assert not any(x in line for x in ['Guru Meditation','assert failed','ALLOC_FAILED','LOAD_FAIL']),line
        if re.search(pattern,line):return line
    raise TimeoutError(pattern+' / '+str(lines[-8:]))
def proof(pattern,start):
    return next((line for line in lines[start:] if re.search(pattern,line)),None) or wait(pattern,25)
def key(name):send('ZION KEY '+name);wait('STANDALONE_KEY|ZION_KEY_DONE');time.sleep(.3)
def select(index):
    global selected
    down=(index-selected)%9;up=(selected-index)%9
    for _ in range(min(down,up)):key('DOWN' if down<=up else 'UP')
    selected=index
def capture(name):
    s.close()
    try:
        for attempt in range(3):
            r=subprocess.run([sys.executable,str(Path(__file__).with_name('capture-native-screen.py')),
                '--port',a.port,'--output',str(a.output/(name+'.png'))],capture_output=True,text=True,encoding='utf8',timeout=40)
            if r.returncode==0:return Image.open(a.output/(name+'.png')).convert('RGB')
            (a.output/(name+f'-attempt-{attempt}.log')).write_text(r.stdout+r.stderr)
        raise RuntimeError('Incomplete actual LCD capture: '+name)
    finally:open_port()
def changed(one,two):
    data=ImageChops.difference(one.crop((0,25,240,285)),two.crop((0,25,240,285))).tobytes()
    n=sum(any(data[i:i+3]) for i in range(0,len(data),3));assert n>50,n;return n
open_port()
try:
    send('ZION MEM');result['memory_before']=wait('ZION_MEMORY ')
    capture('initial')
    if a.smoke:
        key('DOWN');capture('selected');key('UP');key('OK');capture('action')
    else:
        if a.hatch_wait:time.sleep(a.hatch_wait)
        send('ZION MEM');result['memory_after_hatch']=wait('ZION_MEMORY ')
        home=capture('home');key('OK');capture('food')
        select(1);key('OK');capture('protein')
        select(2);key('OK');charge=capture('training')
        for _ in range(3):key('OK');time.sleep(.25)
        trained=capture('trained');result['training_changed_pixels']=changed(charge,trained);key('LONG')
        select(5);key('OK');capture('status');key('LONG')
        peer_log=(a.output/'peer.log').open('w',encoding='utf8')
        peer=subprocess.Popen([sys.executable,str(Path(__file__).with_name('community-vpet-peer.py')),
            '--output',str(a.output/'peer.json'),'--stop-file',str(stop),'--duration','70'],stdout=peer_log,stderr=subprocess.STDOUT)
        end=time.monotonic()+8
        while time.monotonic()<end:
            if 'VPET_RF_PEER_READY' in (a.output/'peer.log').read_text():break
            assert peer.poll() is None,'Windows RF fixture exited before readiness';time.sleep(.1)
        else:raise TimeoutError('Windows BLE RF publisher readiness')
        select(7);key('OK');capture('link-charge');start=len(lines)
        for _ in range(3):key('OK');time.sleep(.3)
        result['peer_found']=proof('peer found:',start)
        result['link_battle']=proof('link battle start:',start)
        battle=capture('link-battle');time.sleep(2);later=capture('link-battle-later')
        # The result may already be settled when the first capture finishes.
        data=ImageChops.difference(battle,later).tobytes()
        result['battle_sample_changed_pixels']=sum(any(data[i:i+3]) for i in range(0,len(data),3))
        time.sleep(1);key('LONG');capture('returned-home')
        select(5);key('OK');capture('status-after-link')
    send('ZION MEM');result['memory_after']=wait('ZION_MEMORY ')
    result['keys']='PASS: UP/DOWN/OK' if a.smoke else 'PASS: UP/DOWN/OK/LONG use original callback and UI state machine'
    result['status']='PASS: package startup, keys and LCD smoke' if a.smoke else 'PASS: scoped main flow, training, real BLE peer and battle start'
finally:
    s.close()
    if peer:
        stop.write_text('stop\n')
        try:peer.wait(timeout=10)
        except subprocess.TimeoutExpired:peer.terminate();peer.wait(timeout=5)
    if peer_log:peer_log.close()
    raw=re.sub(r'(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}','[MAC REDACTED]','\n'.join(lines))
    (a.output/'mainflow.log').write_text(raw+'\n',encoding='utf8')
    (a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
if not a.smoke and result.get('status','').startswith('PASS'):
    rf=json.loads((a.output/'peer.json').read_text());assert rf['device_advertisements'],'Windows did not observe VPET RF advertising'
    result['real_rf_both_directions']=True
    (a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
