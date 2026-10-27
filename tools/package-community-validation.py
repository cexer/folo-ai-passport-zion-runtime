"""Archive selected unchanged-source validation apps and their exact DeviceTest host.

This is a local test bundle, including USB controls and a local TLS peer redirect.
Only allowlisted firmware/debugging files are copied; dependencies and NVS seeds
are excluded. The 0x0 image stops before the device identity region.
"""
import argparse,hashlib,json,re,shutil
from pathlib import Path
from package_minimal import image_length,partitions,independent_app_partition
from zion_packages import extract,parse
parser=argparse.ArgumentParser()
parser.add_argument('--application',action='append',choices=['recorder','bambu-monitor','iphone-agent','octo-radio','bathroom-reader','didaaa','pinball','vpet','snoopy','connect-four','liquid-glass','timer','passport-os'])
parser.add_argument('--report',type=Path)
parser.add_argument('--host-build',type=Path,default=Path('build-layout-apps'))
parser.add_argument('--batch',help='Optional safe subdirectory: keep unchanged-host test batches separate')
parser.add_argument('--plugin-build',action='append',default=[],help='application=module-build-directory')
parser.add_argument('--native-test-layout',action='append',default=[],help='application=identity-preserving native layout with identical APP bytes')
parser.add_argument('--screenshot',action='append',default=[],help='Explicit public-safe name.png=diagnostics/community/...png')
parser.add_argument('--plugin-regression',action='append',default=[],type=Path,help='Extra same-host plugin build, without an independent firmware')
parser.add_argument('--external-plugin',action='append',default=[],type=Path,help='New validated external application without a packaged native/dual route')
args=parser.parse_args()
root=Path(__file__).resolve().parents[1];host=(root/args.host_build).resolve()
module_overrides=dict(item.split('=',1) for item in args.plugin_build)
native_layouts=dict(item.split('=',1) for item in args.native_test_layout)
sha=lambda b:hashlib.sha256(b).hexdigest()
elf=(host/'FoloToy-AI-Passport.elf').read_bytes();app=(host/'FoloToy-AI-Passport.bin').read_bytes()
assert app[176:208]==bytes.fromhex(sha(elf)) and image_length(app)==len(app)
full=(host/'runtime-device-test-full.bin').read_bytes();assert full[0x10000:0x10000+len(app)]==app
for offset,filename in json.loads((host/'flasher_args.json').read_text())['flash_files'].items():
    data=(host/filename).read_bytes();at=int(offset,0);assert full[at:at+len(data)]==data
table=partitions((host/'partition_table/partition-table.bin').read_bytes())
factory=next(p for p in table if p['type']==0 and p['subtype']==0)
end=factory['offset']+factory['size'];assert factory['offset']==0x10000 and end<=0x356000 and len(app)<=factory['size']
for config in ['config/sdkconfig.h','bootloader/config/sdkconfig.h']:
    size=re.search(r'^#define CONFIG_ZION_FACTORY_SIZE (\d+)',(host/config).read_text(),re.M)
    assert size and int(size[1])==factory['size'],'Factory geometry mismatch: '+config
base=root/'dist/community-link-validation'
if args.batch:
    assert re.fullmatch(r'[a-z0-9-]{1,64}',args.batch)
    base=base/args.batch
out=base/sha(elf)[:12];out.mkdir(parents=True,exist_ok=True);records={};apps={}
def put(name,data):
    file=out/name;file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes(data)
    records[name]=dict(bytes=len(data),sha256=sha(data))
for source,name in [('FoloToy-AI-Passport.bin','app.bin'),('FoloToy-AI-Passport.elf','app.elf'),
                    ('FoloToy-AI-Passport.map','app.map'),('bootloader/bootloader.bin','bootloader.bin'),
                    ('partition_table/partition-table.bin','partitions.bin'),('ota_data_initial.bin','ota.bin')]:
    put('host/'+name,(host/source).read_bytes())
put('host/ZION-Runtime-0.5.0-devtest-0x0.bin',full[:end]);put('host/empty-store.bin',b'\xff'*0x6000)
profiles=[('recorder','build-link-recorder','build-recorder-standalone','ai_passport_recorder','recorder-full.bin'),
    ('bambu-monitor','build-link-bambu','build-bambu-standalone','passport_printer','bambu-full.bin'),
    ('iphone-agent','build-link-iphone-agent','build-iphone-standalone','FoloToy-AI-Passport','iphone-full.bin'),
    ('octo-radio','build-link-octo-radio','build-octo-standalone','Octo-Radio','octo-full.bin'),
    ('bathroom-reader','build-link-bathroom-reader','build-reader-standalone','FoloToy-AI-Passport','reader-full.bin'),
    ('didaaa','build-link-didaaa','build-didaaa-compact','FoloToy-AI-Passport','didaaa-full.bin'),
    ('pinball','build-link-pinball','build-pinball-standalone','ai-passport-pinball','pinball-full.bin'),
    ('vpet','build-link-vpet','build-vpet-standalone','FoloToy-AI-Passport','vpet-full.bin'),
    ('snoopy','build-link-snoopy','build-snoopy-standalone','FoloToy-AI-Passport','snoopy-full.bin'),
    ('connect-four','build-link-connect-four','build-connect-four-standalone','FoloToy-AI-Passport','connect-four-full.bin'),
    ('liquid-glass','build-link-liquid-glass','build-liquid-glass-standalone','FoloToy-AI-Passport','liquid-glass-full.bin'),
    ('timer','build-link-timer','build-timer-standalone','FoloToy-AI-Passport','timer-full.bin'),
    ('passport-os','build-link-passport-os','build-passportos-standalone','passport-os','passport-os-full.bin')]
