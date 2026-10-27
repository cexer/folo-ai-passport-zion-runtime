"""Bounded device controls and complete LCD capture for community main-flow cases."""
import json,re,subprocess,sys,time
from pathlib import Path
import serial
from device_protocol import match_record
from PIL import Image,ImageChops

class Device:
    def __init__(self,port,output):
        self.output=Path(output);self.output.mkdir(parents=True,exist_ok=True);self.lines=[]
        self.s=serial.Serial();self.s.port=port;self.s.baudrate=115200;self.s.timeout=.1
        self.s.dtr=False;self.s.rts=False;self.s.open()
    def send(self,command):self.s.write((command+'\n').encode());self.s.flush()
    def wait(self,pattern,seconds=15):
        end=time.monotonic()+seconds
        while time.monotonic()<end:
            line=self.s.readline().decode('utf8','replace').strip()
            if not line:continue
            self.lines.append(line)
            assert not any(s in line for s in ['Guru Meditation','assert failed','ALLOC_FAILED','LOAD_FAIL']),line
            matched=match_record(pattern,self.lines)
            if matched:return matched
        raise TimeoutError(pattern+' / '+str(self.lines[-8:]))
    def key(self,name,delay=.35):
        self.send('ZION KEY '+name);self.wait('STANDALONE_KEY|ZION_KEY_DONE');time.sleep(delay)
    def proof(self,pattern,start=0,seconds=15):
        return next((line for line in self.lines[start:] if re.search(pattern,line)),None) or self.wait(pattern,seconds)
    def memory(self):
        self.send('ZION MEM')
        return self.wait(r'ZION_MEMORY free=\d+(?: allocated=\d+)? largest=\d+')
    def capture(self,name):
        self.s.close()
        try:
            for attempt in range(3):
                r=subprocess.run([sys.executable,str(Path(__file__).with_name('capture-native-screen.py')),
                    '--port',self.s.port,'--output',str(self.output/(name+'.png'))],
                    capture_output=True,text=True,encoding='utf8',timeout=40)
                if r.returncode==0:return Image.open(self.output/(name+'.png')).convert('RGB')
                (self.output/(name+f'-attempt-{attempt}.log')).write_text(r.stdout+r.stderr)
            raise RuntimeError('Incomplete actual LCD capture: '+name)
        finally:self.s.open();self.s.reset_input_buffer()
    @staticmethod
    def changed(one,two,roi=None):
        if roi:one,two=one.crop(roi),two.crop(roi)
        data=ImageChops.difference(one,two).tobytes()
        count=sum(any(data[i:i+3]) for i in range(0,len(data),3));assert count>50,count;return count
    def close(self,result):
        result['environment']='physical device'
        self.s.close()
        raw=re.sub(r'(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}','[MAC REDACTED]','\n'.join(self.lines))
        raw=re.sub(r'C4-[0-9A-Fa-f]{4}', 'C4-[REDACTED]', raw)
        (self.output/'mainflow.log').write_text(raw+'\n',encoding='utf8')
        (self.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
