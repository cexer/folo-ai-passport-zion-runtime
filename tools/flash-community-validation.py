"""Flash a locally built independent test image, preserving the card identity.

Only for the currently authorized development device, not a production flasher.
Read flasher_args as data and verify ELF binding and partition geometry first.
"""
import argparse,hashlib,json,re,subprocess,sys,time
from pathlib import Path
import serial
from package_minimal import image_length,partitions
p=argparse.ArgumentParser();p.add_argument('build',type=Path);p.add_argument('--port',required=True)
p.add_argument('--output',type=Path,required=True);p.add_argument('--image',type=Path)
p.add_argument('--seed-nvs',type=Path,help='Local-only test settings; never packaged in the firmware')
p.add_argument('--reset-store',action='store_true',help='Authorized fresh ZION test layout: reset only its catalog, preserving cardid and NVS')
a=p.parse_args();b=a.build.resolve()
args=json.loads((b/'flasher_args.json').read_text());segments=[];records=[]
table=partitions((b/'partition_table/partition-table.bin').read_bytes())
app_name=Path(json.loads((b/'project_description.json').read_text())['app_bin']).name
app_offsets=[int(off,0) for off,name in args['flash_files'].items() if Path(name).name==app_name]
assert len(app_offsets)==1
factory=next(x for x in table if x['type']==0 and x['offset']==app_offsets[0])
for offset,name in args['flash_files'].items():
    file=(b/name).resolve();assert file.is_relative_to(b)
    data=file.read_bytes();at=int(offset,0)
    assert at+len(data)<=0x356000 or at>=0x35a000,'Identity region overlap'
    if at==factory['offset']:
        elf=file.with_suffix('.elf').read_bytes()
        assert image_length(data)==len(data) and len(data)<=factory['size']
        assert data[176:208]==hashlib.sha256(elf).digest()
    records.append(dict(offset=at,file=str(file),bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
    segments += [offset,str(file)]
source_image=None
if a.image:
    body=a.image.read_bytes()
    for record in records:
        at=record['offset'];assert body[at:at+record['bytes']]==Path(record['file']).read_bytes()
    from zion_packages import extract
    embedded,kind=extract(body);assert kind=='dual' and embedded
    source_image=dict(file=str(a.image.resolve()),bytes=len(body),sha256=hashlib.sha256(body).hexdigest())
    if len(body)<=0x356000:
        segments=['0x0',str(a.image.resolve())]
        records=[dict(offset=0,file=str(a.image.resolve()),bytes=len(body),sha256=hashlib.sha256(body).hexdigest())]
        source_image['write_mode']='raw-hybrid'
    else:
        # The trailer is not needed by independent boot. Never write it over
        # cardid: flash only the verified native segments from the hybrid prefix.
        source_image['write_mode']='verified-native-segments-from-hybrid'
a.output.mkdir(parents=True,exist_ok=True)
if a.reset_store:
    assert not a.image,'Fresh-catalog reset is only supported for segmented host flashing'
    store=next(x for x in table if x['label']=='store')
    assert store['type']==1 and store['subtype']==2 and store['offset']==0x35a000 and store['size']==0x6000
    blank=a.output.resolve()/'empty-test-store.bin';blank.write_bytes(b'\xff'*store['size'])
    segments += [hex(store['offset']),str(blank)]
    records.append(dict(offset=store['offset'],file=str(blank),bytes=store['size'],sha256=hashlib.sha256(blank.read_bytes()).hexdigest(),purpose='authorized fresh test catalog'))
(a.output/'flash-manifest.json').write_text(json.dumps(dict(segments=records,partitions=table,source_image=source_image),indent=2)+'\n')
run=subprocess.run([sys.executable,'-m','esptool','--chip','esp32c3','--port',a.port,'--baud','460800',
    '--before','usb-reset','--after','hard-reset','write-flash','--flash-mode','dio','--flash-size','8MB','--flash-freq','80m',*segments],
    capture_output=True,text=True,encoding='utf8',errors='replace',timeout=180)
safe=re.sub(r'(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}','[MAC REDACTED]',run.stdout+run.stderr)
(a.output/'flash.log').write_text(safe,encoding='utf8');assert run.returncode==0,safe[-1500:]
if a.seed_nvs:
    nvs=next(x for x in table if x['label']=='nvs')
    assert nvs['type']==1 and nvs['subtype']==2
    assert len(a.seed_nvs.read_bytes())==nvs['size']
    assert nvs['offset']+nvs['size']<=0x356000,'Identity overlap'
    run=subprocess.run([sys.executable,'-m','esptool','--chip','esp32c3','--port',a.port,'--baud','460800',
        '--before','usb-reset','--after','hard-reset','write-flash',hex(nvs['offset']),str(a.seed_nvs.resolve())],
        capture_output=True,text=True,timeout=60)
    assert run.returncode==0,'Local NVS seed flash failed'
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.2;s.dtr=False;s.rts=False
for _ in range(40):
    try:s.open();break
    except serial.SerialException:time.sleep(.2)
else:raise RuntimeError('USB did not reappear')
lines=[];end=time.monotonic()+20
try:
    while time.monotonic()<end:
        line=s.readline().decode('utf8',errors='replace').strip()
        if line:lines.append(line)
finally:s.close()
safe=re.sub(r'(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}','[MAC REDACTED]','\n'.join(lines))
safe=re.sub(r'PASS=\S+','PASS=[REDACTED]',safe)
safe=re.sub(r'(wifi:connected with ).*?(, aid =)',r'\1[REDACTED]\2',safe)
safe=re.sub(r'(?m)(连接 Wi-Fi: ).*',r'\1[REDACTED]',safe)
safe=re.sub(r'(?im)^.*(?:Global UID|Device ID|设备.?ID|GUID[:=]).*$', '[IDENTITY REDACTED]',safe)
(a.output/'startup.log').write_text(safe+'\n',encoding='utf8')
assert not any(x in safe for x in ['Guru Meditation','assert failed','Stack smashing']),safe[-1500:]
print(json.dumps(dict(flash='PASS',startup_tail=safe[-1500:]),ensure_ascii=False))
