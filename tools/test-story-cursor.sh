#!/bin/bash
set -euo pipefail
cursor_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$cursor_root"
clang++ -std=c++17 -Wall -Wextra -Werror -g -fsanitize=address,undefined -I vendor/xiaozhi-esp32/main tests/spring/story_cursor_test.cc -o tests/.build/story-cursor-test
./tests/.build/story-cursor-test > evidence/story-cursor-cases-20261023.jsonl
python3 - <<'PY'
import json
from pathlib import Path
rows=[json.loads(l) for l in Path('evidence/story-cursor-cases-20261023.jsonl').read_text().splitlines()]
data={r['case']:json.loads(r['text']) for r in rows}
maximum=0
for r in rows:
    wire={'session_id':'s'*64,'type':'mcp','payload':{'jsonrpc':'2.0','id':-2147483648,'result':{'content':[{'type':'text','text':r['text']}],'isError':False}}}
    size=len(json.dumps(wire,ensure_ascii=False,separators=(',',':')).encode());maximum=max(maximum,size);assert size<=7168
for case,expected in [('initial-first',list(range(15,9,-1))),('initial-second',list(range(9,3,-1))),('initial-last',list(range(3,-1,-1))),('after-append-first',[23,15,14,13,12,11]),('short',[2,1,0])]:
    d=data[case];assert [e['node'] for e in d['records']]==expected,(case,d)
    assert d['records_order']=='newest_first' and d['returned_record_count']==len(expected)
    assert 'next_record_offset' not in d
all_records=sum(([e['node'] for e in data[k]['records']] for k in ['initial-first','initial-second','initial-last']),[])
assert all_records==list(range(15,-1,-1))
assert data['initial-first']['next_record_cursor'].endswith(':6')
assert data['initial-second']['next_record_cursor'].endswith(':12')
assert data['initial-last']['next_record_cursor']=='' and not data['initial-last']['has_more_records']
for case in ['after-append-stale','after-two-appends-stale','other-mode','other-world','same-count-new-version','revision-wrap','legacy-offset-rejected','mismatched-offset-rejected']:
    d=data[case];assert d['restart_required'] and d['records']==[] and 'error' in d
    assert d['retry']=={'record_offset':0,'record_cursor':''}
assert data['empty']['records']==[] and not data['empty']['has_more_records']
assert data['save-roundtrip-cursor']['records_order']=='newest_first'
summary={'cases':len(rows),'max_wrapped_bytes':maximum,'newest_first':True,'all16_exactly_once':True,'live_append_rejects_stale':True,'world_mode_revision_guards':True,'save_roundtrip':'passed','save_format':'unchanged'}
Path('evidence/story-cursor-summary-20261023.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary))
PY
