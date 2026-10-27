#!/usr/bin/env python3
"""Generate a compact, compressed 4-bpp font with lv_font_conv 1.5.3.

Usage: python tools/generate-zion-font.py FONT.ttf LV_FONT_CONV.js [FONTTOOLS_DIRECTORY]
Source: google/fonts ofl/notosanssc, SIL OFL 1.1 (see assets/fonts).
"""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
font = Path(sys.argv[1]).resolve()
converter = Path(sys.argv[2]).resolve()
source_font = font
if len(sys.argv) > 3:
    sys.path.insert(0, str(Path(sys.argv[3]).resolve()))
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
source_ttf = TTFont(font)
font_defaults = {a.axisTag: a.defaultValue for a in source_ttf['fvar'].axes} if 'fvar' in source_ttf else {}
font_temp = tempfile.TemporaryDirectory(prefix='zion-font-')
if 'wght' in font_defaults:
    instance = instantiateVariableFont(source_ttf, {'wght': 600}, inplace=False)
    font = Path(font_temp.name) / 'zion-semibold.ttf'
    instance.save(font)
chars = set(chr(i) for i in range(0x20, 0x7f))
for lead in range(0xb0, 0xf8):
    for tail in range(0xa1, 0xff):
        try:
            ch = bytes([lead, tail]).decode('gb2312')
            if '\u4e00' <= ch <= '\u9fff':
                chars.add(ch)
        except UnicodeDecodeError:
            pass
chars.update('，。：；？！、（）【】《》—·')
symbols = ''.join(sorted(chars))
out = root / 'assets/fonts/zion_font_14.c'
subprocess.run(['node', str(converter), '--font', str(font), '--symbols', symbols,
                '--size', '14', '--bpp', '4', '--format', 'lvgl',
                '--no-kerning', '--lv-font-name', 'zion_font_14', '--lv-include', 'lvgl.h',
                '--output', str(out)], check=True)
code = out.read_text(encoding='utf-8')
# Converter's provenance comment otherwise embeds local absolute paths.
lines = [line for line in code.splitlines() if ' * Opts:' not in line]
out.write_text('\n'.join(lines) + '\n', encoding='utf-8')
(root / 'assets/fonts/coverage.txt').write_text(symbols, encoding='utf-8')
(root / 'assets/fonts/zion-font-manifest.json').write_text(json.dumps({
    'converter': 'lv_font_conv 1.5.3', 'size': 14, 'bpp': 4, 'compressed': True,
    'characters': len(chars), 'source_sha256': hashlib.sha256(source_font.read_bytes()).hexdigest(),
    'source_axis_defaults':font_defaults, 'resolved_weight':600,
    'source_url': 'https://github.com/google/fonts/tree/main/ofl/notosanssc',
    'license': 'SIL OFL 1.1', 'c_sha256': hashlib.sha256(out.read_bytes()).hexdigest()
}, indent=2), encoding='utf-8')
print('Generated font:', len(chars), 'characters; 4 bpp, no kerning, compressed')

font_temp.cleanup()
