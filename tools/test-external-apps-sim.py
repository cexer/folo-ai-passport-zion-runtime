"""External installation + real DSP/CPU execution in the radio/PCM-free profile."""
import argparse,subprocess,json,time,re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--sim',type=Path,required=True);p.add_argument('--build',type=Path,default=root/'build-runtime-sim');a=p.parse_args()
sim=a.sim.resolve();out=root/'diagnostics/external-apps-sim';out.mkdir(exist_ok=True);lines=[];cursor=0;caveats=set();screens=[]
start=subprocess.run([__import__('sys').executable,str(root/'tools/test-runtime-sim.py'),'--sim',str(sim),'--build',str(a.build.resolve()),'--start-only'],capture_output=True,text=True,check=True)
boot=json.loads(start.stdout.strip().splitlines()[-1]);instance=boot['instance'];assert not boot.get('tripwires'),boot
def call(*args):
    r=subprocess.run([str(sim),'--output','json',*map(str,args),'--instance',instance],capture_output=True,text=True,encoding='utf8',timeout=55)
    d=json.loads(r.stdout)
    if r.returncode==10 and d.get('result')=='pass' and not d.get('receipt',{}).get('hle',{}).get('tripwires_hit'):caveats.add(str(args[0]))
    elif r.returncode:raise RuntimeError(str(d)[:600])
    return d
def read():
    global cursor
    new=[]
    for _ in range(100):
        d=call('serial','read','--cursor',cursor,'--max-bytes',2048);n=d['next_cursor'];new+=d['serial'].get('head',[])+d['serial'].get('tail',[])
        if not d['bytes'] or n<=cursor:break
        cursor=n
    lines.extend(new)
    assert not any(x in '\n'.join(new) for x in ['Guru Meditation','assert failed','SOUNDLAB_FAIL','NESCPU_FAIL','LOAD_FAIL'])
    return '\n'.join(new)
def send(command):call('serial','write','--hex',(command+'\n').encode().hex())
def until(pattern):
    for _ in range(100):
        text=read()
        if re.search(pattern,text):return text
        call('run','--for','80ms')
    raise TimeoutError(pattern+' / '+str(lines[-8:]))
def key(name):send('ZION KEY '+name);call('run','--for','160ms')
def snapshot():
    read();send('ZION STATE');text=until('ZION_UI page=');match=re.search(r'ZION_UI page=(\d+) selected=(-?\d+) apps=(\d+)',text);assert match,text
    return {k:int(v) for k,v in re.findall(r'(\w+)=(-?\d+)',text.split('ZION_UI ')[-1].splitlines()[0])}

def state():
    d=snapshot();return d['page'],d['selected'],d['apps']
def list_page():
    page,selected,apps=state()
    for _ in range(4):
        if page==0:return selected,apps
        key('OK' if page==5 else 'LONG');page,selected,apps=state()
    raise AssertionError((page,selected,apps))
def install(file):
    r=subprocess.run([__import__('sys').executable,str(root/'tools/zionctl.py'),str(root/'release/test-apps'/file),
        '--sim',str(sim),'--instance',instance,'--log',str(out/(file+'.log'))],capture_output=True,text=True,encoding='utf8',timeout=180)
    assert r.returncode==0,r.stdout+r.stderr
def power_cycle(up=False):
    call('power','press','--duration','2300ms','--then-run','100ms')
    if up:call('input','up','press')
    call('power','press','--duration','600ms','--then-run','4s');read()
    if up:call('input','up','release','--then-run','500ms');read()
try:
    assert state()[2]==1
    install('ZION-SoundLab-0.3.0.zion');selected,apps=list_page();assert selected==0 and apps==2
    key('DOWN');assert state()[1]==1;key('OK');until('SOUNDLAB_READY mode=')
    text='\n'.join(lines);assert 'LOADED source=external-flash id=2147483651' in text
    assert 'SOUNDLAB_GOLDEN_PASS voices=12' in text and 'SOUNDLAB_PCM_UNAVAILABLE' in text
    assert sum('SOUNDLAB_GOLDEN mode=' in line for line in lines)==12
    key('DOWN');until('SOUNDLAB_MODE mode=1 samples=1');key('LONG');screens.append(call('screenshot'))
    power_cycle();install('ZION-NESCPU-0.3.0.zion');selected,apps=list_page();assert selected==0 and apps==3
    key('DOWN');key('DOWN');assert state()[1]==2;key('OK');until('NESCPU_READY')
    text='\n'.join(lines);assert 'NESCPU_GOLDEN cycles=819539 hash=1738864307 irq=19 nmi=29 pc=32800' in text
    key('OK');until('NESCPU_PROGRESS');key('OK');until('NESCPU_PAUSED');key('LONG');key('OK');until('NESCPU_PROGRESS');screens.append(call('screenshot'))
    power_cycle();assert state()==(0,0,3)
    key('DOWN');key('DOWN');key('LONG');assert state()[:2]==(1,0)
    key('DOWN');key('OK');until('ZION_ACTION action=2');d=snapshot();assert d['default']<=-100 and d['result']==0,d
    mark=len(lines);power_cycle();assert 'NESCPU_READY' in '\n'.join(lines[mark:]),lines[-20:]
    mark=len(lines);power_cycle(up=True);d=snapshot();assert d['page']==0 and d['apps']==3 and d['auto']==0,d
    assert 'NESCPU_READY' not in '\n'.join(lines[mark:])
    before=d['free'];key('DOWN');key('DOWN');key('LONG');key('DOWN');key('DOWN');key('OK');assert state()[:2]==(2,0)
    screens.append(call('screenshot'))
    key('OK');assert state()[:2]==(1,0);d=snapshot();assert d['apps']==3 and d['free']==before,d
    key('DOWN');key('DOWN');key('OK');key('DOWN');key('OK');until('ZION_ACTION action=3');d=snapshot()
    assert d['page']==0 and d['apps']==2 and d['free']>before and d['default']==-1 and d['result']==0,d
    reclaimed=d['free']-before
    key('LONG');key('DOWN');key('DOWN');key('OK');assert state()[:2]==(1,2)
    assert snapshot()['apps']==2
    power_cycle();d=snapshot();assert d['page']==0 and d['apps']==2 and d['default']==-1,d
    result=dict(instance=instance,profile='SIM: SDK6 radio/PCM disabled',boot=boot['boot'],external_install=True,native_ELF=True,
        SoundLab=dict(effects=12,reference='PASS',mode_change=True,PCM='SKIPPED: explicit simulator capability'),
        NESCPU=dict(reference='PASS',IRQ_NMI=True,pause_resume=True),long_OK_restart=False,
        power_cycle_catalog=True,lifecycle=dict(default_autostart=True,up_boot_escape=True,delete_cancel=True,delete_default=True,
            reclaimed_flash_bytes=reclaimed,builtin_delete_blocked=True,persistence_after_delete=True),
        caveat_commands=sorted(caveats),screens=[dict(path=s['path'],sha256=s.get('pixels_sha256')) for s in screens])
    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
finally:
    (out/'serial.log').write_text('\n'.join(lines)+'\n',encoding='utf8')
