"""Exercise the explicit radio/PCM-free SDK6 profile in an unmodified PassportSim.
Never treats this profile as a production voice/network test.
"""
import argparse, hashlib, json, os, re, subprocess, shutil, struct
from PIL import Image
from pathlib import Path

root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--sim',type=Path,required=True)
p.add_argument('--build',type=Path,default=root/'build-runtime-elf-sim')
p.add_argument('--image',type=Path)
p.add_argument('--start-only',action='store_true');args=p.parse_args()
out=root/('diagnostics/runtime-production-smoke' if args.image else 'diagnostics/runtime-sim')
out.mkdir(parents=True,exist_ok=True)
caveat_commands=set()
def call(*command):
    run=subprocess.run([str(args.sim.resolve()),'--output','json',*map(str,command)],
        capture_output=True,text=True,encoding='utf8',errors='replace',timeout=55)
    try: data=json.loads(run.stdout)
    except Exception: raise RuntimeError(run.stdout[:500]+run.stderr[:500])
    # PassportSim CLI exit.rs defines 10 as PASS_WITH_CAVEATS for verdict-bearing
    # calls. Keep that distinction; never turn a guest fault/tripwire into PASS.
    caveat=run.returncode==10 and data.get('result')=='pass' and not data.get('receipt',{}).get('hle',{}).get('tripwires_hit')
    if caveat: caveat_commands.add(str(command[0]))
    if run.returncode and not caveat:
        raise RuntimeError(json.dumps({k:data.get(k) for k in ['code','message','hint','status','result','serial_tail']},ensure_ascii=False))
    return data
def safe(text):
    text=re.sub(r'(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}','[MAC REDACTED]',text)
    return re.sub(r'(?im)^.*(?:uuid|device.?id|\bmac\b|serial.?number|sn:).*$', '[IDENTITY REDACTED]',text)
build=args.build.resolve()
parts={'bin':args.image.resolve() if args.image else build/'runtime-sim-full.bin','elf':build/'FoloToy-AI-Passport.elf',
       'boot_elf':build/'bootloader/bootloader.elf','pt':build/'partition_table/partition-table.bin'}
hashes={k:hashlib.sha256(v.read_bytes()).hexdigest() for k,v in parts.items()}
merged=parts['bin'].read_bytes()
assert merged[0x10000+176:0x10000+208]==bytes.fromhex(hashes['elf']),'Merged image/ELF mismatch'
if not args.start_only:
    assert b'SIM_LONG_OK restart=0' in merged,'Rebuild the corrected simulator profile first'
    assert b'RETURN restart=1 destination=list' not in merged,'Stale OK-restart firmware'
    for module in ['counter.elf','gallery.elf']:
        assert (build/'plugins'/module).read_bytes() in merged,'Stale embedded module: '+module
name='zion-runtime-'+hashes['elf'][:12]
corpus=Path(os.environ['APPDATA'])/'passportsim/corpus.toml'
old=corpus.read_text(encoding='utf8') if corpus.exists() else ''
entry='\n['+name+']\n'+''.join(f'{k} = "{v.as_posix()}"\n' for k,v in parts.items())
entry+='sha256 = { '+', '.join(f'{k} = "{v}"' for k,v in hashes.items())+' }\n'
if '['+name+']' not in old: corpus.write_text(old+entry,encoding='utf8')
start=call('start',name,'--label','ZION shared-runtime SDK test','--boot-timeout','15s')
instance=start['instance'];(out/'instance.txt').write_text(instance)
(out/'start.json').write_text(safe(json.dumps(start,indent=2,ensure_ascii=False)),encoding='utf8')
print(json.dumps({'instance':instance,'boot':start.get('boot'),'tripwires':start.get('receipt',{}).get('hle',{}).get('tripwires_hit')}))
if args.start_only: raise SystemExit()
cursor=0
(out/'serial-stream-sanitized.log').write_text('',encoding='utf8')
def serial():
    global cursor
    lines=[]
    for _ in range(100):
        d=call('serial','read','--instance',instance,'--cursor',cursor,'--max-bytes','2048')
        s=d['serial'];lines+=s.get('head',[])+s.get('tail',[])
        following=d['next_cursor']
        if not d['bytes'] or following<=cursor: break
        cursor=following
    text='\n'.join(lines)
    with (out/'serial-stream-sanitized.log').open('a',encoding='utf8') as f:f.write(safe(text)+'\n')
    return text
def key(k,action='click',duration='120ms'):
    return call('input',k,action,'--duration',duration,'--then-run','600ms','--instance',instance)
def power_cycle(up=False):
    off=call('power','press','--duration','2300ms','--then-run','100ms','--instance',instance)
    if up: call('input','up','press','--instance',instance)
    on=call('power','press','--duration','600ms','--then-run','4s','--instance',instance)
    if up: call('input','up','release','--then-run','500ms','--instance',instance)
    return off,on
def shot(name):
    data=call('screenshot','--instance',instance)
    (out/(name+'.json')).write_text(json.dumps(data,indent=2),encoding='utf8')
    base=Path(call('status')['artifacts_root']).expanduser()
    candidates=list(base.glob('run-*/'+instance+'/'+data['path']))
    assert candidates,'Screenshot artifact missing: '+data['path']
    source=max(candidates,key=lambda f:f.stat().st_mtime)
    expected=data['artifacts'][0]['sha256']
    assert hashlib.sha256(source.read_bytes()).hexdigest()==expected
    shutil.copy2(source,out/(name+'.png'))
    return data['pixels_sha256']
