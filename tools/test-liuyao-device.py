"""Original dual-tool app: dates, animated six casts, results and standby arcs."""
import argparse,json,time
from pathlib import Path
from community_device import Device
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--standby',action='store_true');a=p.parse_args()
d=Device(a.port,a.output);r={'mainflow_pass':False}
try:
    r['memory_before']=d.memory();home=d.capture('home')
    d.key('DOWN');selected=d.capture('selection');r['selection_pixels']=d.changed(home,selected)
    d.key('UP');d.key('OK');d.capture('liuyao-welcome');d.key('DOWN');d.key('UP');d.key('OK');d.capture('category')
    d.key('DOWN');d.key('UP');d.key('OK');date=d.capture('date');d.key('UP');changed=d.capture('date-up');r['date_pixels']=d.changed(date,changed);d.key('DOWN')
    for _ in range(5):d.key('OK')
    d.capture('cast-start')
    for i in range(6):
        d.key('OK',delay=1.3)
        if i in (0,5):d.capture('cast-'+str(i+1))
    d.key('OK');result=d.capture('interpretation');d.key('DOWN');next_page=d.capture('interpretation-next');r['result_pixels']=d.changed(result,next_page)
    d.key('UP');d.key('LONG');d.key('LONG');d.key('DOWN');d.key('OK');d.capture('dressing-welcome');d.key('OK')
    for _ in range(5):d.key('OK')
    d.capture('dressing-result');r['memory_after']=d.memory()
    if a.standby:
        # Original 60-second timer, not a compile-time accelerated replacement.
        for _ in range(7):time.sleep(9);d.memory()
        sleep=d.capture('standby-arcs');d.key('UP');awake=d.capture('standby-wake');r['standby_pixels']=d.changed(sleep,awake)
    d.key('LONG');d.key('LONG');d.capture('returned-home')
    r['automated_flow_pass']=True;r['scope']='Selection, date edit, six animated coin casts, interpretation pages, dressing result, UP/DOWN/OK/LONG and optional original standby/wake'
    r['unverified']=['All divination combinations and calendar correctness','Physical button contacts and audible quality']
finally:d.close(r)
print(json.dumps(r,indent=2))
