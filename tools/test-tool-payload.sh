#!/bin/bash
set -euo pipefail
payload_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$payload_root"
clang++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined -I vendor/xiaozhi-esp32/main tests/spring/tool_payload_test.cc -o tests/.build/tool-payload-test
./tests/.build/tool-payload-test > evidence/tool-payload-cases-20261023.jsonl
python3 - <<'PY'
import json
from pathlib import Path
rows=[json.loads(s) for s in Path('evidence/tool-payload-cases-20261023.jsonl').read_text().splitlines()]
by={r['case']:json.loads(r['text']) for r in rows}
maximum=0
for r in rows:
    text=r['text']
    wire={'session_id':'s'*64,'type':'mcp','payload':{'jsonrpc':'2.0','id':-2147483648,'result':{'content':[{'type':'text','text':text}],'isError':False}}}
    size=len(json.dumps(wire,ensure_ascii=False,separators=(',',':')).encode())
    assert size==r['estimate'],(r['case'],size,r['estimate'])
    assert size<=7168,(r['case'],size)
    maximum=max(maximum,size)
    if r['case'].startswith('world-'):
        d=json.loads(text)
        assert 'plan_history' not in d and 'records' not in d['story']
        assert d['life']['history_included'] is False
        assert d['story']['details_tool']=='get_story_state'
        assert len(d['plants'])==4 and len(d['economy']['facilities'])==8
        assert d['resources']['water']==4294967295
        assert 'cost' in d['economy']['facilities'][0]
        call=int(r['case'].rsplit('-',1)[1])
        assert ('accepted_communication' in d)==(call<7)
# Follow returned cursors: every retained record/fact is still recoverable exactly once.
ids=[];before=0
while True:
    d=by['memory-records-'+str(before)];ids.extend(e['id'] for e in d['records']);before=d['next_before']
    if not before:break
assert ids==list(range(128,0,-1)),ids
facts=[];offset=0
while True:
    d=by['memory-facts-'+str(offset)];facts.extend(e['type'] for e in d['long_term']);offset=d['next_fact_offset']
    if not offset:break
assert facts==list(range(1,33)),facts
records=[];offset=0
while True:
    d=by['story-0-'+str(offset)];records.extend(e['node'] for e in d['records'])
    assert d['records_order']=='newest_first'
    cursor=d['next_record_cursor']
    if not cursor:break
    offset=int(cursor.rsplit(':',1)[1])
assert records==list(range(15,-1,-1)),records
summary={'cases':len(rows),'max_wrapped_bytes':maximum,'budget':7168,'broker_limit':8192,'record_paging':128,'fact_paging':32,'story_paging':16}
Path('evidence/tool-payload-summary-20261023.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary))
PY
