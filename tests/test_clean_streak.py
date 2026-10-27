import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from clean_streak import qualifies,advance
good={'mainflow_pass':True,'automated_flow_pass':True,'visual_review':'actual LCD checked','environment':'physical device'}
assert qualifies(good,True,[],False)
for key,value in [('mainflow_pass',False),('automated_flow_pass',False),('visual_review',''),('environment','PassportSim')]:
    assert not qualifies({**good,key:value},True,[],False)
assert not qualifies(good,False,[],False)
assert not qualifies(good,True,['app_specific_repair'],False)
assert not qualifies(good,True,[],True)
streak=0
for passed,expected in [(True,1),(True,2),(False,0),(True,1),(True,2),(True,3)]:
    streak=advance(streak,passed);assert streak==expected
print('PASS clean streak: physical proof, source/base identity, no repair wrapper, failure resets')
