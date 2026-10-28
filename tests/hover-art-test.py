"""Regression: frame calibration must never use a dark leg as the face panel."""
from pathlib import Path
from PIL import Image
import json,hashlib
from statistics import median
P=Path(__file__).resolve().parents[1]/'assets/art-v1/robot-hover-v1'
m=json.loads((P/'manifest.json').read_text());assert m['version']>=3 and len(m['frames'])==48
for action,names in m['animations'].items():
 frames=[f for f in m['frames'] if f['name'] in names];assert len(frames)==4
 assert len({f['scale'] for f in frames})==1,(action,'per-frame scaling returned')
 heights=[];heads=[]
 for f in frames:
  im=Image.open(P/f['file']);assert im.mode=='RGBA' and im.size==(160,176)
  assert im.getchannel('A').getextrema()[0]==0
  assert hashlib.sha256((P/f['file']).read_bytes()).hexdigest()==f['sha256']
  source=Image.open(P/'masters'/(f['source']+'.png'));box=f['source_face'];height=box[3]-box[1]
  assert .14<height/(source.height/3)<.30,(f['name'],'face detection includes body/joints')
  heights.append(height*f['scale']);heads.append(f['source_head_width']*f['scale'])
 assert max(heights)/min(heights)<1.25,(action,'face size outlier')
 assert abs(median(heads)-56)<.01
 assert max(heads)/min(heads)<1.2
print('48 corrected frames: fixed animation scales, face geometry, alpha and hashes passed.')
