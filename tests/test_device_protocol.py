import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from device_protocol import match_record
p='STANDALONE_KEY|ZION_KEY_DONE'
assert match_record(p,['ZIO','N_KEY_DONE remaining_stack=6784'])=='ZION_KEY_DONE remaining_stack=6784'
assert match_record(p,['STANDALONE_K','EY key=2'])=='STANDALONE_KEY key=2'
assert match_record(p,['ZION_KEY_DONE remaining_stack=4'])
assert match_record(p,['ZIO','N_','KEY_DONE remaining_stack=6788'])=='ZION_KEY_DONE remaining_stack=6788'
assert match_record(p,['ZION_KEY_DONE remaining_stack=4','unrelated text']) is None
assert match_record(r'ZION_MEMORY free=\d+ allocated=\d+ largest=\d+',[
    'ZION_MEMORY free=157428 all','ocated=89276 largest=40960'])
assert match_record(p,['ZIO','I (12) unrelated: N_KEY_DONE']) is None
assert match_record(p,['unrelated ZIO','N_KEY_DONE']) is None
assert match_record(p,['ZION_KEY_DO','ZION_MEMORY bytes=0']) is None
print('PASS protocol adjacent fragments and unrelated-line rejection')
