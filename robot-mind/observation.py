"""Device facts in, bounded model observation out. No story inference or game writes."""
from copy import deepcopy

REASONS={0:'IDLE_OPPORTUNITY',1:'PLAYER_REQUEST_COMPLETED',2:'READ_ARCHIVE_CHANGED_KNOWLEDGE',3:'VERIFIED_MILESTONE_COMPLETED'}


def focus_id(kind,source,target):
    return f'plot-{target+1}' if kind==1 else f'experience-{source}' if kind==2 else f'archive-{source}' if kind==3 else f'milestone-{source}'


def enrich(snapshot,ctx):
    runtime=snapshot['runtime'];obs=runtime.get('observation',{})
    if obs.get('schema')!=55:return ctx
    w=snapshot['world'];m=runtime['mind'];character=deepcopy(obs.get('character',{}))
    if not character.get('available'):raise ValueError('device character state unavailable')
    ctx['observation_version']=55
    if not m.get('mask'):ctx['claims'][0]['text']='本次只考虑已发生的重要事件；没有可用的身体动作。'
    ctx['world_observation']=dict(resources=deepcopy(w.get('resources',{})),region=w.get('economy',{}).get('region',w.get('journey')),
        tree=deepcopy(w.get('tree',{})),robot=dict(action=runtime.get('fsm'),work=w.get('robot',{}).get('work'),life_phase=w['life'].get('phase')),plants=[{k:p[k] for k in ('slot','id','name','ready','progress','next') if k in p} for p in w['plants']],
        facilities=[{k:f[k] for k in ('id','name','built') if k in f} for f in w.get('economy',{}).get('facilities',[])])
    # Archive text comes exclusively from the firmware's read-and-unlocked intersection.
    allowed_archives={a['id'] for a in w.get('story',{}).get('archives',[]) if a.get('read')}
    archives=[deepcopy(a) for a in obs.get('known_archives',[]) if a.get('available') and a.get('player_read') and a.get('id') in allowed_archives][:7]
    milestones=deepcopy(obs.get('milestones',[]))[:10]
    ctx['story_observation']=dict(language_stage=ctx['language_stage'],active_node=w.get('story',{}).get('active_node',255),known_archives=archives,chapter_context=deepcopy(obs.get('chapter_context',{})))
    chapter=obs.get('chapter_context',{})
    if chapter.get('observation_verified'):
        ctx['claims'].append(dict(id='chapter-observation',text='设备记录已完成档案后的头顶幼苗观察；不证明长出新叶或恢复过去记忆。',certainty='fact'))
    if chapter.get('seat_rest_verified'):
        ctx['claims'].append(dict(id='chapter-seat',text='设备记录已经完成乘客椅上的休息。以允许动作与当前许可决定下一步，不证明玩家陪坐或共同观看。',certainty='fact'))
    ctx['character_observation']=character
    trigger=dict(kind=m.get('trigger_kind',0),source_id=m.get('trigger_source',0),target=m.get('trigger_target',0))
    trigger['reason']=REASONS.get(trigger['kind'],'IDLE_OPPORTUNITY')
    ctx['trigger_event']=trigger;ctx['trigger_reason']=trigger['reason']
    ctx['roadshow_mode']=w.get('mode')=='roadshow'
    ctx['attention_budget']=dict(model_cooldown_seconds=30 if ctx['roadshow_mode'] else 120,reply_ttl_seconds=20,max_inflight=1,max_pending_contact=1)
    ctx['claims'].append(dict(id='world',text='当前资源与植物只以world_observation为准；不能凭检查动作断言生长或病害。',certainty='fact'))
    important=character.get('important_invitation')
    if important:
        cid=f"invitation-{important['id']}"
        target=important['target'];name=important.get('target_name') or (f'种植位{target+1}' if target<4 else '纪念花' if target==4 else '动物小窝')
        ctx['claims'].append(dict(id=cid,text=f"你曾应邀请查看{name}，已到达并完成；玩家发出了邀请，未证实全程观看。",certainty='fact'))
        ctx['focus_candidates'].append(dict(id=focus_id(2,important['id'],important['target']),label=f'上次受托查看的{name}',
            question=f'下次查看{name}。' if ctx['language_stage']<2 else '这次被请去检查的经历，值得继续留意吗？',certainty='interest',source_ids=[cid],
            device_focus=dict(kind=2,source=important['id'],target=important['target'])))
    for f in ctx['focus_candidates']:
        if f['id'].startswith('plot-'):f['device_focus']=dict(kind=1,source=0,target=int(f['id'][5:])-1)
    for a in archives:
        cid=f"archive-{a['id']}"
        ctx['claims'].append(dict(id=cid,text=a['excerpt'],certainty='fact'))
        ctx['focus_candidates'].append(dict(id=cid,label=a['title'],question='这份旧记录和现在的照料，有什么不同？',certainty='interest',source_ids=[cid],
            device_focus=dict(kind=3,source=a['id'],target=0)))
    for fact in milestones:
        cid=f"milestone-{fact['id']}"
        label='大树完成成长' if fact['kind']==7 else '设施已经落成'
        ctx['claims'].append(dict(id=cid,text=f"{label}，证据编号{fact['id']}；不证明玩家观看。",certainty='fact'))
        ctx['focus_candidates'].append(dict(id=cid,label=label,question='这里多了一种可以做的事情。',certainty='interest',source_ids=[cid],
            device_focus=dict(kind=4,source=fact['id'],target=fact['type'])))
    focus=character.get('current_focus')
    if focus:
        ctx['current_focus']=dict(focus,id=focus_id(focus['kind'],focus['source_id'],focus['target']),certainty='interest')
    else:ctx['current_focus']=None
    social=runtime.get('communication',{}).get('social',{})
    ctx['social_candidates']=[]
    if social.get('pending'):return ctx
    mask=social.get('topic_mask',0);last=social.get('last_life_source',social.get('last_offered_id',0))
    records={r['id']:r for r in w['life'].get('history',[])}
    if important:
        records[important['id']]=dict(important,intent='inspect_plant',target_slot=important['target']+1 if important['target']<4 else 0,reason='player_invitation')
    latest={}
    for record in records.values():
        intent={'watch_window':1,'inspect_plant':2,'rest':3}.get(record.get('intent'),0)
        invited=intent==2 and record.get('reason')=='player_invitation'
        if record.get('result')!='completed' or not record.get('arrived') or (intent not in (1,3) and not invited and not (ctx['roadshow_mode'] and intent==2)):continue
        if ctx['roadshow_mode']:
            if record['id']<=last:continue
        elif mask&(1<<intent):continue
        candidate=dict(id=f"life:{record['id']}",event_id=record['id'],evidence_kind=1,
            topic='PLAYER_REQUEST_COMPLETED' if invited else 'FIRST_WINDOW_PAUSE' if intent==1 else 'FIRST_REST' if intent==3 else 'ROUTINE_CHECK_COMPLETED',actual_record=deepcopy(record))
        if intent not in latest or record['id']>latest[intent]['event_id']:latest[intent]=candidate
    ctx['social_candidates']=sorted(latest.values(),key=lambda x:(x['topic']=='PLAYER_REQUEST_COMPLETED',x['event_id']),reverse=True)
    # New archive/building thoughts are eligible when this event prompted the thought,
    # rather than turning every old unlocked item into a new notification.
    for a in archives:
        if trigger['kind']==2 and trigger['source_id']==a['id'] and not mask&(1<<(8+a['id'])):
            ctx['social_candidates'].append(dict(id=f"archive:{a['id']}",event_id=a['id'],evidence_kind=2,topic='READ_ARCHIVE_THOUGHT',actual_record=a))
    for fact in milestones:
        bit=16 if fact['type']==3 else 17 if fact['type']==4 else fact['type']
        if trigger['kind']==3 and trigger['source_id']==fact['id'] and not mask&(1<<bit):
            ctx['social_candidates'].append(dict(id=f"milestone:{fact['id']}",event_id=fact['id'],evidence_kind=3,topic='MILESTONE_THOUGHT',actual_record=fact))
    return ctx


