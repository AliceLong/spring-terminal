#!/usr/bin/env python3
"""Compare only game blobs; never print account or Wi-Fi fields."""
from pathlib import Path
import json,base64,argparse

def blobs(path):
 j=json.loads(path.read_text());entries=[e for p in j['pages'] for e in p.get('entries',[]) if e.get('state')=='Written'];names={e['data']['value']:e['key'] for e in entries if e['metadata']['namespace']==0 and isinstance(e['data'].get('value'),int)};out={}
 for e in entries:
  if e['metadata']['type']!='blob_index':continue
  ns=e['metadata']['namespace'];idx=e['data'];pieces=[]
  for n in range(idx['chunk_start'],idx['chunk_start']+idx['chunk_count']):
   c=next(x for x in entries if x['metadata']['namespace']==ns and x['key']==e['key'] and x['metadata']['type']=='blob_data' and x['metadata']['chunk_index']==n);pieces.append(b''.join(base64.b64decode(v['raw']) for v in c['children'])[:c['data']['size']])
  out[(names.get(ns),e['key'])]=b''.join(pieces)
 return out
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('before',type=Path);p.add_argument('after',type=Path);p.add_argument('--extract',type=Path);args=p.parse_args();a=blobs(args.before);b=blobs(args.after)
 for k,v in a.items():
  if k[0] in ('desk_gap_v1','spring_v1') and not (k[0]=='spring_v1' and k[1] in ('world_v3','demo_v3')):
   assert v==b.get(k),'Legacy game blob changed: '+str(k)
 data=b.get(('spring_v1','world_v3'));assert data and len(data)==1536
 if args.extract:args.extract.write_bytes(data);args.extract.chmod(0o600)
 print('Archived game, legacy normal/demo unchanged; all V2 blobs unchanged; new world_v3 is1536 bytes.')
