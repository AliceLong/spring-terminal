/* Offline asset preview only: no device, microphone, network API or world mutations. */
(() => {
  const copy={watch_window:['看窗外','朝窗外的侧后方姿态，嫩芽轻摆，机械肢收拢。单轮2.6秒，可在20秒观察阶段安静循环。'],inspect_plant:['检查植物','低头、空机械夹爪轻探、眼睛专注与屏内扫描。单轮2秒，不拿水壶，不摘叶，不产生资源。']};
  function drawFrame(c,detail,reference,images,assets,state,key,index,{native=false,float=0}={}) {
    window.GARDEN_LAYOUT_V2.draw(c,state,images,assets,{showcase:true,stage:1,renderRobot:false});
    const x=key==='watch_window'?282:204,y=(key==='watch_window'?254:319)+float;
    if(native){const a=window.LIFE_ROBOT.device_assets.find(a=>a.name===`Life_${key}_${index}`);c.drawImage(images[a.name],x+a.offset[0],y+a.offset[1]);}
    else c.drawImage(images[`${key}_${index}`],x-80,y-72,160,176);
    detail.clearRect(0,0,320,352);
    if(native){const a=window.LIFE_ROBOT.device_assets.find(a=>a.name===`Life_${key}_${index}`);detail.drawImage(images[a.name],(80+a.offset[0])*2,(72+a.offset[1])*2,a.width*2,a.height*2);}
    else detail.drawImage(images[`${key}_${index}`],0,0,320,352);
    reference.clearRect(0,0,320,352);reference.drawImage(images.reference,0,0,320,352);
  }
  window.LIFE_PREVIEW={drawFrame,copy};
  if(typeof document==='undefined')return;
  const $=id=>document.getElementById(id),lib=window.LIFE_ROBOT;
  const assets=Object.fromEntries(window.DEVICE_ART.assets.map(a=>[a.name,a])),images={};
  let paused=false,elapsed=0,last=null,ready=false;
  function updateCopy(){const key=$('action').value;elapsed=0;$('title').textContent=copy[key][0];$('description').textContent=copy[key][1];$('frames').replaceChildren();lib.animations[key].frames.forEach((src,i)=>{const box=document.createElement('div');box.className='frame';const im=document.createElement('img');im.src=src;im.alt=`${copy[key][0]}第${i+1}帧`;const text=document.createElement('small');text.textContent=`第${i+1}帧`;box.append(im,text);$('frames').appendChild(box)});}
  $('action').onchange=()=>{updateCopy();draw()};
  $('pause').onclick=()=>{paused=!paused;$('pause').textContent=paused?'继续':'暂停'};
  $('next').onclick=()=>{paused=true;$('pause').textContent='继续';elapsed=(Math.floor(elapsed/lib.animations[$('action').value].frame_ms)+1)*lib.animations[$('action').value].frame_ms;draw()};
  $('native').onchange=draw;$('float').onchange=draw;
  function draw(){if(!ready)return;const key=$('action').value,config=lib.animations[key],index=Math.floor(elapsed/config.frame_ms)%4;
    drawFrame($('scene').getContext('2d'),$('detail').getContext('2d'),$('reference').getContext('2d'),images,assets,window.ECONOMY_REPLAY[0].state,key,index,{native:$('native').checked,float:$('float').checked?Math.sin(elapsed/650)*2:0});
    Array.from($('frames').children).forEach((e,i)=>e.classList.toggle('active',i===index));
    $('status').textContent=`${copy[key][0]} · 第${index+1}/4帧 · ${paused?'已暂停':'循环播放'} · ${$('native').checked?'I4设备编码':'透明PNG原帧'}`;
    $('dimensions').textContent=`8张独立帧，PNG合计${(lib.png_bytes/1024).toFixed(1)} KiB；独立设备I4包${(lib.device_bytes/1024).toFixed(1)} KiB（尚未加入固件）。`;
  }
  const list=[...window.DEVICE_ART.assets.map(a=>({name:a.name,src:'../economy/layout-v3/preview-device/'+a.name+'.png'})),...Object.entries(window.GARDEN_LAYOUT_V2.paths).map(([name,src])=>({name:'Layout_'+name,src:'../'+src})),...lib.frames.map(f=>({name:f.name,src:f.file})),...lib.device_assets.map(a=>({name:a.name,src:a.preview})),{name:'reference',src:'../robot-hover-v1/frames/hover_0-v3.png'}];
  Promise.all(list.map(a=>new Promise((resolve,reject)=>{const im=new Image();im.onload=()=>{images[a.name]=im;resolve()};im.onerror=()=>reject(new Error(a.src));im.src=a.src}))).then(()=>{ready=true;updateCopy();function tick(now){if(last!==null&&!paused)elapsed+=Math.min(100,now-last)*Number($('speed').value);last=now;draw();requestAnimationFrame(tick)}requestAnimationFrame(tick)}).catch(e=>{$('status').textContent='素材载入失败：'+e.message});
})();
