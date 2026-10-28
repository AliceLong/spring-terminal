'use strict';
let currentMemory={},currentArchive={},archiveBefore=0,archiveNext=0,archiveBusy=false;
const actionNames={world_event:'世界事件',requested:'提出意图',traveling:'前往目标',arrived:'实际到达',
 paused:'暂停',resumed:'恢复',finished:'结束',proposed:'提出方案',approved:'玩家批准',rejected:'玩家拒绝',
 superseded:'被新方案替代',started:'开始执行',completed:'完成',scheduled:'已安排下一轮',applied:'已生效',tracking_started:'开始记录'};
const actorNames={unknown:'参与者未知',robot:'园丁',player:'玩家',system:'系统',voice_request:'语音请求'};
const categories={1:'生活',2:'决策',3:'世界',4:'合作',5:'旧档 / 基线'};
function costText(e){
 if(!e.cost_known)return '当时费用未记录';
 const c=e.cost||{};
 const pairs=[['水',c.water],['能源',c.energy],['纤维',c.fiber],['零件',c.parts],['食物',c.food]].filter(x=>x[1]);
 return pairs.length?'实际投入 '+pairs.map(x=>x[0]+x[1]).join('、'):'此步无材料投入';
}
function facilityName(id){
 try{return JSON.parse($('raw').textContent).world.economy.facilities.find(x=>x.id===id)?.name||('设施 '+id)}
 catch{return '设施 '+id}
}
function subject(e){
 if(e.kind===1)return facilityName(e.target);
 if(e.kind===2)return '种植位 '+(e.target+1);
 if(e.kind===3)return goals[e.target]||'目标';
 if(e.kind===4)return '照料装置';
 if(e.kind===5)return ['approved','started','completed'].includes(e.action)?journeys[e.value]||('旅途事件 #'+e.target):'旅途事件 #'+e.target;
 if(e.kind===6)return '拆解回收';
 if(e.kind===7)return '大树';
 return '';
}
function recordTitle(e){
 if(e.flags&8){
  if(e.action==='tracking_started')return '记录启用时已存在：'+(e.kind===5?journeys[e.target]||'动物':subject(e));
  return '记录启用时已有：'+(e.action==='proposed'?'待批方案 · ':'进行中任务 · ')+subject(e);
 }
 if(e.category===1||(e.category===5&&e.action==='finished')){
  const result=['completed','interrupted_by_work','interrupted_by_restart','expired','target_changed'][e.result];
  return tr(['none','watch_window','inspect_plant','rest'][e.kind])+' · '+(e.action==='finished'?tr(result):actionNames[e.action]||e.action);
 }
 if(e.world_event)return events[e.world_event]||e.world_event;
 if(e.category===4 && e.kind>=90 && e.kind<=93)return ['第一场干旱','档案后的幼苗观察','第一次乘客椅休息','乘客椅使用许可'][e.kind-90]+' · '+(actionNames[e.action]||e.action);
 if(e.category===4 && (e.kind===80||e.kind===81))return (e.kind===80?'指定照料':'指定收获')+' · '+(actionNames[e.action]||e.action);
 if(e.category===4)return '玩家协助收获';
 if(e.action==='tracking_started')return '经历记录启用';
 return subject(e)+' · '+(actionNames[e.action]||e.action)+(e.kind===5&&e.result===1?'（跳过）':'');
}
function recordDetail(e){
 let text='#'+e.id+' · 决策链 #'+e.chain+' · 游戏 '+duration(e.game_second)+' · '+(actorNames[e.actor]||e.actor);
 if(e.category===1)text+=(e.actor==='voice_request'?' · 玩家发起邀请（不代表共同在场）':' · 园丁自行执行')+((e.flags&16)?' · 曾到达':' · 未记录到达')+(e.reason?' · '+tr(e.reason):'');
 else text+=' · '+(e.player_participated?'有玩家参与依据':'没有玩家参与证据');
 if(e.category===2&&['approved','started','completed'].includes(e.action))text+=' · '+costText(e);
 if(e.flags&2)text+=' · 从旧记录导入，不补写因果';
 return text;
}
function factTitle(f){
 if(f.type===1)return '经历记录启用';
 if(f.type===2)return '玩家协助收获';
 if(f.type===3)return '大树第一次成长';
 if(f.type===4)return '繁茂树冠长成';
 if(f.type===10)return '寻雨雀救助完成';
 if(f.type===11)return '拾荒鼠救助完成';
 if(f.type>=20&&f.type<28)return facilityName(f.target)+'建成';
 if(f.type===40)return '开始记录时已有大树（阶段 '+f.value+'）';
 if(f.type>=60&&f.type<68)return '开始记录时已有'+facilityName(f.target);
 if(f.type>=90&&f.type<=93)return ['第一场干旱结束','完成档案后的幼苗观察','第一次乘客椅休息','乘客椅使用许可'][f.type-90];
 if(f.type>=70&&f.type<72)return '开始记录时已有'+(f.type===70?'寻雨雀':'拾荒鼠');
 return '里程碑 #'+f.type;
}
function factDetail(f){
 const scope=f.scope==='baseline_already_present'?'旧档基线，原发生时间未知':
  f.scope==='first_recorded_since_tracking'?'自本次记录启用以来首次记录':'从新游戏开始记录';
 return scope+' · 证据 #'+f.evidence_id+' · 决策链 #'+f.chain+' · '+
  (f.player_participated?'有玩家参与依据':'不认定为共同经历')+(f.cost_known?' · '+costText(f):'');
}
function updateArchiveStatus(error){
 $('archive-status').textContent=error||(!currentArchive.available?'等待记忆版设备快照':
  '电脑已归档 '+currentArchive.events+' 条 · 长期记忆 '+currentArchive.long_term+' 条 · 未收到的详细事件 '+
  currentArchive.missing_details+' 条'+(archiveBefore?' · 正在查看较早记录':' · 最新在前'));
}
async function refreshArchive(){
 if(archiveBusy||!currentArchive.available)return;
 archiveBusy=true;
 try{
  const category=$('archive-category').value,chain=$('archive-chain').value||'0';
  const q=new URLSearchParams({category,chain,before:String(archiveBefore),limit:'30'});
  const response=await fetch('/api/archive?'+q,{cache:'no-store'});
  if(!response.ok)throw Error('归档查询失败');
  const result=await response.json();archiveNext=result.next_before||0;
  const target=$('archive-list');target.replaceChildren();
  if(!result.records.length)target.append(el('p','这个范围暂无收到的记录。','empty'));
  for(const record of result.records){
   const item=el('div',undefined,'archive-item');
   item.append(el('strong',(categories[record.category]||'事件')+' · '+recordTitle(record)),el('small',recordDetail(record)));
   target.append(item);
  }
  $('archive-older').disabled=!archiveNext;
 }catch(e){$('archive-status').textContent=String(e)}finally{archiveBusy=false}
}
function setupArchive(){
 $('archive-filter').addEventListener('click',()=>{archiveBefore=0;refreshArchive()});
 $('archive-latest').addEventListener('click',()=>{archiveBefore=0;refreshArchive()});
 $('archive-older').addEventListener('click',()=>{archiveBefore=archiveNext;refreshArchive()});
}
