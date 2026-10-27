"""CW main flows through timed original PRESS/RELEASE callbacks and real UDP."""
import argparse,json,re,subprocess,sys,time,urllib.request
from pathlib import Path
from PIL import Image,ImageChops
import serial
p=argparse.ArgumentParser();p.add_argument('--port',required=True)
p.add_argument('--output',type=Path,required=True);p.add_argument('--startup-log',type=Path)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);lines=[];results={};pending=b''
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.1;s.dtr=False;s.rts=False
base='http://127.0.0.1:21301'
def status():
    with urllib.request.urlopen(base+'/status',timeout=3) as response:return json.load(response)
def send(line):s.write((line+'\n').encode());s.flush()
def wait(pattern,seconds=10):
    global pending
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        pending+=s.read(max(1,min(65536,s.in_waiting)))
        while b'\n' in pending:
            line,pending=pending.split(b'\n',1);line=line.decode('utf8','replace').strip()
            if not line:continue
            lines.append(line)
            assert not any(k in line for k in ['Guru Meditation','assert failed','Stack smashing','LOAD_FAIL']),line
            if re.search(pattern,line):return line
    raise TimeoutError(pattern+' / '+str(lines[-8:]))
def edge(key,phase):
    send(f'ZION EDGE {key} {phase}');return wait(rf'ZION_EDGE_DONE key={key} phase={phase} result=0')
def press(key,seconds=.09):
    send(f'ZION PULSE {key} {round(seconds*1000)}')
    value=wait(rf'ZION_PULSE_DONE key={key} requested=',seconds+5)
    time.sleep(.25);return value
def capture(name):
    global pending
    s.close();pending=b''
    try:
        for attempt in range(3):
            r=subprocess.run([sys.executable,str(Path(__file__).with_name('capture-native-screen.py')),
                '--port',a.port,'--output',str(a.output/(name+'.png'))],capture_output=True,text=True)
            if not r.returncode:print(r.stdout,end='',flush=True);break
            (a.output/(name+'-attempt-'+str(attempt+1)+'.log')).write_text(r.stdout+r.stderr,encoding='utf8')
        else:raise RuntimeError('No complete frame: '+name)
    finally:s.open();time.sleep(1) # intrusive flush capture must release LVGL before key delivery
def new_events(start,kind,condition=lambda x:True,seconds=8):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        values=[x for x in status()['events'][start:] if x['event']==kind and condition(x)]
        if values:return values
        time.sleep(.15)
    raise TimeoutError('Peer event '+kind)
def pixels(first,second,box):
    one=Image.open(a.output/(first+'.png')).crop(box)
    two=Image.open(a.output/(second+'.png')).crop(box)
    raw=ImageChops.difference(one,two).convert('RGB').tobytes()
    return sum(any(raw[i:i+3]) for i in range(0,len(raw),3))
s.open()
try:
    deadline=time.monotonic()+25
    while not status()['authenticated'] and time.monotonic()<deadline:time.sleep(.2)
    assert status()['authenticated'],'UDP authentication did not complete'
    send('ZION MEM');results['memory_before']=wait('ZION_MEMORY')
    capture('offline-main')
    # Original physical-to-logical mapping: 0=keyer/OK, 1=tune up,
    # 2=tune down, 3=the virtual parallel-key level. Boot starts offline.
    start=len(status()['events']);press(3)
    results['online_presence']=new_events(start,'PRESENCE',lambda e:e['online'])[-1]
    capture('online-main')
    for name,duration,low,high in [('dot',.09,40,180),('dash',.28,210,500)]:
        start=len(status()['events']);press(0,duration);time.sleep(.8)
        keys=new_events(start,'KEY',lambda e:e['on']==0 and not e['flags'])
        current=[x for x in status()['events'][start:] if x['event']=='KEY' and not x['flags']]
        down=next(x for x in current if x['on']==1);up=next(x for x in current if x['on']==0)
        delta=(up['dev_ms']-down['dev_ms'])&65535
        assert down['uid']==7 and up['uid']==7 and low<=delta<=high,(name,delta)
        results[name]=dict(device_duration_ms=delta,down_sequence=down['seq'],up_sequence=up['seq'])
    capture('transmitted')
    start=len(status()['events']);press(1)
    up=new_events(start,'HEARTBEAT',lambda e:e['freq']>7100000)[-1];results['tune_up']=up['freq']
    start=len(status()['events']);press(2)
    results['tune_down']=new_events(start,'HEARTBEAT',lambda e:e['freq']==7100000)[-1]['freq']
    capture('before-receive')
    with urllib.request.urlopen(urllib.request.Request(base+'/rx-dot',data=b'',method='POST'),timeout=3) as response:assert json.load(response)['sent']
    time.sleep(1);capture('received')
    results['received_pixels']=pixels('before-receive','received',(45,110,230,133));assert results['received_pixels']>10
    start=len(status()['events']);press(3)
    results['offline_presence']=new_events(start,'PRESENCE',lambda e:not e['online'])[-1]
    press(0,2.2);capture('menu')
    press(2);press(1);press(0);capture('tone-settings')
    press(1);press(2);press(0,2.2);press(0,2.2);capture('main-restored')
    send('ZION MEM');results['memory_after']=wait('ZION_MEMORY')
    if a.startup_log:
        startup=a.startup_log.read_text(encoding='utf8')
        assert 'CODEC_MUTED volume=0 mute=1' in startup and 'I2S_ZERO_OUTPUT' in startup
        results['silenced_i2s']='PASS: actual codec mute and zero PCM write evidence'
    results['status']='PASS: UDP/MQTT, timed key edges, tuning, peer receive, combo and settings'
finally:
    s.close();raw='\n'.join(lines)
    raw=re.sub(r'(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}','[MAC REDACTED]',raw)
    raw=re.sub(r'(?m)(连接 Wi-Fi: ).*',r'\1[REDACTED]',raw)
    raw=re.sub(r'(?im)^.*(?:Global UID|Device ID|设备.?ID|GUID[:=]).*$', '[IDENTITY REDACTED]',raw)
    (a.output/'mainflow.log').write_text(raw+'\n',encoding='utf8')
    (a.output/'result.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2))
