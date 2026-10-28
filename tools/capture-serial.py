#!/usr/bin/env python3
"""Capture native USB logs without asserting DTR/RTS or resetting the board."""
import argparse
import time
from pathlib import Path

import serial

parser = argparse.ArgumentParser()
parser.add_argument('--port', default='/dev/cu.usbmodem1101')
parser.add_argument('--seconds', type=float, default=60)
parser.add_argument('--output', required=True)
args = parser.parse_args()
end = time.monotonic() + args.seconds
with Path(args.output).open('ab', buffering=0) as output:
    while time.monotonic() < end:
        port = serial.Serial()
        port.port, port.baudrate, port.timeout = args.port, 115200, 0.2
        port.dtr, port.rts = False, False
        try:
            port.open()
            while time.monotonic() < end:
                data = port.read(8192)
                if data:
                    output.write(data)
        except (serial.SerialException, OSError):
            time.sleep(0.5)
        finally:
            port.close()
print('Serial capture saved:', args.output)