selected=args.application or ([] if args.external_plugin else ['recorder','bambu-monitor'])
for key,build,native,project,image in profiles:
    if key not in selected:continue
    module=root/module_overrides.get(key,build);independent=root/native;manifest=json.loads((module/'manifest.json').read_text())
    assert manifest['host_elf_sha256']==sha(elf)
    for source in manifest['sources']+manifest.get('compat_headers',[]):assert sha(Path(source['file']).read_bytes())==source['sha256']
    plugin=(module/'app.zion').read_bytes();dual=(module/'app-dual.bin').read_bytes();prefix=(independent/image).read_bytes()
    embedded,kind=extract(dual);assert kind=='dual' and embedded[256:]==plugin[256:] and dual[:-16-len(embedded)]==prefix
    raw=(independent/(project+'.bin')).read_bytes();native_elf=(independent/(project+'.elf')).read_bytes()
    native_table=partitions((independent/'partition_table/partition-table.bin').read_bytes())
    native_factory=independent_app_partition(prefix,native_table)
    native_offset=native_factory['offset']
    assert raw[176:208]==bytes.fromhex(sha(native_elf)) and image_length(raw)==len(raw) and prefix[native_offset:native_offset+len(raw)]==raw
    for name,data in [(key+'-0.5.0-devtest.zion',plugin),(key+'-dual-0.5.0-devtest.bin',dual),
                      (key+'-standalone-0.5.0-devtest.bin',prefix),('plugin.elf',(module/'app.elf').read_bytes()),
                      ('standalone.elf',native_elf),('standalone.map',(independent/(project+'.map')).read_bytes()),
                      ('build-manifest.json',(module/'manifest.json').read_bytes())]:put(key+'/'+name,data)
    native_map=(independent/(project+'.map')).read_text(errors='replace')
    apps[key]=dict(plugin=parse(plugin),code_bytes=manifest['code_bytes'],code_cache_bytes=manifest['code_cache_bytes'],
                  independent_test_output_silenced='__wrap_i2s_channel_write' in native_map)
    if key in native_layouts:
        safe=(root/native_layouts[key]).resolve();assert safe.is_relative_to(root)
        safe_app=(safe/(project+'.bin')).read_bytes();assert safe_app==raw
        safe_table=partitions((safe/'partition_table/partition-table.bin').read_bytes())
        safe_factory=next(p for p in safe_table if p['type']==0 and p['subtype']==0)
        segments=[]
        for at,name in json.loads((safe/'flasher_args.json').read_text())['flash_files'].items():
            data=(safe/name).read_bytes();offset=int(at,0)
            assert offset+len(data)<=0x356000 or offset>=0x35a000
            destination=key+'/native-safe/'+Path(name).name
            put(destination,data);segments.append(dict(offset=offset,file=destination))
        assert safe_factory['offset']+len(raw)<=0x800000
        put(key+'/native-safe/flash-plan.json',(json.dumps(segments,indent=2)+'\n').encode())
        apps[key]['physical_native_layout']=dict(app_identical=True,partitions=safe_table,
            warning='Use segmented native-safe plan; the stock standalone/hybrid prefix overlaps the protected identity')
plan=[dict(offset='0x0',file='host/ZION-Runtime-0.5.0-devtest-0x0.bin'),
      dict(offset='0x35a000',file='host/empty-store.bin'),dict(offset='0x7fe000',file='host/ota.bin')]
for segment in plan:
    at=int(segment['offset'],0);assert at+records[segment['file']]['bytes']<=0x356000 or at>=0x35a000
result=dict(profile='DEVICE TEST: USB test controls and local TLS peer; not a community release',
    host_test_output_silenced='__wrap_i2s_channel_write' in (host/'FoloToy-AI-Passport.map').read_text(errors='replace'),
    host_elf_sha256=sha(elf),files=records,applications=apps,partitions=table,flash_plan=plan,
    identity_preserved=['0x356000','0x35a000'],flash_impact='Resets NVS and installed-app metadata; use the segmented flash plan')
if args.report:put('test-report.json',args.report.read_bytes())
for category,module in [('regressions',m) for m in args.plugin_regression]+[('external',m) for m in args.external_plugin]:
    module=module.resolve();assert module.is_relative_to(root)
    manifest=json.loads((module/'manifest.json').read_text())
    assert manifest['host_elf_sha256']==sha(elf)
    payload=(module/'app.zion').read_bytes();info=parse(payload);key=info['key']
    assert re.fullmatch(r'[a-z0-9-]{1,12}',key)
    assert payload[256:]==(module/'app.elf').read_bytes()
    assert sha(payload)==manifest['package_sha256']
    for source in manifest['sources']+manifest.get('compat_headers',[]):assert sha(Path(source['file']).read_bytes())==source['sha256']
    put(category+'/'+key+'/'+key+'-0.5.0-devtest.zion',payload)
    put(category+'/'+key+'/plugin.elf',(module/'app.elf').read_bytes())
    put(category+'/'+key+'/build-manifest.json',(module/'manifest.json').read_bytes())
for screenshot in args.screenshot:
    name,source=screenshot.split('=',1);target=Path(name);source=Path(source).resolve()
    assert not target.is_absolute() and '..' not in target.parts and target.suffix=='.png'
    assert source.is_relative_to(root/'diagnostics/community') and source.suffix=='.png'
    body=source.read_bytes();assert body.startswith(b'\x89PNG\r\n\x1a\n')
    put('screenshots/'+target.as_posix(),body)
(out/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
for name,record in records.items():assert sha((out/name).read_bytes())==record['sha256']
(root/'diagnostics/community/final-bundle-path.txt').write_text(str(out)+'\n');print(out)
