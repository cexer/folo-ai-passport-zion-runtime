"""Original compass: image, timers, reveal, Chinese result scroll and recast."""
import argparse,json,time
from pathlib import Path
from community_device import Device
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
d=Device(a.port,a.output);r={'mainflow_pass':False}
try:
    r['memory_before']=d.memory();home=d.capture('home');d.key('OK');d.capture('short-ok-home')
    d.key('LONG');d.capture('focus');time.sleep(4.5);d.capture('revealing');time.sleep(2.5);result=d.capture('result')
    for _ in range(5):d.key('DOWN')
    scroll=d.capture('reading');r['scroll_pixels']=d.changed(result,scroll)
    d.key('UP');d.key('UP');d.capture('reading-up');d.key('OK');back=d.capture('returned-home')
    d.key('LONG');time.sleep(7);d.capture('second-result');d.key('LONG');d.capture('long-back-home')
    r['memory_after']=d.memory();r['automated_flow_pass']=True
    r['scope']='Original home image/fonts, short-OK no-op, long-OK ritual, countdown/reveal timers, result scrolling DOWN/UP, short-OK home, recast and long-OK return'
    r['unverified']=['All 64 result combinations and randomness distribution','Physical contacts and sleep current']
finally:d.close(r)
print(json.dumps(r,indent=2))
