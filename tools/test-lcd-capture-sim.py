"""Test the firmware chunk encoder through PassportSim's raw RFC2217 USB.

CLI serial previews truncate long lines and cannot reconstruct pixels.
This validates wire bytes, not physical USB timing or the real panel.
"""
import argparse,json,subprocess,sys
from pathlib import Path
from community_device_sim import SimDevice
p=argparse.ArgumentParser();p.add_argument('--sim',type=Path,required=True);p.add_argument('--instance',required=True)
p.add_argument('--output',type=Path,required=True);a=p.parse_args()
d=SimDevice(a.sim,a.instance,a.output);result={'mainflow_pass':False};opened=False
try:
    endpoint=d.call('endpoint',a.instance,'--tcp',selected=False);opened=True
    r=subprocess.run([sys.executable,str(Path(__file__).with_name('capture-native-screen.py')),
        '--port',endpoint['tcp']['rfc2217'],'--output',str(a.output/'lcd.png')],
        capture_output=True,text=True,encoding='utf8',timeout=25)
    (a.output/'capture.log').write_text(r.stdout+r.stderr,encoding='utf8')
    assert r.returncode==0,r.stdout+r.stderr
    capture=json.loads((a.output/'lcd.json').read_text())
    result.update(wire_capture_pass=True,geometry=capture['geometry'],complete_rows=capture['rows'],
        scope='Actual firmware encoder, raw simulated USB, strict reconstruction; physical timing unverified')
finally:
    if opened:d.call('endpoint',a.instance,'--close',selected=False)
    d.close(result)
print(json.dumps(result,indent=2))
