"""Send one corrupted upgrade and prove the catalog's allocated space is unchanged."""
import argparse,serial,time,re,json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--port',required=True);a=p.parse_args()
package=bytearray((root/'release/test-apps/ZION-SoundLab-0.3.0.zion').read_bytes());package[-1]^=1
lines=[];out=root/'diagnostics/package-rejection';out.mkdir(exist_ok=True)
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.1;s.write_timeout=15;s.dtr=False;s.rts=False;s.open();s.reset_input_buffer()
def until(pattern):
    end=time.monotonic()+20
    while time.monotonic()<end:
        line=s.readline().decode('utf8',errors='replace').strip()
        if not line:continue
        lines.append(line)
        if 'Guru Meditation' in line or 'assert failed' in line:raise RuntimeError(line)
        if re.search(pattern,line):return line
    raise TimeoutError(pattern+' / '+str(lines[-8:]))
def command(text):s.write((text+'\n').encode());s.flush()
def state():
    command('ZION STATE');line=until('ZION_UI page=')
    return {k:int(v) for k,v in re.findall(r'(apps|free|busy)=(-?\d+)',line)}
try:
    before=state();assert not before['busy']
    command('ZION INSTALL '+str(len(package)));until('ZION_UPLOAD_READY')
    s.write(package[:256]);s.flush();until(r'ZION_UPLOAD_ACK bytes=256\b')
    for offset in range(256,len(package),1024):
        chunk=package[offset:offset+1024];s.write(chunk);s.flush();until(r'ZION_UPLOAD_ACK bytes='+str(offset+len(chunk))+r'\b')
    result=until('ZION_UPLOAD_DONE');assert 'result=-1 stage=payload-sha256' in result,result
    after=state();assert before==after,(before,after)
    report=dict(status='PASS',port=a.port,failure=result,before=before,after=after,
        scope='Invalid payload SHA rejected after full upload; unchanged catalog count/free Flash, old application boot tested separately')
    (out/'result.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
finally:
    s.close();safe=re.sub(r'(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}','[MAC REDACTED]','\n'.join(lines))
    (out/'serial.log').write_text(safe+'\n',encoding='utf8')
