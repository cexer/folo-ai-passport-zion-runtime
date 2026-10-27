"""Describe complete original application sources without editing candidate repos."""
import json,os,re
from pathlib import Path
root=Path(__file__).resolve().parents[1];candidates=root.parent/'candidates';base=root/'examples/link-apps'
def save(name,sources,includes,**extra):
    target=base/name;target.mkdir(parents=True,exist_ok=True)
    relative=lambda path:Path(os.path.relpath(path,target)).as_posix()
    data=dict(id={'gallery-original':0x80000011,'soundlab-original':0x80000012,'nes-original':0x80000013}[name],
        key={'gallery-original':'gallery-full','soundlab-original':'soundlab-all','nes-original':'nes-full'}[name],name=name,
        sources=[relative(s) for s in sources],include_dirs=[relative(d) for d in includes],**extra)
    (target/'compat.json').write_text(json.dumps(data,indent=2)+'\n')
gallery=candidates/'gallery/main'
sources=[gallery/p for p in re.findall(r'"([^"\n]+\.c)"',(gallery/'CMakeLists.txt').read_text(encoding='utf8'))]
save('gallery-original',sources,[gallery],cflags=['-Wno-deprecated-declarations','-DWIFI_BW_HT20=WIFI_BW20'],platform_include_dirs=[Path(os.path.relpath(gallery.parent/'components/bsp/include',base/'gallery-original')).as_posix()],idf_include_dirs=['components/fatfs/vfs','components/fatfs/src','components/wear_levelling/include','components/sdmmc/include','components/esp_hal_sd/include','components/esp_driver_sdmmc/include','components/esp_driver_sdspi/include'])
sound=candidates/'nabo-sound-lab/firmware/main'
sources=[sound/'main.c',sound/'mic/nabo_mic.c',sound/'voice/nabo_voice.c',sound/'gfx/fc_gfx.c',sound/'gfx/fc_res.c']
sources+=sorted((sound/'ui').glob('*.c'))+sorted((sound/'link').glob('*.c'))
save('soundlab-original',sources,[sound,*[sound/d for d in ['mic','voice','gfx','ui','link']]],cflags=['-Wno-unused-function','-Wno-format-truncation','-Wno-deprecated-declarations'],platform_include_dirs=[Path(os.path.relpath(sound.parent/'components/bsp/include',base/'soundlab-original')).as_posix()],compat_headers=['legacy-declarations.h'])
nes=candidates/'nes-card';core=nes/'components/nofrendo'
sources=[nes/'main'/p for p in re.findall(r'"([^"\n]+\.c)"',(nes/'main/CMakeLists.txt').read_text(encoding='utf8'))]
sources+=[core/p for p in re.findall(r'"([^"\n]+\.c)"',(core/'CMakeLists.txt').read_text(encoding='utf8'))]
includes=[nes/'main',core,*[d for d in core.iterdir() if d.is_dir()]]
save('nes-original',sources,includes,cflags=['-DNES_VISIBLE_HEIGHT=240','-Wno-discarded-qualifiers'],source_flags={'*/components/nofrendo/*':['-O2','-w']},idf_include_dirs=['components/esp_hid/include'],embed_files=[dict(path=Path(os.path.relpath(nes/'main/splash.bin',base/'nes-original')).as_posix(),symbol='splash_bin')])
print(json.dumps({name:len(json.loads((base/name/'compat.json').read_text())['sources']) for name in ['gallery-original','soundlab-original','nes-original']},indent=2))
