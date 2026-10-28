/* Asset review only. No device, microphone or game commands are connected. */
(() => {
  const $=id=>document.getElementById(id),library=window.HOVER_ROBOT,assets=Object.fromEntries(window.DEVICE_ART.assets.map(a=>[a.name,a])),images={};
  const modes={
    hover:['悬浮待机','收腿悬停，轻微起伏和眨眼。','hover'],
    move:['升降巡游','沿中央窗口的小弧线升降，平滑转向；工作时回到目标高度停稳。','hover'],
    takeoff:['起飞／降落','机械肢逐步收起；同组素材反向播放用于降落。','takeoff'],
    tend:['照料／收获','伸出机械肢整理植物；收获资源特效可另行叠加。','tend'],
    water:['浇水','一次10秒照料：拿壶、倾倒、持续浇灌，最后1秒收壶；实际任务含前往种植位的时间。','water'],
    build:['建设施工','使用小工具，保持悬停姿态。','build'],
    call:['等待接听','朝玩家挥手；复用轻晃开心动作。','wave'],
    listen:['聆听／接收信号','聆听和等待回复共用同一动作；信号由外向内汇入。','listen'],
    speak:['说话／喇叭声波','小喇叭配合向外扩散的声波，字幕独立显示。','speak'],
    drift:['轻晃回应 · 回稳','被晃后轻轻偏移，再主动回稳。','recover'],
    squint:['轻晃回应 · 眯眼','看向玩家、眯眼，嫩芽轻摆两次。','squint'],
    wave:['轻晃回应 · 开心','升高一点，挥一下机械肢，再落回原悬停高度。','wave'],
    rest:['落地休息','展开支撑机械肢，闭眼打盹。','rest'],
    celebrate:['完成庆祝','复用开心姿态，短暂升高并回稳。','wave']
  };
  Object.entries(modes).forEach(([key,v])=>{const o=document.createElement('option');o.value=key;o.textContent=v[0];$('action').appendChild(o)});
  let nowTime=0,lastTick=0,origin=0,paused=false,override=null,cooldownUntil=-Infinity,bigUntil=-Infinity,ackAt=-Infinity,wakeAt=-Infinity,lastReaction='';
  $('action').onchange=()=>{origin=nowTime;override=null;wakeAt=-Infinity;updateCopy()};
  $('pause').onclick=()=>{paused=!paused;$('pause').textContent=paused?'继续':'暂停'};
  $('shake').onclick=()=>{
    if(nowTime<cooldownUntil)return;
    cooldownUntil=nowTime+1000;ackAt=nowTime;
    const base=$('action').value||'hover';
    if(base==='rest'){wakeAt=nowTime;return}
    if(!['hover','move','call'].includes(base)||nowTime<bigUntil)return;
    const choices=['drift','squint','wave'].filter(x=>x!==lastReaction);
    lastReaction=choices[Math.floor(Math.random()*choices.length)];
    override={key:lastReaction,start:nowTime};bigUntil=nowTime+6500;updateCopy();
  };
  function active(){return override?override.key:$('action').value||'hover'}
  function updateCopy(){const v=modes[active()];$('title').textContent=v[0];$('description').textContent=v[1]}
  function pose(mode,t){
    let x=246,y=292+Math.sin(t/650)*2.4,frame=Math.floor(t/260)%4;
    if(mode==='water'){const u=t%10000;frame=u>=9000?3:u<800?0:u<1500?1:2;}
    if(mode==='move'){x=246+Math.sin(t/2400)*34;y=274+Math.cos(t/3000)*24+Math.sin(t/650)*2;}
    if(mode==='takeoff'){const phase=(t%4800)/2400,u=Math.min(1,phase<1?phase:2-phase);y=319-27*(u*u*(3-2*u));frame=Math.min(3,Math.floor(u*4))}
    if(mode==='rest')y=319+Math.sin(t/1100)*.6;
    if(mode==='drift'){const u=Math.min(1,t/1100);x+=Math.sin(u*Math.PI*2)*4*(1-u);frame=Math.min(3,Math.floor(t/220))}
    if(mode==='wave'||mode==='celebrate'){const u=mode==='wave'?Math.min(1,t/1100):(t%2800)/2800;y-=Math.sin(u*Math.PI)*(mode==='wave'?4:11);if(mode==='wave')frame=Math.min(3,Math.floor(t/220))}
    if(mode==='squint')frame=Math.min(3,Math.floor(t/220));
    return {x,y,frame};
  }
  function voiceEffect(c,mode,x,y,t,scale=1){
    if(!['listen','speak'].includes(mode))return;
    c.save();c.translate(x,y);c.scale(scale,scale);c.strokeStyle='#b9eef2';c.fillStyle='#b9eef2';c.lineWidth=2;
    const speak=mode==='speak',phase=Math.floor(t/180)%3;
    if(speak){c.fillRect(28,-2,5,7);c.beginPath();c.moveTo(32,-2);c.lineTo(40,-7);c.lineTo(40,9);c.lineTo(32,4);c.stroke()}
    else {c.beginPath();c.arc(-33,2,2,0,Math.PI*2);c.fill()}
    for(let i=0;i<3;i++){c.globalAlpha=i===(speak?phase:2-phase)?1:.3;c.beginPath();c.arc(speak?41:-33,2,9+4*i,(speak?-45:135)*Math.PI/180,(speak?45:225)*Math.PI/180);c.stroke()}
    c.restore();
  }
  function draw(t){
    if(override&&t-override.start>=1100){override=null;updateCopy()}
    if(t-wakeAt>=1250&&t-wakeAt<1500){wakeAt=-Infinity;$('action').value='hover';origin=t;updateCopy()}
    let mode=active();const base=$('action').value||'hover';
    const elapsed=t-origin,p=pose(base,elapsed);
    let group=modes[mode][2],frame=p.frame;
    if(override){const q=pose(mode,t-override.start);p.x+=q.x-246;p.y+=q.y-(292+Math.sin((t-override.start)/650)*2.4);frame=q.frame;}
    const ackAge=t-ackAt;
    if(ackAge>=0&&ackAge<650){const u=ackAge/650,quiet=['listen','wait','speak'].includes(base);p.x+=Math.sin(u*Math.PI*2)*(quiet?.65:2.5)*(1-u);p.y-=Math.sin(u*Math.PI)*(quiet?.4:1.4);}
    if(base==='rest'&&t-wakeAt>=0){const u=Math.max(0,Math.min(1,(t-wakeAt-350)/900));group='takeoff';frame=t-wakeAt<350?0:Math.min(3,Math.floor(u*4));p.y=319-27*u*u*(3-2*u);}
    const frames=library.animations[group],im=images['Frame_'+frames[frame]],c=$('scene').getContext('2d');
    window.GARDEN_LAYOUT_V2.draw(c,window.ECONOMY_REPLAY[0].state,images,assets,{showcase:true,stage:2,renderRobot:false});
    c.save();c.fillStyle='#23362f';c.globalAlpha=mode==='rest'?.24:.15;c.beginPath();c.ellipse(p.x,365,mode==='rest'?24:Math.max(13,Math.min(20,20-(292-p.y)*.12)),5,0,0,Math.PI*2);c.fill();c.restore();
    c.drawImage(im,p.x-library.anchor[0],p.y-library.anchor[1]);
    voiceEffect(c,mode,p.x,p.y,t);
    c.fillStyle='#293b30e8';c.beginPath();c.roundRect(12,12,318,44,20);c.fill();c.font='19px system-ui';c.fillStyle='white';c.fillText('藤 59     水 20     能源 99',28,41);
    if(['listen','wait','speak','call'].includes(mode)){c.fillStyle='#18362bcc';c.fillRect(0,448,480,32);c.fillStyle='white';c.textAlign='center';c.font='16px system-ui';c.fillText(mode==='listen'?'你：今天先照料哪些植物？':mode==='speak'?'园丁：我会先保证水和能源充足。':mode==='call'?'园丁有一件新发现，想听听你的决定。':'等待回复…',240,470);c.textAlign='left'}
    const d=$('detail').getContext('2d');d.clearRect(0,0,320,320);d.drawImage(im,0,0,320,352);voiceEffect(d,mode,160,144,t,2);
    const left=Math.max(0,Math.ceil((cooldownUntil-t)/1000));$('shake').disabled=left>0;$('shake').textContent=left?`小回应冷却 ${left}秒`:t<bigUntil?`模拟轻晃 · 大动作冷却${Math.ceil((bigUntil-t)/1000)}秒`:'模拟 IMU 轻晃';
    $('status').textContent='素材预览 · '+library.frame_count+'张独立帧 · '+(paused?'已暂停':$('quiet').checked?'安静展示低频播放':'正常播放')+' · 小回应1秒冷却，大动作6.5秒冷却；工作时保留工具';
  }
  const list=[...window.DEVICE_ART.assets.map(a=>({name:a.name,src:'economy/layout-v3/preview-device/'+a.name+'.png'})),...Object.entries(window.GARDEN_LAYOUT_V2.paths).map(([name,src])=>({name:'Layout_'+name,src})),...library.frames.map(f=>({name:'Frame_'+f.name,src:'robot-hover-v1/'+f.file}))];
  Promise.all(list.map(a=>new Promise((resolve,reject)=>{const im=new Image();im.onload=()=>{images[a.name]=im;resolve()};im.onerror=()=>reject(new Error(a.src));im.src=a.src}))).then(()=>{updateCopy();function tick(time){const gap=$('quiet').checked?250:83;if(!paused)nowTime+=lastTick?Math.min(time-lastTick,100):0;lastTick=time;if(!tick.at||time-tick.at>=gap){draw(nowTime);tick.at=time}requestAnimationFrame(tick)}requestAnimationFrame(tick)}).catch(e=>{$('status').textContent='素材载入失败：'+e.message});
})();
