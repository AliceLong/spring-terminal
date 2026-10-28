/* Local art review. No device APIs, game mutation or firmware rendering changes. */
(() => {
  // Inner glass only: fixed train framing remains outside these masks.
  const glass=[[44,88,89,147,7],[153,88,174,147,7],[347,88,87,147,7],[0,103,12,134,3],[468,111,12,134,3]];
  function glassPath(c){c.beginPath();for(const r of glass)c.roundRect(...r);}
  function reflected(c,im,shift,y,w,h){
    const offset=((shift%(2*w))+(2*w))%(2*w);
    for(let i=-1;i<=3;i++){const x=i*w-offset;c.save();if(Math.abs(i%2)===1){c.translate(x+w,y);c.scale(-1,1);c.drawImage(im,0,0,w,h);}else c.drawImage(im,x,y,w,h);c.restore();}
  }
  function composeRoom(c,images,key,time,{near=true,rain=true,native=false}={}){
    c.clearRect(0,0,480,480);c.drawImage(images.room,0,0,480,480);
    c.save();glassPath(c);c.clip();
    reflected(c,images[key+(native?'_far_device':'_far')],time/1000,40,480,240);
    if(near){c.imageSmoothingEnabled=false;reflected(c,images[key+(native?'_near_device':'_near')],time*6/1000,128,480,128);c.imageSmoothingEnabled=true;}
    if(rain&&key==='rain_city'){
      c.strokeStyle='#d5e9ec77';c.lineWidth=.8;c.beginPath();
      for(let i=0;i<24;i++){const x=(i*71+(time/45))%530-20,y=(i*37+time/20)%210+65;c.moveTo(x,y);c.lineTo(x-2,y+8);}c.stroke();
    }
    c.restore();
  }
  function drawScene(c,roomCanvas,images,assets,state,key,time,options={}){
    composeRoom(roomCanvas.getContext('2d'),images,key,time,options);
    const view={...state,tree:{...state.tree,stage:options.stage||0}};
    window.GARDEN_LAYOUT_V2.draw(c,view,{...images,Layout_room:roomCanvas},assets,{showcase:!!options.facilities,stage:options.stage||0,renderRobot:false});
    const x=246,y=292+Math.sin(time/650)*2;c.drawImage(images.robot,x-80,y-72,160,176);
  }
  function drawOutdoor(c,images,key,time,{near=true,native=false}={}){
    c.clearRect(0,0,960,480);
    reflected(c,images[key+(native?'_far_device':'_far')],time*2/1000,0,960,480);
    if(near)reflected(c,images[key+(native?'_near_device':'_near')],time*12/1000,332,960,256);
  }
  window.SCENERY_PREVIEW={composeRoom,drawScene,drawOutdoor,glass,glassPath,reflected};
  if(typeof document==='undefined')return;
  const $=id=>document.getElementById(id),lib=window.WINDOW_REGIONS,images={},assets=Object.fromEntries(window.DEVICE_ART.assets.map(a=>[a.name,a]));
  const room=document.createElement('canvas');room.width=room.height=480;
  lib.regions.forEach(r=>{const o=document.createElement('option');o.value=r.key;o.textContent=r.name+(r.new?' · 新增':' · 现有');$('region').appendChild(o);if(r.new){const a=document.createElement('a');a.href=r.far;const im=document.createElement('img');im.src=r.far;im.alt=r.name;const p=document.createElement('p');p.textContent=r.name;a.append(im,p);$('cards').appendChild(a);}});
  $('region').value='wasteland';let paused=false,elapsed=0,last=null,ready=false;
  function copy(){const r=lib.regions.find(r=>r.key===$('region').value);$('title').textContent=r.name;$('description').textContent=r.description;$('far-image').src=r.far;$('near-image').src=r.near;}
  function draw(){if(!ready)return;const key=$('region').value,options={near:$('near').checked,rain:$('rain').checked,native:$('native').checked,facilities:$('facilities').checked,stage:Number($('stage').value)};
    const state=window.ECONOMY_REPLAY[0].state;
    drawScene($('scene').getContext('2d'),room,images,assets,state,key,elapsed,options);
    drawOutdoor($('outdoor').getContext('2d'),images,key,elapsed,options);
    $('status').textContent=`${lib.regions.find(r=>r.key===key).name} · ${paused?'已暂停':'远近景视差移动'} · ${options.native?'RGB565 / I4设备编码候选':'原PNG颜色'} · 仅本地预览`;
    $('budget').textContent=`每地区编码候选约${(lib.per_region_device_bytes/1024).toFixed(1)}KiB；四套新增合计约${(lib.new_device_bytes/1024/1024).toFixed(2)}MiB。尚未写入固件，接入前需另做Flash与刷新预算。`;
  }
  $('region').onchange=()=>{copy();draw()};$('pause').onclick=()=>{paused=!paused;$('pause').textContent=paused?'继续移动':'暂停移动'};
  for(const id of ['near','rain','native','stage','facilities'])$(id).onchange=draw;
  const list=[...window.DEVICE_ART.assets.map(a=>[a.name,'../economy/layout-v3/preview-device/'+a.name+'.png']),...Object.entries(window.GARDEN_LAYOUT_V2.paths).map(([n,p])=>['Layout_'+n,'../'+p]),['room','../economy/layout-v3/room.png'],['robot','../robot-hover-v1/frames/hover_0-v3.png']];
  for(const r of lib.regions)list.push([r.key+'_far',r.far],[r.key+'_near',r.near],[r.key+'_far_device',r.firmware_far_preview||r.far_device_preview],[r.key+'_near_device',r.firmware_near_preview||r.near_device_preview]);
  Promise.all(list.map(([name,src])=>new Promise((resolve,reject)=>{const im=new Image();im.onload=()=>{images[name]=im;resolve()};im.onerror=()=>reject(new Error(src));im.src=src}))).then(()=>{ready=true;copy();function tick(now){if(last!==null&&!paused)elapsed+=Math.min(100,now-last);last=now;draw();requestAnimationFrame(tick)}requestAnimationFrame(tick)}).catch(e=>{$('status').textContent='素材载入失败：'+e.message});
})();
