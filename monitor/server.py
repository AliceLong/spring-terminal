#!/usr/bin/env python3
"""Loopback-only observer; optional explicit USB mind bridge, never toggles reset."""
import argparse
import base64
import glob
import json
import re
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

FRAME = re.compile(rb'MONITOR_V1 seq=(\d+) part=(\d+) total=(\d+) data=([A-Za-z0-9+/=]+)')
ROOT = Path(__file__).resolve().parent


class Frames:
    def __init__(self):
        self.pending = {}

    def feed(self, line):
        match = FRAME.search(line)
        if not match:
            return None
        seq, part, total = map(int, match.groups()[:3])
        if not 1 <= total <= 512 or not 0 <= part < total:
            return None
        now = time.monotonic()
        self.pending = {k: v for k, v in self.pending.items() if now - v[0] < 20}
        if seq not in self.pending and len(self.pending) >= 4:
            self.pending.pop(next(iter(self.pending)))
        if seq not in self.pending:
            self.pending[seq] = (now, total, {})
        _, expected, parts = self.pending[seq]
        if total != expected:
            self.pending.pop(seq, None)
            return None
        try:
            chunk = base64.b64decode(match[4], validate=True)
        except ValueError:
            return None
        if len(chunk) > 240:
            return None
        parts[part] = chunk
        if len(parts) != total:
            return None
        self.pending.pop(seq, None)
        try:
            data = json.loads(b''.join(parts[i] for i in range(total)))
            if (data.get('schema') != 1 or not isinstance(data.get('uptime_ms'), int)
                    or not isinstance(data.get('world'), dict)
                    or data['world'].get('game') != '终点站是春天'
                    or not isinstance(data.get('runtime'), dict)
                    or not isinstance(data.get('events'), dict)):
                return None
            return data
        except (ValueError, TypeError, UnicodeError, AttributeError):
            return None


class Store:
    def __init__(self, archive=None, audio_diagnostics=None):
        self.lock = threading.Lock()
        self.archive = archive
        self.archive_error = None
        self.audio_diagnostics = audio_diagnostics
        self.audio_diagnostics_error = None
        self.port = None
        self.opened = False
        self.error = '正在寻找 USB 设备'
        self.data = None
        self.received_at = None
        self.received_epoch = None
        self.frames = 0
        self.current_frame = False
        self.mind_bridge = None
        self.roadshow_control = None

    def connection(self, port, opened, error):
        with self.lock:
            self.port, self.opened, self.error = port, opened, error
            self.current_frame = False
        if self.roadshow_control:
            self.roadshow_control.disconnected()

    def commit(self, data):
        if self.roadshow_control:
            self.roadshow_control.on_snapshot(data)
        if self.mind_bridge:
            try:
                self.mind_bridge.on_snapshot(data)
            except Exception as exc:
                with self.mind_bridge.lock:
                    self.mind_bridge.status.update(phase='意图桥接异常，本地规则继续', error=type(exc).__name__)
        if self.audio_diagnostics:
            try:
                self.audio_diagnostics.write(data)
                self.audio_diagnostics_error = None
            except OSError:
                self.audio_diagnostics_error = '音频诊断日志写入失败'
        if self.archive:
            try:
                self.archive.ingest(data.get('memory'))
                self.archive_error = None
            except Exception as exc:
                self.archive_error = '本地归档失败：' + type(exc).__name__
        with self.lock:
            self.current_frame = True
            self.data = data
            self.received_at = time.monotonic()
            self.received_epoch = time.time()
            self.frames += 1

    def get(self):
        with self.lock:
            age = time.monotonic() - self.received_at if self.received_at is not None else None
            live = self.opened and self.current_frame and age is not None and age < 15
            return {'application': 'spring-status-monitor-v1', 'connection': {'port': self.port, 'serial_open': self.opened,
                    'error': self.error, 'age_seconds': age, 'live': live,
                    'received_epoch': self.received_epoch, 'frames': self.frames},
                    'snapshot': self.data, 'mind_bridge': self.mind_bridge.view() if self.mind_bridge else None,
                    'roadshow_control': self.roadshow_control.view() if self.roadshow_control else {'enabled': False},
                    'archive_error': self.archive_error,
                    'audio_diagnostics_error': self.audio_diagnostics_error,
                    'archive': self.archive.summary((self.data or {}).get('memory', {}).get('world_id')) if self.archive else {'available': False}}


