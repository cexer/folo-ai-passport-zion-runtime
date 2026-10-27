"""Same-source app_main regression on the authorized C3 DeviceTest host."""
import argparse,json,re,time
from pathlib import Path
import serial
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--index',type=int,default=4);p.add_argument('--label',default='native');p.add_argument('--list-only',action='store_true');a=p.parse_args()
out=root/'diagnostics/idf-compat';out.mkdir(exist_ok=True);lines=[]
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.1;s.dtr=False;s.rts=False;s.open()
def until(pattern):
    end=time.monotonic()+15
    while time.monotonic()<end:
        line=s.readline().decode('utf8',errors='replace').strip()
        if not line:continue
        lines.append(line)
        assert not any(x in line for x in ['Guru Meditation','assert failed','COMPAT_FAIL','LOAD_FAIL']),line
        if re.search(pattern,line):return line
    raise TimeoutError(pattern+' / '+str(lines[-8:]))
def send(text):s.write((text+'\n').encode());s.flush()
def key(name):send('ZION KEY '+name);time.sleep(.15)
def state():
    send('ZION STATE');line=until('ZION_UI page=')
    return {k:int(v) for k,v in re.findall(r'(\w+)=(-?\d+)',line)}
try:
    d=state()
    if d['page']==5:key('OK');d=state()
    assert d['page']==0,d
    if a.list_only:
        (out/(a.label+'-result.json')).write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2));raise SystemExit(0)
    for _ in range(d['apps']+2):
        if d['selected']==a.index:break
        key('DOWN');d=state()
    assert d['selected']==a.index
    key('OK');contract=until('CONTRACT_ACCEPT fingerprint=');golden=until('COMPAT_GOLDEN cycles=819539 hash=1738864307 irq=19 nmi=29')
    ready=until('COMPAT_WORKER_READY');count=int(re.search(r'count=(-?\d+)',ready)[1])
    key('UP');until('COMPAT_KEY key=0 count='+str(count+1))
    key('DOWN');until('COMPAT_KEY key=1 count='+str(count))
    key('UP');until('COMPAT_KEY key=0 count='+str(count+1))
    key('OK');saved=until('COMPAT_SAVED count='+str(count+1));key('LONG')
    send('ZION MEM');memory=until('ZION_MEMORY')
    result=dict(status='PASS',contract=contract,golden=golden,initial_count=count,saved_count=count+1,worker_after_app_main_return=True,
                native_LVGL=True,FreeRTOS_queue_task=True,NVS_commit=True,hardware_keys='serial injected',memory=memory)
    (out/(a.label+'-result.json')).write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
finally:
    s.close();(out/(a.label+'-serial.log')).write_text('\n'.join(lines)+'\n',encoding='utf8')
