"""Local MQTT + real UDP CW peer. Hardware identifiers are never persisted."""
import argparse,json,socket,struct,threading,time
from pathlib import Path
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True)
lock=threading.Lock();events=[];peer=None;freq=7100000;sequence=1
udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);udp.bind(('0.0.0.0',21303))
def record(**entry):
    with lock:
        events.append(dict(time=time.time(),**entry))
        (a.output/'events.json').write_text(json.dumps(events,indent=2)+'\n')
def frame(kind):return bytearray(b'CW\x01'+bytes([kind])+bytes(12))
def occupancy():
    return b'CW\x01\x03'+struct.pack('>I',int(time.time())&0xffffffff)+bytes([2,1])+struct.pack('<HB',(freq-7000000)//200,1)
def udp_worker():
    global peer,freq
    while True:
        data,address=udp.recvfrom(512)
        if len(data)!=16 or data[:3]!=b'CW\x01':record(event='UDP_REJECT',bytes=len(data));continue
        kind=data[3]
        if kind==5:
            peer=address;freq=struct.unpack_from('>I',data,12)[0]
            auth=frame(6);auth[4:12]=b'V00007  ';struct.pack_into('>H',auth,12,7);auth[14]=1
            udp.sendto(auth,address)
            ack=frame(2);struct.pack_into('>H',ack,4,7);struct.pack_into('>II',ack,8,7000000,7200000)
            udp.sendto(ack,address);udp.sendto(occupancy(),address)
            record(event='AUTH_ACK',freq=freq) # deliberately omit GUID and address
        elif kind==1:
            uid,seq,frequency=struct.unpack_from('>HHI',data,4)
            record(event='KEY',uid=uid,seq=seq,freq=frequency,on=data[12],flags=data[13],dev_ms=struct.unpack_from('>H',data,14)[0])
        elif kind==4:
            record(event='HEARTBEAT',freq=struct.unpack_from('>I',data,8)[0])
        elif kind==2:record(event='HELLO',freq=struct.unpack_from('>I',data,12)[0])
def mqtt_packet(kind,body=b''):
    size=len(body);out=bytearray([kind])
    while True:
        n=size%128;size//=128;out.append(n|(128 if size else 0))
        if not size:break
    return bytes(out)+body
def exact(s,size):
    out=b''
    while len(out)<size:
        more=s.recv(size-len(out))
        if not more:raise EOFError()
        out+=more
    return out
def mqtt_read(s):
    kind=exact(s,1)[0];size=0;scale=1
    for _ in range(4):
        value=exact(s,1)[0];size+=(value&127)*scale
        if not value&128:return kind,exact(s,size)
        scale*=128
    raise ValueError('MQTT length')
def mqtt_publish(s,topic,data):
    topic=topic.encode();s.sendall(mqtt_packet(0x30,struct.pack('>H',len(topic))+topic+data))
def mqtt_client(s):
    try:
        s.settimeout(90)
        while True:
            kind,body=mqtt_read(s);command=kind>>4
            if command==1:s.sendall(mqtt_packet(0x20,b'\0\0'));record(event='MQTT_CONNECT')
            elif command==8:
                length=struct.unpack_from('>H',body,2)[0];topic=body[4:4+length].decode()
                s.sendall(mqtt_packet(0x90,body[:2]+b'\0'))
                record(event='MQTT_SUBSCRIBE',topic=topic)
                if topic=='cw/v1/roster':mqtt_publish(s,topic,f'V00007,{freq},7;V00008,{freq},8'.encode())
                elif topic=='cw/v1/occupy':mqtt_publish(s,topic,occupancy())
                elif topic=='cw/v1/time':mqtt_publish(s,topic,str(int(time.time()*1000)).encode())
                elif topic=='cw/v1/chat':mqtt_publish(s,topic,b'V00008:LOCAL PEER READY')
            elif command==3:
                length=struct.unpack_from('>H',body)[0];topic=body[2:2+length].decode();at=2+length
                if (kind>>1)&3:s.sendall(mqtt_packet(0x40,body[at:at+2]));at+=2
                record(event='PRESENCE',online=body[at:]!=b'0',bytes=len(body)-at)
            elif command==12:s.sendall(mqtt_packet(0xd0))
            elif command==14:break
    except (OSError,EOFError,ValueError):record(event='MQTT_END')
    finally:s.close()
def mqtt_worker():
    listener=socket.socket();listener.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
    listener.bind(('0.0.0.0',1883));listener.listen()
    while True:
        s,_=listener.accept();threading.Thread(target=mqtt_client,args=(s,),daemon=True).start()
def receive_dot():
    global sequence
    if not peer:return False
    for on in [1,0]:
        packet=frame(1);struct.pack_into('>HHI',packet,4,8,sequence,int(time.monotonic()*1000)&0xffffffff)
        struct.pack_into('>H',packet,12,700);packet[14]=9;packet[15]=on
        packet+=bytes([18,0]);udp.sendto(packet,peer);sequence=(sequence+1)&65535
        if on:time.sleep(.09)
    record(event='RX_DOT_SENT',sender_uid=8);return True
class Control(BaseHTTPRequestHandler):
    def log_message(self,*args):pass
    def do_GET(self):
        if self.path=='/status':
            with lock:body=json.dumps(dict(events=events,authenticated=peer is not None)).encode()
            self.send_response(200);self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
        else:self.send_error(404)
    def do_POST(self):
        if self.path!='/rx-dot':self.send_error(404);return
        ok=receive_dot();body=json.dumps(dict(sent=ok)).encode();self.send_response(200 if ok else 409)
        self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
threading.Thread(target=udp_worker,daemon=True).start();threading.Thread(target=mqtt_worker,daemon=True).start()
print('CW_FIXTURE_READY UDP=21303 MQTT=1883 HTTP=21301',flush=True)
ThreadingHTTPServer(('0.0.0.0',21301),Control).serve_forever()
