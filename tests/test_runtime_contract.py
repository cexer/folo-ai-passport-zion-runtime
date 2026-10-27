"""Host checks for the shared font and the power-only exit contract."""
from pathlib import Path
import re, unittest
ROOT=Path(__file__).resolve().parents[1]

class RuntimeContract(unittest.TestCase):
    def test_ui_font_covers_runtime_text(self):
        text=(ROOT/'assets/fonts/zion_font_14.c').read_text(encoding='utf8')
        arrays={}
        for match in re.finditer(r'static const uint\d+_t\s+(\w+)\[\]\s*=\s*\{(.*?)\};',text,re.S):
            body=re.sub(r'/\*.*?\*/','',match[2],flags=re.S)
            arrays[match[1]]=[int(n,0) for n in re.findall(r'0x[0-9a-fA-F]+|\b\d+\b',body)]
        points=set()
        for match in re.finditer(r'\{\s*\.range_start\s*=\s*(\d+),\s*\.range_length\s*=\s*(\d+),(.*?)\}',text,re.S):
            start,length=int(match[1]),int(match[2]);body=match[3]
            sparse=re.search(r'\.unicode_list\s*=\s*(\w+)',body)[1]
            offsets=re.search(r'\.glyph_id_ofs_list\s*=\s*(\w+)',body)[1]
            if sparse!='NULL': points.update(start+i for i in arrays[sparse])
            elif offsets!='NULL': points.update(start+i for i,v in enumerate(arrays[offsets]) if i==0 or v)
            else: points.update(range(start,start+length))
        needed=set()
        for name in ['main/zion_runtime.c','main/zion_runtime_view.cc','main/zion_elf_host.c',
                     'examples/plugins/counter.c',
                     'components/xiaozhi/main.cc','components/xiaozhi/display/text_display.cc',
                     'components/xiaozhi/boards/folotoy/ai-passport/ai_passport_board.cc']:
            needed.update(map(ord,re.findall(r'[\u3400-\u9fff]',(ROOT/name).read_text(encoding='utf8'))))
        self.assertGreater(len(points),6000)
        self.assertFalse(needed-points,'Missing glyphs: '+''.join(chr(c) for c in sorted(needed-points)))

    def test_no_ok_restart_api_or_handler(self):
        for name in ['main/zion_runtime.c','components/zion_sdk/include/zion_sdk.h',
                     'components/xiaozhi/main.cc',
                     'components/xiaozhi/boards/folotoy/ai-passport/ai_passport_board.cc']:
            source=(ROOT/name).read_text(encoding='utf8')
            self.assertNotIn('restart_to_launcher',source,name)
            self.assertNotIn('esp_restart(',source,name)
            self.assertNotIn('zion_builtin_return',source,name)

if __name__=='__main__': unittest.main()
