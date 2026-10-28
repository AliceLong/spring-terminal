#!/usr/bin/env python3
"""Export approved masters to small native LVGL images (no runtime PNG decoder)."""
from pathlib import Path
import hashlib
import json
import struct
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'assets/art-v1'
OUT = ART / 'device'
BOARD = ROOT / 'vendor/xiaozhi-esp32/main/boards/waveshare/esp32-s3-touch-amoled-2.16'
OUT.mkdir(exist_ok=True)
M = json.loads((ART / 'manifest.json').read_text())
images = {k: Image.open(ART / v['file']).convert('RGBA') for k, v in M['assets'].items()}
entries = []

def region(name, key):
    x, y, w, h = M['assets'][name]['regions'][key]
    return images[name].crop((x, y, x + w, y + h))

def resized(im, size):
    return im.resize(size, Image.Resampling.LANCZOS)

def put(canvas, im, xy, size):
    canvas.alpha_composite(resized(im, size), xy)

def add(name, im, offset=(0, 0), opaque=False):
    im = im.convert('RGBA')
    if opaque:
        a = np.asarray(im.convert('RGB'), dtype=np.uint16)
        rgb = ((a[:, :, 0] >> 3) << 11) | ((a[:, :, 1] >> 2) << 5) | (a[:, :, 2] >> 3)
        raw = rgb.astype('<u2').tobytes()
        stride, fmt = im.width * 2, 'RGB565'
        preview = im
    else:
        # Palette entries are LVGL's little-endian BGRA, including actual alpha.
        pal = im.quantize(colors=256, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE)
        rgba = pal.convert('RGBA')
        indices = np.asarray(pal, dtype=np.uint8)
        palette = bytearray(1024)
        colors = np.asarray(rgba)
        for index in np.unique(indices):
            y, x = np.argwhere(indices == index)[0]
            r, g, b, alpha = (int(v) for v in colors[y, x])
            palette[int(index) * 4:int(index) * 4 + 4] = bytes((b, g, r, alpha))
        stride = (im.width + 3) // 4 * 4
        pixels = np.zeros((im.height, stride), dtype=np.uint8)
        pixels[:, :im.width] = indices
        raw, fmt = bytes(palette) + pixels.tobytes(), 'I8'
        preview = rgba
        assert len(raw) == 1024 + stride * im.height
    (OUT / (name + '.bin')).write_bytes(raw)
    preview.save(OUT / (name + '.png'))
    entries.append(dict(name=name, width=im.width, height=im.height, stride=stride, format=fmt,
                        offset=list(offset), bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest(),
                        decoded_cache_bytes=0 if opaque else im.width * im.height * 4))

# Static scenery is flattened once on the host, never recomposed on the MCU.
base = Image.new('RGBA', (480, 480), '#263c32')
put(base, images['landscape-far'], (0, 130), (480, 240))
put(base, images['landscape-near'], (0, 85), (480, 220))
shell = resized(images['carriage-shell'], (480, 480))
base.alpha_composite(shell)
put(base, images['floor-platform'].crop((16, 279, 2155, 471)), (18, 337), (444, 34))
put(base, region('props-atlas', 'bench'), (34, 298), (62, 47))
put(base, region('props-atlas', 'crates'), (407, 305), (42, 39))
for center in (100, 230, 360):
    put(base, region('props-atlas', 'planter'), (center - 38, 301), (76, 43))
    put(base, region('props-atlas', 'soil'), (center - 29, 305), (58, 12))
add('Background', base, opaque=True)
# Only this narrow frame layer covers the moving scenery; no full-screen alpha layer.
add('WindowFrame', shell.crop((0, 176, 480, 286)))
add('Far', resized(images['landscape-far'], (480, 160)), opaque=True)
add('Near', resized(images['landscape-near'], (480, 200)).crop((0, 104, 480, 194)))

