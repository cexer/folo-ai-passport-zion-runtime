"""Generated font coverage, pixel bounds, fallback and widget-selection checks.

Requires neither fontTools nor the source OTF; actual LVGL decode is separately
compared on the test firmware by tests/firmware/zion_font_selftest.c.
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('generate_zion_ui_font',ROOT/'tools/generate-zion-ui-font.py')
generator=importlib.util.module_from_spec(spec);spec.loader.exec_module(generator)


def array(source,name):
    data=re.search(r'\b'+re.escape(name)+r'\[\]\s*=\s*\{(.*?)\};',source,re.S)[1]
    data=re.sub(r'/\*.*?\*/','',data,flags=re.S)
    return [int(x,0) for x in re.findall(r'0x[\da-fA-F]+|\b\d+\b',data)]


class FontTest(unittest.TestCase):
    def test_full_ui_coverage_and_decodable_glyph_bounds(self):
        manifest=json.loads((ROOT/'assets/fonts/zion-ui-font-manifest.json').read_text(encoding='utf8'))
        self.assertEqual(manifest['ui_sources'],list(generator.UI_SOURCES))
        self.assertEqual(manifest['fonts'][0]['symbols'],generator.ui_characters())
        fallback=(ROOT/'assets/fonts/coverage.txt').read_text(encoding='utf8')
        probe=chr(manifest['fallback_probe_codepoint'])
        self.assertIn(probe,fallback);self.assertNotIn(probe,manifest['fonts'][0]['symbols'])
        for info in manifest['fonts']:
            path=ROOT/'assets/fonts'/(info['name']+'.c')
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(),info['c_sha256'])
            source=path.read_text(encoding='utf8')
            self.assertRegex(source,r'\.bpp\s*=\s*4\b')
            self.assertRegex(source,r'\.bitmap_format\s*=\s*0\b')
            self.assertIn('.fallback = &'+info['fallback']+',',source)
            self.assertEqual(source.count('LV_FONT_DECLARE('+info['fallback']+');'),source.count('#include "lvgl.h"'))
            self.assertNotIn(' * Opts:',source)
            bitmap=array(source,'glyph_bitmap')
            entries=re.search(r'glyph_dsc\[\]\s*=\s*\{(.*?)\};',source,re.S)[1]
            glyphs=[dict((k,int(v)) for k,v in re.findall(r'\.(\w+)\s*=\s*(-?\d+)',block))
                    for block in re.findall(r'\{([^{}]*)\}',entries) if 'bitmap_index' in block]
            self.assertEqual(len(glyphs),info['characters']+1)
            cmaps=re.search(r'cmaps\[\]\s*=\s*\{(.*?)\};',source,re.S)[1]
            mapping={}
            for block in re.findall(r'\{([^{}]*)\}',cmaps):
                fields=dict(re.findall(r'\.(\w+)\s*=\s*([\w]+)',block))
                start=int(fields['range_start'],0);first=int(fields['glyph_id_start'],0)
                offsets=(range(int(fields['range_length'],0)) if fields['unicode_list']=='NULL'
                         else array(source,fields['unicode_list']))
                for i,offset in enumerate(offsets):mapping[start+offset]=first+i
            self.assertEqual(set(mapping),{ord(c) for c in info['symbols']})
            self.assertTrue(set(info['symbols']).issubset(fallback))
            for cp,gid in mapping.items():
                self.assertLess(gid,len(glyphs));g=glyphs[gid]
                end=g['bitmap_index']+(g['box_w']*g['box_h']*4+7)//8
                limit=glyphs[gid+1]['bitmap_index'] if gid+1<len(glyphs) else len(bitmap)
                self.assertLessEqual(end,limit,f'U+{cp:04X}')
            self.assertTrue(any((value>>4) not in (0,15) or (value&15) not in (0,15) for value in bitmap))
        full=(ROOT/'assets/fonts/zion_font_14.c').read_text(encoding='utf8')
        self.assertRegex(full,r'\.bpp\s*=\s*1\b')
        self.assertIn('.fallback = NULL,',full)

    def test_actual_widgets_select_the_product_fonts(self):
        policy=(ROOT/'main/zion_fonts.h').read_text()
        self.assertIn('#define ZION_FONT (&zion_font_ui_14)',policy)
        self.assertIn('#define ZION_FONT_SMALL (&zion_font_ui_10)',policy)
        launcher=(ROOT/'main/zion_launcher.c').read_text(encoding='utf8')
        self.assertEqual(launcher.count('lv_font_unscii_8'),1)
        self.assertIn('details[i],&lv_font_unscii_8',launcher)
        self.assertIn('power,ZION_FONT_SMALL',launcher)
        self.assertIn('row(5,y+38,"> 设置"',launcher)
        self.assertNotIn('LABEL(footer,notice)',launcher)
        self.assertIn('LABEL(footer,"短按OK启动")',launcher)
        self.assertIn('LABEL(footer_right,"长按OK管理")',launcher)
        self.assertIn('footer_right,LV_TEXT_ALIGN_RIGHT',launcher)


if __name__=='__main__':
    unittest.main()
