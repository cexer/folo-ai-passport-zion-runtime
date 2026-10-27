"""Minimal platform acceptance: C/C++ linkage, constructors and task primitives."""
import argparse,json,re,time
from pathlib import Path
import serial
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--index',type=int,default=6);a=p.parse_args()
out=root/'diagnostics/link-platform';out.mkdir(exist_ok=True);lines=[]
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.1;s.dtr=False;s.rts=False;s.open()
def send(text):s.write((text+'\n').encode());s.flush()
def until(pattern):
    end=time.monotonic()+15
    while time.monotonic()<end:
        line=s.readline().decode('utf8',errors='replace').strip()
        if not line:continue
        lines.append(line)
        assert not any(x in line for x in ['Guru Meditation','assert failed','LOAD_FAIL','CPP_CAPABILITY_FAIL']),line
        if re.search(pattern,line):return line
    raise TimeoutError(pattern+' / '+str(lines[-10:]))
def key(name):send('ZION KEY '+name);time.sleep(.15)
def state():
    send('ZION STATE');line=until('ZION_UI page=')
    return {k:int(v) for k,v in re.findall(r'(\w+)=(-?\d+)',line)}
try:
    d=state()
    if d['page']==5:key('OK');d=state()
    assert d['page']==0 and d['apps']>a.index,d
    for _ in range(d['apps']+2):
        if d['selected']==a.index:break
        key('DOWN');d=state()
    assert d['selected']==a.index;key('OK');ctor=until('INITIALIZERS count=')
    assert int(re.search(r'count=(\d+)',ctor)[1])>0,ctor
    until('CPP_CONSTRUCTOR_GUARD_MUTEX_C_LINK_PASS');until('CPP_QUEUE_TASK_PASS value=42');until('CPP_CAPABILITY_PASS')
    send('ZION MEM');memory=until('ZION_MEMORY')
    result=dict(status='PASS',constructor=ctor,C_Cpp_linkage=True,global_constructor=True,local_static_guard=True,
        std_string=True,std_mutex=True,FreeRTOS_task_queue=True,memory=memory,scope='Platform primitives only; no business rules')
    (out/'capability-device-result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
finally:s.close();(out/'capability-device-serial.log').write_text('\n'.join(lines)+'\n',encoding='utf8')
