#!/usr/bin/env python3
"""Generate 4-bpp product UI subsets; keep the full 4-bpp font as fallback.

python tools/generate-zion-ui-font.py FONT.ttf LV_FONT_CONV.js [FONTTOOLS_DIRECTORY]
The pinned source, license and converter match generate-zion-font.py.
"""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
UI_SOURCES=('main/zion_launcher.c','main/zion_text.c','main/zion_install_view.c')


def ui_characters(root=ROOT):
    chars=set(chr(cp) for cp in range(32,127))
    for name in UI_SOURCES:
        source=(root/name).read_text(encoding='utf8')
        for literal in re.findall(r'"(?:[^"\\]|\\.)*"|/\*.*?\*/|//[^\n]*',source,flags=re.S):
            if literal.startswith('"'):
                chars.update(c for c in literal if ord(c)>127)
    return ''.join(sorted(chars))


def generate(source,converter,fonttools=None):
    if fonttools:
        sys.path.insert(0,str(Path(fonttools).resolve()))
    from fontTools.ttLib import TTFont
    from fontTools.varLib.instancer import instantiateVariableFont
    original=Path(source).resolve();converter=Path(converter).resolve()
    original_hash=hashlib.sha256(original.read_bytes()).hexdigest()
    baseline=json.loads((ROOT/'assets/fonts/zion-font-manifest.json').read_text())
    if original_hash!=baseline['source_sha256']:
        raise ValueError('Use the same licensed source font as the full fallback')
    symbols=ui_characters()
    if not set(symbols).issubset(set((ROOT/'assets/fonts/coverage.txt').read_text(encoding='utf8'))):
        raise ValueError('UI inventory exceeds the full fallback coverage')
    manifest={'converter':'lv_font_conv 1.5.3','source_sha256':original_hash,'resolved_weight':600,
              'source_url':baseline['source_url'],'license':'SIL OFL 1.1','bpp':4,'compressed':False,
              'ui_sources':list(UI_SOURCES),'fonts':[]}
    coverage=(ROOT/'assets/fonts/coverage.txt').read_text(encoding='utf8')
    probe=next(ord(c) for c in reversed(coverage) if c not in symbols and '\u4e00'<=c<='\u9fff')
    manifest['fallback_probe_codepoint']=probe
    with tempfile.TemporaryDirectory(prefix='zion-ui-font-') as temp:
        font=TTFont(original)
        if 'fvar' in font:
            font=instantiateVariableFont(font,{'wght':600},inplace=False)
        resolved=Path(temp)/'zion-semibold.ttf';font.save(resolved)
        for name,size,chars,fallback in [('zion_font_ui_14',14,symbols,'zion_font_14'),
                                         ('zion_font_ui_10',10,''.join(chr(i) for i in range(32,127)),'zion_font_ui_14')]:
            out=ROOT/'assets/fonts'/f'{name}.c'
            subprocess.run(['node',str(converter),'--font',str(resolved),'--symbols',chars,
                '--size',str(size),'--bpp','4','--format','lvgl','--no-compress','--no-kerning',
                '--lv-font-name',name,'--lv-include','lvgl.h','--output',str(out)],check=True)
            code=out.read_text(encoding='utf8')
            code='\n'.join(line for line in code.splitlines() if ' * Opts:' not in line)+'\n'
            code=code.replace('#include "lvgl.h"','#include "lvgl.h"\nLV_FONT_DECLARE('+fallback+');')
            if code.count('.fallback = NULL,')!=1:
                raise ValueError('Unexpected converter fallback declaration')
            code=code.replace('.fallback = NULL,','.fallback = &'+fallback+',',1)
            out.write_text(code,encoding='utf8',newline='\n')
            manifest['fonts'].append({'name':name,'size':size,'characters':len(chars),'symbols':chars,
                'fallback':fallback,'c_sha256':hashlib.sha256(out.read_bytes()).hexdigest()})
    (ROOT/'assets/fonts/zion-ui-font-manifest.json').write_text(
        json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf8',newline='\n')
    inventory='#pragma once\n#include <stdint.h>\n'
    inventory+=f'#define ZION_UI_FONT_COUNT {len(symbols)}u\n#define ZION_FALLBACK_PROBE {probe}u\n'
    inventory+='static const uint32_t zion_ui_font_codepoints[ZION_UI_FONT_COUNT] = {\n'
    inventory+='\n'.join('    '+','.join(f'0x{ord(c):x}' for c in symbols[i:i+12])+',' for i in range(0,len(symbols),12))
    inventory+='\n};\n'
    (ROOT/'assets/fonts/zion_font_ui_inventory.h').write_text(inventory,encoding='utf8',newline='\n')
    print(f'Generated UI fonts: {len(symbols)} characters at 14px/4bpp, 95 ASCII at 10px/4bpp; full 4bpp fallback retained')


if __name__=='__main__':
    if len(sys.argv) not in (3,4):
        raise SystemExit(__doc__)
    generate(*sys.argv[1:])
