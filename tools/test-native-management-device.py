"""Authorized C3 menu regression; removes/reinstalls the external NES CPU fixture."""
import argparse,json,re,time,subprocess,sys
from pathlib import Path
import serial
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--port',required=True);a=p.parse_args()
out=root/'diagnostics/native-management';out.mkdir(exist_ok=True);lines=[]
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.1;s.dtr=False;s.rts=False;s.open()
def until(pattern):
    end=time.monotonic()+15
    while time.monotonic()<end:
        line=s.readline().decode('utf8',errors='replace').strip()
        if not line:continue
        lines.append(line)
        assert not any(x in line for x in ['Guru Meditation','assert failed','LOAD_FAIL']),line
        if re.search(pattern,line):return line
    raise TimeoutError(pattern+' / '+str(lines[-8:]))
def send(text):s.write((text+'\n').encode());s.flush()
def key(name):send('ZION KEY '+name);time.sleep(.15)
def state():
    send('ZION STATE');line=until('ZION_UI page=')
    return {k:int(v) for k,v in re.findall(r'(\w+)=(-?\d+)',line)}
def settled():
    for _ in range(60):
        d=state()
        if not d['busy'] and d['page']==0:return d
        time.sleep(.1)
    raise TimeoutError(str(d))
try:
    before=state()
    if before['page']==5:key('OK');before=state()
    assert before['page']==0 and before['apps']==6 and before['default']==-1,before
    while state()['selected']!=3:key('DOWN')
    key('LONG');key('DOWN');key('OK');until('ZION_ACTION action=2')
    default=state();assert default['default']<=-100 and default['result']==0,default
    key('DOWN');key('OK');assert state()['page']==2
    key('OK');assert state()['page']==1
    cancel=state();assert cancel['apps']==6 and cancel['free']==before['free'],cancel
    key('DOWN');key('DOWN');key('OK');key('DOWN');key('OK');until('ZION_ACTION action=3')
    removed=settled();assert removed['apps']==5 and removed['free']>before['free'] and removed['default']==-1 and removed['result']==0,removed
    key('LONG');key('DOWN');key('DOWN');key('OK');blocked=state();assert blocked['page']==1 and blocked['selected']==2 and blocked['apps']==5,blocked
    key('LONG');s.close()
    r=subprocess.run([sys.executable,str(root/'tools/zionctl.py'),str(root/'release/test-apps/ZION-NESCPU-0.3.0.zion'),
        '--port',a.port,'--log',str(out/'reinstall.log')],capture_output=True,text=True,timeout=120)
    assert r.returncode==0,r.stdout+r.stderr
    s.open();key('OK');restored=state();assert restored['apps']==6 and restored['free']==before['free'] and restored['default']==-1,restored
    result=dict(port=a.port,default_set=True,delete_cancel_preserves=True,delete_default_clears=True,
        reclaimed_flash_bytes=removed['free']-before['free'],builtin_delete_blocked=True,reinstall_restores_free=True,
        before=before,after_delete=removed,final=restored,keys='serial injected; power-on escape covered in simulator')
    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
finally:
    s.close();(out/'serial.log').write_text('\n'.join(lines)+'\n',encoding='utf8')
