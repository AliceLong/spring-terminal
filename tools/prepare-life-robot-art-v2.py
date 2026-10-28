#!/usr/bin/env python3
"""Deterministically slice/register generated life animation sheets; no firmware edits."""
from pathlib import Path
import hashlib,json
import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'assets/art-v1/robot-life-v2'

def principal(mask):
    labels,n=ndimage.label(mask)
    assert n
    counts=np.bincount(labels.ravel());counts[0]=0
    return labels==np.argmax(counts)

def face(im):
    a=np.asarray(im).astype(np.int16)
    dark=(a[:,:,3]>180)&(a[:,:,:3].max(2)<100)&(a[:,:,2]>a[:,:,0]+2)&(a[:,:,1]>=a[:,:,0])
    mask=principal(dark);y,x=np.where(mask)
    return [int(x.min()),int(y.min()),int(x.max()+1),int(y.max()+1)]

def shell(im):
    a=np.asarray(im).astype(np.int16)
    orange=(a[:,:,3]>180)&(a[:,:,0]>110)&(a[:,:,1]>60)&(a[:,:,0]>a[:,:,1]*1.12)&(a[:,:,1]>a[:,:,2]*1.12)
    mask=principal(ndimage.binary_closing(orange,iterations=2))
    y,x=np.where(mask)
    return [int(x.min()),int(y.min()),int(x.max()+1),int(y.max()+1)]
ref=Image.open(ROOT/'assets/art-v1/robot-hover-v1/frames/hover_0-v3.png').convert('RGBA')
r=shell(ref);reference_width=r[2]-r[0]
reference_anchor=((r[0]+r[2])/2,r[1]+reference_width*.5)
reference_face=face(ref)
reference_face_width=reference_face[2]-reference_face[0]
rest_reference=Image.open(ROOT/'assets/art-v1/robot-hover-v1/frames/rest_0-v3.png').convert('RGBA')
rest_baseline=rest_reference.getchannel('A').point(lambda a:255 if a>24 else 0).getbbox()[3]
m={'version':2,'generator':'built-in image_gen','frame_size':[160,176],'anchor':[80,72],
   'reference':'../robot-hover-v1/frames/hover_0-v3.png','reference_shell_width':reference_width,
   'registration':'One scale per animation; new groups use face-center registration to original hover, existing groups retain v1 shell calibration; rest_tree feet aligned to original rest baseline' ,
   'new_frame_count':16,'new_groups':['care','harvest','inspect_left','rest_tree'],'animations':{},'frames':[],'device_status':'PNG assets and local preview only; no firmware or save changes'}
