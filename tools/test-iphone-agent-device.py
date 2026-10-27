"""Exercise unchanged Agent USB parsing, pause/resume, keys and real LCD.

No input action is sent while resumed: this test must not control a real phone
or the Windows host that may pair for the separate BLE probe.
"""
import argparse,json,re,subprocess,sys,time
from pathlib import Path
import serial
p=argparse.ArgumentParser();p.add_argument('--port',required=True)
p.add_argument('--output',type=Path,required=True);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True);lines=[];results={}
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.1;s.dtr=False;s.rts=False
def open_port():s.open();s.reset_input_buffer()
def send(cmd):s.write((cmd+'\n').encode());s.flush()
def wait(pattern,seconds=12):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        line=s.readline().decode('utf8',errors='replace').strip()
        if not line:continue
        lines.append(line)
        assert not any(x in line for x in ['Guru Meditation','assert failed','Stack smashing','LOAD_FAIL']),line
        if re.search(pattern,line):return line
    raise TimeoutError(pattern+' / '+str(lines[-8:]))
def key(name,expected=None):
    start=len(lines);send('ZION KEY '+name);ack=wait('STANDALONE_KEY|ZION_KEY_DONE')
    # The app task may process its queue before the injecting task prints ACK.
    if expected and not any(re.search(expected,line) for line in lines[start:]):wait(expected)
    return ack
def capture(name):
    s.close()
    try:
        for attempt in range(3):
            completed=subprocess.run([sys.executable,str(Path(__file__).with_name('capture-native-screen.py')),
                '--port',a.port,'--output',str(a.output/(name+'.png'))])
            if completed.returncode==0:break
            # A complete new capture only; never combine missing rows across frames.
            time.sleep(.2)
        else:raise RuntimeError('Incomplete LCD capture after three whole-frame attempts')
    finally:open_port()
open_port()
try:
    send('hello');results['hello']=wait(r'@hello ')
    send('ping');wait(r'^@ok$');results['usb_ping']='PASS'
    send('config');config=wait(r'@config ')
    results['ble_state']=re.search(r'phone=(\w+)',config)[1]
    key('OK',r'bridge: paused');send('move 0 0');wait(r'^@err paused');results['pause']='PASS'
    capture('paused')
    key('OK',r'bridge: resumed');results['resume']='PASS'
    key('DOWN');time.sleep(.3);capture('setup')
    key('UPLONG',r'bonds cleared: 0');results['forget']='PASS'
    key('UP');time.sleep(.3);capture('live')
    key('LONG');results['long_no_restart']='PASS'
    send('ZION MEM');results['memory']=wait('ZION_MEMORY ')
    send('bye');wait(r'^@ok$');results['status']='PASS'
finally:
    s.close()
    raw='\n'.join(lines)
    raw=re.sub(r'(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}','[MAC REDACTED]',raw)
    raw=re.sub(r'(ssid=).*?( pass=)',r'\1[REDACTED]\2',raw)
    (a.output/'keys-usb.log').write_text(raw+'\n',encoding='utf8')
    (a.output/'result.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2))
