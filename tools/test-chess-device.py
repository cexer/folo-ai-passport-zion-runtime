"""Original chess: local moves, Chinese UI, AI reply and cancellation menu."""
import argparse,json,time
from pathlib import Path
from community_device import Device
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
d=Device(a.port,a.output);r={'mainflow_pass':False}
try:
    r['memory_before']=d.memory();board=d.capture('local-start')
    d.key('DOWN');moved=d.capture('focus');r['focus_pixels']=d.changed(board,moved);d.key('UP')
    d.key('OK');d.capture('targets');d.key('OK');white=d.capture('white-move')
    d.key('OK');d.key('OK');black=d.capture('black-move');r['moves_pixels']=d.changed(white,black,(0,30,240,270))
    d.key('LONG');d.capture('pause')
    for _ in range(4):d.key('DOWN')
    d.key('OK');d.capture('home')
    for _ in range(3):d.key('DOWN')
    d.key('OK');d.capture('chinese-home');d.key('UP');d.key('OK');d.capture('difficulty')
    d.key('OK');d.key('DOWN');d.capture('human-black');d.key('OK',delay=2);d.capture('ai-opening')
    start=len(d.lines);d.key('OK');d.key('OK',delay=2)
    r['actual_ai_search']=d.proof('search done outcome=',start,20);d.capture('ai-reply')
    d.key('DOWN');d.key('UP');d.key('LONG');d.capture('chinese-pause')
    r['memory_after']=d.memory();r['automated_flow_pass']=True
    r['scope']='Two local moves, piece/target navigation, pause/home, Chinese language, Easy AI human-black opening and actual off-book search/reply, UP/DOWN/OK/LONG'
    r['unverified']=['All chess rules, promotion/endgame and difficulty strength','Deep sleep/wake and physical contacts','Optional 4096-entry TT skipped by original memory policy']
finally:d.close(r)
print(json.dumps(r,indent=2))
