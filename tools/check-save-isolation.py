#!/usr/bin/env python3
"""Compare old game blob bytes without printing account/Wi-Fi data."""
import argparse, base64, hashlib, json
from pathlib import Path

def old_blob(path):
    source=json.loads(Path(path).read_text())
    entries=[e for p in source['pages'] for e in p.get('entries',[]) if e.get('state')=='Written']
    names={e['data']['value']:e['key'] for e in entries if e['metadata']['namespace']==0 and isinstance(e['data'].get('value'),int)}
    selected=[e for e in entries if names.get(e['metadata']['namespace'])=='desk_gap_v1' and e.get('key')=='world_v3']
    indices=[e for e in selected if e['metadata']['type']=='blob_index']
    assert len(indices)==1, 'Expected one current old-game blob index'
    index=indices[0]['data']
    chunks=[]
    for n in range(index['chunk_start'],index['chunk_start']+index['chunk_count']):
        parts=[e for e in selected if e['metadata']['type']=='blob_data' and e['metadata']['chunk_index']==n]
        assert len(parts)==1, 'Missing/duplicate old-game chunk'
        entry=parts[0]
        raw=b''.join(base64.b64decode(child['raw']) for child in entry['children'])
        chunks.append(raw[:entry['data']['size']])
    payload=b''.join(chunks)
    assert len(payload)==index['size']
    return payload

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('before');parser.add_argument('after',nargs='?');args=parser.parse_args()
    data=old_blob(args.before)
    if args.after: assert old_blob(args.after)==data, 'Old game save changed!'
    print(json.dumps({'old_game_save_bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'unchanged':True if args.after else None}))
