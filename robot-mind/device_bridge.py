"""Opt-in USB intent bridge. Only actions and device-event IDs cross the boundary."""
import json
import queue
import re
import sqlite3
import threading
import time
from copy import deepcopy
from pathlib import Path
from director import decide, policy_hash
from observation import enrich,roadshow_personality

INTENTS={'watch_window':1,'inspect_plant':2,'rest':3}


def context(snapshot,focus=None):
    w=snapshot['world'];m=snapshot['runtime']['mind'];world=snapshot['memory']['world_id']
    if not re.fullmatch(r'(normal|demo)-[0-9a-f]{16}',world):raise ValueError('invalid world')
    life=w['life'];history=life.get('history',[])
    roadshow=w.get('mode')=='roadshow'
    pending=None
    for old in reversed(history):
        if old['result']!='interrupted_by_work':continue
        same=lambda x:(x['intent'],x['target_slot'])==(old['intent'],old['target_slot'])
        if not any(x['id']>old['id'] and same(x) and x['result']=='completed' for x in history):
            pending=old;break
    claims=[dict(id='idle',text='设备确认现在可提出个人活动，没有紧急工作；所列动作当前可用。',certainty='fact')]
    decisions=[];candidates=[]
    for intent,n in INTENTS.items():
        for target in range(4 if n==2 else 1):
            if not m['mask'] & (1<<((n-1)*4+target)):continue
            reason={'watch_window':'WORK_FINISHED_OBSERVE_WINDOW','inspect_plant':'ROUTINE_PLANT_CHECK','rest':'SHORT_REST_AFTER_WORK'}[intent]
            source=['idle']
            if pending and pending['intent']==intent and pending['target_slot']==(target+1 if n==2 else 0):
                reason='RESUME_INTERRUPTED_ACTIVITY';source=['unfinished']
            decisions.append(dict(id='%s:%s'%(n,target),intent=intent,target=target,reason=reason,source_ids=source))
    if pending:
        claims.append(dict(id='unfinished',text='之前的%s被工作打断，实际到达=%s，尚未完成。'%(pending['intent'],pending['arrived']),certainty='fact'))
    social_state=snapshot['runtime'].get('communication',{}).get('social',{})
    social_candidates=[]
    latest_social={}
    if social_state and not social_state.get('pending'):
        for record in history:
            intent=INTENTS.get(record['intent'],0)
            if (intent not in (1,3) and not (roadshow and intent==2)) or record['result']!='completed' or not record['arrived']:continue
            if roadshow:
                if record['id']<=social_state.get('last_life_source',social_state.get('last_offered_id',0)):continue
            elif record['id']<=social_state.get('last_offered_id',0) or social_state.get('topic_mask',0)&(1<<intent):continue
            candidate=dict(id=str(record['id']),event_id=record['id'],
                topic='FIRST_WINDOW_PAUSE' if intent==1 else 'FIRST_REST' if intent==3 else 'ROUTINE_CHECK_COMPLETED',actual_record=deepcopy(record))
            if intent not in latest_social or record['id']>latest_social[intent]['event_id']:
                latest_social[intent]=candidate
        social_candidates=sorted(latest_social.values(),key=lambda x:x['event_id'],reverse=True)
    for plant in w['plants']:
        if any(d['intent']=='inspect_plant' and d['target']==plant['slot']-1 for d in decisions):
            cid='plot-%s'%plant['slot']
            claims.append(dict(id=cid,text='种植位%s当前种植%s，可以例行观察；没有额外生长观察。'%(plant['slot'],plant['name']),certainty='fact'))
            candidates.append(dict(id=cid,label='观察'+plant['name'],question='例行检查这株植物',certainty='interest',source_ids=[cid]))
    ctx=dict(world_id=world,revision=w['revision'],request_id='%s-%s-%s'%(world,m['boot'],m['offer']),
        roadshow_mode=roadshow,language_stage=w.get('story',{}).get('language_stage',life['language_stage']),trigger='idle',
        busy=False,voice_active=False,cooldown_ready=True,current_focus=focus,
        focus_candidates=candidates,decisions=decisions,claims=claims,history=history,
        unfinished_activity=pending,recent_completed=[x for x in history if x['result']=='completed'][-2:],
        social_candidates=social_candidates,recent_bubbles=[])
    ctx = enrich(snapshot,ctx)
    agency=deepcopy(snapshot['runtime'].get('observation',{}).get('agency',{}))
    if agency:
        ctx['agency']=agency
        kept=[]
        for d in ctx['decisions']:
            personal=d['intent'] in ('watch_window','rest') or (d['intent']=='inspect_plant' and d['target']>=4)
            if personal and (agency.get('level',0)==0 or agency.get('pending') or agency.get('wish',{} ) and agency['wish'].get('status')=='executing'):continue
            d['execution_mode']='ASK_PERMISSION' if personal else 'ROUTINE'
            kept.append(d)
        ctx['decisions']=kept
        if agency.get('pending'):ctx['social_candidates']=[]
    return roadshow_personality(snapshot,ctx)


