'use strict';
const $ = id => document.getElementById(id);
const topics = [
 {kicker:'AUTONOMY / 自主生活',title:'你不说话，\n它也有自己的生活。',description:'照料植物、看看窗外、工作后休息。每一次行动，都由设备记录开始、到达和结果。',point:'把设备放下，观察它如何独立行动。'},
 {kicker:'COOPERATION / 语音协作',title:'一句话，\n一起做一件真实的事。',description:'从语音请求，到园丁实际走过去，再到任务完成。对话连接着正在运行的花园。',point:'试着说“照料第一株植物”，同时看设备动作与任务结果。'},
 {kicker:'MEMORY / 真实经历',title:'记得的，\n是确实发生过的事。',description:'生活、协作和重要选择留下记录。设备保留近期经历与长期里程碑，电脑归档收到的历史。',point:'展示一条完成记录。玩家邀请、园丁独处和共同参与分别记录。'},
 {kicker:'BECOMING / 关系与成长',title:'从执行任务，\n到慢慢成为自己。',description:'剧情档案与生活经历逐渐展开。语言阶段、行动许可和真实结果，分别保存在设备中。',point:'打开一份已解锁档案，再展示当前真实阶段；成长需要过程。'}
];
const labels = {none:'无',idle:'空闲',move:'移动',hover:'悬浮',water:'浇水',harvest:'收获',listen:'聆听',speak:'说话',call:'通讯回应',rest:'休息',watch_window:'看窗外',inspect_plant:'检查植物',tend_plants:'照料植物',collect:'收获',construct_facility:'设施施工',recycle:'拆解回收',journey_task:'旅途作业',tend_tree:'培育大树',requested:'已提出请求',traveling:'正在前往',observing:'已到达，正在观察',paused_for_voice:'通话中暂停',completed:'已完成',finished:'已结束',arrived:'已到达',paused:'已暂停',resumed:'已恢复',interrupted_by_work:'被工作打断',interrupted_by_restart:'重启中断',expired:'已过期',target_changed:'目标变化',starting:'启动中',connecting:'连接中',listening:'聆听中',speaking:'正在说话',notifying:'消息提示',routine_plant_check:'例行检查植物',work_finished_observe_window:'工作后看窗外',short_rest_after_work:'工作后休息',player_invitation:'玩家邀请',CARE:'照料',HARVEST:'收获',NONE:'无',QUEUED:'等待处理',ACCEPTED:'已接受',RUNNING:'执行中',TRAVELING:'正在前往',ARRIVED:'已到达',COMPLETED:'已完成',REJECTED:'已拒绝',INTERRUPTED:'已中断',CANCELLED:'已取消',WAIT_INSTRUCTION:'等待明确指令',ASK_PERMISSION:'先提出愿望，再等待许可',ROBOT_AWAKENED:'园丁被唤醒',PLANT_MATURED:'植物成熟',HARVESTED:'完成收获',IRRIGATED:'完成灌溉',TREE_GROWN:'大树长成',TREE_STARTED:'开始培育大树',FACILITY_STARTED:'开始设施施工',FACILITY_BUILT:'设施建成',REGION_CHANGED:'驶入新地区',PROPOSAL_CREATED:'提出待批准方案',COMMUNICATION_ACCEPTED:'接通通讯',JOURNEY_COMPLETED:'旅途任务完成',JOURNEY_FOUND:'发现旅途事件',RECYCLED:'完成回收'};
const tr = value => labels[value] ?? value ?? '未提供';
const num = value => typeof value === 'number' && Number.isFinite(value) ? String(value) : '—';
const node = (tag,text,cls) => { const e=document.createElement(tag); e.textContent=text; if(cls)e.className=cls; return e; };
let topic=0, latest=null, fetchedAt=0;
let sceneSubmitting=false, lastSceneEpoch=null, sceneError='';
const sceneNames=['正式进度','初次相遇','共同劳动','自主生活','旧日档案','返程真相','新叶分享','继续上次路演'];
const sceneReasons={VOICE_ACTIVE:'正在通话，请结束通话后等待切换',ACTIVITY_RUNNING:'等待当前动作完成',COMMANDS_PENDING:'等待设备处理已有操作',SAFE_POINT:'等待安全切换点',WAIT_TIMEOUT:'等待超时，请结束当前活动后重试',STALE_WORLD:'设备情境已变化，请等待新状态后重试',SOURCE_SAVE_FAILED:'当前存档保存失败，未切换',ROADSHOW_SAVE_FAILED:'路演档保存失败，未切换',ROADSHOW_STORAGE_OPEN:'路演存储不可用',NO_ROADSHOW_SAVE:'尚无可继续的路演存档，请先载入一组情境',FORMAL_MODE_REQUIRED:'请先退出旧的十倍演示模式',STORAGE_UNAVAILABLE:'设备存储不可用',NO_FORMAL_CHECKPOINT:'未找到返回检查点，请重启回到正式档',ALREADY_ROADSHOW:'已经在路演档中'};
function renderScene(live){
 const w=latest?.snapshot?.world||{},device=latest?.snapshot?.runtime?.roadshow||{},control=latest?.roadshow_control||{};
 const active=w.mode==='roadshow',supported=device.available&&control.enabled;
 const busy=sceneSubmitting||control.pending||control.phase==='UNCERTAIN'||device.status==='WAITING';
 $('save-mode').textContent=active?'独立路演档 · 正常速度':w.mode==='normal'?'正式进度':'等待设备';$('save-mode').className=active?'show-mode':'';
 const allowed=live&&supported&&['normal','roadshow'].includes(w.mode)&&!busy;
 for(const option of $('scene-select').options)option.disabled=Number(option.value)>=8&&Number(option.value)>(device.scene_count||6);
 $('scene-load').disabled=!allowed;$('scene-resume').disabled=!allowed||active;$('scene-return').disabled=!allowed||!active;
 if(live&&supported&&w.roadshow_epoch===device.epoch&&lastSceneEpoch!==device.epoch){if(lastSceneEpoch!==null){if(active&&device.scene>=1&&device.scene<=11)$('scene-select').value=String(device.scene);selectTopic(active?({1:3,2:1,3:0,4:3,5:3,6:3,8:1,9:3,10:0,11:3})[device.scene]??0:0);}lastSceneEpoch=device.epoch;}
 let message='载入会重新开始选定路演情境；正式进度保留，重启默认回正式档。';
 if(!live)message='实机数据未在线，不能发送情境切换。';
 else if(!device.available)message='当前固件未提供情境入口；完成固件更新后即可使用。';
 else if(!control.enabled)message='请重启园丁状态服务并启用路演控制。';
 else if(sceneSubmitting)message='正在提交实机切换请求…';
 else if(control.phase==='QUEUED'||control.phase==='SENT')message='请求已提交，等待实机确认；页面切换不代表设备已载入。';
 else if(control.phase==='WAITING'||device.status==='WAITING')message=sceneReasons[control.reason||device.reason]||'设备正在等待安全切换点…';
 else if(control.phase==='UNCERTAIN')message=control.reason||'连接中断，切换结果待设备确认；不自动重发。';
 else if(control.phase==='REJECTED'||control.phase==='CANCELLED')message=sceneReasons[control.reason]||control.reason||'切换未执行';
 else if(control.phase==='APPLIED')message=active?`实机已确认：${sceneNames[device.scene]||device.label}。本档包含路演预设，后续动作按实际结果记录。`:'实机已确认返回正式进度，继续原来的旅程。';
 if(sceneError&&!busy)message=sceneError;
 $('scene-status').textContent=message;
}
async function requestScene(scene){
 if(sceneSubmitting)return;sceneSubmitting=true;sceneError='';renderScene(Boolean(latest?.connection?.live));
 try{const response=await fetch('/api/roadshow/switch',{method:'POST',headers:{'Content-Type':'application/json','X-Spring-Roadshow':'1'},body:JSON.stringify({scene}),signal:AbortSignal.timeout(6000)});const result=await readControlReply(response);latest.roadshow_control=result;}catch(e){sceneError=e.name==='TimeoutError'?'请求结果待确认，请等待设备新状态；不要重复点击。':e.message;}finally{sceneSubmitting=false;updateFreshness();}
}
async function readControlReply(response){
 let result;
 try{result=JSON.parse(await response.text());}catch{
  throw Error(response.status===405?'当前路演服务仍是旧版，请重启本地路演服务后刷新页面。':'路演服务响应异常，切换结果待设备确认，请勿连续点击。');
 }
 if(!result||typeof result!=='object'||Array.isArray(result))throw Error('路演服务响应格式异常，切换结果待设备确认。');
 if(!response.ok)throw Error(result.error||`切换请求未被服务接受（HTTP ${response.status}）`);
 return result;
}
function rows(items){$('evidence-rows').replaceChildren(...items.map(([key,value])=>{const e=node('div','','evidence-row');e.append(node('span',key),node('b',String(value??'未提供')));return e;}));}
function setEvidence(label,main,detail,items){$('evidence-label').textContent=label;$('evidence-main').textContent=main;$('evidence-detail').textContent=detail;rows(items);}
function selectTopic(index){topic=index;const t=topics[index];$('topic-kicker').textContent=t.kicker;$('topic-title').replaceChildren(...t.title.split('\n').flatMap((part,i)=>i?[document.createElement('br'),document.createTextNode(part)]:[document.createTextNode(part)]));$('topic-description').textContent=t.description;$('judge-point').textContent=t.point;for(const b of $('topics').querySelectorAll('button'))b.setAttribute('aria-pressed',String(Number(b.dataset.topic)===index));renderEvidence();}
function renderEvidence(){
 const s=latest?.snapshot;if(!s){setEvidence('等待设备','等待完整实机数据','连接设备后显示，不填入演示数值。',[]);return;}
 const w=s.world||{},r=s.runtime||{},l=w.life||{},m=s.memory||{},st=w.story||{},c=r.communication||{},agency=c.agency||r.observation?.agency;
 if(topic===0){
  const active=l.intent&&l.intent!=='none';
  const histories=(l.history||[]).slice().sort((a,b)=>(b.game_second??0)-(a.game_second??0));const last=histories[0];
  setEvidence(active?'当前生活行动':'当前任务与生活记录',active?tr(l.intent):tr(w.robot?.work),active?`${tr(l.phase)} · ${tr(l.reason)}${l.player_participated?' · 玩家邀请':' · 无玩家参与标记'}`:last?`最近一次生活：${tr(last.intent)} · ${tr(last.result)}（历史记录）`:'尚未收到生活完成记录',[
   ['身体动作',tr(r.fsm)],['当前生活是否到达',active?(l.arrived?'是':'尚未到达'):'当前没有生活行动'],['语音状态',tr(r.voice)],['自主行动许可',agency?`等级 ${num(agency.level)} · ${tr(agency.personal_action_mode)}`:'未提供']]);
 }else if(topic===1){
  const a=w.plant_action||{},q=r.tool_queries||{};const hasAction=Boolean(a.request_id)&&a.kind!=='NONE';
  setEvidence('语音 → 身体 → 结果',hasAction?`${tr(a.kind)}种植位 ${num(a.slot)} · ${tr(a.status)}`:'等待一次指定植物作业',hasAction?`请求 #${a.request_id} · ${a.completed?'设备已确认完成':a.arrived?'已到达目标，等待结果':'以设备作业状态为准'}${a.reason&&a.reason!=='none'?' · '+tr(a.reason):''}`:'说“照料第一株植物”，随后观察实际作业。',[
   ['语音状态',tr(r.voice)],['作业到达 / 完成',hasAction?`${a.arrived?'已到达':'未到达'} / ${a.completed?'已完成':'未完成'}`:'暂无请求'],['本次启动 · 照料 / 收获请求',`${num(q.request_plant_care)} / ${num(q.request_plant_harvest)}`],['本次启动 · 世界 / 记忆查询',`${num(q.get_world_state)} / ${num(q.get_memories)}`]]);
 }else if(topic===2){
  const archive=latest.archive||{},records=(m.records||[]).filter(e=>!(e.flags&8));
  setEvidence('设备记忆与本地归档',m.enabled?`${num(m.long_term_count)} 条长期记忆`:'设备暂未提供分层记忆',m.enabled?`近期详细经历 ${num(m.retained)} / ${num(m.capacity)} 条；只展示收到的记录。`:'等待支持记忆数据的实机快照。',[
   ['电脑归档',archive.available?`${num(archive.events)} 条`:'不可用'],['未收到的详细历史',archive.available?`${num(archive.missing_details)} 条`:'未提供'],['最近设备事件',records.length?`#${records[0].id}`:'未提供'],['记忆查询（本次启动）',num(r.tool_queries?.get_memories)]]);
 }else{
  const archives=st.archives||[];
  setEvidence('当前成长事实',`语言阶段 ${num(st.language_stage??w.robot?.language_stage)}`,st.node?.title?`当前可用片段：${st.node.title} · 可用不等于已读`:'等待剧情状态',[
   ['设备称呼',st.robot_name||'未提供'],['可用 / 已读档案',`${archives.length} / ${archives.filter(x=>x.read).length}`],['幼苗观察',st.chapter_experience?(st.chapter_experience.observation_verified?'实际完成':st.chapter_experience.beyond_work_observed?'预设 / 旧进度':'尚未完成'):'未提供'],['乘客椅',st.chapter_experience?(st.chapter_experience.seat_rest_verified?'实际休息完成':st.chapter_experience.bench_permission?'已获准，完成待确认':'等待剧情邀请'):'未提供']]);
 }
}
function recordTitle(e,w){
 const action=tr(e.action);if(e.category===1){const intent=['无','看窗外','检查植物','休息'][e.kind]||'生活行动';const result=['已完成','被工作打断','重启中断','已过期','目标变化'][e.result]||'结果未提供';return `${intent} · ${e.action==='finished'?result:action}`;}
 if(e.plant_action)return `${tr(e.plant_action)}种植位 ${e.target+1} · ${action}`;
 if(e.world_event)return tr(e.world_event);
 if(e.category===2){const target=e.kind===1?w.economy?.facilities?.find(f=>f.id===e.target)?.name:e.kind===7?'大树':'经营安排';return `${target||'经营安排'} · ${tr(({approved:'玩家批准',rejected:'玩家拒绝',started:'开始执行',completed:'已完成',proposed:'提出方案'})[e.action]||e.action)}`;}
 return `${({4:'玩家合作',5:'记录基线'})[e.category]||'设备事件'} · ${action}`;
}
function renderWorld(){
 const s=latest?.snapshot;if(!s){$('world-metrics').replaceChildren(...[['水','—'],['能源','—'],['设施','—'],['长期记忆','—']].map(([k,v])=>metric(k,v)));return;}
 const w=s.world||{},m=s.memory||{},facilities=w.economy?.facilities||[];
 $('world-mode').textContent=w.mode==='roadshow'?'路演世界 · 预设起点 + 实际事件':w.mode==='normal'?'正式世界':w.mode==='demo_10x'?'十倍演示世界':w.mode||'模式未提供';
 $('world-metrics').replaceChildren(...[['水',num(w.resources?.water)],['能源',num(w.resources?.energy)],['已建设施',facilities.length?`${facilities.filter(x=>x.built).length}/${facilities.length}`:'—'],['长期记忆',num(m.long_term_count)]].map(([k,v])=>metric(k,v)));
 $('world-detail').textContent=`${w.journey||'地区未知'} · ${(w.plants||[]).length} 个种植位 · 游戏已运行 ${typeof w.game_seconds==='number'?Math.floor(w.game_seconds/60)+' 分钟':'—'}`;
 const records=(m.records||[]).filter(e=>!(e.flags&8)).slice().sort((a,b)=>b.id-a.id).slice(0,3);
 $('events').replaceChildren(...(records.length?records.map(e=>{const card=node('div','','event');const age=typeof w.game_seconds==='number'&&typeof e.game_second==='number'?Math.max(0,w.game_seconds-e.game_second):null;card.append(node('strong',recordTitle(e,w)),node('small',`#${e.id} · ${age===null?'时间未提供':age<60?Math.floor(age)+' 游戏秒前':Math.floor(age/60)+' 游戏分钟前'}`),node('small',e.actor==='voice_request'?'玩家语音请求':e.player_participated?'有玩家参与依据':e.category===1?'园丁独处记录':'设备世界记录','event-tag'));return card;}):[node('p','尚未收到设备详细经历。','muted')]));
}
function metric(key,value){const e=node('div','','metric');e.append(node('span',key),node('strong',value));return e;}
function updateFreshness(){
 const c=latest?.connection||{};const elapsed=fetchedAt?(performance.now()-fetchedAt)/1000:0;const age=typeof c.age_seconds==='number'?c.age_seconds+elapsed:null;const live=Boolean(c.live)&&age!==null&&age<15;
 document.body.classList.toggle('stale',!live);$('usb-status').className='status '+(live?'live':'stale');$('usb-status').textContent=live?'● 实机数据在线':latest?.snapshot?'◌ 数据已过期':'○ 等待实机';$('evidence-freshness').textContent=live?'USB 真实数据':latest?.snapshot?'最后一次记录':'等待实机';
 $('data-warning').hidden=live;$('data-warning').textContent=latest?.snapshot?'USB 数据已过期，以下为最后一次记录。画面连接状态单独显示。':c.error||'等待 USB 完整快照；连接设备并保持园丁状态服务运行。';
 $('data-age').textContent=age===null?'USB 等待完整快照 · 视频独立采集':`USB 数据 ${Math.floor(age)} 秒前更新 · 快照约 5 秒一份 · 视频独立采集`;
 $('events-scope').textContent=live?'设备经历 · 最新优先':'历史记录 · 非实时';
 renderScene(live);
}
async function poll(){try{const response=await fetch('/api/state',{cache:'no-store',signal:AbortSignal.timeout(4000)});if(!response.ok)throw Error();const result=await response.json();if(!result.connection)throw Error();if(!result.snapshot&&latest?.snapshot){result.snapshot=latest.snapshot;result.archive=latest.archive;result.connection.live=false;result.connection.age_seconds=typeof latest.connection.age_seconds==='number'?latest.connection.age_seconds+(performance.now()-fetchedAt)/1000:null;}latest=result;fetchedAt=performance.now();renderEvidence();renderWorld();}catch{if(latest?.connection)latest.connection.live=false;}updateFreshness();setTimeout(poll,1200);}

