"""Actual unchanged-source Connect Four: AI, two-player, keys and BLE start."""
import argparse,json,time
from pathlib import Path
from community_device import Device
p=argparse.ArgumentParser();p.add_argument('--port');p.add_argument('--output',type=Path,required=True)
p.add_argument('--sim',type=Path);p.add_argument('--instance')
p.add_argument('--smoke',action='store_true');a=p.parse_args()
if a.sim:
    from community_device_sim import SimDevice
    assert a.instance
    d=SimDevice(a.sim,a.instance,a.output)
else:
    assert a.port
    d=Device(a.port,a.output)
r={'mainflow_pass':False}
try:
    r['memory_before']=d.memory();menu=d.capture('menu')
    # Original landscape controls intentionally make UP advance menu rows.
    for _ in range(3):d.key('UP')
    d.key('OK');empty=d.capture('board-empty')
    d.key('UP');start=len(d.lines);d.key('OK');r['ai_reply_1']=d.proof('电脑选列=',start,10)
    board=d.capture('board-ai');r['move_pixels']=d.changed(empty,board,(0,24,320,225))
    if not a.smoke:
        d.key('DOWN');start=len(d.lines);d.key('OK');r['ai_reply_2']=d.proof('电脑选列=',start,10)
        d.capture('board-ai-2');d.key('LONG');d.capture('returned-menu')
        d.key('UP');d.key('OK') # row START -> MODE, human-first -> AI-first
        for _ in range(3):d.key('UP')
        start=len(d.lines);d.key('OK');r['ai_opens']=d.proof('电脑选列=',start,10);d.capture('ai-first')
        d.key('LONG');d.key('UP');d.key('OK') # -> two players
        d.key('UP');d.key('UP');d.key('OK') # LEVEL is skipped in two-player mode
        d.key('OK');d.key('UP');d.key('OK');d.capture('two-players')
        d.key('LONG');d.key('UP');d.key('OK') # -> BLE link
        d.key('UP');d.key('UP');d.key('OK');time.sleep(2)
        d.capture('ble-discovery');r['memory_ble']=d.memory()
        d.key('LONG');d.capture('ble-stopped-menu');r['memory_ble_stopped']=d.memory()
    r['memory_after']=d.memory();r['automated_flow_pass']=True
    r['scope']='AI moves, UP/DOWN/OK/LONG, landscape LCD' if a.smoke else 'human-first AI replies, AI-first, two-player drops, BLE discovery start/stop, landscape LCD, keys'
    r['unverified']=['Second physical C3 BLE opponent','all AI difficulty/endgame branches','deep-sleep power consumption and physical wake','acoustic quality']
finally:d.close(r)
print(json.dumps(r,ensure_ascii=False,indent=2))
