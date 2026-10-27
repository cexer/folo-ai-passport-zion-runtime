"""Small local TLS MQTT 3.1.1 peer for device integration, not a Bambu emulator."""
import argparse,json,socket,ssl,struct,threading,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--certificate',type=Path,required=True);p.add_argument('--key',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--notify',action='store_true',help='Send RUNNING then FINISH to exercise muted notification playback')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.load_cert_chain(a.certificate,a.key)
def packet(kind,body=b''):
    n=len(body);size=bytearray()
    while True:
        b=n%128;n//=128;size.append(b|(128 if n else 0))
        if not n:break
    return bytes([kind])+size+body
def exact(s,n):
    out=b''
    while len(out)<n:
        part=s.recv(n-len(out))
        if not part:raise EOFError()
        out+=part
    return out
def read(s):
    kind=exact(s,1)[0];n=0;scale=1
    for _ in range(4):
        b=exact(s,1)[0];n+=(b&127)*scale
        if not b&128:return kind,exact(s,n)
        scale*=128
    raise ValueError('remaining-length')
def serve(raw):
    events=[];s=None
    try:
        s=context.wrap_socket(raw,server_side=True);s.settimeout(40);events.append(dict(event='TLS',version=s.version()))
        topic=b'device/ZIONTEST001/report'
        payload=json.dumps(dict(print=dict(gcode_state='RUNNING',mc_percent=42,mc_remaining_time=17,
            layer_num=84,total_layer_num=200,nozzle_temper=215,bed_temper=60,
            subtask_name='ZION local TLS test',ams=dict(ams=[]))),separators=(',',':')).encode()
        for _ in range(200):
            kind,body=read(s);command=kind>>4
            if command==1:s.sendall(packet(0x20,b'\0\0'));events.append(dict(event='CONNECT'))
            elif command==8:
                length=int.from_bytes(body[2:4],'big');topic=body[4:4+length]
                s.sendall(packet(0x90,body[:2]+b'\0'))
                s.sendall(packet(0x30,struct.pack('>H',len(topic))+topic+payload))
                events.append(dict(event='SUBSCRIBE_AND_PUBLISH',topic=topic.decode(),percent=42))
                if a.notify:
                    time.sleep(2)
                    finished=json.loads(payload);finished['print']['gcode_state']='FINISH';finished['print']['mc_percent']=100
                    encoded=json.dumps(finished,separators=(',',':')).encode()
                    s.sendall(packet(0x30,struct.pack('>H',len(topic))+topic+encoded))
                    payload=encoded # later report requests must retain FINISH
                    events.append(dict(event='FINISH_NOTIFICATION',percent=100))
            elif command==3:
                length=int.from_bytes(body[:2],'big');qos=(kind>>1)&3
                if qos:s.sendall(packet(0x40,body[2+length:4+length]))
                events.append(dict(event='CLIENT_PUBLISH',bytes=len(body)))
                s.sendall(packet(0x30,struct.pack('>H',len(topic))+topic+payload))
            elif command==12:s.sendall(packet(0xd0))
            elif command==14:break
            (a.output/'mqtt-events.json').write_text(json.dumps(events,indent=2)+'\n')
    except (OSError,EOFError,ValueError) as e:events.append(dict(event='END',error=type(e).__name__))
    finally:
        (a.output/f'mqtt-{time.time_ns()}.json').write_text(json.dumps(events,indent=2)+'\n')
        if s:s.close()
        else:raw.close()
listener=socket.socket();listener.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1);listener.bind(('0.0.0.0',18884));listener.listen()
print('Local TLS MQTT fixture ready :18884',flush=True)
while True:
    raw,_=listener.accept();threading.Thread(target=serve,args=(raw,),daemon=True).start()
