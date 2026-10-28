import sys
import unittest
import json
import tempfile
import time
from unittest.mock import patch
from copy import deepcopy
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from device_bridge import context,encode,DeviceBridge


def snap():
    return dict(uptime_ms=5000,memory=dict(world_id='normal-0000000b00000016'),
        runtime=dict(mind=dict(boot=1,offer=2,mask=0x111,sequence=9,deadline_ms=25000)),
        world=dict(revision=22,story=dict(language_stage=0),plants=[dict(slot=1,name='轨光花')],
            life=dict(language_stage=0,history=[dict(id=9,intent='watch_window',target_slot=0,
                result='interrupted_by_work',arrived=False)])))


class DeviceBridgeTests(unittest.TestCase):
    def test_only_available_actions_and_actual_stage(self):
        c=context(snap());self.assertEqual(c['language_stage'],0)
        self.assertEqual(len(c['decisions']),3)
        self.assertEqual(c['decisions'][0]['reason'],'RESUME_INTERRUPTED_ACTIVITY')
        self.assertFalse(c['unfinished_activity']['arrived'])

    def test_packet_cannot_include_generated_text_or_resources(self):
        p=dict(action=dict(intent='watch_window',target=0),raw_bubble='能源增加999')
        self.assertEqual(encode(snap(),p),b'MIND1 1 2 b 16 9 1 0\n')
        p['action']=None;self.assertTrue(encode(snap(),p).endswith(b' 0 0\n'))

    def test_wrong_target_unavailable_or_invalid_world(self):
        for action in [dict(intent='watch_window',target=1),dict(intent='inspect_plant',target=3)]:
            with self.subTest(action=action),self.assertRaises(ValueError):encode(snap(),dict(action=action))
        s=snap();s['memory']['world_id']='other'
        with self.assertRaises(ValueError):context(s)

    def test_social_packet_uses_only_completed_device_record_and_deduplicates(self):
        s=snap();s['runtime']['communication']=dict(social=dict(pending=False,topic_mask=0,last_offered_id=0))
        r=s['world']['life']['history'][0]
        self.assertEqual(context(s)['social_candidates'],[])
        r.update(result='completed',arrived=True)
        proposal=dict(action=None,social_intent=dict(related_event=9))
        self.assertEqual(encode(s,proposal),b'MIND2 1 2 b 16 9 0 0 9\n')
        s['runtime']['communication']['social']['topic_mask']=2
        self.assertEqual(context(s)['social_candidates'],[])
        with self.assertRaises(ValueError):encode(s,proposal)

    def test_social_candidates_keep_latest_valid_evidence_per_unshared_topic(self):
        s=snap();s['runtime']['communication']=dict(social=dict(pending=False,topic_mask=0,last_offered_id=0))
        s['world']['life']['history']=[
            dict(id=10,intent='watch_window',result='completed',arrived=True),
            dict(id=11,intent='watch_window',result='completed',arrived=True),
            dict(id=12,intent='rest',result='completed',arrived=True),
            dict(id=13,intent='rest',result='interrupted_by_work',arrived=True),
            dict(id=14,intent='inspect_plant',result='completed',arrived=True)]
        for record in s['world']['life']['history']:record['target_slot']=0
        self.assertEqual([x['event_id'] for x in context(s)['social_candidates']],[12,11])
        social=s['runtime']['communication']['social']
        social['topic_mask']=2
        self.assertEqual([x['event_id'] for x in context(s)['social_candidates']],[12])
        social.update(topic_mask=0,last_offered_id=11)
        self.assertEqual([x['event_id'] for x in context(s)['social_candidates']],[12])
        social['pending']=True
        self.assertEqual(context(s)['social_candidates'],[])

    def test_completion_does_not_fabricate_unfinished(self):
        s=snap();h=deepcopy(s['world']['life']['history'][0]);h.update(id=10,result='completed',arrived=True)
        s['world']['life']['history'].append(h)
        self.assertIsNone(context(s)['unfinished_activity'])

    def test_sent_is_not_execution_and_result_requires_device_evidence(self):
        with tempfile.TemporaryDirectory() as folder,patch('device_bridge.threading.Thread.start'):
            b=DeviceBridge(Path(folder)/'facts.sqlite3')
            s=snap();world=s['memory']['world_id'];m=s['runtime']['mind']
            m.update(mask=0,status=1,deadline_ms=0,current_life_sequence=9)
            s['world']['storage_available']=True
            response=dict(device_offer=2,device_boot=1,proposal=dict(action=dict(intent='watch_window',target=0)))
            b.db.execute('INSERT INTO requests VALUES(?,?,?,?,?,?)',('test',world,'sent','{}',json.dumps(response),0));b.db.commit()
            b.on_snapshot(s)
            self.assertNotEqual(b.db.execute('SELECT phase FROM requests').fetchone()[0],'device_accepted')
            b.db.execute("UPDATE requests SET phase='sent'");b.db.commit()
            m.update(status=2,sequence=10,current_life_sequence=10)
            b.on_snapshot(s)
            self.assertEqual(b.db.execute('SELECT phase,sequence FROM requests').fetchone(),('device_accepted',10))
            s['world']['life']['history'].append(dict(id=10,intent='watch_window',target_slot=0,result='interrupted_by_work',arrived=False))
            b.on_snapshot(s)
            phase,raw=b.db.execute('SELECT phase,response FROM requests').fetchone()
            self.assertEqual(phase,'finished')
            self.assertFalse(json.loads(raw)['actual_result']['arrived'])
            b.db.close()

    def test_work_can_cancel_body_without_discarding_fresh_social_reply(self):
        class Stream:
            def __init__(self):self.data=[]
            def write(self,data):self.data.append(data);return len(data)
        with tempfile.TemporaryDirectory() as folder,patch('device_bridge.threading.Thread.start'):
            b=DeviceBridge(Path(folder)/'facts.sqlite3');original=snap()
            live=deepcopy(original);live['runtime']['mind'].update(mask=0,deadline_ms=0,reply_deadline_ms=25000,sequence=10)
            b.last=live;b.received=time.monotonic()
            stream=Stream()
            b.outbox.put((original,'social',b'MIND2 bounded\n',True));b.flush(stream)
            self.assertEqual(len(stream.data),1)
            b.outbox.put((original,'body',b'MIND1 bounded\n',False));b.flush(stream)
            self.assertEqual(len(stream.data),1)
            live['runtime']['mind']['reply_deadline_ms']=5000
            b.outbox.put((original,'expired',b'MIND2 bounded\n',True));b.flush(stream)
            self.assertEqual(len(stream.data),1)
            live['runtime']['mind']['reply_deadline_ms']=25000
            live['memory']['world_id']='demo-0000000b00000016'
            b.outbox.put((original,'wrong_world',b'MIND2 bounded\n',True));b.flush(stream)
            self.assertEqual(len(stream.data),1)
            b.db.close()

    def test_social_only_acceptance_never_claims_body_action_started(self):
        with tempfile.TemporaryDirectory() as folder,patch('device_bridge.threading.Thread.start'):
            b=DeviceBridge(Path(folder)/'facts.sqlite3');s=snap()
            s['world']['storage_available']=True
            s['runtime']['mind'].update(mask=0,status=4,deadline_ms=0,reply_deadline_ms=0,sequence=10,current_life_sequence=10)
            s['runtime']['communication']=dict(social=dict(pending=True,last_offered_id=9))
            response=dict(device_offer=2,device_boot=1,proposal=dict(action=dict(intent='rest',target=0),
                social_intent=dict(related_event=9)))
            b.db.execute('INSERT INTO requests VALUES(?,?,?,?,?,?)',('test',s['memory']['world_id'],'sent','{}',json.dumps(response),0));b.db.commit()
            b.on_snapshot(s)
            phase,raw,seq=b.db.execute('SELECT phase,response,sequence FROM requests').fetchone()
            self.assertEqual((phase,seq),('finished_social_only',0))
            result=json.loads(raw)
            self.assertEqual(result['actual_action_status'],'not_started')
            self.assertNotIn('actual_result',result)
            b.db.close()


if __name__=='__main__':unittest.main()
