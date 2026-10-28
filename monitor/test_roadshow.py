import json
import threading
import unittest
import urllib.error
import urllib.request
from http.server import ThreadingHTTPServer
from unittest.mock import patch

from roadshow_server import Handler

OPEN = urllib.request.build_opener(urllib.request.ProxyHandler({})).open


class RoadshowTests(unittest.TestCase):
    def setUp(self):
        self.server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.base = f'http://127.0.0.1:{self.server.server_port}'

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()

    def test_reuses_observer_and_preserves_queries(self):
        body = b'{"connection":{"live":true},"snapshot":{"world":{}}}'
        with patch('roadshow_server.read_source', return_value=body) as read:
            self.assertEqual(OPEN(self.base + '/api/state').read(), body)
            self.assertEqual(OPEN(self.base + '/api/archive?category=4&limit=3').read(), body)
            read.assert_called_with('/api/archive?category=4&limit=3')

    def test_observer_failure_is_not_live_or_example_data(self):
        with patch('roadshow_server.read_source', side_effect=OSError()):
            state = json.load(OPEN(self.base + '/api/state'))
            self.assertFalse(state['connection']['live'])
            self.assertIsNone(state['snapshot'])
            with self.assertRaises(urllib.error.HTTPError) as error:
                OPEN(self.base + '/api/archive')
            self.assertEqual(error.exception.code, 503)

    def test_fixed_routes_and_local_read_only_boundary(self):
        self.assertIn('路演终端', OPEN(self.base).read().decode())
        self.assertIn(b'modules', OPEN(self.base + '/terminal').read())
        for path, code in [('/../server.py', 404), ('/data/experiences.sqlite3', 404), ('/api/approve', 404)]:
            with self.assertRaises(urllib.error.HTTPError) as error:
                OPEN(self.base + path)
            self.assertEqual(error.exception.code, code)
        for request, code in [
            (urllib.request.Request(self.base, headers={'Host': 'evil.example'}), 403),
            (urllib.request.Request(self.base + '/api/state', method='POST', data=b'{}'), 405),
        ]:
            with self.assertRaises(urllib.error.HTTPError) as error:
                OPEN(request)
            self.assertEqual(error.exception.code, code)
        response = OPEN(self.base)
        self.assertIn('microphone=()', response.headers['Permissions-Policy'])
        self.assertIn("frame-ancestors 'none'", response.headers['Content-Security-Policy'])


if __name__ == '__main__':
    unittest.main()
