import argparse,json,hashlib
from pathlib import Path
from zion_packages import make,parse,append_dual
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--plugins',type=Path,default=root/'build-runtime-plugins')
p.add_argument('--standalone',type=Path);p.add_argument('--version',default='0.3.0');p.add_argument('--output',type=Path,default=root/'release/test-apps')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);records=[]
for key,app_id,name,asset in [('sdk-demo',0x80000001,'公共能力测试',b''),('gallery',0x80000002,'相册动画样例',(a.plugins/'gallery.fga').read_bytes())]:
    source=a.plugins/('counter.elf' if key=='sdk-demo' else 'gallery.elf')
    package=make(source.read_bytes(),asset,app_id,key,name)
    file=a.output/f'ZION-{key}-{a.version}.zion';file.write_bytes(package)
    records.append(dict(file=file.name,kind='plugin-only',sha256=hashlib.sha256(package).hexdigest(),**parse(package)))
if a.standalone:
    firmware=a.standalone.read_bytes()
    simple=a.output/f'Counter-Standalone-{a.version}.bin';simple.write_bytes(firmware)
    records.append(dict(file=simple.name,kind='standalone-only',bytes=len(firmware),sha256=hashlib.sha256(firmware).hexdigest()))
    package=make((a.plugins/'counter.elf').read_bytes(),b'',0x80000001,'sdk-demo','公共能力测试',dual=True)
    binary=append_dual(firmware,package)
    dual=a.output/f'Counter-Dual-{a.version}.bin';dual.write_bytes(binary)
    records.append(dict(file=dual.name,kind='dual',bytes=len(binary),sha256=hashlib.sha256(binary).hexdigest(),plugin=parse(package)))
(a.output/f'manifest-{a.version}.json').write_text(json.dumps(records,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print(json.dumps(records,ensure_ascii=False,indent=2))
