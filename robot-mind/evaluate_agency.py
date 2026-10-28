"""Real local LLM calls against simulated device agency snapshots; never writes USB."""
import argparse,json,time
from copy import deepcopy
from pathlib import Path
from device_bridge import context,encode
from director import decide

def run(output,runs=2):
    root=Path(__file__).resolve().parents[1]
    fixtures=json.loads((root/'tests/.build/v56-agency-fixtures.json').read_text())
    rows=[]
    for case in fixtures:
        for trial in range(runs):
            s=deepcopy(case['snapshot']);c=context(s)
            start=time.monotonic()
            try:
                result=decide(c);p=result['proposal'];line=encode(s,p).decode().strip() if p['decision_accepted'] else None
                action=p.get('action');personal=action and action.get('execution_mode')=='ASK_PERMISSION'
                rows.append(dict(scene=case['name'],trial=trial+1,simulated_world=True,real_model_call=True,not_sent_to_device=True,
                    seconds=round(time.monotonic()-start,3),output=result.get('output'),proposal=p,encoded=line,
                    policy_pass=bool(p['decision_accepted'] and (not personal or (p['execution_status']=='wish_proposed' and not p['animation_plan']))),
                    review_required=True))
            except Exception as e:rows.append(dict(scene=case['name'],trial=trial+1,error=type(e).__name__,policy_pass=False))
    summary=dict(calls=len(rows),policy_pass=sum(x['policy_pass'] for x in rows),wish_choices=sum(x.get('proposal',{}).get('execution_status')=='wish_proposed' for x in rows),
        scope='Protocol and growth policy, simulated worlds with real model calls. Not hardware, XiaoZhi conversation, or personality acceptance.')
    path=root/output;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(dict(summary=summary,rows=rows),ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(summary,ensure_ascii=False))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',default='evidence/v56/agency-model-evaluation.json');p.add_argument('--runs',type=int,default=3);a=p.parse_args();run(a.output,a.runs)
