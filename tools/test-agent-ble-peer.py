"""Observe real HID advertising and attempt a real Windows BLE connection.

Only optional neutral, zero-motion reports are sent. Windows reserves some HID
GATT services; record that boundary instead of substituting mock notifications.
"""
import argparse,asyncio,json,re,threading,time
from pathlib import Path
from bleak import BleakScanner,BleakClient
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
p.add_argument('--pair',action='store_true');p.add_argument('--serial-port')
p.add_argument('--neutral-report',action='store_true');a=p.parse_args()
assert not a.neutral_report or a.serial_port,'Neutral reports need the board USB port'
serial_data=[];stop=threading.Event();serial_stream=None
def observe():
    global serial_stream
    import serial
    s=serial.Serial();s.port=a.serial_port;s.baudrate=115200;s.timeout=.1;s.dtr=False;s.rts=False;s.open()
    serial_stream=s
    try:
        while not stop.is_set():
            chunk=s.read(s.in_waiting or 1)
            if chunk:serial_data.append(chunk)
    finally:s.close()
async def command(line,pattern):
    start=len(b''.join(serial_data));serial_stream.write((line+'\n').encode());serial_stream.flush()
    end=time.monotonic()+8
    while time.monotonic()<end:
        raw=b''.join(serial_data)[start:].decode('utf8','replace')
        match=re.search(pattern,raw)
        if match:return match[0].strip()
        await asyncio.sleep(.05)
    raise TimeoutError('No reply for '+line)
async def run():
    found=await BleakScanner.discover(timeout=12,return_adv=True)
    matches=[(d,v) for d,v in found.values() if v.local_name=='AI Passport' and
             '00001812-0000-1000-8000-00805f9b34fb' in v.service_uuids]
    result=dict(advertising='PASS' if len(matches)==1 else 'FAIL',matches=len(matches))
    if len(matches)==1:
        device,adv=matches[0];result.update(name=adv.local_name,service_uuids=adv.service_uuids,rssi=adv.rssi)
        try:
            async with BleakClient(device,pair=a.pair,timeout=25) as client:
                result['connected']=client.is_connected
                result['services']=[s.uuid for s in client.services]
                await asyncio.sleep(3)
                if a.neutral_report:
                    await command('hello',r'@hello [^\r\n]+')
                    reply=await command('scroll 0 0',r'@(?:ok|err)[^\r\n]*')
                    result['neutral_mouse_report']=reply
                    result['pause_connected']=await command('ZION KEY OK',r'bridge: paused')
                    result['resume_connected']=await command('ZION KEY OK',r'bridge: resumed')
                    result['memory_connected']=await command('ZION MEM',r'ZION_MEMORY [^\r\n]+')
                    await command('bye',r'@ok')
                if a.pair:await client.unpair()
        except Exception as e:result['connection_error']=str(e).replace(device.address,'[DEVICE]')
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
observer=threading.Thread(target=observe) if a.serial_port else None
if observer:observer.start()
try:asyncio.run(run())
finally:
    stop.set()
    if observer:
        observer.join()
        raw=b''.join(serial_data).decode('utf8','replace')
        raw=re.sub(r'(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}','[MAC REDACTED]',raw)
        a.output.with_suffix('.device.log').write_text(raw,encoding='utf8')
