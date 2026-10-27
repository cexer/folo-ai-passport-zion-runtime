"""Main-flow tests through the app's own ADC polling, decoder and screens."""
import argparse,json,re,subprocess,sys,time
from pathlib import Path
import serial
from PIL import Image,ImageChops
p=argparse.ArgumentParser();p.add_argument('--port',required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--expect-memory-failure',action='store_true');a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True);lines=[];results={}
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.1;s.dtr=False;s.rts=False
def open_port():s.open();s.reset_input_buffer()
def send(line):s.write((line+'\n').encode());s.flush()
def wait(pattern,seconds=15):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        raw=s.readline().decode('utf8','replace').strip()
        if not raw:continue
        lines.append(raw)
        assert not any(x in raw for x in ['Guru Meditation','assert failed','Stack smashing','LOAD_FAIL']),raw
        if re.search(pattern,raw):return raw
    raise TimeoutError(pattern+' / '+str(lines[-8:]))
def key(name,expected=None):
    start=len(lines);send('ZION KEY '+name);wait('STANDALONE_KEY|ZION_KEY_DONE')
    time.sleep(1.3 if 'LONG' in name else .4)
    if expected and not any(re.search(expected,l) for l in lines[start:]):return wait(expected)
def capture(name):
    s.close()
    # Upstream FAP capture composites its retained octopus even on menu pages.
    # Menus therefore use the actual LCD-transfer tap, not that cached scene.
    script='capture-native-screen.py' if name in ('settings','volume-setting') else 'capture-fap-screen.py'
    try:
        for attempt in range(3):
            run=subprocess.run([sys.executable,str(Path(__file__).with_name(script)),
                '--port',a.port,'--output',str(a.output/(name+'.png'))],capture_output=True,text=True)
            if run.returncode:
                (a.output/(name+'-attempt-'+str(attempt+1)+'.log')).write_text(run.stdout+run.stderr,encoding='utf8')
            else:print(run.stdout,end='',flush=True)
            if run.returncode==0:break
        else:raise RuntimeError('No complete capture after three whole-frame attempts: '+name)
    finally:open_port()
open_port()
try:
    send('ZION MEM');results['memory_before']=wait('ZION_MEMORY ')
    capture('playing-a');time.sleep(.5);capture('playing-b')
    one=Image.open(a.output/'playing-a.png').crop((0,65,240,216))
    two=Image.open(a.output/'playing-b.png').crop((0,65,240,216))
    delta=ImageChops.difference(one,two).tobytes()
    changed=sum(any(delta[i:i+3]) for i in range(0,len(delta),3))
    assert changed>100,('Scene did not animate',changed);results['animation_changed_pixels']=changed
    results['down_station']=key('DOWN',r'Selected station 2/2')
    results['up_station']=key('UP',r'Selected station 1/2')
    key('OK');time.sleep(.5);capture('paused')
    key('OK')
    if a.expect_memory_failure:
        results['mp3_failure']=wait(r'MP3 decode failed',20)
        assert any('ALLOC_FAILED bytes=8192' in x for x in lines),'Missing allocation-failure evidence'
    else:
        results['mp3_resumed']=wait(r'MP3 stream .*22050Hz/1ch',20)
        wait('Jitter buffer primed',12)
    capture('resumed')
    if a.expect_memory_failure:key('OK') # stop upstream decoder retry spam before menu captures
    key('UPLONG');capture('volume-up')
    key('DOWNLONG');key('LONG');capture('settings')
    key('DOWN');key('UP');key('OK');capture('volume-setting')
    key('LONG');key('LONG');capture('main-final')
    send('ZION MEM');results['memory_after']=wait('ZION_MEMORY ')
    results['status']=('PARTIAL: main keys and animation PASS; MP3 memory FAIL' if a.expect_memory_failure
                       else 'PASS: main keys, animation, stream and screen captures')
finally:
    s.close();raw='\n'.join(lines)
    raw=re.sub(r'(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}','[MAC REDACTED]',raw)
    raw=re.sub(r'(wifi:connected with ).*?(, aid =)',r'\1[REDACTED]\2',raw)
    (a.output/'mainflow.log').write_text(raw+'\n',encoding='utf8')
    (a.output/'result.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2))
