"""Exercise the unchanged reader through its real HTTP form and button path."""
import argparse,json,re,subprocess,sys,time,urllib.parse,urllib.request,urllib.error
from pathlib import Path
import serial
from PIL import Image,ImageChops
p=argparse.ArgumentParser();p.add_argument('--port',required=True)
p.add_argument('--wifi',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
p.add_argument('--device-ip',required=True)
p.add_argument('--skip-fetch',action='store_true');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
wifi=json.loads(a.wifi.read_text());base='http://'+a.device_ip;lines=[];results={}
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.1;s.dtr=False;s.rts=False
pending=b''
def open_port():s.open()
def send(cmd):s.write((cmd+'\n').encode());s.flush()
def wait(pattern,seconds=20):
    global pending
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        pending+=s.read(max(1,min(65536,s.in_waiting)))
        while b'\n' in pending:
            raw,pending=pending.split(b'\n',1);line=raw.decode('utf8','replace').strip()
            if not line:continue
            lines.append(line)
            assert not any(x in line for x in ['Guru Meditation','assert failed','Stack smashing']),line
            if re.search(pattern,line):return line
    raise TimeoutError(pattern+' / '+str(lines[-8:]))
def key(name):
    send('ZION KEY '+name);wait('STANDALONE_KEY|ZION_KEY_DONE');time.sleep(.85)
def capture(name):
    global pending
    s.close();pending=b''
    try:
        for attempt in range(3):
            run=subprocess.run([sys.executable,str(Path(__file__).with_name('capture-native-screen.py')),
                '--port',a.port,'--output',str(a.output/(name+'.png'))],capture_output=True,text=True)
            if run.returncode:
                (a.output/(name+'-attempt-'+str(attempt+1)+'.log')).write_text(run.stdout+run.stderr,encoding='utf8')
            else:print(run.stdout,end='',flush=True)
            if not run.returncode:break
        else:raise RuntimeError('No complete LCD frame: '+name)
    finally:open_port()
def difference(one,two):
    first=Image.open(a.output/(one+'.png')).crop((12,42,228,270))
    second=Image.open(a.output/(two+'.png')).crop((12,42,228,270))
    raw=ImageChops.difference(first,second).convert('RGB').tobytes()
    return sum(any(raw[i:i+3]) for i in range(0,len(raw),3))
open_port()
try:
    send('ZION MEM');results['memory_before']=wait('ZION_MEMORY')
    capture('initial')
    # GET may briefly succeed on the APSTA server just before GOT_IP replaces
    # it with the LAN server. Only retry reads; never blindly repeat POST saves.
    deadline=time.monotonic()+20;stable=0
    while time.monotonic()<deadline:
        try:
            with urllib.request.urlopen(base+'/',timeout=3) as response:
                page=response.read().decode('utf8');assert response.status==200
            stable+=1
            if stable==2:break
        except (urllib.error.URLError,TimeoutError,OSError):stable=0
        time.sleep(.7)
    assert stable==2,'Device configuration server not stable'
    assert all('name='+name in page or 'name="'+name+'"' in page for name in ['ssid','ssid2','ssid3','apikey'])
    results['configuration_get']='PASS: three Wi-Fi slots and API-key form'
    if not a.skip_fetch:
        values=dict(ssid=wifi['ssid'],cfg='1',
                    apikey='zion-fixture-no-real-key',pref='Local validation article')
        # The original form uses "pass", not an adapter-specific field.
        values['pass']=wifi['password']
        data=urllib.parse.urlencode(values).encode()
        try:
            with urllib.request.urlopen(base+'/save',data=data,timeout=15) as response:
                response.read();assert response.status==200
        except urllib.error.HTTPError as error:
            detail=error.read(512).decode('utf8','replace')
            for field in ['ssid','password']:
                if wifi.get(field):detail=detail.replace(wifi[field],'[REDACTED]')
            results['configuration_post_error']=dict(status=error.code,detail=detail)
            raise
        results['configuration_post']='PASS: original HTTP save endpoint'
        results['fetch']=wait(r'article updated',100)
        assert any('perform err=ESP_OK status=200' in x for x in lines),'No successful real HTTP result'
        assert any('HTTPS_LOCAL certificate_validation=enabled' in x for x in lines),'TLS fixture not engaged'
        assert not any('NVS article save failed' in x for x in lines),'Article persistence failed'
        time.sleep(1)
    capture('page-one');key('DOWN');capture('page-two')
    results['page_changed_pixels']=difference('page-one','page-two');assert results['page_changed_pixels']>100
    key('UP');capture('page-one-return');results['page_return_changed_pixels']=difference('page-one','page-one-return')
    assert results['page_return_changed_pixels']<100
    key('UPLONG');capture('brighter');key('DOWNLONG');capture('dimmer')
    key('LONG');capture('long-ok-modal');key('OK');capture('modal-dismissed')
    send('ZION MEM');results['memory_after']=wait('ZION_MEMORY')
    results['status']='PASS: HTTP configuration, TLS/JSON/article persistence, paging and short/long keys' if not a.skip_fetch else 'PASS: persisted article UI and keys'
finally:
    s.close();raw='\n'.join(lines)
    for field in ['ssid','password']:
        if wifi.get(field):raw=raw.replace(wifi[field],'[REDACTED]')
    raw=re.sub(r'(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}','[MAC REDACTED]',raw)
    (a.output/'mainflow.log').write_text(raw+'\n',encoding='utf8')
    (a.output/'result.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2))
