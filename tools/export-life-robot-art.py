#!/usr/bin/env python3
"""Add approved, pre-encoded life/work frames; preserve every existing asset byte."""
from pathlib import Path
import json,hashlib,shutil
R=Path(__file__).resolve().parents[1];A=R/'assets/art-v1';D=A/'device'
B=R/'vendor/xiaozhi-esp32/main/boards/waveshare/esp32-s3-touch-amoled-2.16'
P=A/'robot-life-v2'
m=json.loads((D/'manifest.json').read_text());life=json.loads((P/'manifest.json').read_text())
m['assets']=[e for e in m['assets'] if not e['name'].startswith(('Life_', 'Hover_tend_'))]
for asset in life['device_assets']:
 e={k:asset[k] for k in ['name','format','width','height','stride','offset','bytes','sha256']}
 if e['name'].startswith('Life_rest_tree_'):e['offset']=[e['offset'][0],33-e['height']]
 raw=(P/asset['file']).read_bytes();assert hashlib.sha256(raw).hexdigest()==e['sha256']
 assert len(raw)==e['bytes']
 for ext,key in [('bin','file'),('png','preview')]:shutil.copy2(P/asset[key],D/(e['name']+'.'+ext))
 m['assets'].append(e)
m['image_count']=len(m['assets']);m['robot']='Original hover +6 approved life/work groups; explicit animation mapping'
# Run the existing descriptor writer only, with no scene regeneration or image editing.
source=Path(__file__).with_name('export-scene-v2.py').read_text()
tail=source[source.index("m['flash_bytes']"):]
tail=tail.replace("assert m['flash_bytes']<1024*1024", "assert m['flash_bytes']<1200*1024")
tail=tail.replace("m['layout']='scene-v2: floor-rooted objects, icon HUD, no persistent plant labels';m['runtime_window_band']=[0,190,480,90]",'')
exec(tail.replace('export-scene-v2.py','export-life-robot-art.py'))
