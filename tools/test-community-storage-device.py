"""Delete/reinstall a storage-owning package through the ordinary manager UI."""
import argparse,json,re,subprocess,sys,time
from pathlib import Path
import serial
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--index',type=int,required=True)
p.add_argument('--package',type=Path,required=True);p.add_argument('--storage-bytes',type=int,required=True)
p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.1;s.dtr=False;s.rts=False;s.open();lines=[]
def send(cmd):s.write((cmd+'\n').encode());s.flush()
def key(name):send('ZION KEY '+name);time.sleep(.2)
def state():
    send('ZION STATE');end=time.monotonic()+10
    while time.monotonic()<end:
        line=s.readline().decode('utf8',errors='replace').strip()
        if not line:continue
        lines.append(line)
        assert not any(x in line for x in ['Guru Meditation','assert failed','Stack smashing']),line
        if 'ZION_UI page=' in line:return {k:int(v) for k,v in re.findall(r'(\w+)=(-?\d+)',line)}
    raise TimeoutError(str(lines[-10:]))
try:
    before=state()
    if before['page']==5:key('OK');before=state()
    assert before['page']==0,before
    for _ in range(before['apps']+2):
        if before['selected']==a.index:break
        key('DOWN');before=state()
    assert before['selected']==a.index
    key('LONG');key('DOWN');key('DOWN');key('OK');assert state()['page']==2
    key('DOWN');key('OK')
    for _ in range(50):
        removed=state()
        if removed['page']==0 and not removed['busy']:break
        time.sleep(.1)
    assert removed['apps']==before['apps']-1 and removed['result']==0,removed
    reclaimed=removed['free']-before['free'];assert reclaimed>a.storage_bytes,reclaimed
    s.close()
    subprocess.run([sys.executable,'tools/zionctl.py',str(a.package),'--port',a.port,'--log',str(a.output/'reinstall.log')],check=True)
    s.open();restored=state()
    if restored['page']==5:key('OK');restored=state()
    assert restored['apps']==before['apps'] and abs(restored['free']-before['free'])<65536,restored
    result=dict(status='PASS',private_storage_bytes=a.storage_bytes,reclaimed_bytes=reclaimed,
        before=before,removed=removed,reinstalled=restored)
    (a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
finally:
    s.close();(a.output/'serial.log').write_text('\n'.join(lines)+'\n',encoding='utf8')
