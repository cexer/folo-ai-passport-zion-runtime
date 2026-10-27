"""Local plain-TCP peer for the original iPhone Agent board protocol.

No phone/AI model is simulated. Ping checks the real network command path;
input commands are not sent to a paired keyboard/mouse host.
"""
import argparse,json,socket,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--port',type=int,default=17300)
p.add_argument('--output',type=Path,required=True);a=p.parse_args()
a.output.parent.mkdir(parents=True,exist_ok=True)
def record(**data):
    with a.output.open('a',encoding='utf8') as f:f.write(json.dumps(dict(time=time.time(),**data))+'\n')
with socket.socket() as server:
    server.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
    server.bind(('0.0.0.0',a.port));server.listen(1);server.settimeout(1)
    print('AGENT_TCP_READY',a.port,flush=True)
    while True:
        try:conn,_=server.accept()
        except socket.timeout:continue
        with conn:
            conn.settimeout(10);file=conn.makefile('rb')
            try:
                hello=file.readline(256).decode().strip();assert hello.startswith('hello '),hello
                record(event='hello',reply=hello)
                while True:
                    conn.sendall(b'ping\n');reply=file.readline(256).decode().strip()
                    assert reply=='ok',reply
                    record(event='ping',reply=reply);time.sleep(1)
            except (OSError,AssertionError) as e:record(event='disconnect',reason=str(e))
