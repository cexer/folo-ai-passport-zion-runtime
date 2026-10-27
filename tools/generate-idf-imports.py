"""Generate link-time references for reviewed application import plans.
This forces the SDK linker to prove each provider exists; no address guessing.
"""
import argparse,json,re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('plans',nargs='+',type=Path);a=p.parse_args()
manual=set(re.findall(r'^ZION_IMPORT\((\w+),',(root/'main/zion_idf_exports.inc').read_text(),re.M))
manual.update(['memcpy','memset','memcmp','strlen','__divdi3','__udivdi3','__umoddi3'])
symbols=set()
# Provenance: Pinball exposed scalar math, V-Pet GAP observer APIs, Snoopy clock
# and image-cache APIs, Connect Four central/GATT and landscape drawing, and
# Liquid Glass the lower-level drawing/motion/screenshot surface. Keep the union
# Weiqi exposed scrolling; Liuyao exposed arc widgets, top layers and double-to-int.
# of reviewed plans; docs/runtime-optimization-log.md records failures/evidence.
for file in a.plans:symbols.update(json.loads(file.read_text())['imports'])
symbols=sorted(s for s in symbols if s not in manual and not s.startswith('zion_idf_contract_'))
assert all(re.fullmatch(r'[A-Za-z_][A-Za-z_0-9.$]*',s) for s in symbols)
(root/'main/zion_idf_auto_imports.json').write_text(json.dumps(dict(imports=symbols),indent=2)+'\n')
(root/'main/zion_idf_auto_imports.inc').write_text('/* Generated from real application imports; link failure is a missing provider. */\n'+''.join(f'ZION_AUTO_IMPORT({i},"{s}")\n' for i,s in enumerate(symbols)))
print(json.dumps(dict(auto_imports=len(symbols)),indent=2))
