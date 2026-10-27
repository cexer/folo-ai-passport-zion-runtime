"""Compile unchanged C app_main sources against an exact host build's real SDK.
No substitute SDK headers or automatic arbitrary symbol exports are used.
"""
import argparse,hashlib,json,re,subprocess,fnmatch,struct
from pathlib import Path
from zion_packages import make,append_dual
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('project',type=Path);p.add_argument('--host-build',type=Path,required=True)
p.add_argument('--output',type=Path,required=True);p.add_argument('--standalone-full',type=Path);p.add_argument('--plan',action='store_true');a=p.parse_args()
project=a.project.resolve();spec=json.loads((project/'compat.json').read_text());build=a.host_build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
for filename in ['app.zion','app-dual.bin','manifest.json']:(out/filename).unlink(missing_ok=True)
assert all(Path(s).suffix in ('.c','.cc','.cpp') for s in spec['sources'])
contract=json.loads((build/'esp-idf/main/idf-compat/zion-idf-compat.json').read_text())
# Refuse a changed SDK/config until the host has regenerated its contract.
import os
idf=Path(os.environ['IDF_PATH'])
for item in contract['inputs']:
    file=build/item['name'][6:] if item['name'].startswith('build/') else idf/item['name'][4:] if item['name'].startswith('idf/') else root/item['name']
    if not a.plan:assert hashlib.sha256(file.read_bytes()).hexdigest()==item['sha256'],'Host rebuild required: '+str(file)
db=json.loads((build/'compile_commands.json').read_text())
record=next(x for x in db if x['file'].replace('\\','/').endswith('/main/zion_idf_compat.c'))
command=record['command'];cc=command.split(' -D',1)[0].strip('"');nm=str(Path(cc).with_name('riscv32-esp-elf-nm.exe'))
includes=[s.strip('"') for s in re.findall(r'(?:^|\s)-I("[^"]*"|\S+)',command)]
flags=['@'+str(build/'toolchain/cflags'),'-DESP_PLATFORM','-D__PICOLIBC_ERRNO_FUNCTION=zion_idf_errno','-D_GNU_SOURCE','-DLV_CONF_INCLUDE_SIMPLE','-march=rv32im_zicsr_zifencei','-mabi=ilp32',
    '-mcmodel=medlow','-msmall-data-limit=0','-mno-relax','-fno-pic','-ffreestanding','-fno-builtin','-fno-common',
    '-fno-asynchronous-unwind-tables','-fno-unwind-tables','-ffunction-sections','-fdata-sections','-fno-jump-tables','-fno-tree-switch-conversion','-Os','-std=gnu23','-Wall','-Wextra','-Werror','-Wno-unused-parameter']
# Match IDF's component-local header precedence. Passport OS has its own
# ui_pixel.h; searching the launcher's main/ first silently selects the wrong UI.
flags+=['-I'+str((project/x).resolve()) for x in spec.get('include_dirs',[])]+['-I'+x for x in includes]+spec.get('cflags',[])
defines=[s.replace(chr(92)+chr(34),chr(34)) for s in re.findall(r'(?:^|\s)(-D\S+)',command)]
flags+=defines
flags+=['-I'+str(idf/x) for x in spec.get('idf_include_dirs',[])]
flags=['-I'+str((project/x).resolve()) for x in spec.get('platform_include_dirs',[])]+flags
flags+=['-include'+str((project/x).resolve()) for x in spec.get('compat_headers',[])]
wrapper=out/'entry.c'
button_header='';button_setup=''
# Infer the declared BSP event surface, never an application-name exception.
# Liuyao exposed old headers whose default callback branch treats RELEASE as
# CLICK. Assert the shared enum prefix at compile time and negotiate its mask.
button_headers=[(project/d/'bsp_button.h').resolve() for d in spec.get('platform_include_dirs',[])]
button_headers=[h for h in button_headers if h.exists()]
button_events=None
button_input=spec.get('button_input','callbacks')
assert button_input in ('callbacks','polling')
# Passport OS reads ADC voltage and implements its own event enum/state machine;
# callback enum negotiation would incorrectly reject this valid polling ABI.
if button_headers and button_input=='callbacks':
    header=button_headers[0]
    declaration=re.sub(r'/\*.*?\*/|//[^\n]*','',header.read_text(encoding='utf8'),flags=re.S)
    button_events=31 if re.search(r'\bBSP_BTN_RELEASE\b',declaration) else 15
    button_header='#include "bsp_button.h"\n_Static_assert(BSP_BTN_PRESS==0 && BSP_BTN_CLICK==1 && BSP_BTN_DOUBLE==2 && BSP_BTN_LONG==3,"Unsupported BSP event numbering");\n'
    if button_events==31:
        button_header+='_Static_assert(BSP_BTN_RELEASE>=4 && BSP_BTN_RELEASE<32,"Unsupported BSP release numbering");\nextern void zion_idf_configure_button_release(unsigned);\n'
    button_header+='extern void zion_idf_configure_button_events(unsigned);\n'
    button_setup='zion_idf_configure_button_events('+str(button_events)+'u);'
    if button_events==31:button_setup+='zion_idf_configure_button_release(BSP_BTN_RELEASE);'