log=serial()
assert 'ZION_PRODUCT ready' in log,log[-1200:]
assert 'ZION_RUNTIME_SIM radio=disabled pcm=disabled' in log
shot('01-list')
key('down');key('ok');after=serial();log+='\n'+after
assert 'ENTER id=sdk-demo restart=0' in after,after[-1000:]
assert 'LOADED source=independent-ELF' in after,after[-1000:]
assert 'ZION_PRODUCT ready' not in after,after[-1000:]
key('up');key('down');key('up');key('ok');after=serial();log+='\n'+after
assert 'DEMO_SAVE value=1 result=0' in after,after[-1000:]
shot('02-shared-demo')
key('ok','hold','2s');after=serial();log+='\n'+after
assert 'DEMO_LONG_OK restart=0' in after and 'ZION_PRODUCT ready' not in after,after[-1000:]
power_cycle();after=serial();log+='\n'+after
assert 'ZION_PRODUCT ready' in after,after[-1000:]
shot('03-list-after-reset')
key('down');key('ok');key('ok');after=serial();log+='\n'+after
assert 'DEMO_SAVE value=1 result=0' in after,after[-1000:] # NVS persisted across reset.
power_cycle();serial()
key('ok');after=serial();log+='\n'+after
assert 'ENTER id=xiaozhi restart=0' in after,after[-1000:]
key('up');key('down');key('ok');after=serial();log+='\n'+after
assert 'SIM_KEY code=2' in after,after[-1000:]
shot('04-xiaozhi-adapter-test')
key('ok','hold','2s');after=serial();log+='\n'+after
assert 'SIM_LONG_OK restart=0' in after and 'ZION_PRODUCT ready' not in after
power_cycle();after=serial();log+='\n'+after
assert 'ZION_PRODUCT ready' in after
# Default selection uses the real NVS and Flash prefs implementations.
key('down');key('ok','hold','2s');key('down');key('ok');serial()
key('ok','hold','2s');shot('05-default')
power_cycle()
after=serial();log+='\n'+after
assert 'ENTER id=sdk-demo restart=0' in after,after[-1600:]
# Up held during power-on must bypass the default. No application OK restart.
power_cycle(up=True)
after=serial();log+='\n'+after
assert 'ZION_PRODUCT ready' in after and 'ENTER id=' not in after,after[-1600:]
key('down');key('down');key('ok')
call('run','--for','1200ms','--instance',instance)
after=serial();log+='\n'+after
assert 'ENTER id=gallery restart=0' in after and 'LOADED source=independent-ELF' in after,after[-2000:]
assert 'GALLERY_VALIDATED format=FGA1' in after and 'GALLERY_FRAME first=1 tiles=5' in after,after[-2000:]
assert 'GALLERY_FRAME delta=1' in after and 'GALLERY_FAIL' not in after,after[-2000:]
key('ok');after=serial();log+='\n'+after
assert 'GALLERY_PAUSED' in after,after[-1000:]
paused=shot('06-gallery-paused')
call('run','--for','600ms','--instance',instance)
assert shot('07-gallery-still-paused')==paused
key('ok');after=serial();log+='\n'+after
assert 'GALLERY_PLAYING' in after
frames=set()
rgb_frames=set()
for n in range(6):
    call('run','--for','150ms','--instance',instance)
    frames.add(shot('08-gallery-frame-'+str(n)))
    rgb_frames.add(hashlib.sha256(Image.open(out/('08-gallery-frame-'+str(n)+'.png')).convert('RGB').tobytes()).hexdigest())
assert len(frames)>1,'Animation frames did not change'
# Independent golden decoder checks every pixel, not just that two hashes differ.
asset=(build/'plugins/gallery.fga').read_bytes();position=36;frame=bytearray(240*320*2);expected=set()
for _ in range(3):
    kind,_,delay,size=struct.unpack_from('<BBHI',asset,position);position+=8
    if kind==0: frame[:]=asset[position:position+size]
    else:
        at=position;cursor_pixels=0
        while at<position+size:
            skip,run=struct.unpack_from('<IH',asset,at);at+=6;cursor_pixels+=skip
            frame[cursor_pixels*2:(cursor_pixels+run)*2]=asset[at:at+run*2];at+=run*2;cursor_pixels+=run
    position+=size
    rgb=bytearray()
    for pixel, in struct.iter_unpack('<H',frame):
        red=(pixel>>11)&31;green=(pixel>>5)&63;blue=pixel&31
        rgb.extend(((red<<3)|(red>>2),(green<<2)|(green>>4),(blue<<3)|(blue>>2)))
    expected.add(hashlib.sha256(rgb).hexdigest())
assert rgb_frames<=expected and len(rgb_frames)>1,'Rendered RGB565 pixels differ from fixture'
log+='\n'+serial()
assert not re.search(r'Guru Meditation|assert failed|abort\(\)',log)
(out/'serial-sanitized.log').write_text(safe(log),encoding='utf8')
result={'instance':instance,'scope':'Explicit simulator profile; production radio/audio not tested',
        'direct_start_without_reset':True,'keys':True,'nvs_after_reset':True,
        'return_via_power_cycle':True,'long_ok_does_not_restart':True,
        'default_power_on':True,'up_key_bypasses_default':True,
        'independent_elf_counter':True,'independent_elf_gallery':True,'gallery_pause':True,
        'gallery_distinct_frame_hashes':len(frames),
        'gallery_pixels_match_independent_decoder':True,
        'simulator_verdict':'PASS_WITH_CAVEATS' if caveat_commands else 'PASS',
        'caveat_commands':sorted(caveat_commands),
        'xiaozhi_sdk_test_entry':True,'hashes':hashes}
(out/'result.json').write_text(json.dumps(result,indent=2),encoding='utf8')
print(json.dumps(result,indent=2))
