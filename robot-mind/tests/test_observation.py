import json
import sys
import tempfile
import unittest
from copy import deepcopy
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from device_bridge import context,encode,DeviceBridge
from director import resolve,decide

ROOT=Path(__file__).resolve().parents[2]

def fixture(name):
    rows=json.loads((ROOT/'tests/.build/v55-fixtures.json').read_text())
    return deepcopy(next(r['snapshot'] for r in rows if r['name']==name))

class ObservationTests(unittest.TestCase):
    def test_roadshow_can_share_fresh_routine_completion_but_not_replay_or_invent(self):
        s=fixture('ordinary_idle')
        s['world']['mode']='roadshow'
        social=s['runtime']['communication']['social']
        social.update(pending=False,topic_mask=0,last_life_source=20)
        record=dict(id=21,intent='inspect_plant',target_slot=1,reason='routine_check',
                    result='completed',arrived=True)
        s['world']['life']['history']=[record]
        c=context(s)
        self.assertTrue(c['roadshow_mode'])
        self.assertEqual(c['attention_budget']['model_cooldown_seconds'],30)
        self.assertEqual([x['topic'] for x in c['social_candidates']],['ROUTINE_CHECK_COMPLETED'])
        social['last_life_source']=21
        self.assertEqual(context(s)['social_candidates'],[])
        social['last_life_source']=20
        for key,value in [('arrived',False),('result','interrupted_by_work')]:
            previous=record[key];record[key]=value
            self.assertEqual(context(s)['social_candidates'],[])
            record[key]=previous
        social['pending']=True
        self.assertEqual(context(s)['social_candidates'],[])
        social['pending']=False
        for mode in ('normal','demo'):
            s['world']['mode']=mode;c=context(s)
            self.assertFalse(c['roadshow_mode'])
            self.assertEqual(c['attention_budget']['model_cooldown_seconds'],120)
            self.assertEqual(c['social_candidates'],[])

    def test_only_read_archives_and_real_world_are_observed(self):
        c=context(fixture('locked_future_archive'))
        self.assertEqual(c['world_observation']['resources']['water'],40)
        self.assertEqual([a['id'] for a in c['story_observation']['known_archives']],[0])
        self.assertNotIn('G-17',json.dumps(c,ensure_ascii=False))
        s=fixture('locked_future_archive')
        s['runtime']['observation']['known_archives'].append(dict(id=3,available=True,player_read=True,excerpt='secret'))
        self.assertEqual([a['id'] for a in context(s)['story_observation']['known_archives']],[0])

    def test_completed_invitation_can_be_remembered_focused_and_shared_without_new_action(self):
        s=fixture('invitation_completed');c=context(s)
        self.assertEqual(c['trigger_reason'],'PLAYER_REQUEST_COMPLETED')
        important=c['character_observation']['important_invitation']
        self.assertTrue(important['player_requested']);self.assertIsNone(important['player_watched'])
        focus=next(f for f in c['focus_candidates'] if f['id'].startswith('experience-'))
        social=next(x for x in c['social_candidates'] if x['topic']=='PLAYER_REQUEST_COMPLETED')
        p=resolve(dict(decision_id='NONE',focus_id=focus['id'],social_id=social['id'],bubble=''),c)
        self.assertTrue(p['decision_accepted']);self.assertIsNone(p['action'])
        words=encode(s,p).decode().split()
        self.assertEqual(words[0],'MIND3');self.assertEqual(len(words),14)
        self.assertEqual(words[9],'1');self.assertEqual(words[10],'2')
        s['world']['life']['history']=[];s['memory']['records']=[]
        self.assertTrue(any(x['topic']=='PLAYER_REQUEST_COMPLETED' for x in context(s)['social_candidates']))

    def test_budget_allows_thought_without_body_and_preserves_one_inflight(self):
        s=fixture('shortage_after_invitation');c=context(s)
        self.assertEqual(c['decisions'],[])
        self.assertTrue(c['social_candidates'])
        with tempfile.TemporaryDirectory() as folder,patch('device_bridge.threading.Thread.start'):
            b=DeviceBridge(Path(folder)/'test.sqlite3');b.on_snapshot(s)
            self.assertTrue(b.inflight);self.assertEqual(b.jobs.qsize(),1)
            s['runtime']['mind']['offer']+=1;b.on_snapshot(s)
            self.assertEqual(b.jobs.qsize(),1)
            b.db.close()

    def test_busy_voice_does_not_start_background_model(self):
        s=fixture('invitation_completed');s['runtime']['voice']='speaking'
        with tempfile.TemporaryDirectory() as folder,patch('device_bridge.threading.Thread.start'):
            b=DeviceBridge(Path(folder)/'test.sqlite3');b.on_snapshot(s)
            self.assertFalse(b.inflight);self.assertTrue(b.jobs.empty());b.db.close()

    def test_unknown_focus_event_and_routine_work_cannot_become_memory(self):
        s=fixture('ordinary_idle');c=context(s)
        self.assertEqual(c['social_candidates'],[])
        p=resolve(dict(decision_id='NONE',focus_id='archive-3',social_id='NONE',bubble=''),c)
        self.assertFalse(p['decision_accepted'])
        self.assertFalse(resolve(dict(decision_id='NONE',focus_id='KEEP',social_id='life:999',bubble=''),c)['decision_accepted'])
        with self.assertRaises(ValueError):encode(s,dict(action=None,focus_mode='archive-3',focus_update=dict(id='archive-3',device_focus=dict(kind=3,source=3,target=0))))

    def test_model_failure_is_recorded_and_budget_released(self):
        s=fixture('invitation_completed')
        with tempfile.TemporaryDirectory() as folder,patch('device_bridge.decide',side_effect=TimeoutError('test timeout')):
            b=DeviceBridge(Path(folder)/'test.sqlite3');b.on_snapshot(s);b.jobs.join()
            self.assertFalse(b.inflight);self.assertTrue(b.outbox.empty())
            phase,raw=b.db.execute('SELECT phase,response FROM requests').fetchone()
            self.assertEqual(phase,'model_failed');self.assertEqual(json.loads(raw)['error'],'TimeoutError')
            b.db.close()

    def test_malformed_and_illegal_model_outputs_never_reach_device(self):
        import io
        import urllib.error
        responses=[None,'not-json',json.dumps(dict(decision_id='BUILD_ALL',focus_id='KEEP',social_id='NONE',bubble=''))]
        for content in responses:
            class Opener:
                def open(self,*args,**kwargs):
                    if content is None:raise urllib.error.URLError('offline test')
                    return io.BytesIO(json.dumps(dict(choices=[dict(finish_reason='stop',message=dict(content=content))])).encode())
            with self.subTest(content=content),tempfile.TemporaryDirectory() as folder,patch('director.urllib.request.build_opener',return_value=Opener()):
                b=DeviceBridge(Path(folder)/'test.sqlite3');b.on_snapshot(fixture('invitation_completed'));b.jobs.join()
                self.assertFalse(b.inflight);self.assertTrue(b.outbox.empty())
                phase=b.db.execute('SELECT phase FROM requests').fetchone()[0]
                self.assertIn(phase,['model_failed','model_validation_rejected']);b.db.close()

    def test_animal_invitation_is_never_a_sixth_plant_slot(self):
        s=fixture('invitation_completed')
        important=s['runtime']['observation']['character']['important_invitation']
        important.update(target=5,target_kind='animal_home',target_name='动物小窝',plant_id=None,target_slot=0)
        c=context(s);claim=next(x for x in c['claims'] if x['id'].startswith('invitation-'))
        self.assertIn('动物小窝',claim['text']);self.assertNotIn('种植位6',claim['text'])
        f=next(x for x in c['focus_candidates'] if x['id'].startswith('experience-'))
        self.assertIn('动物小窝',f['label']);self.assertEqual(f['device_focus']['target'],5)

    def test_social_only_ack_records_actual_shared_character_and_source(self):
        s=fixture('invitation_completed');c=context(s)
        f=next(x for x in c['focus_candidates'] if x['id'].startswith('experience-'))
        social=c['social_candidates'][0]
        p=resolve(dict(decision_id='NONE',focus_id=f['id'],social_id=social['id'],bubble=''),c)
        m=s['runtime']['mind'];m.update(status=4,social_status=1,focus_status=1,deadline_ms=0,reply_deadline_ms=0)
        actual=dict(active=False,pending=True,source_id=social['event_id'],source_kind=1)
        s['runtime']['communication']['social']=actual
        with tempfile.TemporaryDirectory() as folder,patch('device_bridge.threading.Thread.start'):
            b=DeviceBridge(Path(folder)/'test.sqlite3')
            response=dict(proposal=p,device_offer=m['offer'],device_boot=m['boot'])
            b.db.execute('INSERT INTO requests VALUES(?,?,?,?,?,?)',('ack',s['memory']['world_id'],'sent','{}',json.dumps(response),0));b.db.commit()
            b.on_snapshot(s)
            phase,raw=b.db.execute("SELECT phase,response FROM requests WHERE id='ack'").fetchone();r=json.loads(raw)
            self.assertEqual(phase,'finished_social_only');self.assertTrue(r['character_accepted']);self.assertEqual(r['actual_social'],actual)
            self.assertEqual(r['device_character'],s['runtime']['observation']['character']);b.db.close()

    def test_serial_failure_releases_attention_budget(self):
        import time
        class Broken:
            def write(self,data):raise OSError('disconnected test')
        with tempfile.TemporaryDirectory() as folder,patch('device_bridge.threading.Thread.start'):
            b=DeviceBridge(Path(folder)/'test.sqlite3');s=fixture('invitation_completed')
            b.last=s;b.received=time.monotonic();b.inflight=True
            b.outbox.put((s,'test',b'MIND3 bounded\n',True));b.flush(Broken())
            self.assertFalse(b.inflight);self.assertEqual(b.status['transport_status'],'write_failed');b.db.close()

if __name__=='__main__':unittest.main()
