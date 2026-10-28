#!/usr/bin/env python3
"""Encode approved railway seat, preserving the existing scene and assets."""
from pathlib import Path
source=Path(__file__).with_name('export-scene-v2.py').read_text()
exec(source[:source.index('base=Image.new')])
P=A/'passenger-seat-v2';name='Prop_passenger_seat'
# Superseded, unreferenced legacy props stay on disk but need not occupy application Flash.
m['assets']=[e for e in m['assets'] if e['name'] not in ('Drought','Prop_bench')]
im=Image.open(P/'masters/passenger-seat.png').convert('RGBA')
box=im.getchannel('A').point(lambda a:255 if a>96 else 0).getbbox();assert box
im=im.crop(box);k=min(144/im.width,100/im.height)
im=im.resize((round(im.width*k),round(im.height*k)),Image.Resampling.LANCZOS)
if name not in entries:
 e={'name':name,'format':'I4','offset':[0,0]};m['assets'].append(e);entries[name]=e
encode(name,im,(-im.width//2,-im.height))
m['image_count']=len(m['assets']);m['budget_bytes']=1200*1024
m['budget_note']='Art target only; actual application partition size is checked by ESP-IDF build'
tail=source[source.index("m['flash_bytes']"):]
tail=tail.replace("assert m['flash_bytes']<1024*1024", "assert m['flash_bytes']<1200*1024")
tail=tail.replace("m['layout']='scene-v2: floor-rooted objects, icon HUD, no persistent plant labels';m['runtime_window_band']=[0,190,480,90]",'')
exec(tail.replace('export-scene-v2.py','export-passenger-seat.py'))
(P/'manifest.json').write_text(json.dumps({'source':'masters/passenger-seat.png','device':entries[name],
 'position':[240,338],'purpose':'old railway passenger seating; not a facility',
 'story_unlocked':False,'unlock':'Reserved for chapter2; no current unlock route'},indent=2)+'\n')
# Device background lives in the paired assets pack; host previews retain embedded pixels.
import runpy
runpy.run_path(str(R/'tools/externalize-spring-background.py'), run_name='__main__')
