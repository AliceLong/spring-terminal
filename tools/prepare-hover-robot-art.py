#!/usr/bin/env python3
"""Slice atlases; register on blue-black face; share ONE scale per animation."""
from pathlib import Path
import json,hashlib
import numpy as np
from PIL import Image
from scipy import ndimage
R=Path(__file__).resolve().parents[1];P=R/'assets/art-v1/robot-hover-v1'
sheets={'hover-flight-rest':['hover','takeoff','rest'],'garden-work':['tend','water','build'],'voice-communication':['listen','wait','speak'],'imu-reactions':['recover','squint','wave']}
manifest={'version':3,'frame_size':[160,176],'anchor':[80,72],'registration':'blue-black face center; head-shell width normalized to56px; fixed scale per animation','generator':'built-in image_gen','animations':{},'frames':[]}
for sheet,rows in sheets.items():
 im=Image.open(P/'masters'/f'{sheet}.png').convert('RGBA');w,h=im.size
 for row,action in enumerate(rows):
  manifest['animations'][action]=[];prepared=[]
  for col in range(4):
   cell=im.crop((round(col*w/4),round(row*h/3),round((col+1)*w/4),round((row+1)*h/3)))
   a=np.asarray(cell).astype(np.int16);yy,xx=np.indices(a.shape[:2])
   # Mechanical joints are dark brown; only the face has the blue-black material.
   dark=(a[:,:,3]>180)&(a[:,:,:3].max(2)<90)&(a[:,:,2]>a[:,:,0]+2)&(a[:,:,1]>=a[:,:,0])&(xx>cell.width*.35)&(yy>cell.height*.25)&(yy<cell.height*.80)
   labels,n=ndimage.label(dark);candidates=[]
   for i,b in enumerate(ndimage.find_objects(labels)):
    if b is None:continue
    ys,xs=b;hh=ys.stop-ys.start;ww=xs.stop-xs.start;area=int((labels[b]==i+1).sum())
    if area>120 and hh>25 and .25<ww/hh<1.5:candidates.append((area,b))
   assert candidates,(sheet,row,col,'face panel not found')
   _,b=max(candidates,key=lambda p:p[0]);ys,xs=b
   face=[xs.start,ys.start,xs.stop,ys.stop]
   cx=(face[0]+face[2])/2;cy=(face[1]+face[3])/2;fh=face[3]-face[1]
   shell=(a[:,:,3]>180)&(a[:,:,0]>110)&(a[:,:,1]>65)&(a[:,:,0]>a[:,:,1]*1.08)&(a[:,:,1]>a[:,:,2]*1.12)&(yy>cy-fh*.45)&(yy<cy+fh*.45)&(xx>cx-fh*3)&(xx<cx+fh*.8)
   shell_x=xx[shell];assert len(shell_x)>100
   head_width=float(np.percentile(shell_x,99)-np.percentile(shell_x,1))
   prepared.append((cell,face,head_width))
  heights=[face[3]-face[1] for _,face,_ in prepared]
  assert max(heights)/min(heights)<1.25,(action,'inconsistent face detection',heights)
  k=56/float(np.median([head for _,_,head in prepared]))
  for col,(cell,face,head_width) in enumerate(prepared):
   # Remove only disconnected grid-boundary residue from a neighboring cell;
   # retain all central effects such as detached water droplets and tool sparks.
   a=np.array(cell);labels,n=ndimage.label(a[:,:,3]>24);counts=np.bincount(labels.ravel());largest=counts[1:].max()
   for i,b in enumerate(ndimage.find_objects(labels)):
    if b is None:continue
    ys,xs=b
    if counts[i+1]<largest*.08 and (ys.start<3 or xs.start<2 or xs.stop>cell.width-2):a[labels==i+1]=0
   cell=Image.fromarray(a);anchor=((face[0]+face[2])/2,(face[1]+face[3])/2)
   resized=cell.resize((round(cell.width*k),round(cell.height*k)),Image.Resampling.LANCZOS);offset=(round(80-anchor[0]*k),round(72-anchor[1]*k))
   frame=Image.new('RGBA',(160,176));frame.alpha_composite(resized,offset);bounds=frame.getchannel('A').point(lambda a:255 if a>24 else 0).getbbox();assert bounds and bounds[0]>0 and bounds[2]<160 and bounds[1]>0 and bounds[3]<176,(sheet,row,col,bounds)
   name=f'{action}_{col}';file=f'frames/{name}-v3.png';frame.save(P/file,optimize=True);payload=(P/file).read_bytes()
   manifest['animations'][action].append(name);manifest['frames'].append({'name':name,'file':file,'source':sheet,'cell':[col,row],'source_face':face,'source_head_width':head_width,'scale':k,'bounds':list(bounds),'bytes':len(payload),'sha256':hashlib.sha256(payload).hexdigest()})
manifest['frame_count']=len(manifest['frames']);manifest['png_bytes']=sum(f['bytes'] for f in manifest['frames']);manifest['device_note']='Not encoded or flashed; PNG master bytes are not device Flash or PSRAM usage.'
(P/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n');(P/'catalog.js').write_text('window.HOVER_ROBOT = '+json.dumps(manifest,ensure_ascii=False)+';\n');print('Prepared',manifest['frame_count'],'registered frames, PNG bytes',manifest['png_bytes'])
