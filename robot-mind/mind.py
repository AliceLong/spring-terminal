"""Local-only proposal generator. No device access, game writes or cloud calls."""
import json
import re
import time
import urllib.request
from pathlib import Path

PERSONA = Path(__file__).with_name('persona.md').read_text()
LIMITS = (12, 28, 40, 56)


def choices(s):
    focuses = ['KEEP', 'NONE'] + [f['id'] for f in s['focus_candidates']]
    available = s['trigger'] == 'idle' and not s['busy'] and not s['voice_active'] and s['cooldown_ready']
    actions = ['NONE'] + ([a['id'] for a in s['actions']] if available else [])
    return focuses, actions


def schema(s):
    focuses, actions = choices(s)
    claim_ids = [c['id'] for c in s['claims']]
    return {'type': 'object', 'additionalProperties': False, 'properties': {
        'focus_id': {'type': 'string', 'enum': focuses},
        'action_id': {'type': 'string', 'enum': actions},
        'bubble': {'type': 'string'},
        'claim_ids': {'type': 'array', 'items': {'type': 'string', 'enum': claim_ids}},
    }, 'required': ['focus_id', 'action_id', 'bubble', 'claim_ids']}


def validate(out, s, current=None):
    """Reject malformed/stale/unsafe proposals; lexical checks are NOT a truth oracle."""
    errors = []
    keys = set(schema(s)['required'])
    if not isinstance(out, dict) or set(out) != keys:
        return ['invalid_fields']
    if any(not isinstance(out[k], str) for k in keys - {'claim_ids'}):
        return ['invalid_types']
    if not isinstance(out['claim_ids'], list) or any(not isinstance(x, str) for x in out['claim_ids']):
        return ['invalid_claim_types']
    focuses, actions = choices(s)
    if out['focus_id'] not in focuses: errors.append('unknown_focus')
    if out['action_id'] not in actions: errors.append('unavailable_action')
    claims = {c['id']: c for c in s['claims']}
    if any(c not in claims for c in out['claim_ids']): errors.append('unknown_claim')
    text = out['bubble']
    if text and not text.strip(): errors.append('empty_expression')
    if not text and out['claim_ids']: errors.append('silent_claims')
    if len(text) > LIMITS[s['language_stage']]: errors.append('stage_length')
    if text and not out['claim_ids']: errors.append('ungrounded_expression')
    if any(claims.get(c, {}).get('certainty') == 'hypothesis' for c in out['claim_ids']):
        if text and not re.search(r'吗|？|可能|也许|不确定|尚未|未知|不知道|不清楚|未确认|未明|未定|不明|找不到|没有确认', text):
            errors.append('hypothesis_as_fact')
    if re.search(r'主人|助手|隙灵|有什么可以|为您服务|阶段[0123]|JSON|schema|工具|允许动作|可用动作|依据|系统提示|任务指令|没有记录|未记录|未认证|许可|等待指令|玩家|假设', text):
        errors.append('persona_or_metadata')
    if re.search(r'[\U0001F000-\U0001FAFF]|[\[\]{}*`（）()]', text): errors.append('non_speech_markup')
    if re.search(r'我想起来了|恢复.{0,4}记忆|你陪我|我们一起|资源.{0,4}增加|能源.{0,4}增加|解锁|已经批准', text):
        errors.append('unsupported_authority_or_memory')
    # These salient visual claims caused previous failures. Unknown prose still needs review.
    evidence = ''.join(c['text'] for c in s['claims'] if c['id'] in out['claim_ids'])
    for term in ['云', '小鸟', '夕阳', '新叶', '长高', '缺水', '有字', '锈', '湿润', '能量异常', '警告', '在生长', '正常', '在动', '无变化', '叶子上', '等车', '发光', '风吹', '只有风']:
        if term in text and term not in evidence: errors.append('unsupported_detail:' + term)
    all_evidence = ''.join(c['text'] for c in s['claims'])
    if re.search(r'没有记录.*(新叶|生长|缺水)', all_evidence) and re.search(r'长出|长高|无变化|缺水了|新叶|生长', text):
        errors.append('absence_of_evidence_as_observation')
    if '没有记录具体窗外景物' in all_evidence and re.search(r'没看见|没看到|没有.{0,6}特别', text):
        errors.append('unrecorded_scenery_as_observation')
    if '坐上去休息尚未' in all_evidence and re.search(r'可以坐|坐一|坐下|坐上', text):
        errors.append('story_locked_seat')
    if '头顶' in all_evidence and '真实植物' in all_evidence and re.search(r'头顶有(东西|活物)', text):
        errors.append('body_identity_too_vague')
    if re.search(r'已经|看过|完成了|到过|已清洁', text) and out['action_id'] != 'NONE':
        errors.append('proposal_as_result')
    if text and text in s.get('recent_bubbles', []): errors.append('repeated_bubble')
    if current:
        if (s['world_id'], s['revision']) != (current['world_id'], current['revision']):
            errors.append('stale_context')
        if out['action_id'] not in choices(current)[1]: errors.append('action_changed')
    return sorted(set(errors))


