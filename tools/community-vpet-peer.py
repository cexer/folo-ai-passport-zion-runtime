"""Real Windows BLE transmitter/scanner carrying a deterministic test battle card."""
import argparse,asyncio,json,sys,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
p.add_argument('--duration',type=float,default=90);p.add_argument('--stop-file',type=Path,required=True)
p.add_argument('--packages',type=Path,default=Path(__file__).resolve().parents[4]/'build-tools/ble-python')
a=p.parse_args();sys.path.insert(0,str(a.packages.resolve()))
from bleak import BleakScanner
from winrt.windows.devices.bluetooth.advertisement import BluetoothLEAdvertisementPublisher,BluetoothLEManufacturerData
from winrt.windows.storage.streams import DataWriter
card=bytes.fromhex('56500101010c0c04030104000001003412')
seen=[];transitions=[]
def receive(device,adv):
    data=adv.manufacturer_data.get(65535,b'')
    if adv.local_name=='VPET' and data.startswith(b'VP') and len(data)==17:
        if not seen or seen[-1]['card']!=data.hex():seen.append(dict(time=time.time(),card=data.hex(),rssi=adv.rssi))
async def main():
    publisher=BluetoothLEAdvertisementPublisher();writer=DataWriter();writer.write_bytes(card)
    data=BluetoothLEManufacturerData();data.company_id=65535;data.data=writer.detach_buffer()
    publisher.advertisement.manufacturer_data.append(data)
    try:
        publisher.start()
        for _ in range(30):
            if int(publisher.status)==2:break
            if int(publisher.status)==5:raise RuntimeError('Windows BLE publisher aborted')
            await asyncio.sleep(.1)
        assert int(publisher.status)==2,('Publisher did not start',int(publisher.status))
        transitions.append(dict(event='STARTED',time=time.time()))
        print('VPET_RF_PEER_READY manufacturer=65535 card='+card.hex(),flush=True)
        async with BleakScanner(detection_callback=receive):
            end=time.monotonic()+a.duration
            while time.monotonic()<end and not a.stop_file.exists():
                assert int(publisher.status)==2,('Windows RF publisher stopped',int(publisher.status))
                await asyncio.sleep(.1)
    finally:
        publisher.stop();transitions.append(dict(event='STOP_REQUESTED',time=time.time()))
        writer.close()
        a.output.write_text(json.dumps(dict(transport='real Windows BLE radio',card=card.hex(),
            manufacturer=65535,device_advertisements=seen,transitions=transitions),indent=2)+'\n')
asyncio.run(main())
