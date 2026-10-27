"""Build three package forms for the same SoundLab and NES CPU application sources."""
from pathlib import Path
import argparse,json,hashlib
from zion_packages import make,append_dual,parse
from package_minimal import image_length,partitions
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--soundlab-build',type=Path,required=True);p.add_argument('--nescpu-build',type=Path,required=True)
p.add_argument('--plugins',type=Path,default=root/'build-runtime-plugins');p.add_argument('--output',type=Path,default=root/'release/test-apps');p.add_argument('--version',default='0.3.0');a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True);records=[]
for title,key,identity,build in [('SoundLab','soundlab',0x80000003,a.soundlab_build),('NESCPU','nescpu',0x80000004,a.nescpu_build)]:
    project=title+'-Standalone';app=(build/(project+'.bin')).read_bytes();elf=(build/(project+'.elf')).read_bytes();full=(build/(project+'-full.bin')).read_bytes()
    assert image_length(app)==len(app) and app[176:208]==hashlib.sha256(elf).digest()
    factory=next(x for x in partitions((build/'partition_table/partition-table.bin').read_bytes()) if x['type']==0 and x['subtype']==0)
    assert factory['offset']==0x10000 and len(app)<=factory['size'] and full[0x10000:0x10000+len(app)]==app
    plugin=(a.plugins/(key+'.elf')).read_bytes();name='ADPCM DSP' if key=='soundlab' else '6502 CPU'
    package=make(plugin,b'',identity,key,name);dual_package=make(plugin,b'',identity,key,name,dual=True);dual=append_dual(full,dual_package)
    assert len(dual)<=0x800000
    for filename,kind,data in [(f'ZION-{title}-{a.version}.zion','plugin-only',package),
        (f'{title}-Dual-{a.version}.bin','dual',dual),(f'{title}-Standalone-{a.version}.bin','standalone-only',full)]:
        (a.output/filename).write_bytes(data)
        records.append(dict(file=filename,kind=kind,application=key,bytes=len(data),sha256=hashlib.sha256(data).hexdigest(),
            plugin=parse(dual_package if kind=='dual' else package) if kind!='standalone-only' else None,
            standalone_app_bytes=len(app),standalone_elf_sha256=hashlib.sha256(elf).hexdigest()))
manifest=dict(version=a.version,host_embeds_examples=False,records=records,
    optimization=dict(soundlab='Os module; static standalone',nescpu='Os native module, O2 standalone CPU core'),
    scope='Source-core porting workloads; not complete original applications',
    licenses={key:json.loads((root/'examples/plugins'/key/'source.json').read_text()) for key in ['soundlab','nescpu']})
(a.output/f'external-apps-{a.version}.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print(json.dumps([dict(file=x['file'],bytes=x['bytes'],kind=x['kind']) for x in records],indent=2))
