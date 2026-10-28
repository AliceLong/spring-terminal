import io
import json
import threading
import time
import unittest
import urllib.error
import urllib.request
from copy import deepcopy
from http.server import ThreadingHTTPServer
from unittest.mock import patch

from roadshow_control import ControlError, RoadshowControl
from server import Store, handler_for
from roadshow_server import Handler

OPEN = urllib.request.build_opener(urllib.request.ProxyHandler({})).open


def fixture():
    return {'world': {'mode': 'normal', 'roadshow_epoch': 0},
            'memory': {'world_id': 'normal-0000000100000002'},
            'runtime': {'mind': {'boot': 42}, 'roadshow': {
                'available': True, 'protocol': 1, 'scene_count': 11, 'active': False, 'epoch': 0,
                'scene': 0, 'status': 'IDLE', 'request_id': 0}}}


def state(snapshot=None):
    return {'connection': {'live': True, 'age_seconds': 1}, 'snapshot': snapshot or fixture()}


class ControlTests(unittest.TestCase):
    def test_bounded_packet_and_no_economic_payload(self):
        c = RoadshowControl()
        response = c.submit(state(), 6)
        stream = io.BytesIO()
        c.flush(stream)
        parts = stream.getvalue().decode().strip().split(' ')
        self.assertEqual(parts[0], 'SHOW1')
        self.assertEqual(len(parts), 7)
        self.assertEqual([int(x, 16) for x in parts[1:]], [42, response['request']['id'], 0, 1, 2, 6])
        with self.assertRaises(ControlError):
            c.submit(state(), 1)
        length = len(stream.getvalue())
        c.flush(stream)
        self.assertEqual(len(stream.getvalue()), length)

    def test_chapter_scenes_are_explicit_bounded_requests(self):
        for scene in (8, 9, 10, 11):
            control = RoadshowControl()
            result = control.submit(state(), scene)
            self.assertEqual(result["request"]["scene"], scene)

    def test_old_firmware_refuses_new_scenes_immediately(self):
        snapshot = fixture()
        snapshot['runtime']['roadshow'].pop('scene_count')
        with self.assertRaises(ControlError):
            RoadshowControl().submit(state(snapshot), 8)

    def test_confirmation_requires_matching_request_world_mode_and_epoch(self):
        c = RoadshowControl()
        request = c.submit(state(), 2)['request']
        s = fixture()
        d = s['runtime']['roadshow']
        d.update(status='APPLIED', request_id=request['id'], active=True, scene=2, epoch=1)
        c.on_snapshot(s)
        self.assertEqual(c.view()['phase'], 'QUEUED')  # Mixed old/new telemetry is not confirmation.
        s['world'].update(mode='roadshow', roadshow_epoch=1)
        c.on_snapshot(s)
        self.assertEqual(c.view()['phase'], 'APPLIED')
        s['memory']['world_id'] = 'demo-0000000300000004'
        request = c.submit(state(s), 0)['request']
        d.update(request_id=request['id'], status='APPLIED', scene=0, active=False, epoch=2)
        s['world']['roadshow_epoch'] = 2
        c.on_snapshot(s)
        self.assertNotEqual(c.view()['phase'], 'APPLIED')
        s['world']['mode'] = 'normal'
        c.on_snapshot(s)
        self.assertEqual(c.view()['phase'], 'APPLIED')

    def test_disconnect_restart_and_timeout_never_replay(self):
        c = RoadshowControl(); c.submit(state(), 1); c.disconnected()
        self.assertEqual(c.view()['phase'], 'CANCELLED')
        stream = io.BytesIO(); c.flush(stream); self.assertFalse(stream.getvalue())
        c.submit(state(), 1); c.flush(stream); c.disconnected()
        self.assertEqual(c.view()['phase'], 'UNCERTAIN')
        with self.assertRaises(ControlError): c.submit(state(), 2)
        before = stream.getvalue(); c.flush(stream); self.assertEqual(stream.getvalue(), before)
        s = fixture(); s['runtime']['mind']['boot'] = 43; c.on_snapshot(s)
        self.assertEqual(c.view()['phase'], 'CANCELLED')
        c.submit(state(), 1); c.started = time.monotonic() - 80
        self.assertEqual(c.view()['phase'], 'UNCERTAIN')

    def test_rejected_acknowledgement(self):
        c = RoadshowControl(); request = c.submit(state(), 1)['request']; s = fixture()
        s['runtime']['roadshow'].update(status='REJECTED', reason='ROADSHOW_SAVE_FAILED', request_id=request['id'])
        c.on_snapshot(s)
        self.assertEqual(c.view()['reason'], 'ROADSHOW_SAVE_FAILED')
        self.assertFalse(c.view()['pending'])

    def test_refuses_stale_unsupported_malformed_and_old_demo(self):
        for scene in (-1, 12, True, '1', None):
            with self.assertRaises(ControlError): RoadshowControl().submit(state(), scene)
        for mutate in (
            lambda s: s['connection'].update(live=False),
            lambda s: s['connection'].update(age_seconds=16),
            lambda s: s['snapshot']['runtime']['roadshow'].update(available=False),
            lambda s: s['snapshot']['runtime']['roadshow'].update(epoch=1),
            lambda s: s['snapshot']['world'].update(mode='demo_10x'),
            lambda s: s['snapshot']['memory'].update(world_id='normal-invalid'),
        ):
            s = state(); mutate(s)
            with self.assertRaises(ControlError): RoadshowControl().submit(s, 1)


