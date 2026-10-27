"""Deterministic external radio directory and paced, locally generated MP3.

Generate the MP3 with ffmpeg's sine source; do not archive someone else's stream.
The ESP32 still executes the unchanged HTTP/catalog/decoder/jitter/I2S code.
"""
import argparse,json,time
from pathlib import Path
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
p=argparse.ArgumentParser();p.add_argument('--mp3',type=Path,required=True)
p.add_argument('--output',type=Path,required=True);p.add_argument('--port',type=int,default=18081);a=p.parse_args()
audio=a.mp3.read_bytes();a.output.parent.mkdir(parents=True,exist_ok=True)
def record(**data):
    with a.output.open('a',encoding='utf8') as f:f.write(json.dumps(dict(time=time.time(),**data))+'\n')
class Handler(BaseHTTPRequestHandler):
    protocol_version='HTTP/1.1'
    def log_message(self,*args):pass
    def do_GET(self):
        path=self.path.split('?',1)[0]
        if path=='/stream.mp3':
            self.send_response(200);self.send_header('Content-Type','audio/mpeg')
            self.send_header('Connection','close');self.end_headers();sent=0
            try:
                for _ in range(100):
                    for at in range(0,len(audio),512):
                        block=audio[at:at+512];self.wfile.write(block);self.wfile.flush();sent+=len(block)
                        time.sleep(len(block)/8000)
            except (ConnectionError,OSError):pass
            finally:record(path=path,sent_bytes=sent);self.close_connection=True
            return
        if path=='/geo':
            data=dict(status='success',success=True,countryCode='CN',country_code='CN',regionName='Shanghai',region='Shanghai',city='Shanghai')
        elif path=='/stations':
            data=[dict(name=name,url_resolved=f'http://192.168.5.100:{a.port}/stream.mp3?station={i}',
                codec='MP3',bitrate=64,lastcheckok=1,hls=0,tags=f'FM {90+i}.1',countrycode='CN')
                for i,name in enumerate(['\u4e2d\u56fd\u4e4b\u58f0','\u6d4b\u8bd5\u7535\u53f0'])]
        elif path=='/metadata':data={}
        else:self.send_error(404);return
        body=json.dumps(data,ensure_ascii=False).encode();self.send_response(200)
        self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(body)))
        self.end_headers();self.wfile.write(body);record(path=path,bytes=len(body))
print('RADIO_HTTP_READY',a.port,flush=True)
ThreadingHTTPServer(('0.0.0.0',a.port),Handler).serve_forever()
