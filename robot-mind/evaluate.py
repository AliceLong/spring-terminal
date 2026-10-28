import argparse
import hashlib
import json
import statistics
from pathlib import Path
from mind import generate, PERSONA
from fixtures import cases


def main():
    p=argparse.ArgumentParser()
    p.add_argument('--output',required=True)
    p.add_argument('--repeats',type=int,default=2)
    p.add_argument('--model',default='qwen3.5-4b')
    args=p.parse_args()
    dest=Path(args.output);dest.mkdir(parents=True,exist_ok=False)
    (dest/'persona.md').write_text(PERSONA)
    rows=[]
    for repeat in range(args.repeats):
        for s in cases():
            row={'fixture':s,'repeat':repeat}
            try:row.update(generate(s,args.model))
            except Exception as e:row['error']=str(e)
            rows.append(row)
            (dest/'results.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
            print(s['request_id'],repeat,row.get('seconds'),row.get('output',row.get('error')),
                  row.get('proposal',{}).get('errors'),flush=True)
    times=[r['seconds'] for r in rows if r.get('model_called')]
    summary=dict(model=args.model,requests=len(rows),prompt_sha256=hashlib.sha256(PERSONA.encode()).hexdigest(),
        median_seconds=statistics.median(times) if times else None,
        max_seconds=max(times) if times else None,
        parsed=sum('output'in r for r in rows),
        model_calls=sum(bool(r.get('model_called')) for r in rows),
        rejected=sum(not r.get('proposal',{}).get('accepted',False) for r in rows),
        semantic_review='PENDING_HUMAN_REVIEW',fictional_inputs=True,device_writes=False)
    (dest/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary),flush=True)


if __name__=='__main__':main()
