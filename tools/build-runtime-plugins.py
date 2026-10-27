"""Compile independent RV32IM ET_REL plugins. No ESP-IDF or SDK libraries linked."""
import argparse,json,subprocess,shutil,hashlib,struct,zlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cc',default=shutil.which('riscv32-esp-elf-gcc'))
p.add_argument('--output',type=Path,default=root/'build-runtime-plugins')
args=p.parse_args()
if not args.cc: p.error('Activate the ESP32-C3 toolchain or pass --cc.')
args.output.mkdir(parents=True,exist_ok=True)
flags=['-march=rv32im_zicsr_zifencei','-mabi=ilp32','-mcmodel=medlow','-msmall-data-limit=0',
       '-mno-relax','-fno-pic','-ffreestanding','-fno-builtin','-fno-common',
       '-fno-asynchronous-unwind-tables','-fno-unwind-tables','-ffunction-sections','-fdata-sections',
       '-Os','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',
       '-I'+str(root/'components/zion_sdk/include')]
source=root/'examples/plugins/counter.c';obj=args.output/'counter.o';elf=args.output/'counter.elf'
subprocess.run([args.cc,*flags,'-c',str(source),'-o',str(obj)],check=True)
subprocess.run([args.cc,'-march=rv32im_zicsr_zifencei','-mabi=ilp32','-mno-relax','-nostdlib','-r',str(obj),'-o',str(elf)],check=True)
manifest={'machine':'rv32im','type':'ET_REL','abi':1,'elf':elf.name,'bytes':elf.stat().st_size,
          'sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'sources':[str(source.relative_to(root))],
          'linked_host_libraries':[]}
(args.output/'counter.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest,indent=2))
gallery=root/'examples/plugins/gallery'
sources=[gallery/'gallery.c',gallery/'upstream/fga_format.c',gallery/'upstream/animation_player.c']
objects=[]
for source in sources:
    obj=args.output/('gallery-'+source.stem+'.o');objects.append(obj)
    subprocess.run([args.cc,*flags,'-I'+str(gallery/'upstream'),'-c',str(source),'-o',str(obj)],check=True)
elf=args.output/'gallery.elf'
subprocess.run([args.cc,'-march=rv32im_zicsr_zifencei','-mabi=ilp32','-mno-relax','-nostdlib','-r',*map(str,objects),'-o',str(elf)],check=True)
manifest={'machine':'rv32im','type':'ET_REL','abi':1,'elf':elf.name,'bytes':elf.stat().st_size,
          'sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),
          'sources':[str(s.relative_to(root)) for s in sources],'linked_host_libraries':[],
          'provenance':json.loads((gallery/'source.json').read_text())}
(args.output/'gallery.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest,indent=2))
# Deterministic, original geometry fixture: no borrowed photographs or movie media.
colors=[0xF800,0x07E0,0x001F,0xFFE0,0x07FF,0xF81F]
pixels=[colors[((x//40)+(y//80))%len(colors)] for y in range(320) for x in range(240)]
full=struct.pack('<76800H',*pixels)
payload=struct.pack('<BBHI',0,0,300,len(full))+full
for color in [0xFFFF,0x0000]:
    delta=b'';cursor=0
    for y in range(144,176):
        at=y*240+104
        delta+=struct.pack('<IH',at-cursor,32)+struct.pack('<32H',*([color]*32));cursor=at+32
    payload+=struct.pack('<BBHI',1,0,300,len(delta))+delta
fixture=struct.pack('<4s6H3I8s',b'FGA1',1,36,240,320,3,0,900,len(payload),zlib.crc32(payload),bytes(8))+payload
(args.output/'gallery.fga').write_bytes(fixture)
(args.output/'gallery-asset.json').write_text(json.dumps({'format':'FGA1','bytes':len(fixture),
    'sha256':hashlib.sha256(fixture).hexdigest(),'frames':3,'duration_ms':900,'origin':'generated original test pattern'},indent=2)+'\n')
