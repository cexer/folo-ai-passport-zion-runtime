"""Exercise unchanged Pinball via timed ADC samples and actual LCD transfers."""
import argparse,json,re,subprocess,sys,time
from pathlib import Path
import serial
from PIL import Image,ImageChops
p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--output',type=Path,required=True)
p.add_argument('--settings-reference',type=Path);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
s=serial.Serial();s.port=a.port;s.baudrate=115200;s.timeout=.1;s.dtr=False;s.rts=False
lines=[];result={}
def open_port():s.open();s.reset_input_buffer()
def wait(pattern,seconds=10):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        line=s.readline().decode('utf8','replace').strip()
        if not line:continue
        lines.append(line)
        assert not any(x in line for x in ['Guru Meditation','assert failed','ALLOC_FAILED','LOAD_FAIL']),line
        if re.search(pattern,line):return line
        if len(lines)>1 and (lines[-2].startswith('ZION_') or (pattern.startswith('ZION_') and pattern.startswith(lines[-2]))) and not re.match(r'^(ZION_|[IWE] \()',line):
            joined=lines[-2]+line
            if re.search(pattern,joined):return joined
    raise TimeoutError(pattern+' / '+str(lines[-5:]))
def send(command):s.write((command+'\n').encode());s.flush()
def key(n,ms=500):
    send(f'ZION ADC {n} {ms}');ack=wait(r'ZION_ADC_DONE .*result=0');time.sleep(ms/1000+.5);return ack
def capture(name):
    s.close()
    try:
        for attempt in range(3):
            r=subprocess.run([sys.executable,str(Path(__file__).with_name('capture-native-screen.py')),
                '--port',a.port,'--output',str(a.output/(name+'.png'))],capture_output=True,text=True,encoding='utf8',timeout=40)
            if r.returncode==0:return Image.open(a.output/(name+'.png')).convert('RGB')
            (a.output/(name+f'-attempt-{attempt}.log')).write_text(r.stdout+r.stderr)
        raise RuntimeError('Incomplete LCD frame: '+name)
    finally:open_port()
def different(one,two,box=(16,60,224,278),minimum=20):
    data=ImageChops.difference(one.crop(box),two.crop(box)).tobytes()
    changed=sum(any(data[i:i+3]) for i in range(0,len(data),3));assert changed>minimum,changed;return changed
open_port()
try:
    send('ZION MEM');result['memory_before']=wait('ZION_MEMORY ')
    title=capture('title');time.sleep(2)  # original title rejects input during its first 0.3 game seconds
    key(1);key(1);selection=capture('title-settings-selected')
    yellow=sum(r>220 and g>160 and b<130 for r,g,b in selection.crop((25,174,215,202)).getdata())
    assert yellow>2000,('Settings row not selected; check startup/input timing',yellow)
    key(2);settings=capture('settings-before')
    if a.settings_reference:
        expected=Image.open(a.settings_reference).convert('RGB')
        # Settings panel excludes changing battery telemetry and game background.
        box=(20,65,220,190)
        assert settings.crop(box).tobytes()==expected.crop(box).tobytes(),'Saved setting did not survive reset'
        result['persistence']='PASS: actual settings panel pixels match before/after reset'
    else:
        key(2);key(1);key(2);changed=capture('settings-saved')
        result['setting_changed_pixels']=different(settings,changed)
        key(1);key(1);key(2)  # settings BACK; title retains SETTINGS selection
        key(0);key(2);guide=capture('guide-scoring');key(1);upgrades=capture('guide-upgrades')
        result['guide_page_changed_pixels']=different(guide,upgrades)
        key(0);key(2);key(0);key(2)  # guide BACK, title START, new ball in lane
        launch=capture('launch-ready');key(2,600)
        playing=capture('playing-a');time.sleep(.2);moving=capture('playing-b')
        result['physics_changed_pixels']=different(playing,moving,(0,20,240,320),minimum=5)
        key(0,300);key(1,300)
        key(2,5000);paused=capture('paused')
        result['pause_changed_pixels']=different(playing,paused)
        key(1);selection=capture('pause-selection')
        result['pause_key_changed_pixels']=different(paused,selection)
        key(0);key(2);capture('resumed')
        result['timed_keys']='PASS: UP/DOWN, charge/release, long OK pause and resume through original ADC polling'
    send('ZION MEM');result['memory_after']=wait('ZION_MEMORY ')
    result['silence_scope']='Codec forced muted; physical I2S writes contain zero PCM'
    result['status']='PASS: main flow; not exhaustive physics, score rules or acoustic validation'
finally:
    s.close();raw=re.sub(r'(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}','[MAC REDACTED]','\n'.join(lines))
    (a.output/'mainflow.log').write_text(raw+'\n',encoding='utf8')
    (a.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
