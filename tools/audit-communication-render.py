#!/usr/bin/env python3
"""Host-only RGB565/2MiB-cache audit. Never opens USB or writes game saves."""
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CMAKE = Path('/Users/alicelong/.espressif/tools/cmake/4.0.3/CMake.app/Contents/bin/cmake')
NINJA = Path('/Users/alicelong/.espressif/tools/ninja/1.12.1/ninja')
BUILD = ROOT / 'tests/.build/communication-render-device'
EVIDENCE = ROOT / 'evidence/harvest-v56'


def main():
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    with (EVIDENCE / 'communication-audit-reproduce-build.log').open('w') as log:
        for args in ([str(CMAKE), '-S', 'tests/touch', '-B', str(BUILD), '-G', 'Ninja',
                      f'-DCMAKE_MAKE_PROGRAM={NINJA}', '-DCMAKE_C_FLAGS=-DLV_BIN_DECODER_RAM_LOAD=1',
                      '-DCMAKE_CXX_FLAGS=-DLV_BIN_DECODER_RAM_LOAD=1'],
                     [str(CMAKE), '--build', str(BUILD), '--target',
                      'spring_communication_render_audit', '-j', '8']):
            subprocess.run(args, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                           check=True, timeout=180)
    rows = []
    for profile in ('chat-scaled', 'chat-native', 'chat-native-frozen',
                    'archive-scaled', 'archive-native', 'archive-scaled-frozen'):
        result = subprocess.run([str(BUILD / 'spring_communication_render_audit'), profile],
                                cwd=ROOT, capture_output=True, text=True, check=True, timeout=20)
        row = json.loads(result.stdout)
        rows.append(row)
        print(json.dumps(row), flush=True)
    by_name = {row['profile']: row for row in rows}
    for kind in ('chat', 'archive'):
        assert by_name[f'{kind}-scaled']['robot_animation_frames'] > 1
        assert by_name[f'{kind}-scaled']['additional_peak_bytes'] > 2 * by_name[f'{kind}-native']['additional_peak_bytes']
    output = dict(host_only=True, native_font_is_experiment_not_deployed=True,
                  settings=dict(color='RGB565', partial_rows=40, image_cache_bytes=2097152,
                                bin_decoder_ram_load=True),
                  notes='Host timings are not ESP32 timings or an audio/restart acceptance test.',
                  profiles=rows)
    (EVIDENCE / 'communication-render-current.json').write_text(json.dumps(output, indent=2) + '\n')


if __name__ == '__main__':
    main()