def compile_proposal(out, s, current=None):
    """Resolve only program-owned candidates. Never promotes prose to world facts."""
    errors = validate(out, s, current)
    if errors:
        return {'accepted': False, 'errors': errors, 'bubble': '', 'action': None, 'focus_update': None}
    action = next((dict(a) for a in s['actions'] if a['id'] == out['action_id']), None)
    focus = next((dict(f) for f in s['focus_candidates'] if f['id'] == out['focus_id']), None)
    return {'accepted': True, 'errors': [], 'request_id': s['request_id'],
            'world_id': s['world_id'], 'revision': s['revision'],
            'bubble': out['bubble'], 'bubble_status': 'generated_expression_not_world_fact',
            'display_allowed': False, 'semantic_review': 'required' if out['bubble'] else 'not_applicable',
            'focus_mode': out['focus_id'], 'focus_update': focus,
            'action': action, 'execution_status': 'proposed' if action else 'none',
            'source_claims': list(out['claim_ids']), 'story_mutations': [], 'resource_mutations': []}


def generate(s, model='qwen3.5-4b', temperature=0.5):
    if s['busy'] or s['voice_active'] or (s['trigger']=='idle' and not s['cooldown_ready']):
        out = dict(focus_id='KEEP', action_id='NONE', bubble='', claim_ids=[])
        return dict(seconds=0, model_called=False, skip_reason='busy_voice_or_cooldown',
                    output=out, proposal=compile_proposal(out,s))
    body = {'model': model, 'messages': [
        {'role': 'system', 'content': PERSONA},
        {'role': 'user', 'content': json.dumps(dict(s, bubble_max_chars=LIMITS[s['language_stage']]), ensure_ascii=False)}],
        'temperature': temperature, 'max_tokens': 256, 'reasoning_effort': 'none', 'stream': False,
        'response_format': {'type': 'json_schema', 'json_schema': {
            'name': 'garden_proposal_v2', 'strict': True, 'schema': schema(s)}}}
    req = urllib.request.Request('http://127.0.0.1:1234/v1/chat/completions',
        data=json.dumps(body).encode(), headers={'Content-Type': 'application/json'})
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    start = time.monotonic()
    response = json.load(opener.open(req, timeout=30))
    duration = time.monotonic() - start
    choice = response['choices'][0]
    msg = choice['message']
    if choice['finish_reason'] != 'stop' or msg.get('reasoning_content'):
        raise ValueError('Truncated response or thinking enabled')
    out = json.loads(msg['content'])
    return {'seconds': round(duration, 3), 'model_called': True, 'raw': response, 'output': out,
            'proposal': compile_proposal(out, s)}