if spec.get('button_config_header'):
    header=(project/spec['button_config_header']).resolve()
    button_header+='#include '+json.dumps(header.as_posix())+'\nstatic const uint16_t zion_button_ranges[][2]=BSP_BTN_MV_TABLE;\nextern int zion_idf_configure_buttons(unsigned,const uint16_t (*)[2]);\n'
    button_setup+='if(zion_idf_configure_buttons(sizeof(zion_button_ranges)/sizeof(zion_button_ranges[0]),zion_button_ranges)!=0){api->text_set(ZION_TEXT_STATUS,"启动失败");api->log("BUTTON_PROFILE_FAILED");vTaskDelete(NULL);return;}'
wrapper.write_text('#include "zion_sdk.h"\n#include "freertos/FreeRTOS.h"\n#include "freertos/task.h"\n'+button_header+
    'const uint32_t zion_plugin_abi=ZION_ABI_VERSION;\nconst uint32_t zion_plugin_api_size=sizeof(zion_api_t);\n'
    'extern void app_main(void);\nextern void zion_idf_run_initializers(void);\nextern void '+contract['symbol']+'(void);\n'
    'void zion_plugin_entry(const zion_api_t *api) {'+contract['symbol']+'();'+button_setup+'zion_idf_run_initializers();app_main();api->log("COMPAT_APP_MAIN_RETURNED");vTaskDelete(NULL);}\n',encoding='utf8')