for kind in ('tracklight', 'dewmoss', 'patchvine'):
    for state in ('seedling', 'growing', 'mature'):
        key = kind + '_' + state
        x, y, w, h = M['assets']['plants-atlas-dry']['regions'][key]
        ax, ay = M['assets']['plants-atlas-dry']['anchors'][key]
        scale = 76 / 418
        add('Plant_' + key, resized(region('plants-atlas-dry', key),
            (max(1, round(w * scale)), max(1, round(h * scale)))),
            (round((x - ax) * scale), round((y - ay) * scale)))
for name, rect, size in [('TreeYoung', (265, 343, 685, 863), (44, 62)),
                         ('TreeGrown', (737, 12, 1763, 864), (94, 88))]:
    add(name, resized(images['tree-atlas'].crop(rect), size), (-size[0] // 2, -size[1]))

actions = ['idle', 'walk', 'tend', 'water', 'rest', 'communicate']
for action in actions:
    for frame in range(4):
        key = f'{action}_{frame}'
        x, y, w, h = M['assets']['robot-atlas']['regions'][key]
        ax, ay = M['assets']['robot-atlas']['anchors'][key]
        scale = 80 / 256
        add('Robot_' + key, resized(region('robot-atlas', key),
            (max(1, round(w * scale)), max(1, round(h * scale)))),
            (round((x - ax) * scale), round((y - ay) * scale)))
for key in M['assets']['icons-atlas']['regions']:
    add('Icon_' + key, resized(region('icons-atlas', key), (20, 20)))
for key in M['assets']['props-atlas']['regions']:
    size = {'ivy': (30, 45), 'moss': (57, 25), 'lamp': (24, 34)}.get(key, (32, 32))
    add('Prop_' + key, resized(region('props-atlas', key), size))
add('Drought', resized(images['event-drought'], (96, 96)), opaque=True)

total = sum(e['bytes'] for e in entries)
assert total < 1024 * 1024, f'Art exceeds 1MiB flash budget: {total}'
header = '''// Generated by tools/export-spring-art.py. Do not edit by hand.
#pragma once
#include <lvgl.h>
#include <cstddef>
namespace spring_art {
enum class Id : unsigned {
'''
header += ''.join('    ' + e['name'] + ',\n' for e in entries)
header += '''    Count
};
struct Asset { const lv_image_dsc_t* image; int x, y; };
const Asset& Get(Id id);
constexpr size_t kFlashBytes = ''' + str(total) + ''';
}  // namespace spring_art
'''
(BOARD / 'spring_art_data.h').write_text(header)
source = '// Generated by tools/export-spring-art.py. Do not edit by hand.\n#include "spring_art_data.h"\nnamespace spring_art {\nnamespace {\n'
for e in entries:
    data = (OUT / (e['name'] + '.bin')).read_bytes()
    source += f'alignas(4) const uint8_t data_{e["name"]}[] = {{\n'
    source += '\n'.join(','.join(str(v) for v in data[i:i + 32]) + ',' for i in range(0, len(data), 32))
    source += '\n};\n'
source += 'const lv_image_dsc_t descriptors[] = {\n'
for e in entries:
    source += ('    {{.magic=LV_IMAGE_HEADER_MAGIC, .cf=LV_COLOR_FORMAT_' + e['format'] +
               ', .flags=0, .w=' + str(e['width']) + ', .h=' + str(e['height']) +
               ', .stride=' + str(e['stride']) + ', .reserved_2=0}, ' + str(e['bytes']) +
               ', data_' + e['name'] + ', nullptr, nullptr},\n')
source += '};\nconst Asset assets[] = {\n'
for i, e in enumerate(entries):
    source += f'    {{&descriptors[{i}], {e["offset"][0]}, {e["offset"][1]}}},\n'
source += '};\n}\nconst Asset& Get(Id id) { return assets[static_cast<unsigned>(id)]; }\n}\n'
(BOARD / 'spring_art_data.cc').write_text(source)
report = dict(flash_bytes=total, budget_bytes=1024 * 1024, image_count=len(entries),
              encoding='RGB565 + I8/BGRA palette; Flash-resident const images', assets=entries)
(OUT / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
print(f'{len(entries)} images, {total:,} bytes ({total / 1024:.1f} KiB) embedded flash data')
