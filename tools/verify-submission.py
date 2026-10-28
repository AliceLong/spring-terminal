#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,sys
r=Path(__file__).resolve().parents[1]
m=json.loads((r/'SUBMISSION_MANIFEST.json').read_text());bad=[]
for item in m['files']:
 p=r/item['path']
 if not p.is_file() or p.stat().st_size!=item['bytes'] or hashlib.sha256(p.read_bytes()).hexdigest()!=item['sha256']:bad.append(item['path'])
if bad:
 print('Changed or missing:',*bad,sep='\n');sys.exit(1)
print('Verified',len(m['files']),'files. Manifest and SHA256SUMS are excluded from their own file list.')
