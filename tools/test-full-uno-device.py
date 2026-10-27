"""Launch the complete, unchanged UNO application and exercise its real UI/radio."""
import argparse,json,re,time
from pathlib import Path
import serial
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--index',type=int,default=5);a=p.parse_args()
out=root/'diagnostics/full-link-uno';out.mkdir(exist_ok=True);lines=[]
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.1;s.dtr=False;s.rts=False;s.open()
def until(pattern,seconds=20):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        line=s.readline().decode('utf8',errors='replace').strip()
        if not line:continue
        lines.append(line)
        assert not any(x in line for x in ['Guru Meditation','assert failed','LOAD_FAIL','Stack smashing']),line
        if re.search(pattern,line):return line
    raise TimeoutError(pattern+' / '+str(lines[-10:]))
def send(text):s.write((text+'\n').encode());s.flush()
def key(name):send('ZION KEY '+name);time.sleep(.2)
def state():
    send('ZION STATE');line=until('ZION_UI page=')
    return {k:int(v) for k,v in re.findall(r'(\w+)=(-?\d+)',line)}
try:
    d=state()
    if d['page']==5:key('OK');d=state()
    assert d['page']==0,d
    for _ in range(d['apps']+2):
        if d['selected']==a.index:break
        key('DOWN');d=state()
    assert d['selected']==a.index,d
    key('OK');ready=until('Standalone local-room firmware ready',seconds=25)
    send('ZION MEM');memory_ready=until('ZION_MEMORY')
    key('OK');radio=until('Starting ESP-NOW');room=until('Created room',seconds=15)
    key('UP');key('DOWN');key('OK');key('LONG');time.sleep(1)
    send('ZION MEM');memory_room=until('ZION_MEMORY')
    result=dict(status='PASS_START_AND_ROOM',ready=ready,radio=radio,room=room,memory_ready=memory_ready,memory_after_keys=memory_room,
        scope='Full original C++/C application; real Wi-Fi/ESP-NOW init and room creation; no second physical player yet',source_modified=False)
    (out/'native-result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
finally:
    s.close();text='\n'.join(lines);text=re.sub(r'(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}','[MAC REDACTED]',text)
    (out/'native-serial.log').write_text(text+'\n',encoding='utf8')
