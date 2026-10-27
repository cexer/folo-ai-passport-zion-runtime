"""External package installation over native USB serial or PassportSim USB."""
import argparse,json,re,subprocess,time,hashlib
from pathlib import Path
from zion_packages import extract,parse
p=argparse.ArgumentParser();p.add_argument('file',type=Path);p.add_argument('--port');p.add_argument('--sim',type=Path);p.add_argument('--instance');p.add_argument('--log',type=Path);p.add_argument('--legacy',action='store_true')
a=p.parse_args();source=a.file.read_bytes();package,kind=extract(source)
legacy=package is None or a.legacy
if legacy:
    from package_minimal import partitions,image_length
    table=partitions(source[0x8000:0x8c00]);app=next(x for x in table if x['type']==0 and x['subtype']==0)
    assert not [x for x in table if x['type']==1 and x['subtype'] not in (0,1,2)],'USB legacy example route requires firmware without private DATA partitions'
    package=source[app['offset']:];package=package[:image_length(package)]
    info=dict(mode='legacy-complete-firmware',app_bytes=len(package),app_sha256=hashlib.sha256(package).hexdigest())
else: info=parse(package)
lines=[];cursor=0;pending=[]
def sim(*args):
    r=subprocess.run([str(a.sim.resolve()),'--output','json',*map(str,args),'--instance',a.instance],capture_output=True,text=True,encoding='utf8',timeout=45)
    d=json.loads(r.stdout)
    if r.returncode not in (0,10): raise RuntimeError(str(d)[:500])
    return d
if a.sim:
    def write(data): sim('serial','write','--hex',data.hex())
    def read():
        global cursor
        sim('run','--for','80ms')
        d=sim('serial','read','--cursor',cursor,'--max-bytes','2048');cursor=d['next_cursor']
        return d['serial'].get('head',[])+d['serial'].get('tail',[])
    # Ignore old console history while retaining a monotonic cursor.
    for _ in range(200):
        d=sim('serial','read','--cursor',cursor,'--max-bytes','4096');new=d['next_cursor']
        if not d['bytes'] or new<=cursor: break
        cursor=new
    connection=None
else:
    import serial
    if not a.port: p.error('--port or --sim/--instance is required')
    connection=serial.Serial();connection.port=a.port;connection.baudrate=115200;connection.timeout=.1;connection.write_timeout=20
    connection.dtr=False;connection.rts=False;connection.open();connection.reset_input_buffer()
    def write(data): connection.write(data);connection.flush()
    serial_pending=b''
    def read():
        global serial_pending
        serial_pending+=connection.read(max(1,min(65536,connection.in_waiting)))
        complete=[]
        while b'\n' in serial_pending:
            raw,serial_pending=serial_pending.split(b'\n',1)
            line=raw.decode('utf8',errors='replace').strip()
            if line:complete.append(line)
        return complete

live_log=None
if a.log:
    a.log.parent.mkdir(parents=True,exist_ok=True)
    live_log=a.log.open('w',encoding='utf8')
def until(pattern,seconds=20):
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline:
        if not pending: pending.extend(read())
        if not pending: continue
        line=pending.pop(0);lines.append(line)
        if live_log:live_log.write(line+'\n');live_log.flush()
        if 'Guru Meditation' in line or 'assert failed' in line:raise RuntimeError('Device fault during upload: '+line)
        if 'ZION_UPLOAD_DONE result=-' in line: raise RuntimeError(line)
        if re.search(pattern,line): return
    raise TimeoutError(pattern+' / '+str(lines[-8:]))
try:
    command=f'ZION LEGACY {len(package)} {hashlib.sha256(package).hexdigest()}' if legacy else f'ZION INSTALL {len(package)}'
    write((command+'\n').encode());until(r'ZION_UPLOAD_READY')
    start=0
    if not legacy:
        write(package[:256]);until(r'ZION_UPLOAD_ACK bytes=256\b');start=256
    for at in range(start,len(package),1024):
        chunk=package[at:at+1024];write(chunk);until(r'ZION_UPLOAD_ACK bytes='+str(at+len(chunk))+r'\b')
    until(r'ZION_UPLOAD_DONE result=0\b',seconds=90)
    print(json.dumps(dict(result='PASS',source_kind=kind,installed=info),ensure_ascii=False,indent=2))
finally:
    if connection: connection.close()
    if live_log:live_log.close()