class ControlHTTPTests(unittest.TestCase):
    def test_proxy_errors_are_json_even_with_old_html_upstream(self):
        frontend = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        thread = threading.Thread(target=frontend.serve_forever, daemon=True)
        thread.start()
        base = f'http://127.0.0.1:{frontend.server_port}'
        headers = {'Content-Type': 'application/json', 'X-Spring-Roadshow': '1', 'Origin': base}
        try:
            html = b'<!DOCTYPE HTML><html>Read-only roadshow</html>'
            cases = [
                (urllib.error.HTTPError('http://observer', 405, 'Old service', {}, io.BytesIO(html)), 405, '路演控制'),
                (OSError('offline'), 503, '状态服务未连接'),
                (urllib.error.HTTPError('http://observer', 409, 'Conflict', {}, io.BytesIO(
                    json.dumps({'error': '正在通话，请先结束'}).encode())), 409, '正在通话'),
            ]
            for upstream_error, code, message in cases:
                with self.subTest(code=code), patch('roadshow_server.OPEN', side_effect=upstream_error):
                    with self.assertRaises(urllib.error.HTTPError) as error:
                        OPEN(urllib.request.Request(base + '/api/roadshow/switch', data=b'{"scene":9}', headers=headers))
                    response = error.exception
                    self.assertEqual(response.code, code)
                    self.assertEqual(response.headers.get_content_type(), 'application/json')
                    self.assertIn(message, json.load(response)['error'])
            upstream = io.BytesIO(html)
            upstream.status = 200
            with patch('roadshow_server.OPEN', return_value=upstream):
                with self.assertRaises(urllib.error.HTTPError) as error:
                    OPEN(urllib.request.Request(base + '/api/roadshow/switch', data=b'{"scene":9}', headers=headers))
                self.assertEqual(error.exception.code, 502)
                self.assertIn('请勿重复点击', json.load(error.exception)['error'])
        finally:
            frontend.shutdown()
            frontend.server_close()
            thread.join()

    def test_single_authorized_route_through_roadshow_proxy(self):
        store = Store(); store.roadshow_control = RoadshowControl()
        store.connection('test', True, None); store.commit(fixture())
        observer = ThreadingHTTPServer(('127.0.0.1', 0), handler_for(store))
        frontend = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        threads = [threading.Thread(target=s.serve_forever, daemon=True) for s in (observer, frontend)]
        for t in threads: t.start()
        base = f'http://127.0.0.1:{frontend.server_port}'
        source = f'http://127.0.0.1:{observer.server_port}'
        headers = {'Content-Type': 'application/json', 'X-Spring-Roadshow': '1', 'Origin': base}
        try:
            with patch('roadshow_server.SOURCE', source):
                for changes, expected in [({'X-Spring-Roadshow': ''}, 403), ({'Origin': 'https://evil.example'}, 403)]:
                    with self.assertRaises(urllib.error.HTTPError) as error:
                        OPEN(urllib.request.Request(base + '/api/roadshow/switch', data=b'{"scene":1}', headers=dict(headers, **changes)))
                    self.assertEqual(error.exception.code, expected)
                for body in (b'{"scene":1,"resources":99}', b'[]', b'{bad'):
                    with self.assertRaises(urllib.error.HTTPError) as error:
                        OPEN(urllib.request.Request(base + '/api/roadshow/switch', data=body, headers=headers))
                    self.assertEqual(error.exception.code, 400)
                response = OPEN(urllib.request.Request(base + '/api/roadshow/switch', data=b'{"scene":1}', headers=headers))
                self.assertEqual(response.status, 202)
                self.assertEqual(json.load(response)['phase'], 'QUEUED')
                self.assertEqual(json.load(OPEN(base + '/api/state'))['roadshow_control']['phase'], 'QUEUED')
        finally:
            for s in (frontend, observer): s.shutdown(); s.server_close()
            for t in threads: t.join()


if __name__ == '__main__': unittest.main()
