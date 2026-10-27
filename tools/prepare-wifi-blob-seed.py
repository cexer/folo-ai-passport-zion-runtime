"""Private test-only NVS seed using wifi_config_t's actual target ABI offsets.
Never place the generated credential-bearing files in a firmware or package.
"""
import argparse,json,re,struct,subprocess,sys,csv
from pathlib import Path
from elftools.elf.elffile import ELFFile
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--wifi-json',type=Path,required=True);p.add_argument('--namespace',required=True);p.add_argument('--key',required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
out=a.output.resolve();assert not out.is_relative_to(root),'Credential seed must be outside firmware checkout'
assert re.fullmatch(r'[A-Za-z0-9_]{1,15}',a.namespace) and re.fullmatch(r'[A-Za-z0-9_]{1,15}',a.key)
out.mkdir(parents=True,exist_ok=True);build=a.build.resolve()
record=next(x for x in json.loads((build/'compile_commands.json').read_text()) if x['file'].replace('\\','/').endswith('/main/zion_idf_compat.c'))
command=record['command'];cc=command.split(' -D',1)[0].strip('"')
includes=[s.strip('"') for s in re.findall(r'(?:^|\s)-I("[^"]*"|\S+)',command)]
defines=[s.replace(chr(92)+chr(34),chr(34)) for s in re.findall(r'(?:^|\s)(-D\S+)',command)]
source=out/'wifi-abi.c';obj=out/'wifi-abi.o'
source.write_text('#include <stdint.h>\n#include <stddef.h>\n#include "esp_wifi.h"\nconst uint32_t facts[]={sizeof(wifi_config_t),offsetof(wifi_config_t,sta.ssid),sizeof(((wifi_config_t*)0)->sta.ssid),offsetof(wifi_config_t,sta.password),sizeof(((wifi_config_t*)0)->sta.password)};\n')
subprocess.run([cc,'@'+str(build/'toolchain/cflags'),'-DESP_PLATFORM',*defines,*['-I'+x for x in includes],'-c',str(source),'-o',str(obj)],check=True)
with obj.open('rb') as f:
    elf=ELFFile(f);sym=elf.get_section_by_name('.symtab').get_symbol_by_name('facts')[0]
    data=elf.get_section(sym['st_shndx']).data();facts=struct.unpack_from('<5I',data,sym['st_value'])
size,ssid_at,ssid_size,password_at,password_size=facts
assert 96<=size<=4096 and ssid_size==32 and password_size==64 and password_at+password_size<=size
wifi=json.loads(a.wifi_json.read_text(encoding='utf-8-sig'));ssid=wifi['ssid'].encode('utf8');password=wifi['password'].encode('utf8')
assert 0<len(ssid)<=ssid_size and len(password)<password_size
body=bytearray(size);body[ssid_at:ssid_at+len(ssid)]=ssid;body[password_at:password_at+len(password)]=password
blob=out/'station.bin';blob.write_bytes(body)
table=out/'seed.csv'
with table.open('w',newline='',encoding='utf8') as f:
    w=csv.writer(f);w.writerow(['key','type','encoding','value']);w.writerow([a.namespace,'namespace','','']);w.writerow([a.key,'file','binary',str(blob)])
idf=Path(json.loads((build/'project_description.json').read_text())['idf_path'])
run=subprocess.run([sys.executable,str(idf/'components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py'),'generate',str(table),str(out/'nvs.bin'),'0x6000'],capture_output=True,text=True)
(out/'generator.log').write_text(run.stdout+run.stderr)
assert run.returncode==0,'NVS generator failed; inspect private generator.log'
assert (out/'nvs.bin').stat().st_size==0x6000
(out/'abi.json').write_text(json.dumps(dict(wifi_config_bytes=size,ssid_offset=ssid_at,password_offset=password_at))+'\n')
print(json.dumps(dict(private_seed=str(out/'nvs.bin'),bytes=0x6000,wifi_config_bytes=size)))
