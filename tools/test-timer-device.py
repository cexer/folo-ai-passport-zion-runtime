"""Unchanged timer: pause/resume/cancel and real five-minute expiry, silent sink."""
import argparse,json,time
from pathlib import Path
from community_device import Device
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--quick',action='store_true');a=p.parse_args()
d=Device(a.port,a.output);r={'mainflow_pass':False}
try:
    r['memory_before']=d.memory();first=d.capture('select-5m');d.key('DOWN');next_preset=d.capture('select-10m');r['preset_pixels']=d.changed(first,next_preset);d.key('UP')
    d.key('OK');time.sleep(3);d.capture('running');d.key('OK');d.capture('paused');time.sleep(3);d.capture('paused-later')
    d.key('OK');time.sleep(2);d.capture('resumed');d.key('LONG');d.capture('cancelled')
    if not a.quick:
        start=len(d.lines);d.key('OK');began=time.monotonic()
        for i in range(16):
            time.sleep(20);memory=d.memory();elapsed=time.monotonic()-began
            (a.output/'progress.json').write_text(json.dumps({'elapsed_seconds':elapsed,'memory':memory})+'\n')
            if i in (6,14):d.capture('countdown-'+str(i+1))
        d.capture('expired');r['actual_silent_alarm']=d.proof('I2S_ZERO_OUTPUT|I2S_ZERO_PROGRESS',start,15)
        r['elapsed_seconds']=time.monotonic()-began;d.key('OK');d.capture('reset-select')
    r['memory_after']=d.memory();r['automated_flow_pass']=True
    r['scope']='Preset UP/DOWN, start/pause/resume/cancel'+('' if a.quick else ', original five-minute countdown expiry, TIME UP flash, actual silent I2S alert and OK reset')
    r['unverified']=['All six preset durations','Physical contacts and audible tone quality']+(['Expiry/alarm not repeated on this boot route'] if a.quick else [])
finally:d.close(r)
print(json.dumps(r,indent=2))
