"""Select a known installed package through the real launcher key path."""
import argparse,json,re,time
from pathlib import Path
import serial
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--index',type=int,required=True)
p.add_argument('--id',type=int,required=True);p.add_argument('--ready',required=True);p.add_argument('--output',type=Path,required=True)
p.add_argument('--proof',action='append',default=[],help='Additional real device log evidence to retain before closing the port')
p.add_argument('--load-timeout',type=float,default=30,help='Bounded ELF/cache relocation observation window')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);lines=[]
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.1;s.dtr=False;s.rts=False;s.open()
def send(cmd):s.write((cmd+'\n').encode());s.flush()
def wait(pattern,seconds=30):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        line=s.readline().decode('utf8',errors='replace').strip()
        if not line:continue
        lines.append(line)
        assert not any(x in line for x in ['Guru Meditation','assert failed','Stack smashing','LOAD_FAIL']),line
        if re.search(pattern,line):return line
        # USB VFS/log writers can split a protocol record across newlines.
        # Preserve both raw fragments in the log; only reassemble adjacent
        # fragments with a known protocol prefix, without inventing bytes.
        if len(lines)>1 and (lines[-2].startswith('ZION_') or (pattern.startswith('ZION_') and pattern.startswith(lines[-2]))) and not re.match(r'^(ZION_|[IWE] \()',line):
            joined=lines[-2]+line
            if re.search(pattern,joined):return joined
    raise TimeoutError(pattern+' / '+str(lines[-10:]))
def key(name):send('ZION KEY '+name);time.sleep(.15)
def state():
    # Snoopy installation left an install-result page. Each injected key also
    # emits ZION_UI, so consuming it as the next query reply can lag by one
    # selection and accidentally open the community QR. Drain and retain old
    # output before issuing a new state query; do not discard device evidence.
    while s.in_waiting:
        line=s.readline().decode('utf8',errors='replace').strip()
        if line:lines.append(line)
    send('ZION STATE');return {k:int(v) for k,v in re.findall(r'(\w+)=(-?\d+)',wait('ZION_UI page='))}
try:
    d=state()
    if d['page']==5:key('LONG');d=state()
    assert d['page']==0 and d['apps']>a.index,d
    for _ in range(d['apps']+2):
        if d['selected']==a.index:break
        key('DOWN');d=state()
    assert d['selected']==a.index,d
    key('OK');loaded=wait(r'LOADED source=.*id='+str(a.id)+r'\b',a.load_timeout)
    ready=wait(a.ready,45);proofs={}
    for pattern in a.proof:
        prior=next((line for line in lines if re.search(pattern,line)),None)
        proofs[pattern]=prior or wait(pattern,60)
    time.sleep(.5)  # allow app_main's logging/task cleanup to finish first
    send('ZION MEM');memory=wait('ZION_MEMORY')
    result=dict(status='PASS',loaded=loaded,ready=ready,memory=memory)
    if proofs:result['proofs']=proofs
    (a.output/'launch.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
finally:
    s.close();raw=re.sub(r'(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}','[MAC REDACTED]','\n'.join(lines))
    raw=re.sub(r'(?m)(连接 Wi-Fi: ).*',r'\1[REDACTED]',raw)
    raw=re.sub(r'(?im)^.*(?:Global UID|Device ID|设备.?ID|GUID[:=]).*$', '[IDENTITY REDACTED]',raw)
    (a.output/'launch.log').write_text(raw+'\n',encoding='utf8')
