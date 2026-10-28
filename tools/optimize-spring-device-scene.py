#!/usr/bin/env python3
"""Flatten approved device exports; preserve all sprite frames and their anchors."""
from pathlib import Path
import hashlib,json,re
import numpy as np
from PIL import Image
root=Path(__file__).resolve().parents[1]
out=root/'assets/art-v1/device'
manifest=json.loads((out/'manifest.json').read_text())
assets={a['name']:a for a in manifest['assets']}
for name,size in {'Background':(480,480),'Far':(480,224),'Near':(480,90),'WindowFrame':(480,208)}.items():
    assert (assets[name]['width'],assets[name]['height'])==size, 'Unexpected input layout: '+name
assert all('Robot_walk_left_'+str(i) in assets for i in range(4)), 'Approved registered gait required'
source=out/'source';source.mkdir(exist_ok=True)
for ext in ('png','bin'):
    f=source/('Background.'+ext)
    if not f.exists(): f.write_bytes((out/('Background.'+ext)).read_bytes())
room=Image.open(source/'Background.png').convert('RGBA')
far=Image.open(out/'Far.png').convert('RGBA')
near=Image.open(out/'Near.png').convert('RGBA')
frame=Image.open(out/'WindowFrame.png').convert('RGBA')
window=far.crop((0,8,440,216))
window.alpha_composite(near.crop((0,0,440,90)),(0,110))
room.alpha_composite(window,(20,72))
room.alpha_composite(frame,(0,72))
room.alpha_composite(Image.new('RGBA',(480,190),(20,43,34,153)),(0,0))
a=np.asarray(room.convert('RGB'),dtype=np.uint16)
raw=(((a[:,:,0]>>3)<<11)|((a[:,:,1]>>2)<<5)|(a[:,:,2]>>3)).astype('<u2').tobytes()
assert len(raw)==480*480*2
(out/'Background.bin').write_bytes(raw);room.save(out/'Background.png')
p=root/'vendor/xiaozhi-esp32/main/boards/waveshare/esp32-s3-touch-amoled-2.16/spring_art_data.cc'
s=p.read_text();array='\n'.join(','.join(str(v) for v in raw[i:i+32])+',' for i in range(0,len(raw),32))
s,count=re.subn(r'(alignas\(4\) const uint8_t data_Background\[\] = \{\n).*?(\n\};)',lambda m:m[1]+array+m[2],s,flags=re.S)
assert count==1;p.write_text(s)
p=out/'manifest.json';m=json.loads(p.read_text())
for item in m['assets']:
    if item['name']=='Background':item['sha256']=hashlib.sha256(raw).hexdigest()
m['baked_top_hud']=True;m['runtime_window_band']=[0,190,480,90]
p.write_text(json.dumps(m,indent=2)+'\n')
(out.parent/'device-catalog.js').write_text('window.DEVICE_ART = '+json.dumps(m)+';\n')
print('Baked RGB565 room + static top HUD; sprites/anchors unchanged; runtime band86400B')