def observe(store, chosen, evidence_log=None):
    import serial
    while True:
        devices = sorted(glob.glob('/dev/cu.usbmodem*'))
        port = chosen or (devices[0] if len(devices) == 1 else None)
        if not port:
            store.connection(None, False, '未连接设备' if not devices else '发现多个串口，请启动时指定 --serial')
            time.sleep(1)
            continue
        stream = serial.Serial()
        stream.port, stream.baudrate, stream.timeout = port, 115200, 0.3
        stream.dtr, stream.rts = False, False
        stream.exclusive = True
        stream.write_timeout = 1
        parser, buffer = Frames(), b''
        try:
            stream.open()
            store.connection(port, True, None)
            while True:
                if store.roadshow_control:
                    store.roadshow_control.flush(stream)
                if store.mind_bridge and not (store.roadshow_control and store.roadshow_control.pending()):
                    store.mind_bridge.flush(stream)
                buffer += stream.read(8192)
                while b'\n' in buffer:
                    line, buffer = buffer.split(b'\n', 1)
                    if evidence_log and any(marker in line for marker in
                        (b'COMM_ENTRY ', b'COMM_ROUTE ', b'VOICE_ACTION ', b'VOICE_PLANT ', b'VOICE_PLANT_COMMIT ', b'ARCHIVE ', b'SOCIAL ', b'CHARACTER ', b'CHARACTER_MEMORY ', b'WISH ', b'LIFE_START ', b'COMMUNICATION_CLOSE ', b'UI_PERF ', b'TOUCH_PERF ',
                         b'COLLECTION_DIAG ', b'Guru Meditation Error', b'Backtrace:', b'abort() was called',
                         b'Stack canary watchpoint', b'assert failed:', b'EXCCAUSE',
                         b'EXCVADDR', b'PC      :', b'rst:', b'stack overflow',
                         b'TOOL_REPLY ', b'MIND_ACCEPT ', b'MIND_REJECT ', b'ROADSHOW ',
                         b'WORLD_WORKSPACE ', b'GAME_STORAGE ', b'ECONOMY schema=',
                         b'ACTION_REJECT ', b'ACTION kind=')):
                        with open(evidence_log, 'ab') as record:
                            record.write(line + b'\n')
                    if store.mind_bridge:
                        store.mind_bridge.on_line(line)
                    frame = parser.feed(line)
                    if frame is not None:
                        store.commit(frame)
                        if evidence_log:
                            runtime = frame.get('runtime', {})
                            life = frame.get('world', {}).get('life', {})
                            trace = dict(uptime_ms=frame.get('uptime_ms'),
                                world_id=frame.get('memory', {}).get('world_id'),
                                voice=runtime.get('voice'), fsm=runtime.get('fsm'),
                                communication=runtime.get('communication'),
                                tool_queries=runtime.get('tool_queries'),
                                life={k:life.get(k) for k in ('intent','phase','arrived','reason')},
                                plant_action=frame.get('world', {}).get('plant_action'))
                            with open(evidence_log, 'ab') as record:
                                record.write(b'COMM_TRACE '+json.dumps(trace,ensure_ascii=False).encode()+b'\n')
                if len(buffer) > 65536:
                    buffer = b''
        except (serial.SerialException, OSError) as exc:
            store.connection(port, False, 'USB 未连接或串口被占用：' + type(exc).__name__)
        finally:
            stream.close()
        time.sleep(1)


