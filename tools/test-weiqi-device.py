"""Original Weiqi: bounded page, scroll and board interaction acceptance."""
import argparse,json
from pathlib import Path
from community_device import Device
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
d=Device(a.port,a.output);r={'mainflow_pass':False}
try:
    r['memory_before']=d.memory();d.capture('title')
    d.key('DOWN');d.key('OK');d.capture('volume');d.key('UP');d.key('OK');world=d.capture('world')
    d.key('DOWN');world_next=d.capture('world-next');r['world_navigation_pixels']=d.changed(world,world_next)
    d.key('UP');d.key('OK');levels=d.capture('levels')
    for _ in range(7):d.key('DOWN')
    scrolled=d.capture('levels-scrolled');r['levels_scroll_pixels']=d.changed(levels,scrolled)
    for _ in range(7):d.key('UP')
    d.key('OK');d.capture('brief');d.key('DOWN');d.capture('brief-scroll');d.key('UP');d.key('OK');board=d.capture('board')
    d.key('UP');moved=d.capture('board-up');r['cursor_pixels']=d.changed(board,moved)
    d.key('DOWN');d.key('OK');d.capture('wrong-feedback')
    for name in ['back-brief','back-levels','back-world','back-title']:
        d.key('LONG');d.capture(name)
    r['memory_after']=d.memory();r['automated_flow_pass']=True
    r['scope']='Title volume, world/level navigation, scroll APIs, brief, board cursor, wrong-move feedback, UP/DOWN/OK/LONG, return to title'
    r['unverified']=['Double-click horizontal board movement','Winning/endgame branches and all 154 levels','Physical switch contacts and audible sound quality']
finally:d.close(r)
print(json.dumps(r,indent=2))
