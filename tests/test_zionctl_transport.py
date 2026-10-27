"""Run the actual USB client with split ACK lines and temporary empty reads."""
import contextlib,hashlib,io,json,runpy,struct,sys,tempfile,types,zlib
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools'))
class SerialFixture:
    instances=[]
    def __init__(self):
        self.rx=[];self.writes=[];self.received=0;self.closed=False;self.instances.append(self)
    def open(self):pass
    def reset_input_buffer(self):self.rx.clear()
    def flush(self):pass
    def close(self):self.closed=True
    @property
    def in_waiting(self):return len(self.rx[0]) if self.rx else 0
    def write(self,data):
        self.writes.append(bytes(data))
        if len(self.writes)==1:
            self.total=int(data.decode().split()[-1])
            self.rx.extend([b'old log\nZION_UP',b'',b'LOAD_READY bytes=',str(self.total).encode()+b'\r\n'])
        else:
            self.received+=len(data)
            self.rx.extend([b'ZION_UPLOAD_A',b'',b'CK bytes=',str(self.received).encode()+b'\r\n'])
            if self.received==self.total:self.rx.extend([b'I unrelated log\nZION_UPLOAD_DO',b'NE result=0 stage=installed\n'])
        return len(data)
    def read(self,size):
        assert self.rx,'Client read after exhausting fixture responses'
        part=self.rx.pop(0);result=part[:size]
        if len(part)>size:self.rx.insert(0,part[size:])
        return result
    def readline(self):raise AssertionError('Timeout-fragmented readline is not a protocol boundary')
with tempfile.TemporaryDirectory(prefix='zion-usb-fragments-') as directory:
    payload=bytes(range(256))*12;h=bytearray(256)
    struct.pack_into('<4sHH10I',h,0,b'ZPK1',1,256,1,256+len(payload),0x80000070,1,96,256,len(payload),256+len(payload),0,0)
    h[48:57]=b'Transport';h[88:98]=b'transport\0';h[104:136]=hashlib.sha256(payload).digest()
    struct.pack_into('<I',h,252,zlib.crc32(h[:252]));package=bytes(h)+payload
    file=Path(directory)/'input.zion';file.write_bytes(package);log=Path(directory)/'client.log'
    old_args=sys.argv;old_serial=sys.modules.get('serial');sys.modules['serial']=types.SimpleNamespace(Serial=SerialFixture)
    try:
        sys.argv=['zionctl',str(file),'--port','MOCK','--log',str(log)]
        captured=io.StringIO()
        with contextlib.redirect_stdout(captured):runpy.run_path(str(root/'tools/zionctl.py'),run_name='__main__')
        assert json.loads(captured.getvalue())['result']=='PASS'
        transport=SerialFixture.instances[-1]
        assert b''.join(transport.writes[1:])==package and transport.closed
        assert 'ZION_UPLOAD_DONE result=0' in log.read_text()
    finally:
        sys.argv=old_args
        if old_serial is None:sys.modules.pop('serial',None)
        else:sys.modules['serial']=old_serial
print('USB client: PASS (fragmented lines, empty reads, exact payload, complete ACK and live log)')
