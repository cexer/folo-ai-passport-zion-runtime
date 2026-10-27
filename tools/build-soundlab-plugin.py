import argparse,subprocess,json,hashlib,shutil
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cc',default=shutil.which('riscv32-esp-elf-gcc'));a=p.parse_args()
assert a.cc,'Activate ESP-IDF first'
folder=root/'examples/plugins/soundlab';out=root/'build-runtime-plugins';out.mkdir(exist_ok=True)
flags=['-march=rv32im_zicsr_zifencei','-mabi=ilp32','-mcmodel=medlow','-msmall-data-limit=0','-mno-relax','-fno-pic',
    '-ffreestanding','-fno-builtin','-fno-common','-fno-asynchronous-unwind-tables','-fno-unwind-tables',
    '-ffunction-sections','-fdata-sections','-Os','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',
    '-I'+str(root/'components/zion_sdk/include'),'-I'+str(folder),'-I'+str(folder/'upstream')]
objects=[]
for source in [folder/'soundlab.c',folder/'upstream/nabo_voice_fixed.c']:
    obj=out/('soundlab-'+source.stem+'.o');objects.append(str(obj));subprocess.run([a.cc,*flags,'-c',str(source),'-o',str(obj)],check=True)
elf=out/'soundlab.elf'
subprocess.run([a.cc,'-march=rv32im_zicsr_zifencei','-mabi=ilp32','-mno-relax','-nostdlib','-r',*objects,'-o',str(elf)],check=True)
manifest=dict(file=elf.name,bytes=elf.stat().st_size,sha256=hashlib.sha256(elf.read_bytes()).hexdigest(),
    source=json.loads((folder/'source.json').read_text()),builtin=False,scope='ADPCM and twelve voice DSP effects, PCM, persistence')
(out/'soundlab.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(manifest,indent=2))
