import argparse,subprocess,json,hashlib,shutil
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cc',default=shutil.which('riscv32-esp-elf-gcc'));p.add_argument('--opt',choices=['O2','Os'],default='Os');a=p.parse_args();assert a.cc
folder=root/'examples/plugins/nescpu';out=root/'build-runtime-plugins';out.mkdir(exist_ok=True)
flags=['-march=rv32im_zicsr_zifencei','-mabi=ilp32','-mcmodel=medlow','-msmall-data-limit=0','-mno-relax','-fno-pic',
    '-ffreestanding','-fno-builtin','-fno-common','-fno-asynchronous-unwind-tables','-fno-unwind-tables',
    '-ffunction-sections','-fdata-sections','-'+a.opt,'-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',
    '-I'+str(root/'components/zion_sdk/include'),'-I'+str(folder),'-I'+str(folder/'upstream'),'-I'+str(folder/'portable')]
objects=[]
for source in [folder/'nescpu.c',folder/'upstream/nes6502.c']:
    obj=out/('nescpu-'+source.stem+'.o');objects.append(str(obj));extra=['-Wno-discarded-qualifiers'] if source.name=='nes6502.c' else []
    subprocess.run([a.cc,*flags,*extra,'-c',str(source),'-o',str(obj)],check=True)
elf=out/'nescpu.elf'
subprocess.run([a.cc,'-march=rv32im_zicsr_zifencei','-mabi=ilp32','-mno-relax','-nostdlib','-r',*objects,'-o',str(elf)],check=True)
manifest=dict(file=elf.name,optimization=a.opt,bytes=elf.stat().st_size,sha256=hashlib.sha256(elf.read_bytes()).hexdigest(),
    source=json.loads((folder/'source.json').read_text()),builtin=False)
(out/'nescpu.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(manifest,indent=2))
