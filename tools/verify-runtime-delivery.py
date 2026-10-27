"""Read-only verification of a packaged runtime and matching debugging files."""
import argparse,json,hashlib
from pathlib import Path
from package_minimal import image_length,partitions
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('manifest',type=Path);args=p.parse_args()
manifest=args.manifest.resolve();data=json.loads(manifest.read_text(encoding='utf8'))
archive=Path(data['archive']).resolve();assert archive.is_relative_to(root/'dist')
for relative,record in data['artifacts'].items():
    file=(archive/relative).resolve();assert file.is_relative_to(archive)
    body=file.read_bytes();assert len(body)==record['bytes']
    assert hashlib.sha256(body).hexdigest()==record['sha256'],relative
firmware=manifest.with_suffix('').read_bytes()
assert hashlib.sha256(firmware).hexdigest()==data['firmware_sha256']
assert len(firmware)==data['firmware_bytes']
elf=(archive/'app.elf').read_bytes();app=(archive/'app.bin').read_bytes()
assert hashlib.sha256(elf).hexdigest()==data['elf_sha256']
assert app[176:208]==bytes.fromhex(data['elf_sha256'])
assert image_length(app)==len(app) and firmware[0x10000:0x10000+len(app)]==app
assert image_length((archive/'bootloader.bin').read_bytes())==len((archive/'bootloader.bin').read_bytes())
table=partitions((archive/'partitions.bin').read_bytes());assert table==data['partitions']
for record in data['plugins']:
    file=(manifest.parent/record['file']).resolve();assert file.parent==manifest.parent
    body=file.read_bytes();assert len(body)==record['bytes'] and hashlib.sha256(body).hexdigest()==record['sha256']
    assert body in app
print(json.dumps({'verification':'PASS','firmware_sha256':data['firmware_sha256'],
    'elf_sha256':data['elf_sha256'],'app_bytes':len(app),'firmware_bytes':len(firmware),
    'plugins':data['plugins'],'archive':str(archive)},indent=2))
