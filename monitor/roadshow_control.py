"""Explicit local scene requests over the observer's one existing serial connection."""
import secrets
import threading
import time
from copy import deepcopy


class ControlError(ValueError):
    pass


class RoadshowControl:
    def __init__(self):
        self.lock = threading.RLock()
        self.request = None
        self.packet = None
        self.phase = 'IDLE'
        self.reason = None
        self.started = 0
        self.device = None

    def view(self):
        with self.lock:
            if self.pending() and time.monotonic() - self.started > 75:
                self.phase, self.reason = 'UNCERTAIN', '未收到设备最终确认，请等待重新连接；不自动重发。'
            return dict(enabled=True, phase=self.phase, reason=self.reason,
                        pending=self.pending(), request=deepcopy(self.request),
                        device=deepcopy(self.device))

    def pending(self):
        with self.lock:
            return self.phase in ('QUEUED', 'SENT', 'WAITING')

    def submit(self, state, scene):
        if type(scene) is not int or not 0 <= scene <= 11:
            raise ControlError('无效的路演情境')
        connection, snapshot = state.get('connection', {}), state.get('snapshot') or {}
        runtime = snapshot.get('runtime', {})
        device = runtime.get('roadshow', {})
        world = snapshot.get('world', {})
        world_id = snapshot.get('memory', {}).get('world_id', '')
        age = connection.get('age_seconds')
        if not connection.get('live') or not isinstance(age, (int, float)) or not 0 <= age < 15:
            raise ControlError('实机数据未在线，不能切换')
        if not device.get('available') or device.get('protocol') != 1:
            raise ControlError('当前固件未提供路演情境入口')
        if scene >= 8 and device.get('scene_count', 6) < scene:
            raise ControlError('当前固件尚未提供两章剧情情境，请先更新固件')
        if world.get('roadshow_epoch') != device.get('epoch'):
            raise ControlError('设备正在切换情境，请等下一份完整快照')
        if world.get('mode') not in ('normal', 'roadshow'):
            raise ControlError('请先在设备上退出旧的十倍演示模式')
        if scene == 7 and device.get('active'):
            raise ControlError('当前已在路演档，可选择情境重新开始')
        try:
            prefix, identity = world_id.split('-')
            if prefix not in ('normal', 'demo') or len(identity) != 16:
                raise ValueError()
            hi, lo = int(identity[:8], 16), int(identity[8:], 16)
            boot, epoch = runtime['mind']['boot'], device['epoch']
            if any(type(x) is not int or not 0 <= x <= 0xffffffff for x in (boot, epoch)) or not boot:
                raise ValueError()
        except (ValueError, KeyError, TypeError):
            raise ControlError('设备身份字段无效') from None
        with self.lock:
            self.view()
            if self.pending() or self.phase == 'UNCERTAIN' or device.get('status') == 'WAITING':
                raise ControlError('上一项切换尚未结束，请等待实机确认')
            request_id = secrets.randbelow(0xffffffff) + 1
            self.request = dict(id=request_id, scene=scene, boot=boot, epoch=epoch,
                                source_world=world_id)
            self.packet = ('SHOW1 ' + ' '.join(format(x, 'x') for x in
                (boot, request_id, epoch, hi, lo, scene)) + '\n').encode('ascii')
            self.started = time.monotonic()
            self.phase, self.reason = 'QUEUED', None
            return self.view()

    def flush(self, stream):
        with self.lock:
            if self.phase != 'QUEUED' or not self.packet:
                return
            packet, self.packet = self.packet, None
            try:
                if stream.write(packet) != len(packet):
                    raise OSError('Incomplete scene command')
                stream.flush()
                self.phase = 'SENT'
            except OSError:
                self.phase, self.reason = 'UNCERTAIN', 'USB 写入中断，切换结果待设备确认。'
                raise

    def on_snapshot(self, snapshot):
        runtime, world = snapshot.get('runtime', {}), snapshot.get('world', {})
        device = runtime.get('roadshow')
        with self.lock:
            self.device = deepcopy(device)
            if not self.request or self.phase in ('IDLE', 'REJECTED', 'CANCELLED'):
                return
            request = self.request
            if runtime.get('mind', {}).get('boot') != request['boot']:
                self.phase, self.reason, self.packet = 'CANCELLED', '设备已重启，旧切换请求已失效。', None
                return
            if not device or device.get('request_id') != request['id']:
                # A queued command is never replayed after a timeout or disconnect.
                self.view()
                return
            if world.get('roadshow_epoch') != device.get('epoch'):
                return
            status = device.get('status')
            if status == 'APPLIED':
                expected = world.get('mode') == ('roadshow' if request['scene'] else 'normal')
                expected &= request['scene'] in (0, 7) or device.get('scene') == request['scene']
                if expected and device.get('epoch', -1) >= request['epoch']:
                    self.phase, self.reason, self.packet = 'APPLIED', None, None
            elif status == 'REJECTED':
                self.phase, self.reason, self.packet = 'REJECTED', device.get('reason'), None
            elif status == 'WAITING':
                self.phase, self.reason = 'WAITING', device.get('reason')

    def disconnected(self):
        with self.lock:
            if self.pending():
                was_queued = self.phase == 'QUEUED'
                self.phase = 'CANCELLED' if was_queued else 'UNCERTAIN'
                self.reason = 'USB 已断开；不自动重发切换命令。'
                self.packet = None
