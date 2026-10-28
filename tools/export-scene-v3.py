#!/usr/bin/env python3
"""Encode the approved preview layers for the device; retains registered robot frames."""
from pathlib import Path
from PIL import Image
source=Path(__file__).with_name('export-scene-v2.py').read_text()
# Share the established indexed/RGB565 encoder and descriptor generator.
exec(source[:source.index("base=Image.new")])
base_encode=encode
def encode(name,im,offset=None):
 if not name.startswith('Tree'):
  return base_encode(name,im,offset)
 # Large tree silhouettes need a richer palette than the tiny original sprites.
 e=entries[name];q=im.convert('RGBA').quantize(colors=256,method=Image.Quantize.FASTOCTREE,dither=Image.Dither.NONE)
 preview=q.convert('RGBA');idx=np.asarray(q,dtype=np.uint8);rgba=np.asarray(preview);palette=bytearray(1024)
 for index in np.unique(idx):
  y,x=np.argwhere(idx==index)[0];r,g,b,a=map(int,rgba[y,x]);palette[int(index)*4:int(index)*4+4]=bytes((b,g,r,a))
 stride=(im.width+3)//4*4;p=np.zeros((im.height,stride),dtype=np.uint8);p[:,:im.width]=idx;raw=bytes(palette)+p.tobytes()
 e.update(format='I8',width=im.width,height=im.height,stride=stride,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest(),decoded_cache_bytes=im.width*im.height*4)
 if offset is not None:e['offset']=list(offset)
 (D/(name+'.bin')).write_bytes(raw);preview.save(D/(name+'.png'))
P=A/'preview-layout-v3'
background=Image.open(P/'Room.png').convert('RGBA')
background.alpha_composite(Image.open(P/'Grass-integrated.png').convert('RGBA'))
box=Image.open(P/'Planter.png').convert('RGBA')
for x in (70,151,232):background.alpha_composite(box,(x-37,351))
encode('Background',background)
# Static far scenery and window frames are now baked into the background.
encode('Far',Image.new('RGB',(1,1)))
encode('WindowFrame',Image.new('RGBA',(1,1)))
for plant in ('tracklight','dewmoss','patchvine'):
 for phase in ('seedling','growing','mature'):
  key=plant+'_'+phase;x,y,w,h=master['assets']['plants-atlas-dry']['regions'][key];ax,ay=master['assets']['plants-atlas-dry']['anchors'][key];k=120/418*.68
  encode('Plant_'+key,size(crop('plants-atlas-dry',key),(max(1,round(w*k)),max(1,round(h*k)))),(round((x-ax)*k),round((y-ay)*k)))
if 'TreeCanopy' not in entries:
 e={'name':'TreeCanopy','format':'I4','offset':[0,0]};m['assets'].append(e);entries['TreeCanopy']=e
for stage,name in enumerate(('TreeYoung','TreeGrown','TreeCanopy')):
 im=Image.open(P/f'Tree{stage}.png').convert('RGBA');bounds=im.getchannel('A').getbbox();x,y,x2,y2=bounds
 encode(name,im.crop(bounds),(x-450,y-408))
m['layout']='scene-v3: approved three-stage tree, smaller left planters, integrated moss floor; static far / moving near only'
m['runtime_window_band']=[0,204,480,90]
tail=source[source.index("m['flash_bytes']"):]
tail=tail.replace("m['layout']='scene-v2: floor-rooted objects, icon HUD, no persistent plant labels';m['runtime_window_band']=[0,190,480,90]",'')
tail=tail.replace('export-scene-v2.py','export-scene-v3.py').replace("print('scene-v2:'","print('scene-v3:'")
exec(tail)
