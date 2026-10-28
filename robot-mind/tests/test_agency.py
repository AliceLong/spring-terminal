import json
import tempfile
import unittest
from copy import deepcopy
from pathlib import Path
from unittest.mock import patch
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from device_bridge import context,encode,DeviceBridge
from director import resolve

ROOT=Path(__file__).resolve().parents[2]
def fixture(name):
    return deepcopy(next(r['snapshot'] for r in json.loads((ROOT/'tests/.build/v56-agency-fixtures.json').read_text()) if r['name']==name))
class AgencyTests(unittest.TestCase):
    def test_stage0_has_only_routine_choices_and_rejects_hallucinated_rest(self):
        s=fixture('stage0');c=context(s)
        self.assertTrue(c['decisions'])
        self.assertTrue(all(d['intent']=='inspect_plant' for d in c['decisions']))
        self.assertFalse(resolve(dict(decision_id='3:0',focus_id='KEEP',bubble='',social_id='NONE'),c)['decision_accepted'])
    def test_stage1_personal_decision_is_a_wish_without_animation(self):
        s=fixture('stage1');c=context(s)
        p=resolve(dict(decision_id='3:0',focus_id='KEEP',bubble='想……休息。可以吗？',social_id='NONE'),c)
        self.assertTrue(p['decision_accepted']);self.assertEqual(p['execution_status'],'wish_proposed')
        self.assertEqual(p['animation_plan'],[]);self.assertEqual(p['resource_mutations'],[])
        self.assertTrue(encode(s,p).startswith(b'MIND3 '))
    def test_pending_wish_cannot_offer_another_or_claim_actual_movement(self):
        s=fixture('stage1_pending');c=context(s)
        self.assertTrue(all(d['intent']=='inspect_plant' for d in c['decisions']))
        self.assertEqual(c['social_candidates'],[])
        self.assertEqual(c['agency']['wish']['status'],'proposed')
        self.assertEqual(c['agency']['wish']['action_sequence'],0)
    def test_device_wish_ack_is_not_action_completion(self):
        s=fixture('stage1');c=context(s)
        p=resolve(dict(decision_id='3:0',focus_id='KEEP',bubble='',social_id='NONE'),c)
        response=dict(proposal=p,device_offer=1,device_boot=1)
        with tempfile.TemporaryDirectory() as folder,patch('device_bridge.threading.Thread.start'):
            b=DeviceBridge(Path(folder)/'test.sqlite3')
            b.db.execute('INSERT INTO requests VALUES(?,?,?,?,?,?)',('test',c['world_id'],'sent',json.dumps(c),json.dumps(response),0));b.db.commit()
            actual=fixture('stage1_pending');actual['runtime']['mind']['status']=6
            b.on_snapshot(actual)
            phase,raw=b.db.execute('SELECT phase,response FROM requests WHERE id=?',('test',)).fetchone()
            self.assertEqual(phase,'finished_wish_pending');r=json.loads(raw)
            self.assertEqual(r['actual_action_status'],'awaiting_permission');self.assertIsNone(r['actual_result'])
            actual['runtime']['communication']['agency']['wish'].update(status='executing',action_sequence=7,chain=44)
            b.on_snapshot(actual)
            self.assertEqual(b.db.execute('SELECT phase FROM requests WHERE id=?',('test',)).fetchone()[0],'wish_executing')
            actual['runtime']['communication']['agency']['wish'].update(status='interrupted',arrived=False,life_result=1)
            b.on_snapshot(actual)
            phase,raw=b.db.execute('SELECT phase,response FROM requests WHERE id=?',('test',)).fetchone()
            self.assertEqual(phase,'finished_wish_result');self.assertEqual(json.loads(raw)['actual_result']['status'],'interrupted')
            b.db.close()
