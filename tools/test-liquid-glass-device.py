"""Original eight-scene Liquid Glass workflow: navigation, focus, actions, teardown."""
import argparse,json
from pathlib import Path
from community_device import Device
p=argparse.ArgumentParser();p.add_argument('--port');p.add_argument('--sim',type=Path);p.add_argument('--instance')
p.add_argument('--output',type=Path,required=True);p.add_argument('--smoke',action='store_true');a=p.parse_args()
if a.sim:
    from community_device_sim import SimDevice
    assert a.instance;d=SimDevice(a.sim,a.instance,a.output)
else:
    assert a.port;d=Device(a.port,a.output)
r={'mainflow_pass':False,'pages':[]}
try:
    r['memory_before']=d.memory()
    # First input stops the source's unattended seven-second reel.
    d.key('DOWN',.7);previous=None
    for i in range(2 if a.smoke else 8):
        before=d.capture(f'page-{i}-before')
        if previous is not None:r['pages'][-1]['next_page_pixels']=d.changed(previous,before,(0,24,240,295))
        d.key('DOWN',.6);focused=d.capture(f'page-{i}-focus')
        changes=d.changed(before,focused,(0,24,240,295))
        d.key('OK',.6);acted=d.capture(f'page-{i}-action')
        r['pages'].append({'focus_pixels':changes})
        previous=acted;d.key('UP',.8)
    if not a.smoke:
        d.key('LONG',.5);d.capture('hardware-menu')
        d.key('OK',.8);d.capture('reentered-showcase')
    r['memory_after']=d.memory();r['automated_flow_pass']=True
    r['scope']='two scenes, focus, actions, UP/DOWN/OK' if a.smoke else 'eight scenes, focus/actions, UP/DOWN/OK/LONG, teardown to diagnostic menu and reentry'
    r['unverified']=['Optional hardware diagnostic pages','Public networking and BLE services','all visual quality profiles and prolonged animation stability','physical switch feel and acoustics']
finally:d.close(r)
print(json.dumps(r,indent=2))
