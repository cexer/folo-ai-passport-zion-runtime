"""Focused original recorder main-flow acceptance on an authorized real board."""
import argparse,json,re,time
from pathlib import Path
import serial
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);lines=[]
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.1;s.dtr=False;s.rts=False;s.open()
def send(text):s.write((text+'\n').encode());s.flush()
def read_for(seconds,pattern=None):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        line=s.readline().decode('utf8',errors='replace').strip()
        if not line:continue
        lines.append(line)
        assert not any(x in line for x in ['Guru Meditation','assert failed','Stack smashing','LOAD_FAIL']),line
        if pattern and re.search(pattern,line):return line
    if pattern:raise TimeoutError(pattern+' / '+str(lines[-12:]))
def key(name):send('ZION KEY '+name)
try:
    send('ZION MEM');before=read_for(5,'ZION_MEMORY')
    key('OK');encoder=read_for(10,'Opus encoder ready');read_for(4)
    key('OK');saved=read_for(10,r'Saved R\d+, [1-9]\d* samples');read_for(1)
    key('UP');read_for(.3);key('DOWN');read_for(.3);key('LONG');read_for(.8)
    key('UP');higher=read_for(5,'Playback volume applied:');key('DOWN');lower=read_for(5,'Playback volume applied:')
    read_for(6)
    send('ZION MEM');after=read_for(5,'ZION_MEMORY')
    result=dict(status='PASS',encoder=encoder,saved=saved,playback_volume_up=higher,playback_volume_down=lower,
        memory_before=before,memory_after=after,scope='Real microphone/I2S/Opus/FAT record-save-read-decode/play path and key callbacks; audible fidelity not instrumented')
    (a.output/'main-flow.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
finally:
    s.close();(a.output/'main-flow.log').write_text('\n'.join(lines)+'\n',encoding='utf8')
