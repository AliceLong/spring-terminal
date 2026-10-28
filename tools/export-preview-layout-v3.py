#!/usr/bin/env python3
"""Compose approved masters for the local preview only; no firmware assets touched."""
from pathlib import Path
import json
from PIL import Image, ImageDraw, ImageFilter

A = Path(__file__).resolve().parents[1] / 'assets/art-v1'
O = A / 'preview-layout-v3'
O.mkdir(exist_ok=True)
m = json.loads((A / 'manifest.json').read_text())
ims = {k: Image.open(A / v['file']).convert('RGBA') for k, v in m['assets'].items()}
def resize(im, wh): return im.resize(wh, Image.Resampling.LANCZOS)
def put(dst, im, xy, wh): dst.alpha_composite(resize(im, wh), xy)
def prop(key):
    x,y,w,h = m['assets']['props-atlas']['regions'][key]
    return ims['props-atlas'].crop((x,y,x+w,y+h))

base = Image.new('RGBA', (480,480), '#263c32')
put(base, ims['landscape-far'], (0,130), (480,240))
put(base, ims['landscape-near'], (0,85), (480,220))
put(base, ims['carriage-shell'], (0,0), (480,480))
put(base, ims['floor-platform'].crop((16,279,2155,471)), (0,324), (480,156))
room = resize(base.crop((0,140,480,380)), (480,480))
far = Image.open(O/'SourceFar.png').convert('RGBA')
near = Image.open(A/'device/Near.png').convert('RGBA')
window = far.crop((0,8,440,216))
# Far view stays still; near foliage is a separate full-window animation layer.
nearloop=Image.new('RGBA',(960,90))
nearloop.alpha_composite(near,(0,0))
nearloop.alpha_composite(near.transpose(Image.Transpose.FLIP_LEFT_RIGHT),(480,0))
nearloop.save(O/'WindowNear.png')
room.alpha_composite(window, (20,72))
room.alpha_composite(Image.open(O/'SourceWindowFrame.png').convert('RGBA'), (0,72))
# Replace all glass contents as one uninterrupted static landscape. The old
# frame/background combination carried a tinted horizontal edge inside glass.
glass_mask=Image.new('L',(480,480));gd=ImageDraw.Draw(glass_mask)
for x,y,w,h in [(56,90,75,204),(151,90,179,204),(353,90,70,204)]:
    gd.rounded_rectangle((x,y,x+w,y+h),radius=9,fill=255)
frame_alpha=Image.open(O/'SourceWindowFrame.png').convert('RGBA').getchannel('A')
lamp_mask=Image.new('L',(480,480))
lamp_mask.paste(frame_alpha.crop((0,0,480,54)).point(lambda a:255 if a>180 else 0),(0,72))
from PIL import ImageChops
glass_mask=ImageChops.subtract(glass_mask,lamp_mask)
landscape=Image.new('RGBA',(480,480));landscape.alpha_composite(far,(0,72))
landscape.putalpha(ImageChops.multiply(landscape.getchannel('A'),glass_mask))
room.alpha_composite(landscape)
put(room, prop('bench'), (3,333), (39,44))
room.save(O/'Room.png')

# Floor generated as an edit of this room, retaining its perspective and palette.
source=resize(Image.open(O/'room-floor-integrated-master.png').convert('RGBA'),(480,480))
grass=Image.new('RGBA',(480,480))
# Transition along the baseboard instead of a rectangular pasted texture edge.
mask=Image.new('L',(480,480))
md=ImageDraw.Draw(mask)
for y in range(374,394):md.line((0,y,480,y),fill=round(255*(y-374)/19))
md.rectangle((0,394,480,480),fill=255)
source.putalpha(mask)
grass.alpha_composite(source)
grass.save(O/'Grass-integrated.png')
box=Image.new('RGBA',(74,49))
put(box,prop('planter'),(0,0),(74,49))
put(box,prop('soil'),(9,4),(56,11))
box.save(O/'Planter.png')
young=ims['tree-atlas'].crop((265,343,685,863))
grown=ims['tree-atlas'].crop((737,12,1763,864))
for i,(im,xy,wh) in enumerate([(young,(407,263),(106,145)),(grown,(265,98),(370,310)),(grown,(112,-112),(630,520))]):
    layer=Image.new('RGBA',(480,480))
    if i==2:
        # Reuse the approved leafy crown to carry growth along the ceiling.
        crown=resize(grown.crop((20,0,1000,430)),(590,225))
        alpha=crown.getchannel('A')
        fade=Image.new('L',crown.size,255)
        fd=ImageDraw.Draw(fade)
        for y in range(160,225): fd.line((0,y,590,y),fill=round(255*(224-y)/64))
        from PIL import ImageChops
        crown.putalpha(ImageChops.multiply(alpha,fade))
        layer.alpha_composite(crown,(-80,-60))
    put(layer,im,xy,wh)
    layer.save(O/f'Tree{i}.png')

# Standalone composition checks (not browser screenshots).
native=json.loads((A/'device/manifest.json').read_text())
entries={e['name']:e for e in native['assets']}
checks=[]
for stage in range(3):
    scene=room.copy()
    pane_mask=Image.new('L',(480,480));pd=ImageDraw.Draw(pane_mask)
    for x,y,w,h in [(56,90,75,204),(151,90,179,204),(353,90,70,204)]:pd.rounded_rectangle((x,y,x+w,y+h),radius=9,fill=255)
    foliage=Image.new('RGBA',(480,480));foliage.alpha_composite(near,(0,204))
    from PIL import ImageChops
    foliage.putalpha(ImageChops.multiply(foliage.getchannel('A'),pane_mask))
    scene.alpha_composite(foliage);scene.alpha_composite(grass)
    scene.alpha_composite(Image.open(O/f'Tree{stage}.png'))
    for x,key in zip((70,151,232),('tracklight','dewmoss','patchvine')):
        scene.alpha_composite(box,(x-37,351))
        name=f'Plant_{key}_'+('seedling' if stage==0 else 'mature')
        e=entries[name];im=Image.open(A/'device'/f'{name}.png').convert('RGBA');k=1.0
        put(scene,im,(round(x+e['offset'][0]*k),round(363+e['offset'][1]*k)),(round(im.width*k),round(im.height*k)))
    e=entries['Robot_idle_0'];scene.alpha_composite(Image.open(A/'device/Robot_idle_0.png').convert('RGBA'),(232+e['offset'][0],440+e['offset'][1]))
    checks.append(scene)
sheet=Image.new('RGB',(1440,480))
for i,im in enumerate(checks):sheet.paste(im,(480*i,0))
sheet.save(O/'layout-check.png')
print('Preview-only layers written to',O)