def roadshow_personality(snapshot,ctx):
    """Shape autonomous opportunities, not device permissions or simulated facts."""
    if not ctx.get('roadshow_mode'):
        return ctx
    social=snapshot['runtime'].get('communication',{}).get('social',{})
    # One ordinary report per roadshow world is enough. Explicit invitations,
    # milestones and genuinely completed personal experiences remain available.
    routine_seen=bool(social.get('topic_mask',0)&(1<<2))
    candidates=ctx.get('social_candidates',[])
    personal=[c for c in candidates if c.get('actual_record',{}).get('intent') in ('watch_window','rest')]
    if personal:
        ctx['social_candidates']=[c for c in candidates if c.get('topic')!='ROUTINE_CHECK_COMPLETED']
    elif routine_seen:
        ctx['social_candidates']=[c for c in candidates if c.get('topic')!='ROUTINE_CHECK_COMPLETED']
    ctx['sharing_style']=dict(personal_experiences_first=True,routine_report_already_offered=routine_seen,
        expression_channel='INVITATION_THEN_VOICE',
        voice_goal='接听后从真实经历表达当下的一点感受、犹豫或愿望；不是新发现，也不证明恢复记忆。')
    if personal and ctx.get('trigger_event',{}).get('kind',0)==0:
        ctx['decisions']=[]  # Share a completed pause before proposing another outing.
        ctx['sharing_style']['personal_sharing_due']=True
    agency=ctx.get('agency',{})
    can_propose=(agency.get('level',0)>=1 and not agency.get('pending') and
                 (agency.get('wish') or {}).get('status')!='executing')
    personal_decisions=[d for d in ctx['decisions'] if d['intent'] in ('watch_window','rest')]
    if can_propose and personal_decisions and ctx.get('trigger_event',{}).get('kind',0)==0:
        # Choosing remains the model's job: two permitted wishes or NONE.
        # Removing extra routine inspections here never stops automatic care.
        ctx['decisions']=personal_decisions
        ctx['sharing_style']['personal_opportunity']=True
    return ctx
