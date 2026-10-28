/* Offline asset preview only: no device, microphone, network API or world mutations. */
(() => {
  const copy={care:['日常照顾','空夹爪轻托、整理和放回，不拿产物。与收获组并排比较。'],harvest:['收获','伸手、取得叶片、带回、空手放松。预览重复演示；实际收获应只执行一次，不能按动画重复发放资源。'],inspect_left:['检查植物 · 朝左','重新绘制的朝左观察姿态，眼部扫描，空夹爪轻探；机身01保持正向。右侧对照朝右版本。'],rest_tree:['树下休息','轻靠树干、机械肢放松、困倦与闭眼。脚底统一落地点；树干和阴影由场景独立绘制。'],watch_window:['看窗外','朝窗外的侧后方姿态，嫩芽轻摆，机械肢收拢。单轮2.6秒，可在20秒观察阶段安静循环。'],inspect_plant:['检查植物','低头、空机械夹爪轻探、眼睛专注与屏内扫描。单轮2秒，不拿水壶，不摘叶，不产生资源。']};
  function drawFrame(c,detail,reference,images,assets,state,key,index,{native=false,float=0}={}) {
    window.GARDEN_LAYOUT_V2.draw(c,state,images,assets,{showcase:true,stage:key==='rest_tree'?2:1,renderRobot:false});
    const x=key==='watch_window'?282:key==='inspect_left'?264:key==='rest_tree'?425:204;
    const y=(key==='watch_window'?254:key==='rest_tree'?382:319)+(key==='rest_tree'?0:float);
    if(key==='rest_tree'){c.save();c.fillStyle='#203b3244';c.beginPath();c.ellipse(x-10,y+35,30,4,0,0,Math.PI*2);c.fill();c.restore();}
    if(native){const a=window.LIFE_ROBOT.device_assets.find(a=>a.name===`Life_${key}_${index}`);c.drawImage(images[a.name],x+a.offset[0],y+a.offset[1]);}
    else c.drawImage(images[`${key}_${index}`],x-80,y-72,160,176);
    detail.clearRect(0,0,320,352);
    if(native){const a=window.LIFE_ROBOT.device_assets.find(a=>a.name===`Life_${key}_${index}`);detail.drawImage(images[a.name],(80+a.offset[0])*2,(72+a.offset[1])*2,a.width*2,a.height*2);}
    else detail.drawImage(images[`${key}_${index}`],0,0,320,352);
    reference.clearRect(0,0,320,352);const other=key==='care'?images[`harvest_${index}`]:key==='harvest'?images[`care_${index}`]:key==='inspect_left'?images[`inspect_plant_${index}`]:key==='rest_tree'?images.reference_rest:images.reference;reference.drawImage(other,0,0,320,352);
  }
  window.LIFE_PREVIEW={drawFrame,copy};
  if(typeof document==='undefined')return;
  const $=id=>document.getElementById(id),lib=window.LIFE_ROBOT;
  const assets=Object.fromEntries(window.DEVICE_ART.assets.map(a=>[a.name,a])),images={};
  let paused=false,elapsed=0,last=null,ready=false;
  function updateCopy(){const key=$('action').value;elapsed=0;$('title').textContent=copy[key][0];$('description').textContent=copy[key][1];$('reference-label').textContent=key==='care'?'对照：收获动作':key==='harvest'?'对照：日常照顾':key==='inspect_left'?'对照：朝右检查':key==='rest_tree'?'对照：普通休息':'对照：原有悬浮';$('frames').replaceChildren();lib.animations[key].frames.forEach((src,i)=>{const box=document.createElement('div');box.className='frame';const im=document.createElement('img');im.src=src;im.alt=`${copy[key][0]}第${i+1}帧`;const text=document.createElement('small');text.textContent=`第${i+1}帧`;box.append(im,text);$('frames').appendChild(box)});}
  $('action').onchange=()=>{updateCopy();draw()};
  $('pause').onclick=()=>{paused=!paused;$('pause').textContent=paused?'继续':'暂停'};
  $('next').onclick=()=>{paused=true;$('pause').textContent='继续';elapsed=(Math.floor(elapsed/lib.animations[$('action').value].frame_ms)+1)*lib.animations[$('action').value].frame_ms;draw()};
  $('native').onchange=draw;$('float').onchange=draw;
  function draw(){if(!ready)return;const key=$('action').value,config=lib.animations[key],index=Math.floor(elapsed/config.frame_ms)%4;
    drawFrame($('scene').getContext('2d'),$('detail').getContext('2d'),$('reference').getContext('2d'),images,assets,window.ECONOMY_REPLAY[0].state,key,index,{native:$('native').checked,float:$('float').checked?Math.sin(elapsed/650)*2:0});
    Array.from($('frames').children).forEach((e,i)=>e.classList.toggle('active',i===index));
    $('status').textContent=`${copy[key][0]} · 第${index+1}/4帧 · ${paused?'已暂停':'循环播放'} · ${$('native').checked?'I4设备编码':'透明PNG原帧'}`;
    $('dimensions').textContent=`本次新增16帧，合并前批共24帧；PNG合计${(lib.png_bytes/1024).toFixed(1)} KiB；独立设备I4包${(lib.device_bytes/1024).toFixed(1)} KiB（尚未加入固件）。`;
  }
  const list=[...window.DEVICE_ART.assets.map(a=>({name:a.name,src:'../economy/layout-v3/preview-device/'+a.name+'.png'})),...Object.entries(window.GARDEN_LAYOUT_V2.paths).map(([name,src])=>({name:'Layout_'+name,src:'../'+src})),...lib.frames.map(f=>({name:f.name,src:f.file})),...lib.device_assets.map(a=>({name:a.name,src:a.preview})),{name:'reference',src:'../robot-hover-v1/frames/hover_0-v3.png'},{name:'reference_rest',src:'../robot-hover-v1/frames/rest_0-v3.png'}];
  Promise.all(list.map(a=>new Promise((resolve,reject)=>{const im=new Image();im.onload=()=>{images[a.name]=im;resolve()};im.onerror=()=>reject(new Error(a.src));im.src=a.src}))).then(()=>{ready=true;updateCopy();function tick(now){if(last!==null&&!paused)elapsed+=Math.min(100,now-last)*Number($('speed').value);last=now;draw();requestAnimationFrame(tick)}requestAnimationFrame(tick)}).catch(e=>{$('status').textContent='素材载入失败：'+e.message});
})();
