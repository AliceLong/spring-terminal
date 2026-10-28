import base64
import json
import threading
import time
import unittest
import urllib.error
import urllib.request
from http.server import ThreadingHTTPServer
from pathlib import Path
from server import Frames, Store, handler_for

OPEN = urllib.request.build_opener(urllib.request.ProxyHandler({})).open
ROOT = Path(__file__).resolve().parent.parent
DATA = json.loads((ROOT/'evidence/monitor-host-snapshot.json').read_text())


def chunks(data=DATA, seq=1):
    raw = json.dumps(data, ensure_ascii=False).encode()
    parts = [raw[i:i+240] for i in range(0, len(raw), 240)]
    return [f'MONITOR_V1 seq={seq} part={i} total={len(parts)} data='.encode()+base64.b64encode(part) for i,part in enumerate(parts)]


class MonitorTests(unittest.TestCase):
    def test_fragmented_unicode_and_out_of_order(self):
        parser=Frames(); lines=chunks(); result=None
        for line in reversed(lines):
            frame=parser.feed(b'I (1000) SpringMonitor: '+line)
            if frame is not None: result=frame
        self.assertEqual(result,DATA)

    def test_incomplete_never_committed(self):
        parser=Frames()
        for line in chunks()[:-1]: self.assertIsNone(parser.feed(line))
        self.assertIsNone(parser.feed(b'normal log, not state'))

    def test_malformed_and_bounds(self):
        parser=Frames()
        for line in [b'MONITOR_V1 seq=1 part=0 total=513 data=e30=',b'MONITOR_V1 seq=1 part=2 total=1 data=e30=',b'MONITOR_V1 seq=1 part=0 total=1 data=!!!!']:
            self.assertIsNone(parser.feed(line))
        for line in chunks({'schema':1}): self.assertIsNone(parser.feed(line))
        for seq in range(9): parser.feed(chunks(seq=seq)[0])
        self.assertLessEqual(len(parser.pending),4)

    def test_stale_and_reconnect(self):
        store=Store();store.connection('usb',True,None);self.assertFalse(store.get()['connection']['live'])
        store.commit(DATA);self.assertTrue(store.get()['connection']['live'])
        store.connection('usb',False,'disconnected');self.assertFalse(store.get()['connection']['live'])
        store.connection('usb',True,None);self.assertFalse(store.get()['connection']['live'])
        self.assertEqual(store.get()['snapshot'],DATA)
        store.commit(DATA);store.received_at=time.monotonic()-20
        self.assertFalse(store.get()['connection']['live'])

    def test_http_read_only_and_no_traversal(self):
        store=Store();store.commit(DATA)
        server=ThreadingHTTPServer(('127.0.0.1',0),handler_for(store))
        thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
        base=f'http://127.0.0.1:{server.server_port}'
        try:
            self.assertEqual(json.load(OPEN(base+'/api/state'))['snapshot'],DATA)
            self.assertIn(b'12',OPEN(base+'/').read())
            for path in ['/../server.py','/api/approve']:
                with self.assertRaises(urllib.error.HTTPError) as error: OPEN(base+path)
                self.assertEqual(error.exception.code,404)
            with self.assertRaises(urllib.error.HTTPError) as error: OPEN(urllib.request.Request(base+'/api/state',data=b'{}',method='POST'))
            self.assertEqual(error.exception.code,405)
            with self.assertRaises(urllib.error.HTTPError) as error: OPEN(urllib.request.Request(base+'/',headers={'Host':'evil.example'}))
            self.assertEqual(error.exception.code,403)
        finally:
            server.shutdown();server.server_close();thread.join()


if __name__=='__main__': unittest.main()
