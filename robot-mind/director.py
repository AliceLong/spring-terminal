"""Decision-first local prototype. Coordinator below is a simulator, not device code."""
import hashlib
import json
import time
import urllib.request
from copy import deepcopy
from pathlib import Path
from mind import compile_proposal, LIMITS

POLICY = Path(__file__).with_name('director-persona.md').read_text()
VISUALS = {
    'watch_window': ['前往窗边', '到达后停留观察'],
    'inspect_plant': ['前往种植位', '到达后检查植物'],
    'rest': ['前往现有休息点', '到达后原地休息'],
}


def options(s):
    focus_ids=['KEEP','NONE']+[x['id'] for x in s['focus_candidates']]
    active=s['trigger']=='idle' and not s['busy'] and not s['voice_active'] and s['cooldown_ready']
    decisions=[dict(id='NONE', intent='none', target=None, reason='NO_NEW_ACTION',source_ids=[])]
    if active:decisions.extend(s['decisions'])
    return focus_ids,decisions


def contract(s):
    f,d=options(s)
    invitation_only=s.get('sharing_style',{}).get('expression_channel')=='INVITATION_THEN_VOICE'
    bubble_schema=dict(type='string',enum=['']) if invitation_only else dict(type='string')
    return dict(type='object',additionalProperties=False,properties={
        'decision_id':dict(type='string',enum=[x['id'] for x in d]),
        'focus_id':dict(type='string',enum=f),'bubble':bubble_schema,
        'social_id':dict(type='string',enum=['NONE']+[x['id'] for x in s.get('social_candidates',[])])},
        required=['decision_id','focus_id','bubble','social_id'])


def resolve(out,s,current=None):
    """Validate decisions independently from prose so a bad bubble cannot cancel a sound action."""
    errors=[]
    if not isinstance(out,dict) or set(out) not in ({'decision_id','focus_id','bubble'}, {'decision_id','focus_id','bubble','social_id'}) or any(not isinstance(v,str) for v in out.values()):
        return dict(decision_accepted=False,errors=['invalid_contract'],expression_errors=[],bubble='',action=None)
    f,d=options(s);selected=next((x for x in d if x['id']==out['decision_id']),None)
    if selected is None: errors.append('unavailable_decision')
    if out['focus_id'] not in f:errors.append('unavailable_focus')
    social_id=out.get('social_id','NONE')
    social=next((deepcopy(x) for x in s.get('social_candidates',[]) if x['id']==social_id),None)
    if social_id!='NONE' and social is None:errors.append('unavailable_social_event')
    if social and (s['busy'] or s['voice_active'] or not s['cooldown_ready']):errors.append('social_not_idle')
    if current:
        if (s['world_id'],s['revision'])!=(current['world_id'],current['revision']):errors.append('stale_context')
        current_f,current_d=options(current)
        if out['decision_id'] not in [x['id'] for x in current_d]:errors.append('decision_changed')
        if out['focus_id'] not in current_f:errors.append('focus_changed')
        if selected and selected['intent']!='none' and not any(x==selected for x in current_d):errors.append('target_or_reason_changed')
        if social and social not in current.get('social_candidates',[]):errors.append('social_event_changed')
        if social and (current['busy'] or current['voice_active'] or not current['cooldown_ready']):errors.append('social_not_idle')
    if errors:return dict(decision_accepted=False,errors=errors,expression_errors=[],bubble='',action=None)
    selected=deepcopy(selected)
    focus=next((deepcopy(x) for x in s['focus_candidates'] if x['id']==out['focus_id']),None)
    # Reuse the prototype's conservative prose guard, without letting it override decisions.
    claims=list(dict.fromkeys(selected['source_ids']+(focus['source_ids'] if focus else [])))
    if not claims:claims=[x['id'] for x in s['claims']]
    legacy=dict(s,actions=[dict(id=x['id'],intent=x['intent'],target=x['target']) for x in d if x['intent']!='none'])
    speech=compile_proposal(dict(focus_id=out['focus_id'],action_id=out['decision_id'],
        bubble=out['bubble'],claim_ids=claims if out['bubble'] else []),legacy)
    action=selected if selected['intent']!='none' else None
    return dict(decision_accepted=True,errors=[],request_id=s['request_id'],world_id=s['world_id'],revision=s['revision'],
        action=action,reason=selected['reason'],related_event_ids=selected['source_ids'],
        focus_mode=out['focus_id'],focus_update=focus,
        animation_plan=[] if selected.get('execution_mode')=='ASK_PERMISSION' else VISUALS.get(selected['intent'],[]),
        social_intent=dict(type='SOCIAL_INTENT',intent='SHARE_EXPERIENCE',related_event=social['event_id'],evidence_kind=social.get('evidence_kind',1),topic=social['topic']) if social else None,
        requested_expression='SHORT_BUBBLE' if out['bubble'] else 'NONE',
        expression_errors=speech['errors'],bubble=speech['bubble'],raw_bubble=out['bubble'],
        expression_status='REQUIRES_REVIEW' if speech['bubble'] else 'SILENT',
        display_allowed=False,execution_status=('wish_proposed' if selected.get('execution_mode')=='ASK_PERMISSION' else 'proposed') if action else 'none',
        resource_mutations=[],story_mutations=[])


