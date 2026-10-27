"""Local HTTPS completion peer. Never calls a paid AI endpoint or logs keys."""
import argparse,json,ssl,time
from pathlib import Path
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
p=argparse.ArgumentParser();p.add_argument('--cert',type=Path,required=True)
p.add_argument('--key',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
p.add_argument('--port',type=int,default=18082);a=p.parse_args();a.output.parent.mkdir(parents=True,exist_ok=True)
article='##\u6d4b\u8bd5\u6587\u7ae0##\n\n'+('\u4eca\u5929\u6211\u4eec\u4e00\u8d77\u9605\u8bfb\u3002\u8fd9\u662f\u4e00\u7bc7\u672c\u5730\u6d4b\u8bd5\u6587\u7ae0\uff0c\u7528\u6765\u68c0\u67e5\u7f51\u7edc\u3001\u4fdd\u5b58\u548c\u7ffb\u9875\u3002\n'*30)
class Handler(BaseHTTPRequestHandler):
    protocol_version='HTTP/1.1'
    def log_message(self,*args):pass
    def do_POST(self):
        size=int(self.headers.get('Content-Length','0'))
        if self.path!='/chat/completions' or size<=0 or size>16384:
            self.send_error(400);return
        body=self.rfile.read(size)
        if self.headers.get('Authorization')!='Bearer zion-fixture-no-real-key':
            self.send_error(401);return
        request=json.loads(body)
        assert request.get('model')=='deepseek-chat' and request.get('messages')
        response=json.dumps(dict(choices=[dict(message=dict(role='assistant',content=article))]),ensure_ascii=False).encode()
        self.send_response(200);self.send_header('Content-Type','application/json; charset=utf-8')
        self.send_header('Content-Length',str(len(response)));self.end_headers()
        for offset in range(0,len(response),173):
            self.wfile.write(response[offset:offset+173]);self.wfile.flush();time.sleep(.01)
        record=dict(time=time.time(),request_bytes=size,response_bytes=len(response),
                    message_count=len(request['messages']),tls=self.connection.version(),cipher=self.connection.cipher()[0])
        with a.output.open('a',encoding='utf8') as f:f.write(json.dumps(record)+'\n')
server=ThreadingHTTPServer(('0.0.0.0',a.port),Handler)
context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.load_cert_chain(a.cert,a.key)
server.socket=context.wrap_socket(server.socket,server_side=True)
print('READER_HTTPS_READY',a.port,flush=True);server.serve_forever()
