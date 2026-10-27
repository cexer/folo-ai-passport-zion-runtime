"""Execute the real managed LVGL decoder over the full Chinese font."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
lv=root/'managed_components/lvgl__lvgl'
compiler=os.environ.get('CC') or shutil.which('cc') or shutil.which('clang')
if not compiler:
    raise SystemExit('A host C compiler is required for the actual font decoder test')
with tempfile.TemporaryDirectory(prefix='zion-font-decode-') as tmp:
    exe=Path(tmp)/('decode.exe' if os.name=='nt' else 'decode')
    flags=['-std=c11','-O2','-D_CRT_SECURE_NO_WARNINGS','-DLV_CONF_SKIP=1',
           '-DLV_USE_OS=0','-DLV_USE_LOG=0','-DLV_USE_FONT_COMPRESSED=1',
           '-DLV_USE_STDLIB_STRING=1','-I'+str(lv)]
    subprocess.run([compiler,*flags,str(root/'tests/test_zion_font_decode.c'),
        str(root/'assets/fonts/zion_font_14.c'),
        str(lv/'src/font/fmt_txt/lv_font_fmt_txt.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