for action,source,title,timing in [('care','care.png','Plant care',550),('harvest','harvest.png','Harvest',400),('inspect_left','inspect-left.png','Inspect left',500),('rest_tree','rest-tree.png','Rest under tree',900),('watch_window','watch-window.png','Watch window',650),('inspect_plant','inspect-plant.png','Inspect right',500)]:
    im=Image.open(OUT/'masters'/source).convert('RGBA');prepared=[]
    for i in range(4):
        col,row=i%2,i//2
        cell=im.crop((round(col*im.width/2),round(row*im.height/2),round((col+1)*im.width/2),round((row+1)*im.height/2)))
        a=np.array(cell)
        keep=ndimage.binary_dilation(principal(a[:,:,3]>8),iterations=2)
        a[~keep]=0
        cell=Image.fromarray(a);box=face(cell) if action in m['new_groups'] else shell(cell)
        prepared.append((cell,box))
    widths=[b[2]-b[0] for _,b in prepared]
    assert max(widths)/min(widths)<1.10,(action,widths)
    scale=(reference_face_width if action in m['new_groups'] else reference_width)/float(np.median(widths))
    m['animations'][action]={'title':title,'frame_ms':timing,'loop':action!='harvest','frames':[],'source':source,'scale':scale}
    images=[]
    for i,(cell,box) in enumerate(prepared):
        width=box[2]-box[0]
        anchor=((box[0]+box[2])/2,(box[1]+box[3])/2) if action in m['new_groups'] else ((box[0]+box[2])/2,box[1]+width*.5)
        size=(round(cell.width*scale),round(cell.height*scale))
        resized=cell.resize(size,Image.Resampling.LANCZOS)
        frame=Image.new('RGBA',(160,176))
        target=(80,72) if action in m['new_groups'] else reference_anchor
        offset=(round(target[0]-anchor[0]*scale),round(target[1]-anchor[1]*scale))
        if action=='rest_tree':
            foot=resized.getchannel('A').point(lambda a:255 if a>24 else 0).getbbox()[3]
            offset=(offset[0],rest_baseline-foot)
        frame.alpha_composite(resized,offset)
        bounds=frame.getchannel('A').point(lambda p:255 if p>24 else 0).getbbox()
        assert bounds and bounds[0]>1 and bounds[1]>1 and bounds[2]<159 and bounds[3]<175,(action,i,bounds)
        name=f'{action}_{i}';file=f'frames/{name}.png';frame.save(OUT/file,optimize=True)
        data=(OUT/file).read_bytes()
        m['animations'][action]['frames'].append(file)
        m['frames'].append({'name':name,'file':file,'source_cell':[i%2,i//2],'source_anchor_box':box,'calibration':'face' if action in m['new_groups'] else 'shell',
                            'registered_width':round(width*scale,3),'scale':scale,'bounds':list(bounds),
                            'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
        images.append(frame)
    images[0].save(OUT/f'{action}.webp',save_all=True,append_images=images[1:],duration=timing,loop=1 if action=='harvest' else 0,lossless=True)
m['reference_face_width']=reference_face_width
m['rest_baseline']=rest_baseline
m['frame_count']=len(m['frames']);m['png_bytes']=sum(f['bytes'] for f in m['frames'])
# Standalone device-format pack, intentionally not added to firmware asset tables.
(OUT/'device').mkdir(exist_ok=True)
m['device_assets']=[]
for f in m['frames']:
    im=Image.open(OUT/f['file']).convert('RGBA')
    # Use the current runtime's .98 scale and alpha crop convention.
    k=.98
    im=im.resize((round(im.width*k),round(im.height*k)),Image.Resampling.LANCZOS)
    bounds=im.getchannel('A').point(lambda a:255 if a>96 else 0).getbbox()
    x,y,x2,y2=bounds;im=im.crop(bounds)
    q=im.quantize(colors=16,method=Image.Quantize.FASTOCTREE,dither=Image.Dither.NONE)
    preview=q.convert('RGBA');idx=np.asarray(q,dtype=np.uint8);rgba=np.asarray(preview);palette=bytearray(64)
    for index in np.unique(idx):
        py,px=np.argwhere(idx==index)[0];r,g,b,a=map(int,rgba[py,px]);palette[int(index)*4:int(index)*4+4]=bytes((b,g,r,a))
    stride=((im.width+1)//2+3)//4*4
    packed=np.zeros((im.height,stride),dtype=np.uint8)
    packed[:,:(im.width+1)//2]=np.pad(idx,((0,0),(0,im.width%2)))[:,::2]<<4
    packed[:,:im.width//2]|=idx[:,1::2]
    raw=bytes(palette)+packed.tobytes();name='Life_'+f['name'];file='device/'+name+'.bin'
    (OUT/file).write_bytes(raw);preview.save(OUT/('device/'+name+'.png'))
    # Decode actual packed nibbles and BGRA palette to verify the exported bytes.
    colors=np.frombuffer(raw[:64],dtype=np.uint8).reshape(16,4)[:,[2,1,0,3]]
    rows=np.frombuffer(raw[64:],dtype=np.uint8).reshape(im.height,stride)
    decoded=np.empty((im.height,im.width),dtype=np.uint8)
    decoded[:,::2]=rows[:,:(im.width+1)//2]>>4
    decoded[:,1::2]=rows[:,:im.width//2]&15
    assert np.array_equal(colors[decoded],rgba)
    m['device_assets'].append({'name':name,'format':'I4','width':im.width,'height':im.height,
        'stride':stride,'offset':[x-round(80*k),y-round(72*k)],'bytes':len(raw),'file':file,
        'preview':'device/'+name+'.png','sha256':hashlib.sha256(raw).hexdigest()})
m['device_bytes']=sum(a['bytes'] for a in m['device_assets'])
(OUT/'manifest.json').write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n')
(OUT/'catalog.js').write_text('window.LIFE_ROBOT = '+json.dumps(m,ensure_ascii=False)+';\n')
contact=Image.new('RGB',(1280,400*len(m['animations'])),'#203b32');d=ImageDraw.Draw(contact)
for row,(action,config) in enumerate(m['animations'].items()):
    d.text((24,row*400+12),config['title']+' - fixed scale / 4 frames',fill='#f7ebc9')
    for i,file in enumerate(config['frames']):
        frame=Image.open(OUT/file).resize((320,352),Image.Resampling.LANCZOS)
        contact.paste(frame,(i*320,row*400+36),frame)
        d.text((i*320+20,row*400+378),f'Frame {i+1}',fill='#f7ebc9')
contact.save(OUT/'contact-sheet.png')
contact.crop((0,0,1280,1600)).save(OUT/'new-actions-contact-sheet.png')
print(json.dumps({'frames':m['frame_count'],'png_bytes':m['png_bytes'],'reference_shell_width':reference_width,'scale_check':{k:[f['registered_width'] for f in m['frames'] if f['name'].startswith(k)] for k in m['animations']}},indent=2))