def encode(snapshot,proposal):
    m=snapshot['runtime']['mind'];world=snapshot['memory']['world_id'].split('-')[1]
    action=proposal['action'];intent=INTENTS[action['intent']] if action else 0
    target=action['target'] if action else 0
    if not isinstance(target,int) or not 0<=target<=3 or (intent!=2 and target):raise ValueError('invalid target')
    if intent and not m['mask'] & (1<<((intent-1)*4+target)):raise ValueError('action unavailable')
    values=[m['boot'],m['offer'],int(world[:8],16),int(world[8:],16),m['sequence'],intent,target]
    social=proposal.get('social_intent')
    if snapshot['runtime'].get('observation',{}).get('schema')==55:
        candidates=context(snapshot)['social_candidates']
        kind=social.get('evidence_kind',1) if social else 0
        source=social['related_event'] if social else 0
        if social and not any(x['event_id']==source and x.get('evidence_kind',1)==kind for x in candidates):raise ValueError('social event unavailable')
        focus=proposal.get('focus_update');mode=2 if focus else 1 if proposal.get('focus_mode')=='NONE' else 0
        f=focus.get('device_focus',{}) if focus else {}
        if focus and not any(x['id']==focus.get('id') and x.get('device_focus')==f for x in context(snapshot)['focus_candidates']):raise ValueError('focus evidence unavailable')
        values.extend([source,kind,mode,f.get('kind',0),f.get('source',0),f.get('target',0)])
        if any(not isinstance(x,int) or not 0<=x<=0xffffffff for x in values):raise ValueError('invalid word')
        return ('MIND3 '+' '.join(format(x,'x')for x in values)+'\n').encode('ascii')
    if social:
        candidates=context(snapshot)['social_candidates']
        if not any(x['event_id']==social['related_event'] for x in candidates):raise ValueError('social event unavailable')
        values.append(social['related_event'])
    if any(not isinstance(x,int) or not 0<=x<=0xffffffff for x in values):raise ValueError('invalid word')
    return (('MIND2 ' if social else 'MIND1 ')+' '.join(format(x,'x')for x in values)+'\n').encode('ascii')


