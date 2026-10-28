#!/usr/bin/env python3
"""Local roadshow frontend; reuses the running observer without owning USB."""
import argparse
import json
import subprocess
import sys
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SOURCE = 'http://127.0.0.1:8766'
OPEN = urllib.request.build_opener(urllib.request.ProxyHandler({})).open
FILES = {'/': 'roadshow.html', '/roadshow': 'roadshow.html',
         '/roadshow.html': 'roadshow.html', '/roadshow.js': 'roadshow.js',
         '/roadshow.css': 'roadshow.css', '/terminal': 'index.html',
         '/index.html': 'index.html', '/app.js': 'app.js', '/memory.js': 'memory.js',
         '/style.css': 'style.css'}


def read_source(path):
    with OPEN(SOURCE + path, timeout=3) as response:
        return response.read()


def observer_available():
    try:
        return json.loads(read_source('/api/state')).get('application') == 'spring-status-monitor-v1'
    except (OSError, ValueError):
        return False


class Handler(BaseHTTPRequestHandler):
    def control_reply(self, code, payload):
        body = json.dumps(payload, ensure_ascii=False).encode()
        self.send_response(code)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.headers.get('Host') not in (f'127.0.0.1:{self.server.server_port}',
                                            f'localhost:{self.server.server_port}'):
            self.send_error(403)
            return
        name = self.path.split('?', 1)[0]
        if name == '/api/roadshow':
            body = b'{"application":"spring-roadshow-v1"}'
            kind = 'application/json'
        elif name in ('/api/state', '/api/archive', '/api/mind'):
            try:
                body = read_source(self.path)
            except urllib.error.HTTPError as exc:
                self.send_error(exc.code)
                return
            except OSError:
                if name != '/api/state':
                    self.send_error(503, 'Observer unavailable')
                    return
                body = json.dumps({'application': 'spring-status-monitor-v1',
                    'connection': {'live': False, 'age_seconds': None,
                                   'error': '园丁状态服务未运行，请打开园丁状态页。'},
                    'snapshot': None, 'archive': {'available': False}}, ensure_ascii=False).encode()
            kind = 'application/json'
        elif name in FILES:
            file = ROOT / FILES[name]
            body = file.read_bytes()
            kind = {'.html': 'text/html', '.css': 'text/css', '.js': 'text/javascript'}[file.suffix]
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header('Content-Type', kind + '; charset=utf-8')
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('Content-Security-Policy', "default-src 'self'; connect-src 'self'; media-src 'self' blob:; style-src 'self'; script-src 'self'; frame-ancestors 'none'")
        self.send_header('Permissions-Policy', 'camera=(self), microphone=(), display-capture=(self)')
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def do_POST(self):
        if self.path != '/api/roadshow/switch':
            self.control_reply(405, {'error': '此接口不支持写入。'})
            return
        allowed = {f'127.0.0.1:{self.server.server_port}', f'localhost:{self.server.server_port}'}
        origin = self.headers.get('Origin')
        if (self.headers.get('Host') not in allowed or self.headers.get('X-Spring-Roadshow') != '1'
                or origin is not None and origin not in {'http://' + host for host in allowed}):
            self.control_reply(403, {'error': '情境切换请求未通过本机来源校验。'})
            return
        try:
            length = int(self.headers.get('Content-Length', '0'))
            if not 0 < length <= 512 or self.headers.get_content_type() != 'application/json':
                raise ValueError()
        except ValueError:
            self.control_reply(400, {'error': '情境切换请求格式不正确。'})
            return
        request = urllib.request.Request(SOURCE + self.path, data=self.rfile.read(length),
            method='POST', headers={'Content-Type': 'application/json', 'X-Spring-Roadshow': '1', 'Origin': SOURCE})
        try:
            with OPEN(request, timeout=4) as response:
                body, code = response.read(), response.status
        except urllib.error.HTTPError as exc:
            body, code = exc.read(), exc.code
        except OSError:
            self.control_reply(503, {'error': '园丁状态服务未连接，请先启动状态服务。'})
            return
        try:
            payload = json.loads(body)
            if not isinstance(payload, dict):
                raise ValueError()
        except (ValueError, UnicodeError):
            if code == 405:
                message = '园丁状态服务尚未启用路演控制，请重启状态服务并启用路演控制。'
            elif code >= 400:
                message = f'情境切换服务返回错误（HTTP {code}），设备状态未确认。'
            else:
                message = '情境切换服务响应异常，结果待设备确认，请勿重复点击。'
                code = 502
            payload = {'error': message}
        self.control_reply(code, payload)

    def log_message(self, *_):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=8767)
    parser.add_argument('--no-browser', action='store_true')
    parser.add_argument('--no-start-observer', action='store_true')
    args = parser.parse_args()
    url = f'http://127.0.0.1:{args.port}'
    try:
        server = ThreadingHTTPServer(('127.0.0.1', args.port), Handler)
    except OSError:
        try:
            with OPEN(url + '/api/roadshow', timeout=2) as response:
                if json.load(response).get('application') != 'spring-roadshow-v1':
                    raise ValueError()
        except (OSError, ValueError):
            parser.error('端口已被其他程序占用，请使用 --port 另选端口')
        if not args.no_browser:
            import webbrowser
            webbrowser.open(url)
        print('路演终端已运行：' + url)
        return
    if not args.no_start_observer and not observer_available():
        log_path = ROOT / 'data' / 'roadshow-observer.log'
        log_path.parent.mkdir(parents=True, exist_ok=True)
        with log_path.open('ab') as log:
            subprocess.Popen([sys.executable, str(ROOT / 'server.py'), '--no-browser', '--roadshow-control'],
                             stdin=subprocess.DEVNULL, stdout=log, stderr=log, start_new_session=True)
        print('已启动园丁只读状态服务；若多设备请另行指定串口。', flush=True)
    print('本地路演终端：' + url + '（复用状态服务，不占用第二个串口）', flush=True)
    if not args.no_browser:
        import webbrowser
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == '__main__':
    main()
