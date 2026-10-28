#!/usr/bin/env python3
"""Replace walking sprites by tightly packed, registered hover frames."""
from pathlib import Path
from PIL import Image
source=Path(__file__).with_name('export-scene-v2.py').read_text()
exec(source[:source.index('base=Image.new')])
P=A/'robot-hover-v1';hover=json.loads((P/'manifest.json').read_text())
# Remove old motion frames from the binary; scene/preview source copies are untouched.
m['assets']=[e for e in m['assets'] if not e['name'].startswith(('Robot_','Hover_'))]
entries={a['name']:a for a in m['assets']}
for action,names in hover['animations'].items():
 if action=='wait':continue # ASR response waiting uses the receiving/listen animation.
 for name in names:
  f=next(f for f in hover['frames'] if f['name']==name);im=Image.open(P/f['file']).convert('RGBA')
  k=.98;im=im.resize((round(im.width*k),round(im.height*k)),Image.Resampling.LANCZOS)
  bounds=im.getchannel('A').point(lambda a:255 if a>96 else 0).getbbox();x,y,x2,y2=bounds;im=im.crop(bounds)
  key='Hover_'+name;e={'name':key,'format':'I4','offset':[x-round(80*k),y-round(72*k)]};m['assets'].append(e);entries[key]=e
  encode(key,im,e['offset'])
m['image_count']=len(m['assets']);m['robot']='44 registered hover frames; merged listening/waiting; no legacy walking data'
tail=source[source.index("m['flash_bytes']"):].replace("m['layout']='scene-v2: floor-rooted objects, icon HUD, no persistent plant labels';m['runtime_window_band']=[0,190,480,90]",'').replace('export-scene-v2.py','export-hover-robot-art.py')
exec(tail)
