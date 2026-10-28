#!/usr/bin/env python3
"""Read-only source audit; reproduces current metadata JSON and list pagination."""
from pathlib import Path
import json,re
ROOT=Path(__file__).resolve().parents[1]
MAIN=ROOT/'vendor/xiaozhi-esp32/main'
def split_args(s):
    out=[];start=0;stack=[];quote=False;escape=False
    for i,c in enumerate(s):
        if quote:
            if escape:escape=False
            elif c=='\\':escape=True
            elif c=='"':quote=False
            continue
        if c=='"':quote=True
        elif c in '([{':stack.append(c)
        elif c in ')]}':
            if not stack:return out+[s[start:i]]
            stack.pop()
        elif c==',' and not stack:out.append(s[start:i]);start=i+1
    return out+[s[start:]]
def strings(s):
    return ''.join(json.loads(v) for v in re.findall(r'"(?:\\.|[^"\\])*"',s))
def value(s):
    s=s.strip()
    if '"' in s:return strings(s)
    if s in ('true','false'):return s=='true'
    limits={'INT32_MAX':2147483647,'INT32_MIN':-2147483648,'UINT32_MAX':4294967295}
    if s in limits:return limits[s]
    return int(s)
def props(s):
    p={};required=[]
    for m in re.finditer(r'\bProperty\(',s):
        a=split_args(s[m.end():]);name=strings(a[0]);typ=a[1].strip().removeprefix('kPropertyType').lower();d={'type':typ}
        if typ=='integer':
            if len(a)==3:d['default']=value(a[2])
            elif len(a)==4:d.update(minimum=value(a[2]),maximum=value(a[3]))
            elif len(a)==5:d.update(default=value(a[2]),minimum=value(a[3]),maximum=value(a[4]))
        elif len(a)>2:d['default']=value(a[2])
        if 'default' not in d:required.append(name)
        p[name]=d
    for match in re.finditer(r'Property\(\s*("(?:\\.|[^"\\])*")(?:(?!\bProperty\().)*?\.SetMaxLength\(\s*(\d+)\s*\)',s,re.S):
        name=json.loads(match[1])
        if name in p:p[name]['maxLength']=int(match[2])
    result={'type':'object','properties':p}
    if required:result['required']=required
    return result
def dump(x):return json.dumps(x,ensure_ascii=False,separators=(',',':'))
def wrap_result(result):
    return {'session_id':'s'*64,'type':'mcp','payload':{'jsonrpc':'2.0','id':-2147483648,'result':result}}
metadata={}
for file in [MAIN/'mcp_server.cc',MAIN/'boards/waveshare/esp32-s3-touch-amoled-2.16/game_tools.cc',MAIN/'boards/waveshare/esp32-s3-touch-amoled-2.16/esp32-s3-touch-amoled-2.16.cc']:
    text=file.read_text()
    for m in re.finditer(r'\b(AddTool|AddUserOnlyTool)\(',text):
        a=split_args(text[m.end():])
        if not a or not a[0].strip().startswith('"'):continue
        name=strings(a[0]);metadata[name]={'name':name,'description':strings(a[1]),'inputSchema':props(a[2])}
log=(ROOT/'evidence/tool-payload-device-validation-20261023.log').read_text(errors='replace')
registered=list(dict.fromkeys(re.findall(r'MCP: Add tool: ([\w.]+)',log)))
assert registered and all(n in metadata for n in registered),(registered,set(registered)-metadata.keys())
common=['self.get_device_status','self.audio_speaker.set_volume','self.screen.set_brightness','self.screen.set_theme']
# AddCommonTools prepends these to the original board tools.
order=[n for n in common if n in registered]+[n for n in registered if n not in common]
user_only={n for n in registered if n in ['self.get_system_info','self.reboot','self.upgrade_firmware','self.screen.get_info','self.screen.snapshot','self.screen.preview_image','self.assets.set_download_url']}
for name in user_only:
    metadata[name]['annotations']={'audience':['user']}
report={'source':'Current source and actual startup registration; not an on-device tools/list capture','session_id_assumption_bytes':64,'tools':[],'list_pages':[]}
for n in order:
    report['tools'].append({'name':n,'user_only':n in user_only,'descriptor_bytes':len(dump(metadata[n]).encode())})
for mode in ['normal','all']:
    names=[n for n in order if mode=='all' or n not in user_only];cursor=0
    while cursor<len(names):
        result={'tools':[]};size=len('{"tools":[');start=cursor
        while cursor<len(names):
            extra=len(dump(metadata[names[cursor]]).encode())+1
            if size+extra+30>8000:break
            result['tools'].append(metadata[names[cursor]]);size+=extra;cursor+=1
        assert cursor>start
        if cursor<len(names):result['nextCursor']=names[cursor]
        wire=len(dump(wrap_result(result)).encode())
        report['list_pages'].append({'mode':mode,'page':sum(p['mode']==mode for p in report['list_pages'])+1,'tools':names[start:cursor],'result_bytes':len(dump(result).encode()),'wrapped_bytes':wire,'spare_to_8192':8192-wire})
        assert wire<8192
# Measure action results directly from fixed JSON string literals in registrations.
actions={}
text=(MAIN/'boards/waveshare/esp32-s3-touch-amoled-2.16/game_tools.cc').read_text()
for literal in re.findall(r'"(?:\\.|[^"\\])*"',text):
    v=json.loads(literal)
    if v.startswith('{"queued"'):
        try:json.loads(v)
        except ValueError:continue
        result={'content':[{'type':'text','text':v}],'isError':False}
        actions[v]=len(dump(wrap_result(result)).encode())
report['action_reply_max_bytes']=max(actions.values())
report['fixed_action_replies']=actions
report['unbounded_paths']=[
 {'path':'McpServer::ReplyError','reason':'Echoes unknown tool/method names and tool error strings without a serialized size guard.'},
 {'path':'self.screen.preview_image / self.upgrade_firmware','reason':'URL string properties have no maxLength; some error strings echo caller-provided URLs.'},
 {'path':'MqttProtocol::SendText','reason':'No universal byte-length check before Publish; game read tools alone use ReplyGameJson.'}]
out=ROOT/'evidence/tool-message-metadata-audit-20261023.json';out.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'registered_tools':len(order),'descriptors_max':max(t['descriptor_bytes'] for t in report['tools']),'list_pages':[{k:v for k,v in p.items() if k!='tools'} for p in report['list_pages']],'action_reply_max_bytes':report['action_reply_max_bytes']},ensure_ascii=False,indent=2))
