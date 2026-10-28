#!/usr/bin/env python3
"""Package reviewed local layers; independent facilities remain separate device sprites."""
from pathlib import Path
from PIL import Image
source=Path(__file__).with_name('export-scene-v2.py').read_text()
exec(source[:source.index('base=Image.new')])
# Preserve painterly color in large furniture and trees with an indexed 256-color palette.
base_encode=encode
def encode(name,im,offset=None):
 if name=='Background' or im.width*im.height<5000 or name.startswith(('Robot_','Plant_')):
  return base_encode(name,im,offset)
 e=entries[name];im=im.convert('RGBA');q=im.quantize(colors=256,method=Image.Quantize.FASTOCTREE,dither=Image.Dither.NONE)
 preview=q.convert('RGBA');idx=np.asarray(q,dtype=np.uint8);rgba=np.asarray(preview);palette=bytearray(1024)
 for index in np.unique(idx):
  y,x=np.argwhere(idx==index)[0];r,g,b,a=map(int,rgba[y,x]);palette[int(index)*4:int(index)*4+4]=bytes((b,g,r,a))
 stride=(im.width+3)//4*4;p=np.zeros((im.height,stride),dtype=np.uint8);p[:,:im.width]=idx;raw=bytes(palette)+p.tobytes()
 e.update(format='I8',width=im.width,height=im.height,stride=stride,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest(),decoded_cache_bytes=im.width*im.height*4)
 if offset is not None:e['offset']=list(offset)
 (D/(name+'.bin')).write_bytes(raw);preview.save(D/(name+'.png'))
P=A/'economy/layout-v3';S=A/'economy/layout-v3/preview-device'
def original(name):return Image.open(S/(name+'.png')).convert('RGBA')
def output(name,im,wh=None,offset=(0,0)):
 if name not in entries:
  e={'name':name,'format':'I4','offset':[0,0]};m['assets'].append(e);entries[name]=e
 if wh:im=size(im,wh)
 entries[name]['format']='RGB565' if name=='Background' else 'I4'
 encode(name,im,offset)
room=size(Image.open(P/'room.png').convert('RGBA'),(480,480))
room.alpha_composite(size(Image.open(P/'equipment-split/rack-empty.png').convert('RGBA'),(147,229)),(-7,129))
output('Background',room)
# Retain window animation but restrict it to the new clear center glass.
output('Planter',Image.open(P/'planter.png').convert('RGBA'),(102,62))
for e in list(m['assets']):
 name=e['name']
 if name.startswith('Plant_') or name.startswith('Robot_'):
  im=original(name);k=.88 if name.startswith('Plant_') else .92
  old=next(x for x in json.loads((S/'manifest.json').read_text())['assets'] if x['name']==name)
  output(name,im,(max(1,round(im.width*k)),max(1,round(im.height*k))),tuple(round(v*k) for v in old['offset']))
# Tree silhouettes match the reviewed stage positions around the same root anchor.
im=original('TreeYoung');old=next(x for x in json.loads((S/'manifest.json').read_text())['assets'] if x['name']=='TreeYoung')
output('TreeYoung',im,(round(im.width*1.18),round(im.height*1.18)),tuple(round(v*1.18) for v in old['offset']))
tree=Image.open(A/'economy/layout-v2/tree.png').convert('RGBA')
output('TreeGrown',tree,(226,274),(307-451,72-346))
output('TreeCanopy',tree,(300,362),(252-451,-16-346))
files={'tank':(A/'economy/layout-v2/pool.png',(161,121)), 'rain':(P/'barrel.png',(95,139)),
 'solar':(P/'equipment-split/solar.png',(103,86)), 'battery':(P/'equipment-split/battery.png',(78,64)),
 'bench':(P/'equipment-split/bench.png',(100,68)), 'nest':(A/'economy/layout-v2/bird-home.png',(106,73))}
for key,(path,wh) in files.items():output('Facility_'+key,Image.open(path).convert('RGBA'),wh)
output('TreeHome',Image.open(A/'economy/layout-v2/animal-home.png').convert('RGBA'),(85,78))
for key,k in [('bird',1.15),('mouse',1.05)]:
 im=original('Animal_'+key);output('Animal_'+key,im,(round(im.width*k),round(im.height*k)))
for key in ['tank','rain','solar','battery','nursery','bench','care','nest']:
 im=Image.open(D/('Facility_'+key+'.png')).convert('RGBA');k=min(48/im.width,40/im.height)
 output('Thumb_'+key,im,(max(1,round(im.width*k)),max(1,round(im.height*k))))
m['layout']='split-carriage: fixed empty shelf, independent solar/battery/bench; foreground wooden planters and right tree homes/pool'
m['image_count']=len(m['assets']);m['runtime_window_band']=[0,158,480,90]
tail=source[source.index("m['flash_bytes']"):].replace("m['layout']='scene-v2: floor-rooted objects, icon HUD, no persistent plant labels';m['runtime_window_band']=[0,190,480,90]",'').replace('export-scene-v2.py','export-split-carriage-art.py')
exec(tail)
