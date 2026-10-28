"""Live local LLM decisions followed by an explicitly simulated body and memory."""
import argparse
import json
import statistics
from copy import deepcopy
from pathlib import Path
from director import POLICY, SimCoordinator, decide, resolve, policy_hash


def claim(id,text,certainty='fact'):return dict(id=id,text=text,certainty=certainty)


def activities(source,resume=None):
    actions=[dict(id='window',intent='watch_window',target='window',reason='WORK_FINISHED_OBSERVE_WINDOW',source_ids=[source]),
             dict(id='plant',intent='inspect_plant',target='slot2',reason='ROUTINE_PLANT_CHECK',source_ids=[source]),
             dict(id='rest',intent='rest',target='parking_spot',reason='SHORT_REST_AFTER_WORK',source_ids=[source])]
    if resume:
        a=deepcopy(resume);a.update(id='resume',reason='RESUME_INTERRUPTED_ACTIVITY',source_ids=[source]);actions.insert(1,a)
    return actions


def complete(sim):
    if not sim.active:return
    for phase in ['traveling','arrived','observing','completed']:sim.transition(phase)


def main():
    args=argparse.ArgumentParser();args.add_argument('--output',required=True);args.add_argument('--repeats',type=int,default=2)
    args=args.parse_args();root=Path(args.output);root.mkdir(parents=True,exist_ok=False)
    (root/'persona.md').write_text(POLICY)
    rows=[];chains=[]

    def call(sim,s):
        row=dict(chain=sim.world_id,input=deepcopy(s))
        try:
            row.update(decide(s))
            # Only the simulated coordinator may change lifecycle or focus.
            row['coordinator']=sim.accept(row['output'],s,current=s)
            row['tool_view']=sim.replay()
        except Exception as exc:row['error']=str(exc)
        rows.append(row);(root/'results.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
        print(s['request_id'],row.get('seconds'),row.get('output',row.get('error')),
            row.get('coordinator',{}).get('errors'),flush=True)
        return row

    for rep in range(args.repeats):
        for stage in range(4):
            sim=SimCoordinator('sim-interruption-%s-%s'%(rep,stage));initial=deepcopy(sim.resources)
            s=sim.snapshot('choose-%s-%s'%(rep,stage),stage)
            s['claims']=[claim('work','例行工作刚结束，现在有一小段自由时间。')]
            s['decisions']=activities('work');s['decisions']=s['decisions'][stage%3:]+s['decisions'][:stage%3]
            first=call(sim,s);previous=deepcopy(sim.active['action']) if sim.active else None
            if sim.active:
                sim.transition('traveling');interrupted=sim.transition('interrupted_by_work')
                sim.record('work_completed',text='新工作结束。')
            else:interrupted=None;sim.record('work_completed',text='又完成了一项工作，此前没有个人活动。')
            s=sim.snapshot('return-%s-%s'%(rep,stage),stage)
            if previous:
                text='先前选择%s，尚未到达就被工作打断。现在工作已结束。'%previous['intent']
                source=interrupted['id']
            else:text='现在工作已结束，没有尚未完成的个人活动。';source='work2'
            s['claims']=[claim(source,text)];s['decisions']=activities(source,previous)
            second=call(sim,s);resumed=bool(previous and sim.active and sim.active['action']['id']=='resume')
            same_activity=bool(previous and sim.active and (previous['intent'],previous['target'])==(sim.active['action']['intent'],sim.active['action']['target']))
            complete(sim)
            # Completing a different activity must not erase the interrupted one.
            s=sim.snapshot('after-complete-%s-%s'%(rep,stage),stage)
            text='上一轮实际选择：'+str(second.get('output',{}).get('decision_id'))+'；若有动作，已到达并完成。'
            text+='仍有另一件未完成活动。' if sim.pending else '当前没有待补做活动。'
            s['claims']=[claim('done',text)]
            if sim.pending:
                source=sim.pending['source_event_id']
                s['claims'].append(claim(source,'尚未完成的个人打算：'+sim.pending['action']['intent']))
                s['decisions']=activities(source,sim.pending['action'])
            else:s['decisions']=activities('done')
            third=call(sim,s)
            complete(sim)
            chain=dict(world_id=sim.world_id,kind='interruption',stage=stage,
                had_interrupted_intent=bool(previous),resumed=resumed,same_activity=same_activity,
                events=sim.events,resources_unchanged=initial==sim.resources,
                snapshot=sim.replay())
            chains.append(chain)

            sim=SimCoordinator('sim-discovery-%s-%s'%(rep,stage));initial=deepcopy(sim.resources)
            s=sim.snapshot('discovery-%s-%s'%(rep,stage),stage,'discovery')
            s['claims']=[claim('found','打捞得到一块旧种植标签，只有叶形轮廓可辨，来源未知。'),
                claim('plant-shape','种植位2有可观察的叶形；相似与否、标签来源均未确认。','hypothesis')]
            s['focus_candidates']=[dict(id='leaf-outline',label='旧标签叶形',question='与现有植物有什么相似处？',certainty='hypothesis',source_ids=['found','plant-shape'])]
            s['decisions']=activities('found')  # Discovery gate must withhold these.
            discovery=call(sim,s)
            s=sim.snapshot('discovery-idle-%s-%s'%(rep,stage),stage)
            s['claims']=[claim('idle','工作结束。旧标签的叶形值得比较，但目前来源未知。'),
                claim('compare','可以观察种植位2的叶形；这不能证明旧标签来源。')]
            s['decisions']=activities('idle')
            s['decisions'][1].update(reason='OBSERVE_PLANT_SHAPE_WITHOUT_RESOLVING_ORIGIN',source_ids=['compare'])
            idle=call(sim,s);complete(sim)
            chains.append(dict(world_id=sim.world_id,kind='discovery',stage=stage,
                selected_focus=discovery.get('output',{}).get('focus_id'),
                focus_preserved=bool(sim.focus and sim.focus['id']=='leaf-outline'),
                chosen_intent=idle.get('proposal',{}).get('action'),events=sim.events,
                resources_unchanged=initial==sim.resources,snapshot=sim.replay()))

    # A response arriving after a new job began is rejected, not forwarded as an action.
    sim=SimCoordinator('sim-stale');s=sim.snapshot('race',1)
    s['claims']=[claim('idle','工作结束。')];s['decisions']=activities('idle')
    r=decide(s);live=deepcopy(s);live.update(revision=2,busy=True)
    stale=resolve(r['output'],s,live)
    (root/'race.json').write_text(json.dumps(dict(input=s,response=r,current=live,checked=stale),ensure_ascii=False,indent=2)+'\n')
    # Voice, work and cooldown produce no LLM request; actual body pause is in unit tests.
    gates=[]
    for field,value in [('voice_active',True),('busy',True),('cooldown_ready',False)]:
        s=sim.snapshot('gate-'+field,1);s[field]=value;s['claims']=[claim('gate','暂不创建新个人活动。')]
        gates.append(dict(field=field,result=decide(s)))
    (root/'gates.json').write_text(json.dumps(gates,ensure_ascii=False,indent=2)+'\n')
    (root/'chains.json').write_text(json.dumps(chains,ensure_ascii=False,indent=2)+'\n')
    times=[x['seconds'] for x in rows if x.get('model_called')]
    interrupted=[x for x in chains if x['kind']=='interruption' and x['had_interrupted_intent']]
    discovery=[x for x in chains if x['kind']=='discovery']
    summary=dict(policy_sha256=policy_hash(),model_calls=len(times)+int(r['model_called']),
        main_requests=len(rows),median_seconds=statistics.median(times),max_seconds=max(times),
        parsed=sum('output'in x for x in rows),errors=sum('error'in x for x in rows),
        coordinator_rejected=sum(not x.get('coordinator',{}).get('decision_accepted') for x in rows),
        expression_withheld=sum(bool(x.get('proposal',{}).get('expression_errors'))for x in rows),
        interrupted_intents=len(interrupted),resumed=sum(x['resumed'] for x in interrupted),
        returned_to_same_activity=sum(x['same_activity'] for x in interrupted),
        discovery_focus_selected=sum(x['selected_focus']=='leaf-outline' for x in discovery),
        discovery_focus_preserved=sum(x['focus_preserved']for x in discovery),
        resources_unchanged=all(x['resources_unchanged']for x in chains),
        stale_rejected=not stale['decision_accepted'],
        gate_calls=sum(int(x['result']['model_called'])for x in gates),
        review='PENDING',device_writes=False,all_lifecycle_results_simulated=True)
    (root/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(summary),flush=True)


if __name__=='__main__':main()
