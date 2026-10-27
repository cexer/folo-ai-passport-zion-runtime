"""Require real decode, buffering and silenced I2S, not just app_main readiness."""
import argparse, json, re, time
from pathlib import Path
import serial
p=argparse.ArgumentParser()
p.add_argument('--port',required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--startup-log',type=Path,help='Earlier matched-device startup proof; errors are checked in the new observation window')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.1;s.dtr=False;s.rts=False
lines=[];pending=b'';s.open();end=time.monotonic()+25
try:
    while time.monotonic()<end:
        pending+=s.read(max(1,min(8192,s.in_waiting)))
        while b'\n' in pending:
            line,pending=pending.split(b'\n',1)
            lines.append(line.decode('utf8','replace').strip())
    s.write(b'ZION MEM\n');s.flush();time.sleep(.4)
    lines.extend(s.read(s.in_waiting).decode('utf8','replace').splitlines())
finally:s.close()
raw='\n'.join(lines)
raw=re.sub(r'(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}','[MAC REDACTED]',raw)
raw=re.sub(r'(wifi:connected with ).*?(, aid =)',r'\1[REDACTED]\2',raw)
(a.output/'stream.log').write_text(raw+'\n',encoding='utf8')
startup=a.startup_log.read_text(encoding='utf8') if a.startup_log else ''
required=['MP3 stream ','Jitter buffer primed','CODEC_MUTED volume=0 mute=1','I2S_ZERO_OUTPUT']
missing=[x for x in required if x not in raw+startup]
progress=[int(x) for x in re.findall(r'I2S_ZERO_PROGRESS bytes=(\d+)',raw)]
if len(progress)<2 or progress[-1]<=progress[0]:missing.append('advancing real I2S output counter')
errors=[x for x in lines if any(k in x for k in ['Guru Meditation','assert failed','ALLOC_FAILED','MP3 decode failed'])]
result=dict(status='FAIL' if missing or errors else 'PASS',missing=missing,errors=errors,
            proof=[x for x in lines if any(k in x for k in required+['ZION_MEMORY','I2S_ZERO_PROGRESS'])],
            startup_log=str(a.startup_log) if a.startup_log else None,i2s_progress_bytes=progress,
            scope='Real MP3/jitter/I2S with zero PCM output; no acoustic or public-radio test')
(a.output/'stream.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2));assert result['status']=='PASS'
