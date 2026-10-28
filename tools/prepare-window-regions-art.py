#!/usr/bin/env python3
"""Export generated window layers and standalone device-format candidates. No firmware edits."""
from pathlib import Path
import hashlib,json
from PIL import Image,ImageDraw
import numpy as np
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'assets/art-v1/window-regions-v1'
(OUT/'device').mkdir(exist_ok=True);(OUT/'layers').mkdir(exist_ok=True)
specs=[('plains','晴朗平原','Open plain','现有场景，拱桥遗迹与开阔绿地'),('wasteland','干旱荒原','Dry wasteland','断桥、锈蚀输电塔、废弃集能板与耐旱植物'),('rain_city','雨中旧城','Rainy old city','空楼、旧车站和高架桥，藤蔓与雨雾交织'),('tunnel','废弃隧道','Abandoned tunnel','旧廊道、信号与管线，苔藓蕨类从破口长入'),('forest','繁茂森林','Reclaimed forest','站台雨棚与栈桥，被树根、藤蔓和花草包围')]
m={'version':1,'generator':'built-in image_gen','new_regions':4,'new_masters':8,'regions':[],
   'device_status':'Standalone candidates only; not added to firmware or flashed',
   'repeat':'Reflected-repeat scrolling avoids demanding identical generated image edge pixels',
   'far_size':[480,240],'near_size':[480,128]}
for i,(key,name,title,desc) in enumerate(specs):
 base=ROOT/'assets/art-v1/masters' if key=='plains' else OUT/'masters'
 far_src=base/('landscape-far.png' if key=='plains' else key+'-far.png')
 near_src=base/('landscape-near.png' if key=='plains' else key+'-near.png')
 far=Image.open(far_src).convert('RGB').resize((480,240),Image.Resampling.LANCZOS)
 near_master=Image.open(near_src).convert('RGBA')
 assert near_master.getchannel('A').getextrema()==(0,255)
 # The alpha in the unpainted upper half is retained; crop common lower strip.
 near=near_master.crop((0,round(near_master.height*.48),near_master.width,near_master.height)).resize((480,128),Image.Resampling.LANCZOS)
 far_file=f'layers/{key}-far.png';near_file=f'layers/{key}-near.png'
 far.save(OUT/far_file,optimize=True);near.save(OUT/near_file,optimize=True)
 a=np.asarray(far,dtype=np.uint16);pixels=(((a[:,:,0]>>3)<<11)|((a[:,:,1]>>2)<<5)|(a[:,:,2]>>3)).astype('<u2')
 far_raw=pixels.tobytes();far_bin=f'device/{key}-far-rgb565.bin';(OUT/far_bin).write_bytes(far_raw)
 decoded=np.stack((((pixels>>11)&31)<<3,((pixels>>5)&63)<<2,(pixels&31)<<3),axis=-1).astype(np.uint8)
 far_device=f'device/{key}-far-rgb565.png';Image.fromarray(decoded).save(OUT/far_device)
 assert len(far_raw)==480*240*2
 q=near.quantize(colors=16,method=Image.Quantize.FASTOCTREE,dither=Image.Dither.NONE);idx=np.asarray(q,dtype=np.uint8);rgba=np.asarray(q.convert('RGBA'));pal=bytearray(64)
 for j in np.unique(idx):
  y,x=np.argwhere(idx==j)[0];r,g,b,alpha=map(int,rgba[y,x]);pal[int(j)*4:int(j)*4+4]=bytes((b,g,r,alpha))
 stride=240;packed=(idx[:,::2]<<4)|idx[:,1::2];near_raw=bytes(pal)+packed.tobytes();near_bin=f'device/{key}-near-i4.bin';(OUT/near_bin).write_bytes(near_raw)
 palette=np.frombuffer(near_raw[:64],np.uint8).reshape(16,4)[:,[2,1,0,3]]
 indices=np.empty((128,480),dtype=np.uint8);indices[:,::2]=packed>>4;indices[:,1::2]=packed&15
 assert np.array_equal(palette[indices],rgba)
 near_device=f'device/{key}-near-i4.png';Image.fromarray(rgba).save(OUT/near_device)
 def meta(file,fmt,w,h,stride):
  data=(OUT/file).read_bytes();return {'file':file,'format':fmt,'width':w,'height':h,'stride':stride,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
 m['regions'].append({'id':i,'key':key,'name':name,'title':title,'description':desc,'new':key!='plains','far':far_file,'near':near_file,'far_master':str(far_src.relative_to(ROOT)),'near_master':str(near_src.relative_to(ROOT)),'device_far':meta(far_bin,'RGB565',480,240,960),'device_near':meta(near_bin,'I4',480,128,240),'far_device_preview':far_device,'near_device_preview':near_device})
m['new_device_bytes']=sum(r['device_far']['bytes']+r['device_near']['bytes'] for r in m['regions'] if r['new'])
m['per_region_device_bytes']=230400+64+480*128//2
(OUT/'manifest.json').write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n');(OUT/'catalog.js').write_text('window.WINDOW_REGIONS = '+json.dumps(m,ensure_ascii=False)+';\n')
contact=Image.new('RGB',(960,560),'#203b32');d=ImageDraw.Draw(contact)
for j,r in enumerate(m['regions'][1:]):
 im=Image.open(OUT/r['far']);near=Image.open(OUT/r['near']);im.paste(near,(0,166),near)
 x=j%2*480;y=j//2*280;contact.paste(im,(x,y+28));d.text((x+16,y+8),r['title'],fill='#f5ebd0')
contact.save(OUT/'overview.png')
print(json.dumps({'new_regions':4,'masters':8,'per_region_device_bytes':m['per_region_device_bytes'],'new_device_bytes':m['new_device_bytes']},indent=2))
