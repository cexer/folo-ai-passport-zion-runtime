"""Capture actual DeviceTest LCD flush rows, with bounded serial observation."""
import argparse,json,re,time
from pathlib import Path
from PIL import Image
import serial
from lcd_capture import rgb888,combine_chunks
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
s=serial.serial_for_url(a.port,do_not_open=True);s.baudrate=115200;s.timeout=.1;s.dtr=False;s.rts=False;s.open();rows={};chunks={};lines=[]
try:
    s.reset_input_buffer();s.write(b'ZION TASKS\nZION CAPTURE\n');s.flush();end=time.monotonic()+10
    pending=b'';done=False;geometry=None
    # Bulk reads keep up with USB JTAG output. Serial.readline() reads one byte
    # per Win32 operation and can make the device's TX timeout drop pixel rows.
    while time.monotonic()<end and not done:
        pending+=s.read(max(1,min(65536,s.in_waiting)))
        while b'\n' in pending:
            raw,pending=pending.split(b'\n',1);line=raw.decode('utf8',errors='replace').strip()
            if not line:continue
            if any(x in line for x in ['Guru Meditation','assert failed']):
                tail=bytearray(pending);finish=time.monotonic()+1.2
                while time.monotonic()<finish:
                    tail.extend(s.read(max(1,min(65536,s.in_waiting))))
                a.output.parent.mkdir(parents=True,exist_ok=True)
                panic=a.output.with_suffix('.panic.log')
                panic.write_text(line+'\n'+tail.decode('utf8','replace'),encoding='utf8')
                raise RuntimeError('Firmware panic; full context: '+str(panic))
            row=re.search(r'ZION_LCD_ROW y=(\d+) data=([0-9a-f]+)$',line)
            chunk=re.search(r'ZION_LCD_CHUNK y=(\d+) offset=(\d+) data=([0-9a-f]+)$',line)
            if row:rows[int(row[1])]=bytes.fromhex(row[2])
            elif chunk:
                parts=chunks.setdefault(int(chunk[1]),{});at=int(chunk[2]);data=bytes.fromhex(chunk[3])
                assert at not in parts or parts[at]==data,'Conflicting LCD chunks'
                parts[at]=data
            elif line.startswith('ZION_LCD_') or line.startswith('ZION_TASK'):lines.append(line)
            if 'ZION_LCD_ARMED result=0' in line:raise RuntimeError('LVGL lock unavailable')
            end_record=re.search(r'ZION_LCD_END width=(\d+) height=(\d+) format=RGB565_BE',line)
            if end_record:geometry=tuple(map(int,end_record.groups()));done=True;break
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.with_suffix('.json').write_text(json.dumps(dict(rows=len(rows) or len(chunks),geometry=geometry,diagnostics=lines),indent=2)+'\n')
    assert geometry is not None,(len(rows),lines)
    if chunks:
        assert not rows,'Mixed LCD wire formats'
        rows=combine_chunks(chunks,*geometry)
    rgb=rgb888(rows,*geometry)
    Image.frombytes('RGB',geometry,rgb).save(a.output);print(json.dumps(dict(status='PASS',image=str(a.output),geometry=geometry)))
finally:s.close()
