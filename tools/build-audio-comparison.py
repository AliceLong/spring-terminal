#!/usr/bin/env python3
"""Build and seal diagnostic candidates only. Never opens a serial port or flashes."""
import hashlib
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / 'vendor/xiaozhi-esp32'
BUILD = FIRMWARE / 'build'
OUT = ROOT / 'dist/audio-comparison-20261023'
IDF = Path('/Users/alicelong/esp/esp-idf-v6.1/tools/idf.py')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main(output=OUT):
    output.mkdir(parents=True, exist_ok=True)
    reports = []
    # Finish with embedded to leave normal builds on the conservative storage path.
    for mode in ('flash', 'psram', 'embedded'):
        with (ROOT / f'evidence/audio-diagnostics-{mode}-build-20261023.log').open('w') as log:
            subprocess.run([sys.executable, str(IDF), '-C', str(FIRMWARE),
                            f'-DSPRING_BACKGROUND_STORAGE={mode}', 'build'],
                           stdout=log, stderr=subprocess.STDOUT, check=True)
        folder = output / mode
        folder.mkdir(exist_ok=True)
        shutil.copy2(BUILD/'xiaozhi.bin', folder/'xiaozhi.bin')
        assets = BUILD / ('generated_assets.bin' if mode == 'embedded' else 'spring_assets.bin')
        shutil.copy2(assets, folder/'assets.bin')
        size = (folder/'xiaozhi.bin').stat().st_size
        if size > 0x3f0000:
            raise ValueError('Application partition exceeded')
        report = dict(mode=mode, status='BUILT_NOT_FLASHED_NOT_AUDIO_VERIFIED',
                      app_size=size, app_headroom_bytes=0x3f0000-size,
                      app_sha256=digest(folder/'xiaozhi.bin'),
                      assets_sha256=digest(folder/'assets.bin'), assets_bytes=assets.stat().st_size,
                      app_offset='0x20000', assets_offset='0x800000',
                      background_sha256=digest(ROOT/'assets/art-v1/device/Background.bin'))
        (folder/'manifest.json').write_text(json.dumps(report, indent=2)+'\n')
        reports.append(report)
        print(mode, 'built, application headroom', report['app_headroom_bytes'], flush=True)
    # Equivalent pixels, and precisely the same package for the flash/PSRAM comparison.
    assert reports[0]['assets_sha256'] == reports[1]['assets_sha256']
    (output/'manifest.json').write_text(json.dumps(dict(status='LOCAL_CANDIDATES_ONLY',
        device_write_performed=False, variants=reports), indent=2)+'\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=OUT)
    main(parser.parse_args().output)
