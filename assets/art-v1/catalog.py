"""Read original PNGs and write a metadata catalog; never modify image pixels."""
from pathlib import Path
import hashlib
import json
from PIL import Image
import numpy as np
from scipy import ndimage

ROOT = Path(__file__).resolve().parent
assets = {}
for path in sorted((ROOT / 'masters').glob('*.png')):
    im = Image.open(path)
    alpha = im.getchannel('A') if 'A' in im.getbands() else None
    bbox = alpha.point(lambda a: 255 if a > 32 else 0).getbbox() if alpha else (0, 0, *im.size)
    assets[path.stem] = dict(file='masters/' + path.name, width=im.width, height=im.height,
                            mode=im.mode, alpha_range=alpha.getextrema() if alpha else None,
                            visible_bounds=bbox, bytes=path.stat().st_size,
                            sha256=hashlib.sha256(path.read_bytes()).hexdigest())

def grid(name, columns, rows, names):
    item = assets[name]
    item['grid'] = [columns, rows]
    regions = {}
    for i, label in enumerate(names):
        col, row = i % columns, i // columns
        x, y = round(col * item['width'] / columns), round(row * item['height'] / rows)
        right, bottom = round((col + 1) * item['width'] / columns), round((row + 1) * item['height'] / rows)
        regions[label] = [x, y, right - x, bottom - y]
    item['regions'] = regions

actions = ['idle', 'walk', 'tend', 'water', 'rest', 'communicate']
grid('robot-atlas', 4, 6, [f'{a}_{i}' for a in actions for i in range(4)])
grid('plants-atlas-dry', 3, 3, [f'{p}_{s}' for p in ['tracklight', 'dewmoss', 'patchvine']
                               for s in ['seedling', 'growing', 'mature']])
# Generative layouts are not guaranteed regular grids. Explicit rectangles retain
# the full prop/tree silhouettes instead of cutting on nominal cell boundaries.
assets['props-atlas']['regions'] = dict(zip(
    ['planter', 'soil', 'bench', 'crates', 'watering_can', 'trowel', 'ivy', 'moss', 'lamp'],
    [[20, 125, 430, 270], [455, 180, 390, 210], [852, 85, 385, 315],
     [45, 460, 370, 340], [465, 490, 382, 310], [865, 490, 365, 310],
     [90, 820, 310, 420], [455, 915, 390, 270], [900, 825, 310, 415]]))
assets['tree-atlas']['regions'] = {'young': [0, 0, 736, 887], 'grown': [736, 0, 1038, 887]}

def silhouette_regions(name, columns, rows, names):
    """Locate connected sprites without cutting or changing the source image."""
    item = assets[name]
    alpha = np.array(Image.open(ROOT / item['file']).getchannel('A'))
    labeled, _ = ndimage.label(alpha > 64)
    counts = np.bincount(labeled.ravel())
    objects = ndimage.find_objects(labeled)
    boxes = {}
    for label, slices in enumerate(objects, 1):
        if counts[label] < 500:
            continue
        ys, xs = slices
        col = min(columns - 1, int((xs.start + xs.stop) / 2 / item['width'] * columns))
        row = min(rows - 1, int((ys.start + ys.stop) / 2 / item['height'] * rows))
        index = row * columns + col
        assert index not in boxes, f'Duplicate silhouette in {name}: {index}'
        boxes[index] = (xs.start, ys.start, xs.stop, ys.stop)
    assert len(boxes) == columns * rows, (name, len(boxes))
    regions, anchors = {}, {}
    for index, label in enumerate(names):
        left, top, right, bottom = boxes[index]
        row, col = divmod(index, columns)
        baseline = max(boxes[i][3] for i in range(row * columns, (row + 1) * columns))
        x, y = max(0, left - 5), max(0, top - 5)
        r, b = min(item['width'], right + 5), min(item['height'], bottom + 5)
        regions[label] = [x, y, r - x, b - y]
        anchors[label] = [round((col + .5) * item['width'] / columns), baseline]
    item['regions'] = regions
    item['anchors'] = anchors
    item['layout_note'] = 'Explicit silhouette bounds; grid describes ordering only, not crop rectangles.'

silhouette_regions('robot-atlas', 4, 6, [f'{a}_{i}' for a in actions for i in range(4)])
silhouette_regions('plants-atlas-dry', 3, 3, [f'{p}_{s}' for p in ['tracklight', 'dewmoss', 'patchvine']
                                          for s in ['seedling', 'growing', 'mature']])
grid('icons-atlas', 4, 5, ['water', 'energy', 'fiber', 'dew', 'chat', 'journal', 'tree', 'demo',
                           'ready', 'low_water', 'low_energy', 'building', 'shake', 'neutral',
                           'tilt_left', 'tilt_right', 'muted', 'unread', 'sparkle', 'leaf'])
assets['icons-atlas']['regions']['shake'] = [24, 845, 266, 251]

for item in assets.values():
    for label, (x, y, w, h) in item.get('regions', {}).items():
        assert x >= 0 and y >= 0 and w > 0 and h > 0
        assert x + w <= item['width'] and y + h <= item['height'], label

manifest = dict(version='art-v1', date='2026-10-20', target_logical_size=[480, 480],
                generator='built-in image_gen', status='art masters; firmware integration pending',
                assets=assets, animations={a: dict(frames=[f'{a}_{i}' for i in range(4)],
                fps={'idle': 3, 'walk': 6, 'tend': 4, 'water': 4, 'rest': 2, 'communicate': 4}[a],
                loop=a in ['idle', 'walk', 'rest']) for a in actions},
                procedural=['parallax_scroll', 'lamp_breath', 'growth_transition', 'dew_path',
                            'water_drops', 'harvest_particles', 'resource_fly', 'notification_pulse'],
                caveats=['Animation anchor tuning and 480px readability require integration review.',
                         'Parallax loop seams are not certified seamless; use seam blending if needed.',
                         'Master PNGs are not firmware-sized or encoded; do not flash them directly.'])
(ROOT / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
(ROOT / 'catalog-data.js').write_text('window.ART_MANIFEST = ' + json.dumps(manifest, ensure_ascii=False) + ';\n')
print(f'Cataloged {len(assets)} PNG masters; 24 robot frames, 9 plant stages, 2 trees, 9 props, 20 icons.')
