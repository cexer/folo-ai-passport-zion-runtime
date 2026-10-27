"""Exercise upstream Snoopy pages through real key routing and LCD transfers."""
import argparse,json,re,subprocess,sys,time
from pathlib import Path
import serial
from PIL import Image,ImageChops
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--output',type=Path,required=True)
p.add_argument('--smoke',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.1;s.dtr=False;s.rts=False
lines=[];result={'mainflow_pass':False};selected=0
def open_port():s.open();s.reset_input_buffer()
def send(cmd):s.write((cmd+'\n').encode());s.flush()
def wait(pattern,seconds=15):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        line=s.readline().decode('utf8','replace').strip()
        if not line:continue
        lines.append(line)
        assert not any(x in line for x in ['Guru Meditation','assert failed','ALLOC_FAILED','LOAD_FAIL']),line
        if re.search(pattern,line):return line
    raise TimeoutError(pattern+' / '+str(lines[-8:]))
def key(name):send('ZION KEY '+name);wait('STANDALONE_KEY|ZION_KEY_DONE');time.sleep(.35)
def select(index):
    global selected
    for _ in range((index-selected)%6):key('DOWN')
    selected=index
def capture(name):
    s.close()
    try:
        for attempt in range(3):
            r=subprocess.run([sys.executable,str(Path(__file__).with_name('capture-native-screen.py')),
                '--port',a.port,'--output',str(a.output/(name+'.png'))],capture_output=True,text=True,timeout=40)
            if r.returncode==0:return Image.open(a.output/(name+'.png')).convert('RGB')
            (a.output/(name+f'-attempt-{attempt}.log')).write_text(r.stdout+r.stderr)
        raise RuntimeError('Incomplete actual LCD capture: '+name)
    finally:open_port()
def difference(one,two):
    # Exclude live clock/battery; a page must actually change its body.
    data=ImageChops.difference(one.crop((0,38,240,308)),two.crop((0,38,240,308))).tobytes()
    count=sum(any(data[i:i+3]) for i in range(0,len(data),3));assert count>50,count;return count
open_port()
try:
    send('ZION MEM');result['memory_before']=wait('ZION_MEMORY')
    key('UP');key('DOWN') # also wakes the screen after idle, net zero at home
    home=capture('home');key('OK');food=capture('food')
    result['food_page_pixels']=difference(home,food)
    key('DOWN');key('OK');capture('fed')
    if not a.smoke:
        select(1);key('OK');game=capture('game')
        for i in range(5):key('DOWN' if i%2 else 'UP');key('OK')
        capture('game-finished')
        select(3);key('OK');status=capture('status');key('OK')
        select(4);key('OK');gallery=capture('gallery-0');key('DOWN');gallery1=capture('gallery-1')
        result['gallery_pixels']=difference(gallery,gallery1)
        key('UP');key('OK');key('LONG');settings=capture('settings')
        key('DOWN');key('DOWN');key('OK');capture('date-editor')
        # Exercise date/time conversion without real external NTP.
        for _ in range(5):key('OK')
        capture('date-saved');key('LONG');capture('returned-home')
    send('ZION MEM');result['memory_after']=wait('ZION_MEMORY')
    result['automated_flow_pass']=True
    result['scope']='food, five-round game, status, animated gallery, manual time editor, settings, UP/DOWN/OK/LONG' if not a.smoke else 'startup, keys, food page'
    result['unverified']=['Optional external Wi-Fi/NTP','physical switch contacts','all 50 states and overnight schedule','acoustic quality']
    # Visual review of saved LCD images is required before setting mainflow_pass.
finally:
    result['environment']='physical device'
    s.close();raw=re.sub(r'(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}','[MAC REDACTED]','\n'.join(lines))
    (a.output/'mainflow.log').write_text(raw+'\n',encoding='utf8')
    (a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