objects=[];sources=[]
for i,source in enumerate([(project/s).resolve() for s in spec['sources']]+[wrapper]):
    obj=out/(str(i)+'.o');cpp=source.suffix in ('.cc','.cpp')
    compiler=str(Path(cc).with_name('riscv32-esp-elf-g++.exe')) if cpp else cc
    compile_flags=[f for f in flags if f not in ('-std=gnu23','-ffreestanding')]+['-std=gnu++17','-fno-exceptions','-fno-rtti','-Wno-missing-field-initializers'] if cpp else flags
    extra=[]
    for pattern,options in spec.get('source_flags',{}).items():
        if fnmatch.fnmatch(source.as_posix(),pattern):extra+=options
    subprocess.run([compiler,*compile_flags,*extra,'-c',str(source),'-o',str(obj)],check=True);objects.append(str(obj))
    sources.append(dict(file=str(source),sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
if spec.get('button_config_header'):
    sources.append(dict(file=str(header),sha256=hashlib.sha256(header.read_bytes()).hexdigest()))
for name in spec.get('header_inputs',[]):
    file=(project/name).resolve()
    sources.append(dict(file=str(file),sha256=hashlib.sha256(file.read_bytes()).hexdigest()))
for i,embed in enumerate(spec.get('embed_files',[])):
    file=(project/embed['path']).resolve()
    expected=file.name.replace('.','_').replace('-','_')
    # Word Learning Notebook uses IDF target_add_binary_data(RENAME_TO ...).
    # Honor that build contract without renaming/editing the upstream asset.
    rename=embed.get('rename_to')
    if rename:
        assert re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*',rename) and rename==embed['symbol']
    else:assert expected==embed['symbol']
    # A stable basename gives the same _binary_* names as IDF EMBED_FILES.
    basename=rename or file.name;local=out/basename
    # Match ESP-IDF EMBED_TXTFILES, whose symbols expose a NUL-terminated string.
    embedded=file.read_bytes()+(b'\0' if embed.get('text',False) else b'')
    local.write_bytes(embedded);obj=out/f'embedded-{i}.o'
    objcopy=str(Path(cc).with_name('riscv32-esp-elf-objcopy.exe'))
    subprocess.run([objcopy,'-I','binary','-O','elf32-littleriscv','-B','riscv','--rename-section','.data=.rodata.embedded,alloc,load,readonly,data,contents',basename,str(obj)],cwd=out,check=True)
    objects.append(str(obj));sources.append(dict(file=str(file),sha256=hashlib.sha256(file.read_bytes()).hexdigest()))
wrap=spec.get('link_wrap',[])
assert all(re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*',name) for name in wrap)
elf=out/'app.elf';subprocess.run([cc,'-march=rv32im_zicsr_zifencei','-mabi=ilp32','-mno-relax','-nostdlib','-r',
    *['-Wl,--wrap='+name for name in wrap],*objects,'-o',str(elf)],check=True)
imports=[line.split()[-1] for line in subprocess.check_output([nm,'-u',str(elf)],text=True).splitlines() if line.strip()]
allowed=set(re.findall(r'^ZION_IMPORT\((\w+),', (root/'main/zion_idf_exports.inc').read_text(),re.M))
allowed.update(['memcpy','memset','memcmp','strlen','__divdi3','__udivdi3','__umoddi3',contract['symbol']])
profile=root/'main/zion_idf_auto_imports.json'
if profile.exists():allowed.update(json.loads(profile.read_text())['imports'])
missing=sorted(set(imports)-allowed)
if a.plan:
    (out/'import-plan.json').write_text(json.dumps(dict(imports=[s for s in imports if not s.startswith('zion_idf_contract_')],missing=missing,sources=sources),indent=2)+'\n')
    print(json.dumps(dict(elf_bytes=elf.stat().st_size,missing=len(missing),plan=str(out/'import-plan.json')),indent=2));raise SystemExit()
assert not missing,'Unsupported host imports (no package generated): '+', '.join(missing)
body=elf.read_bytes();code_bytes=0;readonly_bytes=0
shoff=struct.unpack_from('<I',body,32)[0];section_count=struct.unpack_from('<H',body,48)[0]
sections=[struct.unpack_from('<10I',body,shoff+40*i) for i in range(section_count)]
relocated={s[7] for s in sections if s[1] in (4,9)}
for i in range(section_count):
    section=sections[i]
    if section[2]&6==6 and section[5]:
        align=max(4,section[8]);code_bytes=(code_bytes+align-1)&~(align-1);code_bytes+=section[5]
    elif section[1]==1 and section[2]&7==2 and section[5] and (i in relocated or section[4]%max(1,section[8])):
        # ZPK ELF starts at a 16-byte aligned address. Borrow other RO directly.
        align=max(4,section[8]);readonly_bytes=(readonly_bytes+align-1)&~(align-1);readonly_bytes+=section[5]
code_cache=(((code_bytes+15)&~15)+readonly_bytes+4095)&~4095 if spec.get('execution','flash')=='flash' else 0
package=make(body,b'',spec['id'],spec['key'],spec['name'],storage=spec.get('storage'),entry_stack=spec.get('entry_stack',0),code_cache=code_cache,bluetooth=spec.get('bluetooth',False),usb=spec.get('usb',False))
(out/'app.zion').write_bytes(package)
if a.standalone_full:
    from package_minimal import image_length,partitions,independent_app_partition
    full=a.standalone_full.read_bytes();table=partitions(full[0x8000:0x8c00]);factory=independent_app_partition(full,table)
    assert factory['offset']>=0x10000 and factory['offset']%0x10000==0
    assert image_length(full[factory['offset']:])<=factory['size']
    dual=append_dual(full,make(body,b'',spec['id'],spec['key'],spec['name'],dual=True,storage=spec.get('storage'),entry_stack=spec.get('entry_stack',0),code_cache=code_cache,bluetooth=spec.get('bluetooth',False),usb=spec.get('usb',False)));assert len(dual)<=0x800000
    (out/'app-dual.bin').write_bytes(dual)
manifest=dict(contract=contract['fingerprint'],host_elf_sha256=hashlib.sha256((build/'FoloToy-AI-Passport.elf').read_bytes()).hexdigest(),
    sources=sources,imports=imports,elf_bytes=len(body),package_bytes=len(package),package_sha256=hashlib.sha256(package).hexdigest(),
    business_source_modified=False,link_wrappers=wrap,compile_definitions=defines,
    profile_sha256=hashlib.sha256((project/'compat.json').read_bytes()).hexdigest(),
    application_cflags=spec.get('cflags',[]),source_flags=spec.get('source_flags',{}),button_event_mask=button_events,button_input=button_input,
    compat_headers=[dict(file=str((project/h).resolve()),sha256=hashlib.sha256((project/h).read_bytes()).hexdigest()) for h in spec.get('compat_headers',[])],
    code_bytes=code_bytes,readonly_cache_bound=readonly_bytes,code_cache_bytes=code_cache,scope='C/C++ app_main with same-build imports; not arbitrary ESP-IDF compatibility')
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(manifest,indent=2))

