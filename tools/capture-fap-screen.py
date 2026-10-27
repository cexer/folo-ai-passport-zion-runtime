"""Read a complete original FAP_SCREENSHOT_V1 frame; never fill missing pixels."""
import argparse,json,re,time
from pathlib import Path
import serial
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('--port',required=True)
p.add_argument('--output',type=Path,required=True);a=p.parse_args()
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.1;s.dtr=False;s.rts=False;s.open()
try:
    s.reset_input_buffer();s.write(b'FAP_SCREENSHOT_V1\n');s.flush()
    pending=b'';end=time.monotonic()+20;header=None
    while time.monotonic()<end and not header:
        pending+=s.read(s.in_waiting or 1)
        while b'\n' in pending:
            line,pending=pending.split(b'\n',1)
            m=re.fullmatch(rb'FAP_SCREENSHOT_V1 (\d+) (\d+) RGB565LE (\d+)\r?',line)
            if m:header=tuple(map(int,m.groups()));break
    assert header,'No FAP screenshot header'
    w,h,size=header;assert w==240 and h==320 and size==w*h*2,header
    while len(pending)<size and time.monotonic()<end:
        pending+=s.read(min(65536,size-len(pending)))
    assert len(pending)>=size,('Incomplete FAP frame',len(pending),size)
    rgb=bytearray()
    for at in range(0,size,2):
        v=int.from_bytes(pending[at:at+2],'little')
        rgb+=bytes(((v>>11)*255//31,((v>>5)&63)*255//63,(v&31)*255//31))
    a.output.parent.mkdir(parents=True,exist_ok=True)
    Image.frombytes('RGB',(w,h),bytes(rgb)).save(a.output)
    a.output.with_suffix('.json').write_text(json.dumps(dict(protocol='original FAP_SCREENSHOT_V1',bytes=size,width=w,height=h))+'\n')
    print(json.dumps(dict(status='PASS',image=str(a.output))))
finally:s.close()
