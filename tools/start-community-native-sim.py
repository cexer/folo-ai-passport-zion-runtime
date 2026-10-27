"""Start a native community image with an immutable matching debug snapshot."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
from package_minimal import partitions,independent_app_partition,image_length
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--image',type=Path,required=True)
p.add_argument('--sim',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
b=a.build.resolve();a.output.mkdir(parents=True,exist_ok=True);snapshot=(a.output/'artifacts').resolve();snapshot.mkdir(exist_ok=True)
description=json.loads((b/'project_description.json').read_text());app=Path(description['app_bin']).name
elf=b/Path(app).with_suffix('.elf');body=a.image.read_bytes()
table=partitions((b/'partition_table/partition-table.bin').read_bytes());factory=independent_app_partition(body,table)
native=(b/app).read_bytes();assert body[factory['offset']:factory['offset']+len(native)]==native
assert native[176:208]==hashlib.sha256(elf.read_bytes()).digest() and image_length(native)==len(native)
files={'bin':a.image,'elf':elf,'boot_elf':b/'bootloader/bootloader.elf','pt':b/'partition_table/partition-table.bin'}
parts={};hashes={}
for key,file in files.items():
    target=snapshot/(key+file.suffix);shutil.copyfile(file,target);parts[key]=target;hashes[key]=hashlib.sha256(target.read_bytes()).hexdigest()
map_file=elf.with_suffix('.map')
if map_file.exists():shutil.copyfile(map_file,snapshot/'app.map')
(snapshot/'manifest.json').write_text(json.dumps(hashes,indent=2)+'\n')
name='community-native-'+hashes['elf'][:12]
corpus=Path(os.environ['APPDATA'])/'passportsim/corpus.toml';old=corpus.read_text(encoding='utf8') if corpus.exists() else ''
if '['+name+']' not in old:
    entry='\n['+name+']\n'+''.join(f'{k} = "{v.as_posix()}"\n' for k,v in parts.items())
    entry+='sha256 = { '+', '.join(f'{k} = "{v}"' for k,v in hashes.items())+' }\n'
    corpus.parent.mkdir(parents=True,exist_ok=True);corpus.write_text(old+entry,encoding='utf8')
r=subprocess.run([str(a.sim.resolve()),'--output','json','start',name,'--boot-timeout','15s'],capture_output=True,text=True,encoding='utf8',timeout=55)
(a.output/'start.json').write_text(r.stdout,encoding='utf8');d=json.loads(r.stdout)
assert r.returncode in (0,10) and not d.get('receipt',{}).get('hle',{}).get('tripwires_hit'),d
(a.output/'instance.txt').write_text(d['instance'])
print(json.dumps({'instance':d['instance'],'boot':d.get('boot'),'elf_sha256':hashes['elf']}))