class DeviceBridge:
    def __init__(self,path):
        path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
        self.db=sqlite3.connect(str(path),check_same_thread=False)
        path.chmod(0o600)
        self.db.execute('CREATE TABLE IF NOT EXISTS requests(id TEXT PRIMARY KEY, world TEXT, phase TEXT, input TEXT, response TEXT, sequence INTEGER)')
        self.db.execute('CREATE TABLE IF NOT EXISTS focus(world TEXT PRIMARY KEY, value TEXT)');self.db.commit()
        self.lock=threading.RLock();self.jobs=queue.Queue(maxsize=1);self.outbox=queue.Queue(maxsize=1)
        self.last=None;self.received=0;self.inflight=False;self.handled=set();self.status=dict(phase='等待设备机会',automatic_bubbles=False)
        threading.Thread(target=self.worker,daemon=True).start()

    def view(self):
        with self.lock:
            recent=[]
            for rid,phase,raw in self.db.execute('SELECT id,phase,response FROM requests ORDER BY rowid DESC LIMIT 5'):
                result=json.loads(raw);proposal=result.get('proposal',{})
                recent.append(dict(id=rid,phase=phase,decision_source=result.get('decision_source','LLM'),
                    trigger_reason=result.get('trigger_reason','legacy_record'),seconds=result.get('seconds'),
                    action=proposal.get('action'),social=proposal.get('social_intent'),focus=proposal.get('focus_update'),
                    actual_result=result.get('actual_result'),error=result.get('error')))
            return dict(deepcopy(self.status),inflight=self.inflight,recent_decisions=recent)

    def on_line(self,line):
        match=re.search(rb'MIND_(ACCEPT|REJECT) offer=([0-9]+)',line)
        if match:
            with self.lock:
                self.status.update(device_reply=match[1].decode(),device_reply_offer=int(match[2]))

    def on_snapshot(self,s):
        with self.lock:
            self.last=deepcopy(s)
            self.received=time.monotonic()
            m=s['runtime'].get('mind')
            if not m:
                self.status.update(phase='固件尚无USB意图入口');return
            world=s['memory']['world_id']
            communication=s['runtime'].get('communication',{})
            self.status.update(automatic_bubbles=bool(communication),communication=deepcopy(communication))
            # Confirmation uses device telemetry, not the fact that bytes were written.
            for row in self.db.execute("SELECT id,response,sequence FROM requests WHERE world=? AND phase IN ('sent','device_accepted','finished_wish_pending','wish_executing')",(world,)).fetchall():
                rid,raw,sequence=row;response=json.loads(raw);offer=response['device_offer'];boot=response['device_boot']
                action=response['proposal']['action']
                same_offer=(s['world']['storage_available'] and m['boot']==boot and m['offer']==offer)
                if response.get('actual_wish'):
                    agency=communication.get('agency',{})
                    records=agency.get('history',[])+([agency['wish']] if agency.get('wish') else [])
                    wish=next((x for x in reversed(records) if x['token']==response['actual_wish']['token']),None)
                    if wish:
                        response['actual_wish']=deepcopy(wish)
                        status=wish['status']
                        if status=='executing':
                            response['actual_action_status']='executing_after_permission'
                            phase='wish_executing'
                        elif status not in ('proposed','presented'):
                            response['actual_action_status']=status
                            response['actual_result']=deepcopy(wish)
                            phase='finished_wish_result'
                        else:phase='finished_wish_pending'
                        self.db.execute('UPDATE requests SET phase=?,response=?,sequence=? WHERE id=?',
                            (phase,json.dumps(response,ensure_ascii=False),wish['action_sequence'],rid))
                        self.status.update(phase='愿望实际状态：'+status,actual_wish=deepcopy(wish),action_status=response['actual_action_status'])
                    continue
                social=response['proposal'].get('social_intent')
                actual_social=communication.get('social',{})
                social_confirmed=bool(social and (m.get('social_status')==1 or
                    (actual_social.get('source_id',actual_social.get('last_offered_id'))==social['related_event'] and actual_social.get('source_kind',1)==social.get('evidence_kind',1))))
                if same_offer and m['status']==6:
                    actual_wish=communication.get('agency',{}).get('wish')
                    if actual_wish and actual_wish.get('status') in ('proposed','presented'):
                        response.update(actual_action_status='awaiting_permission',actual_wish=deepcopy(actual_wish),actual_result=None,social_accepted=False)
                        self.db.execute('UPDATE requests SET phase=?,response=? WHERE id=?',
                            ('finished_wish_pending',json.dumps(response,ensure_ascii=False),rid))
                        self.status.update(phase='愿望已提出；等待玩家许可，身体未行动',action_status='awaiting_permission',actual_wish=deepcopy(actual_wish))
                        continue
                if same_offer and m['status']==4 and social_confirmed:
                    response.update(accepted_social_event=social['related_event'],actual_action_status='not_started',social_accepted=True,
                        character_accepted=m.get('focus_status')==1,device_character=deepcopy(s['runtime'].get('observation',{}).get('character')),actual_social=deepcopy(actual_social))
                    self.db.execute('UPDATE requests SET phase=?,response=? WHERE id=?',
                        ('finished_social_only',json.dumps(response,ensure_ascii=False),rid))
                    self.status.update(phase='分享邀请已保留；动作未开始' if action else '分享邀请已保留；未请求新动作',social_accepted=True,character_accepted=m.get('focus_status')==1,action_status='not_started')
                    continue
                if same_offer and m.get('focus_status')==1 and m['status']==5:
                    response.update(actual_action_status='not_started',character_accepted=True,device_character=deepcopy(s['runtime'].get('observation',{}).get('character')))
                    self.db.execute('UPDATE requests SET phase=?,response=? WHERE id=?',
                        ('finished_character_only',json.dumps(response,ensure_ascii=False),rid))
                    self.status.update(phase='共享关注已确认；没有新身体动作',action_status='not_started',character_accepted=True)
                    continue
                confirmed=(s['world']['storage_available'] and m['boot']==boot and m['offer']==offer and
                    m['status'] in [2,3] and (not action or (m['status']==2 and m.get('current_life_sequence')==m['sequence'])))
                if not sequence and confirmed:
                    sequence=m['sequence'];response['confirmed_sequence']=sequence
                    response['social_accepted']=social_confirmed
                    response['character_accepted']=m.get('focus_status')==1
                    response['device_character']=deepcopy(s['runtime'].get('observation',{}).get('character'))
                    response['actual_social']=deepcopy(actual_social) if social_confirmed else None
                    self.status.update(social_accepted=social_confirmed,character_accepted=m.get('focus_status')==1)
                    if response['proposal'].get('focus_update') and s['runtime'].get('observation',{}).get('schema')!=55:
                        self.db.execute('INSERT OR REPLACE INTO focus VALUES(?,?)',(world,json.dumps(response['proposal']['focus_update'],ensure_ascii=False)))
                    self.db.execute('UPDATE requests SET phase=?,response=?,sequence=? WHERE id=?',('device_accepted' if action else 'finished_no_action',json.dumps(response,ensure_ascii=False),sequence,rid))
                    self.status.update(phase='设备已接受',life_sequence=sequence)
                history=s['world']['life'].get('history',[])
                result=next((x for x in history if sequence and x['id']==sequence),None)
                if result and response['proposal']['action']:
                    response['actual_result']=result
                    self.db.execute('UPDATE requests SET phase=?,response=? WHERE id=?',('finished',json.dumps(response,ensure_ascii=False),rid))
                    self.status.update(phase='设备实际结果：'+result['result'],actual_result=result)
                elif not sequence and m['boot']==boot and m['offer']==offer and not m['mask'] and not m.get('reply_deadline_ms',m['deadline_ms']) and m['status']==1:
                    self.db.execute('UPDATE requests SET phase=? WHERE id=?',('device_not_accepted',rid))
                    self.status.update(phase='机会已过或工作状态改变，等待下次空闲')
            self.db.commit()
            deadline=m.get('reply_deadline_ms',m['deadline_ms'])
            voice=s['runtime'].get('voice','idle')
            eligible=m.get('mask') or m.get('trigger_kind',0)
            # Work may invalidate the next body action before a USB snapshot arrives.
            # A completed personal experience can still be shared in the device's
            # existing reply window; this never creates an unoffered action/request.
            if (not eligible and s['world'].get('mode')=='roadshow' and
                    voice=='idle' and deadline>s['uptime_ms']+2500):
                eligible=any(c.get('actual_record',{}).get('intent') in ('watch_window','rest')
                             for c in context(s).get('social_candidates',[]))
            self.status['budget']=dict(eligible=bool(eligible),voice_idle=voice=='idle',inflight=self.inflight,remaining_ms=max(0,deadline-s['uptime_ms']),cooldown_seconds=30 if s['world'].get('mode')=='roadshow' else 120)
            if not eligible or voice!='idle' or self.inflight or deadline<=s['uptime_ms']+2500:return
            key=(world,m['boot'],m['offer'])
            if key in self.handled:return
            self.handled.add(key)
            if len(self.handled)>64:self.handled={key}
            value=self.db.execute('SELECT value FROM focus WHERE world=?',(world,)).fetchone()
            focus=json.loads(value[0]) if value else None
            ctx=context(s,focus)
            if self.db.execute('SELECT 1 FROM requests WHERE id=?',(ctx['request_id'],)).fetchone():return
            try:self.jobs.put_nowait((deepcopy(s),ctx,time.monotonic()+(deadline-s['uptime_ms'])/1000))
            except queue.Full:return
            self.inflight=True
            self.status.update(phase='本地模型正在选择',request_id=ctx['request_id'],actual_result=None,
                social_accepted=False,action_status='not_started',device_reply=None)

    def worker(self):
        while True:
            s,ctx,expires=self.jobs.get()
            try:
                remaining=expires-time.monotonic()-2
                if remaining<=0:raise TimeoutError('request expired before model call')
                result=decide(ctx,timeout=min(15,remaining));proposal=result['proposal']
                result.update(trigger_reason=ctx.get('trigger_reason','IDLE_OPPORTUNITY'),decision_source='LLM',action_request_id=ctx['request_id'])
                if not proposal['decision_accepted']:
                    result['failure']='model_validation_rejected'
                    with self.lock:
                        self.db.execute('INSERT OR IGNORE INTO requests VALUES(?,?,?,?,?,?)',(ctx['request_id'],ctx['world_id'],'model_validation_rejected',json.dumps(ctx,ensure_ascii=False),json.dumps(result,ensure_ascii=False),0));self.db.commit()
                    raise ValueError('rejected proposal')
                result.update(device_boot=s['runtime']['mind']['boot'],device_offer=s['runtime']['mind']['offer'])
                data=encode(s,proposal)
                with self.lock:
                    self.db.execute('INSERT OR IGNORE INTO requests VALUES(?,?,?,?,?,?)',
                        (ctx['request_id'],ctx['world_id'],'generated',json.dumps(ctx,ensure_ascii=False),json.dumps(result,ensure_ascii=False),0));self.db.commit()
                    self.status.update(phase='决定等待设备校验',decision=proposal['action'],reason=proposal['reason'],
                        social_intent=proposal.get('social_intent'),
                        trigger_reason=ctx.get('trigger_reason','IDLE_OPPORTUNITY'),validated_intent=deepcopy(proposal),
                        raw_bubble=proposal['raw_bubble'],bubble_displayed=False,focus=proposal['focus_update'],model_seconds=result['seconds'])
                self.outbox.put_nowait((s,ctx['request_id'],data,bool(proposal.get('social_intent'))))
            except Exception as exc:
                with self.lock:
                    failure=dict(error=type(exc).__name__,detail=str(exc)[:160],decision_source='LLM',trigger_reason=ctx.get('trigger_reason','IDLE_OPPORTUNITY'),policy_sha256=policy_hash())
                    self.db.execute('INSERT OR IGNORE INTO requests VALUES(?,?,?,?,?,?)',(ctx['request_id'],ctx['world_id'],'model_failed',json.dumps(ctx,ensure_ascii=False),json.dumps(failure,ensure_ascii=False),0));self.db.commit()
                    self.inflight=False
                    self.status.update(phase='模型不可用，设备将使用本地规则',error=type(exc).__name__)
            finally:self.jobs.task_done()

    def flush(self,stream):
        try:s,rid,data,has_social=self.outbox.get_nowait()
        except queue.Empty:return
        with self.lock:
            live=self.last or {};m=live.get('runtime',{}).get('mind',{});old=s['runtime']['mind']
            elapsed=time.monotonic()-self.received
            extended=data.startswith(b'MIND3 ')
            deadline=m.get('reply_deadline_ms',m.get('deadline_ms',0)) if has_social or extended else m.get('deadline_ms',0)
            valid=(elapsed<10 and live.get('memory',{}).get('world_id')==s['memory']['world_id'] and
                m.get('boot')==old['boot'] and m.get('offer')==old['offer'] and (has_social or extended or m.get('mask')) and
                deadline>live.get('uptime_ms',0)+elapsed*1000+1000)
            if valid:
                stream.write_timeout=.3
                try:
                    written=stream.write(data)
                    phase='sent' if written==len(data) else 'write_failed'
                except (OSError,ValueError):phase='write_failed'
            else:phase='stale_discarded'
            self.db.execute('UPDATE requests SET phase=? WHERE id=?',(phase,rid));self.db.commit()
            self.status.update(transport_status=phase)
            self.inflight=False
        self.outbox.task_done()
