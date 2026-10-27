"""Unchanged virtual pet: actual hatch timing, care, sleep and menu lifecycle."""
import argparse,json,time
from pathlib import Path
from community_device import Device
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--persistence',action='store_true');a=p.parse_args()
d=Device(a.port,a.output);r={'mainflow_pass':False}
try:
    r['memory_before']=d.memory();d.capture('initial')
    if not a.persistence:
        # Preserve original one-minute hatch rule; no patched clock/model.
        for _ in range(7):time.sleep(9);d.memory()
        d.key('UP');d.key('DOWN');d.capture('hatched')
        for i,name in enumerate(['feed','play','clean','heal','sleep']):
            if i:d.key('DOWN')
            d.key('OK',delay=.7);d.capture(name)
        d.key('OK',delay=.7);d.capture('wake')
        d.key('LONG');d.capture('menu');d.key('OK');d.capture('reentry')
    else:
        d.key('DOWN');d.key('UP');d.capture('persisted-pet')
    r['memory_after']=d.memory();r['automated_flow_pass']=True
    r['scope']='Saved pet after MCU reset' if a.persistence else 'Original timed hatch, five care actions, sleep/wake, menu teardown/reentry, UP/DOWN/OK/LONG'
    r['unverified']=['Adult growth, sickness/rescue and long-duration decay','Wi-Fi/BLE/low-power demo pages','Physical contacts and audible quality']
finally:d.close(r)
print(json.dumps(r,indent=2))
