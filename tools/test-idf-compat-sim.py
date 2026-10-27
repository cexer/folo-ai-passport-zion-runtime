"""Execute unchanged app_main code with real LVGL/RTOS in PassportSim."""
import argparse,hashlib,json,os,re,subprocess,shutil
from pathlib import Path
from zion_packages import make
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--sim',type=Path,required=True);p.add_argument('--standalone',action='store_true');p.add_argument('--image',type=Path);a=p.parse_args()
sim=a.sim.resolve();out=root/'diagnostics/idf-compat';out.mkdir(exist_ok=True);lines=[];cursor=0;caveats=set();instance=None
build=root/('build-idf-compat-standalone' if a.standalone else 'build-runtime-sim')
prefix='IDF-Compat-Standalone' if a.standalone else 'FoloToy-AI-Passport'
parts={'bin':build/('IDF-Compat-Standalone-full.bin' if a.standalone else 'runtime-sim-full.bin'),
       'elf':build/(prefix+'.elf'),'boot_elf':build/'bootloader/bootloader.elf','pt':build/'partition_table/partition-table.bin'}
assert not a.image or a.standalone
if a.image:parts['bin']=a.image.resolve()
label='dual-independent-sim' if a.image else 'standalone-sim' if a.standalone else 'native-sim'
hashes={k:hashlib.sha256(v.read_bytes()).hexdigest() for k,v in parts.items()}
assert parts['bin'].read_bytes()[0x10000+176:0x10000+208]==bytes.fromhex(hashes['elf'])
name='idf-compat-'+hashes['bin'][:16];corpus=Path(os.environ['APPDATA'])/'passportsim/corpus.toml';old=corpus.read_text()
if '['+name+']' not in old:
    corpus.write_text(old+'\n['+name+']\n'+''.join(f'{k} = "{v.as_posix()}"\n' for k,v in parts.items())+'sha256 = { '+', '.join(f'{k} = "{v}"' for k,v in hashes.items())+' }\n')
def call(*args,selected=True):
    r=subprocess.run([str(sim),'--output','json',*map(str,args),*(['--instance',instance] if selected else [])],capture_output=True,text=True,encoding='utf8',timeout=55)
    d=json.loads(r.stdout)
    if r.returncode==10 and d.get('result')=='pass' and not d.get('receipt',{}).get('hle',{}).get('tripwires_hit'):caveats.add(str(args[0]))
    else:assert r.returncode==0,d
    return d
def read():
    global cursor
    new=[]
    for _ in range(150):
        d=call('serial','read','--cursor',cursor,'--max-bytes',2048);n=d['next_cursor'];new+=d['serial'].get('head',[])+d['serial'].get('tail',[])
        if not d['bytes'] or n<=cursor:break
        cursor=n
    lines.extend(new);text='\n'.join(new)
    assert not any(x in text for x in ['Guru Meditation','assert failed','COMPAT_FAIL']),text[-1000:]
    return text
def until(pattern):
    for _ in range(120):
        text=read()
        if re.search(pattern,text):return text
        call('run','--for','100ms')
    raise TimeoutError(pattern+' / '+str(lines[-10:]))
def send(text):call('serial','write','--hex',(text+'\n').encode().hex())
def install(package):
    r=subprocess.run([os.sys.executable,str(root/'tools/zionctl.py'),str(package),'--sim',str(sim),'--instance',instance,
        '--log',str(out/'sim-install.log')],capture_output=True,text=True,encoding='utf8',timeout=120)
    assert r.returncode==0,r.stdout+r.stderr
def serial_key(key):send('ZION KEY '+key);call('run','--for','200ms')
def enter():
    serial_key('OK');serial_key('DOWN');serial_key('OK')
try:
    start=call('start',name,'--boot-timeout','15s',selected=False);instance=start['instance'];call('run','--for','3s');read()
    if not a.standalone:
        plugin=root/'build-idf-compat-sim';manifest=json.loads((plugin/'manifest.json').read_text());elf=(plugin/'app.elf').read_bytes()
        symbol=('zion_idf_contract_'+manifest['contract']).encode();assert symbol in elf
        bad=elf.replace(symbol,symbol[:-1]+(b'0' if symbol[-1:]!=b'0' else b'1'))
        badfile=plugin/'bad-contract.zion';badfile.write_bytes(make(bad,b'',0x80000005,'idf-compat','IDF Compat CPU'))
        install(badfile);enter();until('LOAD_FAIL stage=elf:unresolved-symbol')
        assert 'COMPAT_GOLDEN' not in '\n'.join(lines)
        call('power','press','--duration','2300ms','--then-run','100ms');call('power','press','--duration','600ms','--then-run','4s');read()
        install(plugin/'app.zion');enter();until('COMPAT_WORKER_READY')
    else:until('COMPAT_WORKER_READY') if not any('COMPAT_WORKER_READY' in x for x in lines) else None
    text='\n'.join(lines);assert 'COMPAT_GOLDEN cycles=819539 hash=1738864307 irq=19 nmi=29' in text
    initial=int(re.findall(r'COMPAT_WORKER_READY count=(-?\d+)',text)[-1])
    call('input','up','click','--duration','120ms','--then-run','500ms');until('COMPAT_KEY key=0 count='+str(initial+1))
    call('input','ok','click','--duration','120ms','--then-run','500ms');until('COMPAT_SAVED count='+str(initial+1))
    shot=call('screenshot');candidates=list((Path(os.environ['LOCALAPPDATA'])/'passportsim/data/artifacts').glob('run-*/'+instance+'/'+shot['path']))
    screenshot=out/(label+'.png');shutil.copy2(max(candidates,key=lambda p:p.stat().st_mtime),screenshot)
    result=dict(status='PASS_WITH_CAVEATS' if caveats else 'PASS',instance=instance,boot=start.get('boot'),golden=True,
        unchanged_app_main=True,real_LVGL=True,physical_sim_keys=True,NVS_commit=True,initial_count=initial,
        bad_contract_rejected=not a.standalone,app_main_return_worker_alive=True,caveats=sorted(caveats),image_sha256=hashes['bin'])
    (out/(label+'-result.json')).write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
finally:
    (out/(label+'-serial.log')).write_text('\n'.join(lines)+'\n',encoding='utf8')
