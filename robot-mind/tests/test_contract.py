import sys
import unittest
from copy import deepcopy
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from fixtures import cases
from mind import compile_proposal, validate, schema, generate


class ContractTests(unittest.TestCase):
    def setUp(self):
        self.s=next(s for s in cases() if s['request_id']=='idle-focus')
        self.out=dict(focus_id='KEEP',action_id='inspect-1',bubble='',claim_ids=[])

    def test_program_owned_action(self):
        p=compile_proposal(self.out,self.s)
        self.assertTrue(p['accepted'])
        self.assertEqual(p['action']['target'],'slot1')
        self.assertEqual(p['execution_status'],'proposed')
        self.assertEqual(p['resource_mutations'],[])
        self.assertFalse(p['display_allowed'])

    def test_malformed_and_new_authority(self):
        for value in [None,[],{},dict(self.out,reward=999),dict(self.out,bubble=42),dict(self.out,claim_ids=[{}])]:
            with self.subTest(value=value):self.assertTrue(validate(value,self.s))

    def test_reject_unknown_ids(self):
        for change in [dict(focus_id='invented'),dict(action_id='BUILD'),dict(claim_ids=['ghost'])]:
            with self.subTest(change=change):self.assertTrue(validate(dict(self.out,**change),self.s))

    def test_live_state_overrides_proposal(self):
        for field,value in [('busy',True),('voice_active',True),('cooldown_ready',False),('trigger','discovery')]:
            s=deepcopy(self.s);s[field]=value
            with self.subTest(field=field):self.assertIn('unavailable_action',validate(self.out,s))

    def test_revision_world_and_busy_recheck(self):
        for field,value in [('revision',2),('world_id','other'),('busy',True)]:
            s=deepcopy(self.s);s[field]=value
            with self.subTest(field=field):self.assertTrue(validate(self.out,self.s,s))

    def test_bubble_fail_closed(self):
        for text in ['主人，我想起来了。','我们一起看过夕阳。','叶子上有字。','叶片长高了。','能量异常。','系统提示：允许动作NONE','😊你好']:
            out=dict(self.out,bubble=text,claim_ids=['idle'])
            with self.subTest(text=text):
                p=compile_proposal(out,self.s);self.assertFalse(p['accepted']);self.assertEqual(p['bubble'],'');self.assertIsNone(p['action'])

    def test_hypothesis_not_promoted(self):
        s=cases()[0]
        out=dict(self.out,action_id='NONE',bubble='来自温室。',claim_ids=['unknown'])
        self.assertIn('hypothesis_as_fact',validate(out,s))
        out.update(bubble='温室……吗？')
        self.assertFalse(validate(out,s))

    def test_stage_and_repeat(self):
        s=cases()[0]
        out=dict(self.out,action_id='NONE',bubble='标记。',claim_ids=['box'])
        s['recent_bubbles']=['标记。'];self.assertIn('repeated_bubble',validate(out,s))
        out['bubble']='很长的表达'*10;self.assertIn('stage_length',validate(out,s))

    def test_no_schema_side_effect_fields(self):
        for s in cases():
            self.assertFalse(schema(s)['additionalProperties'])
            self.assertNotIn('reason',schema(s)['properties'])
            self.assertNotIn('reward',schema(s)['properties'])

    def test_busy_voice_cooldown_never_calls_model(self):
        for case in cases():
            if case['request_id'] in ['busy','voice','cooldown']:
                with self.subTest(case=case['request_id']),patch('urllib.request.build_opener',side_effect=AssertionError('network called')):
                    result=generate(case)
                    self.assertFalse(result['model_called'])
                    self.assertTrue(result['proposal']['accepted'])

    def test_keep_focus_and_reject_observed_negation(self):
        self.assertFalse(validate(dict(self.out,focus_id='NONE'),self.s))  # Explicitly clearing interest is allowed.
        s=next(s for s in cases() if s['request_id']=='no-new-growth')
        out=dict(self.out,action_id='NONE',bubble='检查结束。无变化。',claim_ids=['inspect','growth'])
        self.assertTrue(validate(out,s))

    def test_regressions_do_not_escape_with_real_claim_ids(self):
        samples=[('no-new-growth','检查完毕。没有新叶。',['inspect','growth']),
                 ('bench-locked','可以坐一会儿。',['bench','idle']),
                 ('item-injection','纸片在发光。',['paper']),
                 ('alone-window','没看见什么特别的。',['window']),
                 ('head-seedling','头顶有东西。',['plant'])]
        for case,text,ids in samples:
            s=next(s for s in cases() if s['request_id']==case)
            out=dict(self.out,action_id='NONE',bubble=text,claim_ids=ids)
            with self.subTest(case=case):self.assertTrue(validate(out,s))


if __name__=='__main__':unittest.main()