// Media remains in the browser. The device's USB observer never receives video commands.
let stream=null,sourceKind='',captureBusy=false,mediaGeneration=0,lastVideoFrame=0;
let zoom=1,rotation=0,mirror=false,panX=0,panY=0,drag=null,canvasWidth=0,canvasHeight=0;
const video=$('source-video'),canvas=$('screen'),ctx=canvas.getContext('2d');
function reportMedia(message){$('capture-status').textContent=message;}
function mediaState(active){$('video-empty').hidden=active;$('screen').hidden=!active;$('video-stop').disabled=!active;$('video-dot').className='dot'+(active?' live':'');$('video-label').textContent=active?(sourceKind==='camera'?'摄像头 · 实机拍摄':'投屏窗口 · 本机采集'):'设备画面 · 尚未接入';$('screen-caption').textContent=active?(sourceKind==='camera'?'LIVE CAMERA / 实机拍摄':'LIVE WINDOW / 窗口采集'):'LIVE DEVICE / 等待画面';if(!active)$('video-meta').textContent='选择实时画面来源';}
function stopMedia(){mediaGeneration++;const old=stream;stream=null;video.srcObject=null;if(old)old.getTracks().forEach(t=>t.stop());mediaState(false);$('video-interrupted').hidden=true;reportMedia('画面已停止，可重新选择来源');}
function resetFrame(){zoom=1;rotation=0;mirror=false;panX=panY=0;$('zoom').value='1';$('mirror').setAttribute('aria-pressed','false');}
function mediaError(e){return ({NotAllowedError:'未获得画面权限。请在浏览器与系统设置中允许相机或屏幕录制后重试。',NotFoundError:'没有发现摄像头。请连接 USB 摄像头，或选择已有投屏窗口。',NotReadableError:'画面来源正被其他应用占用，或系统尚未允许访问。',AbortError:'已取消接入，现有画面保持原样。',TypeError:'当前浏览器不支持此画面来源。请在本机 Chrome 中打开。'})[e.name]||'接入失败，请检查设备与浏览器权限后重试。';}
async function enumerateCameras(){try{const devices=await navigator.mediaDevices.enumerateDevices();const select=$('camera-device'),value=stream?.getVideoTracks()[0]?.getSettings().deviceId||select.value;select.replaceChildren(new Option('默认摄像头',''),...devices.filter(x=>x.kind==='videoinput').map((x,i)=>new Option(x.label||`摄像头 ${i+1}`,x.deviceId)));if([...select.options].some(x=>x.value===value))select.value=value;}catch{/* Labels are optional until camera permission is granted. */}}
async function startMedia(kind){
 if(captureBusy)return;captureBusy=true;const generation=++mediaGeneration;let next=null;const old=stream,oldKind=sourceKind;reportMedia('请在浏览器中选择并允许画面来源…');
 try{
  if(!navigator.mediaDevices)throw new TypeError();
  if(kind==='camera')next=await navigator.mediaDevices.getUserMedia({audio:false,video:{deviceId:$('camera-device').value?{exact:$('camera-device').value}:undefined,width:{ideal:1920},height:{ideal:1080},frameRate:{ideal:30,max:30}}});
  else{if(!navigator.mediaDevices.getDisplayMedia)throw new TypeError();next=await navigator.mediaDevices.getDisplayMedia({audio:false,video:{frameRate:{ideal:30,max:30}},selfBrowserSurface:'exclude'});}
  if(generation!==mediaGeneration){next.getTracks().forEach(t=>t.stop());return;}
  video.srcObject=next;await video.play();if(generation!==mediaGeneration){next.getTracks().forEach(t=>t.stop());return;}stream=next;sourceKind=kind;if(old)old.getTracks().forEach(t=>t.stop());
  resetFrame();lastVideoFrame=performance.now();$('video-interrupted').hidden=true;mediaState(true);reportMedia(kind==='camera'?'实时拍摄中 · 可在画面设置中放大、旋转和取景':'窗口采集中 · 请保持源窗口运行，避免选择路演页本身');
  for(const track of next.getVideoTracks())track.addEventListener('ended',()=>{if(stream===next){stopMedia();reportMedia('画面来源已关闭，请重新接入');}});
  await enumerateCameras();
 }catch(e){if(next)next.getTracks().forEach(t=>t.stop());if(generation===mediaGeneration){stream=old;sourceKind=oldKind;video.srcObject=old;if(old)video.play().catch(()=>{});reportMedia(mediaError(e));}}finally{captureBusy=false;}
}
function draw(){
 if(stream&&video.readyState>=2&&video.videoWidth){
  const rect=$('viewport').getBoundingClientRect(),dpr=Math.min(window.devicePixelRatio||1,2);const width=Math.max(1,Math.round(rect.width*dpr)),height=Math.max(1,Math.round(rect.height*dpr));
  if(canvasWidth!==width||canvasHeight!==height){canvas.width=canvasWidth=width;canvas.height=canvasHeight=height;}
  ctx.fillStyle='#1b2a23';ctx.fillRect(0,0,width,height);ctx.save();ctx.translate(width/2+panX*width,height/2+panY*height);ctx.rotate(rotation*Math.PI/180);ctx.scale(mirror?-1:1,1);const swapped=rotation%180!==0;const scale=Math.min(width/(swapped?video.videoHeight:video.videoWidth),height/(swapped?video.videoWidth:video.videoHeight))*zoom;ctx.drawImage(video,-video.videoWidth*scale/2,-video.videoHeight*scale/2,video.videoWidth*scale,video.videoHeight*scale);ctx.restore();
  const track=stream.getVideoTracks()[0],settings=track?.getSettings()||{};$('video-meta').textContent=`${video.videoWidth} × ${video.videoHeight}${settings.frameRate?' / '+Math.round(settings.frameRate)+' fps':''}`;
  if(!video.requestVideoFrameCallback){if(video.currentTime!==draw.lastTime){lastVideoFrame=performance.now();draw.lastTime=video.currentTime;}}
  const stalled=performance.now()-lastVideoFrame>4000||track?.muted;$('video-interrupted').hidden=!stalled;$('video-dot').className='dot'+(stalled?'':' live');
 }
 requestAnimationFrame(draw);
}
if(video.requestVideoFrameCallback){const onFrame=()=>{lastVideoFrame=performance.now();video.requestVideoFrameCallback(onFrame);};video.requestVideoFrameCallback(onFrame);}
canvas.addEventListener('pointerdown',e=>{drag={x:e.clientX,y:e.clientY,px:panX,py:panY};canvas.setPointerCapture(e.pointerId);});
canvas.addEventListener('pointermove',e=>{if(!drag)return;const rect=canvas.getBoundingClientRect();panX=Math.max(-1,Math.min(1,drag.px+(e.clientX-drag.x)/rect.width));panY=Math.max(-1,Math.min(1,drag.py+(e.clientY-drag.y)/rect.height));});
for(const event of ['pointerup','pointercancel','lostpointercapture'])canvas.addEventListener(event,()=>{drag=null;});
function toggle(id,button){const e=$(id);e.hidden=!e.hidden;$(button).setAttribute('aria-expanded',String(!e.hidden));}
async function fullscreen(){try{if(document.fullscreenElement)await document.exitFullscreen();else await document.documentElement.requestFullscreen();}catch{reportMedia('当前浏览器不支持页面全屏，可使用系统的全屏窗口。');}}
document.addEventListener('fullscreenchange',()=>{$('fullscreen').textContent=document.fullscreenElement?'退出全屏':'全屏路演';});
$('topics').addEventListener('click',e=>{const b=e.target.closest('button[data-topic]');if(b)selectTopic(Number(b.dataset.topic));});
$('scene-load').addEventListener('click',()=>requestScene(Number($('scene-select').value)));
$('scene-resume').addEventListener('click',()=>requestScene(7));
$('scene-return').addEventListener('click',()=>requestScene(0));
for(const id of ['camera-start','camera-switch'])$(id).addEventListener('click',()=>startMedia('camera'));
for(const id of ['window-start','window-switch'])$(id).addEventListener('click',()=>startMedia('window'));
$('video-stop').addEventListener('click',stopMedia);$('settings-toggle').addEventListener('click',()=>{toggle('settings','settings-toggle');enumerateCameras();});$('notes-toggle').addEventListener('click',()=>toggle('presenter-notes','notes-toggle'));$('fullscreen').addEventListener('click',fullscreen);
$('zoom').addEventListener('input',e=>{zoom=Number(e.target.value);});$('rotate').addEventListener('click',()=>{rotation=(rotation+90)%360;});$('mirror').addEventListener('click',()=>{mirror=!mirror;$('mirror').setAttribute('aria-pressed',String(mirror));});$('reset-frame').addEventListener('click',resetFrame);
document.addEventListener('keydown',e=>{if(e.altKey||e.ctrlKey||e.metaKey||/INPUT|SELECT|TEXTAREA/.test(e.target.tagName))return;if(/^[1-4]$/.test(e.key))selectTopic(Number(e.key)-1);if(e.key.toLowerCase()==='f')fullscreen();if(e.key.toLowerCase()==='n')toggle('presenter-notes','notes-toggle');});
window.addEventListener('pagehide',stopMedia);
selectTopic(0);renderWorld();updateFreshness();poll();setInterval(updateFreshness,1000);requestAnimationFrame(draw);
