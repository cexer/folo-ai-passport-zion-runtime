"""Original Xiao Liu Ren: read-only voice energy, numeric cast, keys and QR UI."""
import argparse,json,time
from pathlib import Path
from community_device import Device
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--private-output',type=Path,required=True);a=p.parse_args()
d=Device(a.port,a.output);r={'mainflow_pass':False}
def edge(key,phase):
    d.send(f'ZION EDGE {key} {phase}');d.wait(f'ZION_EDGE_DONE key={key} phase={phase} result=0')
def question():
    start=len(d.lines);edge(2,0);time.sleep(.8);edge(2,4)
    proof=d.proof(r'samples=\d+ mean_abs=\d+',start,15);time.sleep(.3);return proof
try:
    r['memory_before']=d.memory();d.capture('welcome');d.key('UP');d.capture('keys');d.key('OK')
    d.key('DOWN');d.capture('about');d.key('DOWN');d.key('UP');d.key('OK');d.key('OK');d.capture('record-prompt')
    r['actual_microphone']=question();d.capture('method');d.key('UP');one=d.capture('number-1');d.key('UP');two=d.capture('number-2');r['number_pixels']=d.changed(one,two)
    d.key('DOWN');d.key('OK');d.key('UP');d.key('OK');d.key('UP');d.key('UP');d.key('OK');d.capture('result-0')
    for i in range(1,4):d.key('DOWN');d.capture('result-'+str(i))
    d.key('UP');d.key('LONG');d.capture('ask-again');question();d.key('DOWN');d.capture('time-unconfigured');d.key('DOWN');time.sleep(2)
    # Hotspot QR contains a fresh device access password: never put this image
    # or payload in the public archive/report.
    public=d.output;a.private_output.mkdir(parents=True,exist_ok=True);d.output=a.private_output
    try:qr=d.capture('ap-qr')
    finally:d.output=public
    import zxingcpp
    decoded=zxingcpp.read_barcode(qr);assert decoded and decoded.bytes.startswith(b'WIFI:T:WPA;S:')
    r['actual_ap_qr']='PASS actual LCD decoded as Wi-Fi provisioning; credential payload kept private'
    d.key('OK');d.output=a.private_output
    try:url=d.capture('ap-url')
    finally:d.output=public
    decoded=zxingcpp.read_barcode(url);assert decoded and decoded.bytes==b'http://192.168.4.1'
    r['actual_url_qr']='PASS exact http://192.168.4.1';d.key('UP');d.key('OK');d.capture('returned-welcome')
    r['memory_after']=d.memory();r['automated_flow_pass']=True
    r['scope']='Chinese guide/about, actual I2S microphone sample count (no audio retained), RELEASE=5 mapping, three numbers, four result pages, ask again, AP and URL QR from actual LCD, UP/DOWN/OK/LONG'
    r['unverified']=['DPP enrolment on a physical phone','Station/NTP/time casting','All number outcomes','Physical switch contacts and acoustic quality']
finally:d.close(r)
print(json.dumps(r,indent=2))