def handler_for(store):
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            allowed = {f'127.0.0.1:{self.server.server_port}', f'localhost:{self.server.server_port}'}
            if self.headers.get('Host') not in allowed:
                self.send_error(403)
                return
            name = self.path.split('?', 1)[0]
            if name == '/api/archive':
                from urllib.parse import parse_qs, urlsplit
                query = parse_qs(urlsplit(self.path).query)
                try:
                    category = int(query.get('category', ['0'])[0])
                    chain = int(query.get('chain', ['0'])[0])
                    before = int(query.get('before', ['0'])[0])
                    limit = int(query.get('limit', ['40'])[0])
                    if not (0 <= category <= 5 and 0 <= chain <= 4294967295 and 0 <= before <= 4294967295 and 1 <= limit <= 100):
                        raise ValueError()
                except ValueError:
                    self.send_error(400)
                    return
                current = store.get()
                world = (current['snapshot'] or {}).get('memory', {}).get('world_id')
                result = store.archive.query(world, category, chain, before, limit) if store.archive and world else {'records': [], 'long_term': [], 'summary': {'available': False}}
                body = json.dumps(result, ensure_ascii=False).encode()
                kind = 'application/json; charset=utf-8'
            elif name == '/api/state':
                body = json.dumps(store.get(), ensure_ascii=False).encode()
                kind = 'application/json; charset=utf-8'
            elif name == '/api/mind':
                body = json.dumps(store.mind_bridge.view() if store.mind_bridge else {'enabled': False}, ensure_ascii=False).encode()
                kind = 'application/json; charset=utf-8'
            elif name in ('/', '/index.html', '/app.js', '/memory.js', '/style.css'):
                file = ROOT / ('index.html' if name in ('/', '/index.html') else name[1:])
                body = file.read_bytes()
                kind = {'.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css'}[file.suffix] + '; charset=utf-8'
            else:
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header('Content-Type', kind)
            self.send_header('Content-Length', str(len(body)))
            self.send_header('Cache-Control', 'no-store')
            self.send_header('X-Content-Type-Options', 'nosniff')
            self.send_header('Content-Security-Policy', "default-src 'self'; connect-src 'self'; style-src 'self'; script-src 'self'; frame-ancestors 'none'")
            self.end_headers()
            try:
                self.wfile.write(body)
            except (BrokenPipeError, ConnectionResetError):
                pass

        def do_POST(self):
            if self.path != '/api/roadshow/switch' or not store.roadshow_control:
                self.send_error(405, 'No control endpoint')
                return
            allowed = {f'127.0.0.1:{self.server.server_port}', f'localhost:{self.server.server_port}'}
            origin = self.headers.get('Origin')
            if (self.headers.get('Host') not in allowed or self.headers.get('X-Spring-Roadshow') != '1'
                    or origin is not None and origin not in {'http://' + host for host in allowed}):
                self.send_error(403)
                return
            from roadshow_control import ControlError
            try:
                length = int(self.headers.get('Content-Length', '0'))
                if not 0 < length <= 512 or self.headers.get_content_type() != 'application/json':
                    raise ValueError()
                payload = json.loads(self.rfile.read(length))
                if not isinstance(payload, dict) or set(payload) != {'scene'}:
                    raise ValueError()
                result = store.roadshow_control.submit(store.get(), payload['scene'])
            except ControlError as exc:
                body = json.dumps({'error': str(exc)}, ensure_ascii=False).encode()
                self.send_response(409)
            except (ValueError, TypeError):
                self.send_error(400)
                return
            else:
                body = json.dumps(result, ensure_ascii=False).encode()
                self.send_response(202)
            self.send_header('Content-Type', 'application/json; charset=utf-8')
            self.send_header('Content-Length', str(len(body)))
            self.send_header('Cache-Control', 'no-store')
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *_):
            pass
    return Handler


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--serial')
    parser.add_argument('--port', type=int, default=8766)
    parser.add_argument('--no-browser', action='store_true')
    parser.add_argument('--roadshow-control', action='store_true', help='Enable explicit local scene switching; separate roadshow save')
    parser.add_argument('--mind-bridge', action='store_true', help='Opt in to bounded USB life-intent requests; no economic commands')
    parser.add_argument('--evidence-log', help='Save numeric action/communication evidence for hardware tests; no transcripts')
    args = parser.parse_args()
    from archive import Archive
    from audio_diagnostics import AudioDiagnosticsLog
    store = Store(Archive(ROOT / 'data' / 'experiences.sqlite3'),
                  AudioDiagnosticsLog(ROOT / 'data' / 'audio-diagnostics.jsonl'))
    if args.roadshow_control:
        from roadshow_control import RoadshowControl
        store.roadshow_control = RoadshowControl()
    if args.mind_bridge:
        import sys
        sys.path.insert(0, str(ROOT.parent / 'robot-mind'))
        from device_bridge import DeviceBridge
        store.mind_bridge = DeviceBridge(ROOT / 'data' / 'mind-experiences.sqlite3')
    try:
        server = ThreadingHTTPServer(('127.0.0.1', args.port), handler_for(store))
    except OSError:
        import urllib.request
        url = f'http://127.0.0.1:{args.port}'
        try:
            opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
            data = json.load(opener.open(url + '/api/state', timeout=2))
        except Exception:
            parser.error('端口已被其他程序占用，请使用 --port 另选端口')
        if data.get('application') != 'spring-status-monitor-v1':
            parser.error('端口已被其他程序占用，请使用 --port 另选端口')
        if not args.no_browser:
            import webbrowser
            webbrowser.open(url)
        print('园丁状态页已运行：' + url)
        return
    threading.Thread(target=observe, args=(store, args.serial, args.evidence_log), daemon=True).start()
    url = f'http://127.0.0.1:{server.server_port}'
    import os
    pidfile = ROOT / 'server.pid'
    pidfile.write_text(str(os.getpid()) + '\n')
    print('园丁只读状态页：' + url, flush=True)
    if not args.no_browser:
        import webbrowser
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        try:
            if pidfile.read_text().strip() == str(os.getpid()):
                pidfile.unlink()
        except FileNotFoundError:
            pass


if __name__ == '__main__':
    main()
