"""Collect the verified production host and three same-source application forms."""
import argparse,hashlib,json,shutil
from pathlib import Path
from zion_packages import extract,parse
from package_minimal import image_length
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--app-build',type=Path,required=True);p.add_argument('--host-build',type=Path,required=True)
p.add_argument('--standalone-build',type=Path,required=True);p.add_argument('--version',default='0.4.0');a=p.parse_args()
app=a.app_build.resolve();host=a.host_build.resolve();standalone=a.standalone_build.resolve()
manifest=json.loads((app/'manifest.json').read_text());contract=json.loads((host/'esp-idf/main/idf-compat/zion-idf-compat.json').read_text())
assert manifest['contract']==contract['fingerprint']
assert manifest['host_elf_sha256']==hashlib.sha256((host/'FoloToy-AI-Passport.elf').read_bytes()).hexdigest()
for record in manifest['sources']:assert hashlib.sha256(Path(record['file']).read_bytes()).hexdigest()==record['sha256']
plugin=(app/'app.zion').read_bytes();dual=(app/'app-dual.bin').read_bytes();full=(standalone/'IDF-Compat-Standalone-full.bin').read_bytes()
info=parse(plugin);embedded,kind=extract(dual);assert kind=='dual' and parse(embedded)['flags']==3
assert dual[:-16-len(embedded)]==full and embedded[256:]==plugin[256:]
elf=(standalone/'IDF-Compat-Standalone.elf').read_bytes();raw=(standalone/'IDF-Compat-Standalone.bin').read_bytes()
assert raw[176:208]==hashlib.sha256(elf).digest() and image_length(raw)==len(raw) and full[0x10000:0x10000+len(raw)]==raw
body=(host/'FoloToy-AI-Passport.bin').read_bytes()
assert b'ZION KEY OK' not in body and b'COMPAT_GOLDEN' not in body,'Host must be production and must not embed the fixture'
out=root/'release'/('idf-compat-'+a.version);out.mkdir(exist_ok=True)
records=[]
for name,data in [(f'ZION-IDF-Compat-{a.version}.zion',plugin),(f'IDF-Compat-Dual-{a.version}.bin',dual),(f'IDF-Compat-Standalone-{a.version}.bin',full)]:
    (out/name).write_bytes(data);records.append(dict(file=name,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
shutil.copyfile(app/'manifest.json',out/'build-manifest.json')
result=dict(version=a.version,contract=manifest['contract'],host_elf_sha256=manifest['host_elf_sha256'],
    packages=records,plugin=info,standalone_elf_sha256=hashlib.sha256(elf).hexdigest(),
    source_main_sha256=manifest['sources'][0]['sha256'],source_core_sha256=manifest['sources'][1]['sha256'],
    scope='Ordinary C app_main compatibility fixture plus unchanged third-party 6502 core; not full NES',
    production_hardware_execution='NOT RUN; DeviceTest equivalent was executed',
    documentation='docs/idf-source-compat.zh_CN.md')
(out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
