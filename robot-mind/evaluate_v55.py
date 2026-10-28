"""Same-state evaluation: real model calls, simulated game fixtures, no USB writes."""
import argparse
import json
import statistics
from pathlib import Path
from copy import deepcopy
from device_bridge import context
from director import decide,resolve,policy_hash


def matching(ctx,kind,source):
    f=next((x for x in ctx['focus_candidates'] if x.get('device_focus',{}).get('kind')==kind and x['device_focus']['source']==source),None)
    social_kind={2:1,3:2,4:3}.get(kind)
    social=next((x for x in ctx['social_candidates'] if x.get('evidence_kind')==social_kind and x['event_id']==source),None)
    return f,social


def event_rule(ctx):
    e=ctx['trigger_event'];kind={1:2,2:3,3:4}.get(e['kind'])
    f,s=matching(ctx,kind,e['source_id']) if kind else (None,None)
    return dict(decision_id='NONE',focus_id=f['id'] if f else 'KEEP',social_id=s['id'] if s else 'NONE',bubble='')


def assess(ctx,result):
    p=result.get('proposal',{});f=p.get('focus_update') or {};social=p.get('social_intent') or {};e=ctx.get('trigger_event',{})
    kind={1:2,2:3,3:4}.get(e.get('kind'))
    focus=f.get('device_focus',{})
    retained=False
    if not f and result.get('output',{}).get('focus_id')=='KEEP':
        current=ctx.get('current_focus') or {}
        focus=dict(kind=current.get('kind'),source=current.get('source_id'))
        retained=bool(current)
    related=(focus.get('kind')==kind and focus.get('source')==e.get('source_id')) or (
        social.get('evidence_kind')==e.get('kind') and social.get('related_event')==e.get('source_id'))
    return dict(legal=bool(p.get('decision_accepted')),related=bool(related) if kind else None,
        action=p.get('action'),focus=f.get('id'),retained_focus=retained,social=social or None,expression_errors=p.get('expression_errors',[]),
        no_body_when_unavailable=bool(ctx['decisions']) or p.get('action') is None)


def run(fixtures,runs,output):
    rows=[]
    for case in fixtures:
        s=case['snapshot'];ctx=context(s)
        # A0 is the actual existing deterministic life choice exported by C++.
        # A1 is a deliberately simple event rule using the same focus/share candidates.
        baseline=event_rule(ctx);bp=resolve(baseline,ctx)
        for trial in range(runs):
            c=deepcopy(ctx);c['request_id']=f"evaluation:{case['name']}:{trial+1}"
            try:r=decide(c);metrics=assess(c,r)
            except Exception as exc:r=dict(error=type(exc).__name__,detail=str(exc)[:160]);metrics=dict(legal=False,related=False if ctx['trigger_event']['kind'] else None)
            row=dict(scene=case['name'],trial=trial+1,simulated_world=True,real_model_call=True,not_sent_to_device=True,
                input=c,existing_rule=case['rule'],simple_event_rule=dict(output=baseline,metrics=assess(ctx,dict(proposal=bp))),result=r,metrics=metrics)
            rows.append(row)
            print(json.dumps(dict(scene=row['scene'],trial=row['trial'],seconds=r.get('seconds'),output=r.get('output'),metrics=metrics),ensure_ascii=False),flush=True)
            Path(output).write_text(json.dumps(dict(policy_sha256=policy_hash(),rows=rows),ensure_ascii=False,indent=2))
    durations=[x['result']['seconds'] for x in rows if 'seconds' in x['result']]
    related=[x for x in rows if x['metrics'].get('related') is not None]
    summary=dict(calls=len(rows),model_success=len(durations),legal=sum(x['metrics']['legal'] for x in rows),
        related=sum(x['metrics']['related'] for x in related),related_trials=len(related),
        simple_event_rule_related=sum(x['simple_event_rule']['metrics']['related'] for x in related),
        mean_seconds=statistics.mean(durations) if durations else None,max_seconds=max(durations) if durations else None,
        expression_rejected=sum(bool(x['metrics'].get('expression_errors')) for x in rows))
    Path(output).write_text(json.dumps(dict(policy_sha256=policy_hash(),summary=summary,rows=rows),ensure_ascii=False,indent=2))
    print(json.dumps(dict(summary=summary),ensure_ascii=False),flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--fixtures',default='tests/.build/v55-fixtures.json');parser.add_argument('--runs',type=int,default=5);parser.add_argument('--output',default='evidence/v55/model-evaluation.json')
    args=parser.parse_args();Path(args.output).parent.mkdir(parents=True,exist_ok=True)
    run(json.loads(Path(args.fixtures).read_text()),args.runs,args.output)
