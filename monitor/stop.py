from pathlib import Path
import os, signal, subprocess
root = Path(__file__).resolve().parent
pidfile = root / 'server.pid'
if pidfile.exists():
    pid = int(pidfile.read_text().strip())
    result = subprocess.run(['ps', '-p', str(pid), '-o', 'args='], capture_output=True, text=True)
    if str(root / 'server.py') in result.stdout:
        os.kill(pid, signal.SIGTERM)
        print('园丁状态服务已停止，USB串口已释放。')
    else:
        print('原服务已停止，没有终止其他程序。')
    pidfile.unlink(missing_ok=True)
else:
    print('没有找到正在运行的状态服务。')
