"""Unchanged Passport OS: real ADC debounce, C++ drawer, timer and game UI.
Keep raw output private: startup/network diagnostics may contain Wi-Fi identity.
"""
import argparse,json,re,time
from pathlib import Path
from community_device import Device
from device_protocol import match_record

p=argparse.ArgumentParser();p.add_argument('--port',required=True);p.add_argument('--output',type=Path,required=True)
p.add_argument('--mode',required=True,choices=['native-safe-layout','plugin-only','hybrid-plugin'])
p.add_argument('--drawer-ready',action='store_true');a=p.parse_args()
root=Path(__file__).resolve().parents[1]
assert not a.output.resolve().is_relative_to(root),'Use the private diagnostics sibling for raw network logs'
d=Device(a.port,a.output);r={'mode':a.mode,'mainflow_pass':False,'automated_flow_pass':False,'visual_review':''}

def mark(step):
    (a.output/"progress.json").write_text(json.dumps({"step":step,"time":time.time()},indent=2)+"\n")

def capture(name):
    mark("capture "+name);image=d.capture(name);mark("captured "+name);return image

def wait(pattern,start=0,seconds=15):
    end=time.monotonic()+seconds
    while True:
        match=match_record(pattern,d.lines[start:])
        if match:return match
        if time.monotonic()>end:raise TimeoutError(pattern+' / '+str(d.lines[-8:]))
        line=d.s.readline().decode('utf8','replace').strip()
        if line:
            d.lines.append(line)
            if any(x in line for x in ['Guru Meditation','assert failed','Task watchdog got triggered','Stack protection fault','LOAD_FAIL']):
                # Preserve the task, PC/SP and stack dump, not only panic's
                # first line. Immediate port closure used to lose the evidence.
                tail=bytearray();finish=time.monotonic()+1.2
                while time.monotonic()<finish:
                    tail.extend(d.s.read(max(1,min(65536,d.s.in_waiting))))
                d.lines.extend(tail.decode('utf8','replace').splitlines())
                raise AssertionError(line)

def pulse(key,ms=140):
    mark(f"ADC key={key} ms={ms}");start=len(d.lines);d.send(f'ZION ADC {key} {ms}')
    wait(r'ZION_ADC_DONE key='+str(key)+r' ms='+str(ms)+r' result=0',start)
    time.sleep(ms/1000+.35)
    return start

def memory():
    start=len(d.lines);d.send('ZION MEM');return wait(r'ZION_MEMORY free=\d+(?: allocated=\d+)? largest=\d+',start)

try:
    r['memory_before']=memory()
    if not a.drawer_ready:
        for _ in range(3):pulse(2,1050)
        pulse(2)
    drawer=capture('01-drawer')
    pulse(0);last=capture('02-drawer-settings');r['up_pixels']=d.changed(drawer,last,(0,40,240,275))
    pulse(1)
    for _ in range(7):pulse(1)
    capture('03-stopwatch-selected')
    start=pulse(2);r['stopwatch_entry']=wait(r'Target ready: app=9\b',start)
    zero=capture('04-stopwatch-zero')
    start=pulse(2);r['timer_start']=wait(r'stopwatch: running=1 .*laps=0',start)
    time.sleep(1.1);start=pulse(0);r['lap']=wait(r'stopwatch: running=1 .*laps=1',start)
    lap=capture('05-stopwatch-lap');r['timer_pixels']=d.changed(zero,lap,(0,30,240,290))
    start=pulse(2);r['timer_pause']=wait(r'stopwatch: running=0 .*laps=1',start)
    paused=capture('06-stopwatch-paused');time.sleep(.7);paused2=capture('07-stopwatch-still')
    assert paused.crop((8,65,232,135)).tobytes()==paused2.crop((8,65,232,135)).tobytes()
    start=pulse(1);r['timer_reset']=wait(r'stopwatch: running=0 elapsed_us=0 laps=0',start)
    capture('08-stopwatch-reset')
    start=pulse(2,1050);r['back']=wait(r'Target ready: app=0\b',start)
    pulse(0);start=pulse(2);r['tetris_entry']=wait(r'Target ready: app=8\b',start)
    board=capture('09-tetris')
    pulse(2)  # leave the original waiting screen before testing in-game keys
    for key in [0,1,2]:
        start=pulse(key);wait(r'Dispatch Key: btn='+str(key)+r', evt=0, .*cur_active=8',start)
    played=capture('10-tetris-input');r['game_pixels']=d.changed(board,played,(0,35,240,295))
    start=pulse(2,1050);wait(r'Target ready: app=0\b',start)
    for _ in range(3):pulse(1)
    start=pulse(2);r['settings_entry']=wait(r'Target ready: app=5\b',start)
    settings=capture('11-settings');pulse(1);settings2=capture('12-settings-next')
    r['settings_pixels']=d.changed(settings,settings2,(0,35,240,295))
    pulse(0);pulse(2)  # exercise the original selected setting action
    capture('13-settings-action');pulse(2,1050)
    r['memory_after']=memory();r['automated_flow_pass']=True
    r['unverified']=['visual review pending','external Muse pairing/chat and AI accounts','radio/voice microphone sessions','complete game rules','physical power-switch boot']
finally:
    d.close(r)
print(json.dumps(r,indent=2))