def decide(s,model='qwen3.5-4b',timeout=15):
    if s['busy'] or s['voice_active'] or (s['trigger']=='idle' and not s['cooldown_ready']):
        out=dict(decision_id='NONE',focus_id='KEEP',bubble='',social_id='NONE')
        return dict(model_called=False,seconds=0,output=out,proposal=resolve(out,s))
    f,d=options(s)
    context={k:deepcopy(v) for k,v in s.items() if k!='decisions'}
    context.update(decisions=d,allowed_focus_ids=f,bubble_max_chars=LIMITS[s['language_stage']])
    payload=dict(model=model,reasoning_effort='none',temperature=0.5,max_tokens=192,stream=False,
        messages=[dict(role='system',content=POLICY),dict(role='user',content=json.dumps(context,ensure_ascii=False))],
        response_format=dict(type='json_schema',json_schema=dict(name='life_decision',strict=True,schema=contract(s))))
    req=urllib.request.Request('http://127.0.0.1:1234/v1/chat/completions',data=json.dumps(payload).encode(),headers={'Content-Type':'application/json'})
    op=urllib.request.build_opener(urllib.request.ProxyHandler({}));start=time.monotonic()
    response=json.load(op.open(req,timeout=timeout));duration=time.monotonic()-start
    c=response['choices'][0]
    if c['finish_reason']!='stop' or c['message'].get('reasoning_content'):raise ValueError('truncated or thinking')
    out=json.loads(c['message']['content'])
    return dict(model_called=True,seconds=round(duration,3),output=out,raw=response,proposal=resolve(out,s),policy_sha256=policy_hash(),request_messages=payload['messages'])


class SimCoordinator:
    """All lifecycle events are explicitly marked simulated. No economic implementation."""
    def __init__(self,world_id):
        self.world_id=world_id;self.revision=1;self.events=[];self.active=None
        self.seen=set();self.focus=None;self.resources=dict(water=16,energy=12,fiber=0,food=0,parts=2)
        self.pending=None

    def snapshot(self,request_id,stage=1,trigger='idle'):
        return dict(world_id=self.world_id,revision=self.revision,request_id=request_id,
            language_stage=stage,trigger=trigger,busy=False,voice_active=False,cooldown_ready=True,
            current_focus=deepcopy(self.focus),focus_candidates=[],claims=[],decisions=[],
            unfinished_activity=deepcopy(self.pending),
            recent_completed=[deepcopy(e['action']) for e in self.events if e['kind']=='completed'][-2:],
            history=deepcopy(self.events[-12:]),recent_bubbles=[])

    def record(self,kind,**data):
        e=dict(id='sim-'+str(len(self.events)+1),kind=kind,simulated=True,**deepcopy(data))
        self.events.append(e);self.revision+=1;return e

    def accept(self,out,requested,current=None):
        if requested['request_id'] in self.seen:return dict(decision_accepted=False,errors=['duplicate_request'])
        live=deepcopy(current or requested)
        if current is None:live.update(world_id=self.world_id,revision=self.revision)
        elif (current['world_id'],current['revision'])!=(self.world_id,self.revision):
            return dict(decision_accepted=False,errors=['coordinator_context_mismatch'])
        p=resolve(out,requested,live)
        if not p['decision_accepted']:return p
        if self.active and p['action']:return dict(decision_accepted=False,errors=['body_busy'])
        self.seen.add(requested['request_id'])
        if p['focus_update']:self.focus=deepcopy(p['focus_update'])
        elif p['focus_mode']=='NONE':self.focus=None
        event=self.record('decision',decision_id=out['decision_id'],action=p['action'],
            focus=deepcopy(self.focus),reason=p['reason'],source_ids=p['related_event_ids'])
        if p['action']:self.active=dict(chain_id=event['id'],action=deepcopy(p['action']),phase='requested',arrived=False)
        return p

    def transition(self,phase):
        if not self.active:raise ValueError('no active action')
        allowed={'requested':['traveling','interrupted_by_work'],
            'traveling':['arrived','interrupted_by_work','interrupted_by_restart'],
            'arrived':['observing','interrupted_by_work'],
            'observing':['paused_for_voice','completed','interrupted_by_work'],
            'paused_for_voice':['observing','interrupted_by_work']}
        if phase not in allowed.get(self.active['phase'],[]):raise ValueError('illegal lifecycle transition')
        if phase=='arrived':self.active['arrived']=True
        self.active['phase']=phase
        e=self.record(phase,**deepcopy(self.active))
        if phase in ['interrupted_by_work','interrupted_by_restart']:
            self.pending=dict(action=deepcopy(self.active['action']),source_event_id=e['id'],
                arrived=self.active['arrived'],status='unfinished')
        if phase=='completed' and self.pending:
            a=self.pending['action'];b=self.active['action']
            if (a['intent'],a['target'])==(b['intent'],b['target']):self.pending=None
        if phase in ['completed','interrupted_by_work','interrupted_by_restart']:self.active=None
        return e

    def replay(self):
        """Materialized view for a future XiaoZhi tool adapter, not generated by a model."""
        cue='idle'
        if self.active:
            phase=self.active['phase'];intent=self.active['action']['intent']
            cue=('walk_to_target' if phase=='traveling' else
                 'listen' if phase=='paused_for_voice' else
                 intent if phase in ['arrived','observing'] else 'wait_for_dispatch')
        return dict(world_id=self.world_id,revision=self.revision,focus=deepcopy(self.focus),unfinished_activity=deepcopy(self.pending),
            current=deepcopy(self.active),presentation_cue=cue,recent=deepcopy(self.events[-12:]),simulated=True)


def policy_hash():return hashlib.sha256(POLICY.encode()).hexdigest()
