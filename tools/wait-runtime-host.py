"""Bounded recovery for the already-authorized, asleep development device.

Flash only a frozen verified host build when its known USB port reappears.
No application tests run here; the normal sequential scenario runner owns those.
"""
import argparse,hashlib,json,subprocess,sys,time
from pathlib import Path
from serial.tools import list_ports
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--port',required=True)
p.add_argument('--output',type=Path,required=True);p.add_argument('--timeout',type=int,default=600);a=p.parse_args()
assert 1<=a.timeout<=600
a.output.mkdir(parents=True,exist_ok=True)
manifest=json.loads((a.build/'frozen-sha256.json').read_text())
def verify():
    for name,digest in manifest.items():assert hashlib.sha256((a.build/name).read_bytes()).hexdigest()==digest,name
verify();end=time.monotonic()+a.timeout
while time.monotonic()<end:
    ports=[s for s in list_ports.comports() if s.device==a.port and s.vid==0x303a and s.pid==0x1001]
    if ports:
        verify()
        r=subprocess.run([sys.executable,str(Path(__file__).with_name('flash-community-validation.py')),
            str(a.build),'--port',a.port,'--output',str(a.output/'flash')],capture_output=True,text=True,encoding='utf8')
        (a.output/'result.log').write_text(r.stdout+r.stderr,encoding='utf8')
        (a.output/'result.json').write_text(json.dumps({'status':'PASS' if r.returncode==0 else 'FAIL','exit':r.returncode,'port':a.port})+'\n')
        print('FROZEN_HOST_FLASH_EXIT',r.returncode,flush=True);sys.exit(r.returncode)
    time.sleep(1)
(a.output/'result.json').write_text(json.dumps({'status':'NOT RUN','reason':'known USB port did not reappear within bounded wait'})+'\n')
print('DEVICE_WAKE_TIMEOUT',flush=True);sys.exit(2)
