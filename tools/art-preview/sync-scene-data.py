"""Refresh the offline preview's embedded manifest after frame metadata changes."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2] / 'assets/art-v2-20261018'
manifest = json.loads((root / 'manifest.json').read_text())
(root / 'scene-demo-data.js').write_text(
    'globalThis.DESK_SCENE_ART=' + json.dumps(manifest['assets'], ensure_ascii=False) + ';\n'
)
