#!/usr/bin/env python3
"""Append game background to a verified device assets pack; preserve every old file."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]
NAME = 'spring_background.rgb565'
PARTITION_SIZE = 8 * 1024 * 1024


def codec():
    path = ROOT / 'vendor/xiaozhi-esp32/scripts/build_default_assets.py'
    spec = importlib.util.spec_from_file_location('assets_codec', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def pack(files):
    # Standard XiaoZhi table/ZZ marker. Align data for native RGB565 access.
    entry = struct.Struct('<32sIIHH')
    table, payload = bytearray(), bytearray()
    base = 12 + len(files) * entry.size
    for name, (data, width, height) in files.items():
        encoded = name.encode()
        if not encoded or len(encoded) >= 32:
            raise ValueError('Invalid assets filename')
        payload.extend(b'\0' * (-(base + len(payload) + 2) % 4))
        table.extend(entry.pack(encoded, len(data), len(payload), width, height))
        payload.extend(b'ZZ' + data)
    body = table + payload
    result = struct.pack('<III', len(files), sum(body) & 0xffff, len(body)) + body
    if len(result) > PARTITION_SIZE:
        raise ValueError('Assets partition capacity exceeded')
    return result


def build(base, output, background=None):
    module = codec()
    original = module.read_assets_entries(base)
    files = dict(original)
    pixels = (background or ROOT / 'assets/art-v1/device/Background.bin').read_bytes()
    if len(pixels) != 480 * 480 * 2:
        raise ValueError('Background must be 480x480 RGB565')
    files[NAME] = (pixels, 480, 480)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(pack(files))
    reread = module.read_assets_entries(output)
    assert reread == files
    for name, value in original.items():
        if name != NAME:
            assert reread[name] == value, name
    report = dict(bytes=output.stat().st_size, free_bytes=PARTITION_SIZE-output.stat().st_size,
                  preserved_files=len(original)-int(NAME in original),
                  background_bytes=len(pixels), sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
                  base_sha256=hashlib.sha256(base.read_bytes()).hexdigest(),
                  background_sha256=hashlib.sha256(pixels).hexdigest())
    output.with_suffix('.json').write_text(json.dumps(report, indent=2)+'\n')
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(build(args.base, args.output), indent=2))
