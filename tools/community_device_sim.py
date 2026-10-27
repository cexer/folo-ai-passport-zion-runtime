"""PassportSim adapter for the same bounded application scenarios.

Simulated BLE receipts remain simulator evidence, never physical-radio evidence.
"""
import json,re,shutil,subprocess,time
from pathlib import Path
from PIL import Image
from community_device import Device

class SimDevice(Device):
    def __init__(self,sim,instance,output):
        self.sim=Path(sim).resolve();self.instance=instance
        self.output=Path(output);self.output.mkdir(parents=True,exist_ok=True)
        self.lines=[];self.cursor=0;self.receipts=[]
        r=self.call('status',instance,selected=False)
        self.artifacts=Path(r['artifacts_root']).expanduser()
    def call(self,*args,selected=True):
        cmd=[str(self.sim),'--output','json',*map(str,args)]
        if selected:cmd+=['--instance',self.instance]
        r=subprocess.run(cmd,capture_output=True,text=True,encoding='utf8',timeout=55)
        d=json.loads(r.stdout);receipt=d.get('receipt',{})
        self.receipts.append({'command':list(map(str,args)),'exit':r.returncode,'receipt':receipt})
        assert r.returncode in (0,10),d
        assert not receipt.get('hle',{}).get('tripwires_hit'),receipt
        assert d.get('result') not in ('fail','error'),d
        return d
    def send(self,command):self.call('serial','write','--hex',(command+'\n').encode().hex())
    def read(self):
        d=self.call('serial','read','--cursor',self.cursor,'--max-bytes',4096)
        self.cursor=d['next_cursor'];new=d['serial'].get('head',[])+d['serial'].get('tail',[])
        for line in new:
            assert not any(s in line for s in ['Guru Meditation','assert failed','ALLOC_FAILED','LOAD_FAIL']),line
        self.lines+=new;return new
    def wait(self,pattern,seconds=15):
        for _ in range(int(seconds*10)):
            new=self.read()
            found=next((line for line in new if re.search(pattern,line)),None)
            if found:return found
            self.call('run','--for','100ms')
        raise TimeoutError(pattern+' / '+str(self.lines[-8:]))
    def key(self,name,delay=.35):
        self.send('ZION KEY '+name);self.call('run','--for',str(int(delay*1000))+'ms')
        self.wait('STANDALONE_KEY|ZION_KEY_DONE')
    def capture(self,name):
        self.call('run','--for','200ms')
        d=self.call('screenshot','glass','--save-as',name)
        files=list(self.artifacts.glob('*/'+self.instance+'/'+d['path']))
        assert len(files)==1,files
        target=self.output/(name+'.png');shutil.copyfile(files[0],target)
        return Image.open(target).convert('RGB')
    def close(self,result):
        result['environment']='PassportSim; not physical device acceptance'
        (self.output/'sim-receipts.json').write_text(json.dumps(self.receipts,indent=2)+'\n')
        raw=re.sub(r'(?i)(?:[0-9a-f]{2}:){5}[0-9a-f]{2}','[MAC REDACTED]','\n'.join(self.lines))
        (self.output/'mainflow.log').write_text(raw+'\n',encoding='utf8')
        (self.output/'result.json').write_text(json.dumps(result,indent=2)+'\n')
