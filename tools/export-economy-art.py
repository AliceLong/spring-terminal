#!/usr/bin/env python3
from pathlib import Path
from PIL import Image
# Reuse the v3 encoder without regenerating/repositioning approved base scenery.
source=Path(__file__).with_name('export-scene-v3.py').read_text()
exec(source[:source.index("P=A/'preview-layout-v3'")])
P=A/'economy'
def add(name,im,wh,offset=(0,0)):
 if name not in entries:
  e={'name':name,'format':'I4','offset':list(offset)};m['assets'].append(e);entries[name]=e
 box=im.getchannel('A').point(lambda a:255 if a>24 else 0).getbbox()
 if box:im=im.crop(box)
 # Fit preserves each object's proportions; registered root below is bottom-center.
 k=min(wh[0]/im.width,wh[1]/im.height);im=im.resize((max(1,round(im.width*k)),max(1,round(im.height*k))),Image.Resampling.LANCZOS)
 encode(name,im,(-im.width//2,-im.height) if offset==(-1,-1) else offset)
bean=Image.open(P/'bean-master.png').convert('RGBA')
for i,phase in enumerate(('seedling','growing','mature')):
 w=bean.width/3;add('Plant_travelbean_'+phase,bean.crop((round(i*w),0,round((i+1)*w),bean.height)),(68,30 if i==0 else 65),(-1,-1))
atlas=Image.open(P/'facilities-animals-master.png').convert('RGBA')
for i,name in enumerate(['Facility_tank','Facility_rain','Facility_solar','Facility_battery','Facility_nursery','Facility_bench','Facility_care','Facility_nest','Animal_bird','Animal_mouse','Icon_food','Icon_parts']):
 x=i%4;y=i//4;im=atlas.crop((round(x*atlas.width/4),round(y*atlas.height/3),round((x+1)*atlas.width/4),round((y+1)*atlas.height/3)))
 add(name,im,(24,24) if i>=10 else (31,25) if i>=8 else (50,42))
for key in ('water','energy','fiber'):
 add('Icon_'+key,Image.open(D/('Icon_'+key+'.png')).convert('RGBA'),(24,24))
add('Planter',Image.open(A/'preview-layout-v3/Planter.png').convert('RGBA'),(64,44))
# Remove baked planters: scene objects need to reflow when the fourth slot opens.
bg=Image.open(A/'preview-layout-v3/Room.png').convert('RGBA');bg.alpha_composite(Image.open(A/'preview-layout-v3/Grass-integrated.png').convert('RGBA'));encode('Background',bg)
m['layout']='economy-v1 fixed slots and eight facility overlays'
# Descriptor writer comes from the original shared exporter.
tail=Path(__file__).with_name('export-scene-v2.py').read_text();tail=tail[tail.index("m['flash_bytes']"):].replace("m['layout']='scene-v2: floor-rooted objects, icon HUD, no persistent plant labels';m['runtime_window_band']=[0,190,480,90]",'').replace('export-scene-v2.py','export-economy-art.py')
exec(tail)
