import unittest
import tempfile
from pathlib import Path
from unittest.mock import patch
from copy import deepcopy
from test_agency import fixture
from device_bridge import context,encode,DeviceBridge
from director import resolve,contract


def roadshow(stage='stage1'):
    s=fixture(stage)
    s['world']['mode']='roadshow'
    s['world']['story']['language_stage']=1
    s['runtime']['communication']['social'].update(pending=False,topic_mask=0,last_life_source=0)
    return s


def record(identifier,intent,result='completed',arrived=True):
    return dict(id=identifier,intent=intent,target_slot=1 if intent=='inspect_plant' else 0,
                reason='routine_plant_check' if intent=='inspect_plant' else 'player_invitation',
                result=result,arrived=arrived,game_second=identifier*10,player_participated=False)


class PersonalSharingTests(unittest.TestCase):
    def test_personal_pause_beats_newer_ordinary_check(self):
        for intent in ('watch_window','rest'):
            s=roadshow();s['world']['life']['history']=[record(10,intent),record(11,'inspect_plant')]
            c=context(s)
            self.assertEqual([x['event_id'] for x in c['social_candidates']],[10])
            self.assertEqual(c['decisions'],[]) # Do not bury the completed pause under a new wish.
            social=c['social_candidates'][0]
            p=resolve(dict(decision_id='NONE',focus_id='KEEP',bubble='',social_id=social['id']),c)
            self.assertTrue(p['decision_accepted']);self.assertIsNone(p['action'])
            self.assertEqual(p['social_intent']['related_event'],10)
            self.assertTrue(encode(s,p).startswith(b'MIND3 '))
            bad=dict(p,social_intent=dict(related_event=11,evidence_kind=1))
            with self.assertRaises(ValueError):encode(s,bad)

    def test_no_consecutive_routine_reports_but_explicit_requests_still_count(self):
        s=roadshow();s['runtime']['communication']['social']['topic_mask']=4
        s['world']['life']['history']=[record(11,'inspect_plant')]
        self.assertEqual(context(s)['social_candidates'],[])
        s['world']['life']['history'][0]['reason']='player_invitation'
        self.assertEqual(context(s)['social_candidates'][0]['topic'],'PLAYER_REQUEST_COMPLETED')

    def test_idle_model_can_choose_personal_wish_or_none_not_more_routine_work(self):
        s=roadshow();c=context(s)
        self.assertEqual({d['intent'] for d in c['decisions']},{'watch_window','rest'})
        self.assertEqual(c['sharing_style']['expression_channel'],'INVITATION_THEN_VOICE')
        self.assertEqual(contract(c)['properties']['bubble']['enum'],[''])
        self.assertTrue(all(d['execution_mode']=='ASK_PERMISSION' for d in c['decisions']))
        for decision in ('1:0','3:0','NONE'):
            p=resolve(dict(decision_id=decision,focus_id='KEEP',bubble='',social_id='NONE'),c)
            self.assertTrue(p['decision_accepted'])
            if p['action']:self.assertEqual(p['execution_status'],'wish_proposed')
        self.assertFalse(resolve(dict(decision_id='2:0',focus_id='KEEP',bubble='',social_id='NONE'),c)['decision_accepted'])

    def test_incomplete_personal_experience_is_not_a_reflection_candidate(self):
        for result,arrived in [('completed',False),('interrupted_by_work',True),('expired',True)]:
            s=roadshow();s['world']['life']['history']=[record(10,'rest',result,arrived)]
            self.assertEqual(context(s)['social_candidates'],[])

    def test_stage0_formal_and_pending_permission_are_preserved(self):
        s=roadshow('stage0');c=context(s)
        self.assertTrue(all(d['intent']=='inspect_plant' for d in c['decisions']))
        s=roadshow('stage1_pending');c=context(s)
        self.assertTrue(all(d['intent']=='inspect_plant' for d in c['decisions']))
        self.assertEqual(c['social_candidates'],[])
        s=roadshow();s['world']['mode']='normal';c=context(s)
        self.assertNotIn('sharing_style',c)
        self.assertTrue(any(d['intent']=='inspect_plant' for d in c['decisions']))

    def test_reflection_survives_work_closing_body_mask_but_requires_device_reply_window(self):
        for deadline,expected in [(25000,True),(0,False)]:
            s=roadshow();s['runtime']['mind'].update(mask=0,trigger_kind=0,reply_deadline_ms=deadline)
            s['world']['life']['history']=[record(10,'rest')]
            with tempfile.TemporaryDirectory() as folder,patch('device_bridge.threading.Thread.start'):
                bridge=DeviceBridge(Path(folder)/'test.sqlite3');bridge.on_snapshot(s)
                self.assertEqual(bridge.inflight,expected)
                self.assertEqual(bridge.jobs.qsize(),int(expected))
                bridge.db.close()

    def test_important_event_is_not_deprived_of_its_normal_choices(self):
        s=roadshow();s['runtime']['mind']['trigger_kind']=1;c=context(s)
        self.assertTrue(any(d['intent']=='inspect_plant' for d in c['decisions']))


if __name__=='__main__':unittest.main()
