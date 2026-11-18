"""Exercise the real authenticated HTTP bridge using a local fake model endpoint."""
import json
import sys
import threading
import unittest
import urllib.request
import urllib.error
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
import assistant_bridge as bridge
OUTPUT=Path(sys.argv.pop(1))
PROPOSAL={"records":[{"kind":"task","title":"Comprar café","detail":"Lista de compras","when":"","fields":{"priority":"2"}}]}

class Model(BaseHTTPRequestHandler):
    seen=[]
    def do_POST(self):
        body=json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        self.seen.append(body)
        data=json.dumps({"message":{"content":json.dumps(PROPOSAL)}}).encode()
        self.send_response(200);self.send_header("Content-Length",str(len(data)));self.end_headers();self.wfile.write(data)
    def log_message(self,*args):pass

class BridgeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.model=ThreadingHTTPServer(("127.0.0.1",0),Model)
        threading.Thread(target=cls.model.serve_forever,daemon=True).start()
        endpoint=f"http://127.0.0.1:{cls.model.server_port}/api/chat"
        generator=lambda text,now:bridge.generate(endpoint,"test","ollama","",text,now)
        cls.server=ThreadingHTTPServer(("127.0.0.1",0),bridge.handler("test-code",generator))
        threading.Thread(target=cls.server.serve_forever,daemon=True).start()
    @classmethod
    def tearDownClass(cls):
        for server in (cls.server,cls.model):server.shutdown();server.server_close()
    def request(self,token="test-code",data=b"Crie uma tarefa",now="2026-11-17 09:00"):
        request=urllib.request.Request(f"http://127.0.0.1:{self.server.server_port}/assistant",data=data,headers={"Authorization":"Bearer "+token,"X-PDA-Now":now})
        return urllib.request.urlopen(request,timeout=5)
    def test_success(self):
        with self.request() as response:data=response.read()
        OUTPUT.write_bytes(data)
        records=[json.loads(line) for line in data.splitlines()]
        self.assertEqual(records[2]["title"],"Comprar café")
        self.assertEqual(Model.seen[-1]["format"],"json")
        self.assertFalse(Model.seen[-1]["stream"])
    def test_auth(self):
        with self.assertRaises(urllib.error.HTTPError) as caught:self.request(token="wrong")
        self.assertEqual(caught.exception.code,403)
    def test_limits(self):
        with self.assertRaises(urllib.error.HTTPError) as caught:self.request(data=b"x"*4097)
        self.assertEqual(caught.exception.code,413)
        with self.assertRaises(urllib.error.HTTPError) as caught:self.request(now="invalid")
        self.assertEqual(caught.exception.code,422)
    def test_model_validation(self):
        for value in ({"records":[]},{"records":[{"kind":"delete","title":"X"}]},{"records":[{"kind":"task","title":"X","fields":{"parent":"1"}}]}):
            with self.assertRaises(ValueError):bridge.proposals(value)

if __name__=="__main__":unittest.main()
