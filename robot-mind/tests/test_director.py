import sys
import unittest
from copy import deepcopy
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from director import SimCoordinator,resolve,decide
from evaluate_decisions import activities,claim


class DirectorTests(unittest.TestCase):
    def setUp(self):
        self.sim=SimCoordinator('fixture');self.s=self.sim.snapshot('r1',1)
        self.s['claims']=[claim('work','工作完成，可以休息或观察。')]
        self.s['decisions']=activities('work')
        self.out=dict(decision_id='window',focus_id='KEEP',bubble='')

    def test_decision_has_body_mapping_but_no_automatic_completion(self):
        p=self.sim.accept(self.out,self.s,self.s)
        self.assertTrue(p['decision_accepted']);self.assertEqual(p['action']['intent'],'watch_window')
        self.assertEqual(len(p['animation_plan']),2);self.assertFalse(self.sim.active['arrived'])
        self.assertEqual(self.sim.active['phase'],'requested')
        self.assertEqual(self.sim.replay()['presentation_cue'],'wait_for_dispatch')
        self.assertFalse(any(e['kind']=='completed'for e in self.sim.events))

    def test_bad_text_does_not_cancel_sound_decision(self):
        p=resolve(dict(self.out,bubble='主人，我看过夕阳了。'),self.s)
        self.assertTrue(p['decision_accepted']);self.assertEqual(p['action']['intent'],'watch_window')
        self.assertTrue(p['expression_errors']);self.assertEqual(p['bubble'],'')

    def test_social_decision_is_grounded_and_independent_from_body(self):
        self.s['social_candidates']=[dict(id='12',event_id=12,topic='FIRST_REST')]
        p=resolve(dict(self.out,decision_id='NONE',social_id='12'),self.s)
        self.assertTrue(p['decision_accepted']);self.assertIsNone(p['action'])
        self.assertEqual(p['social_intent']['related_event'],12)
        self.assertFalse(resolve(dict(self.out,social_id='999'),self.s)['decision_accepted'])
        live=deepcopy(self.s);live['social_candidates']=[]
        self.assertFalse(resolve(dict(self.out,social_id='12'),self.s,live)['decision_accepted'])

    def test_illegal_or_stale_decision_is_rejected(self):
        self.assertFalse(resolve(dict(self.out,decision_id='BUILD'),self.s)['decision_accepted'])
        for field,value in [('revision',99),('world_id','demo'),('busy',True),('voice_active',True)]:
            live=deepcopy(self.s);live[field]=value
            with self.subTest(field=field):self.assertFalse(resolve(self.out,self.s,live)['decision_accepted'])

    def test_target_change_without_revision_still_rejected(self):
        live=deepcopy(self.s);live['decisions'][0]['target']='deleted-window'
        self.assertFalse(resolve(self.out,self.s,live)['decision_accepted'])

    def test_coordinator_does_not_overwrite_supplied_live_identity(self):
        live=deepcopy(self.s);live['world_id']='demo'
        self.assertIn('coordinator_context_mismatch',self.sim.accept(self.out,self.s,live)['errors'])
        self.assertFalse(self.sim.events)

    def test_duplicate_and_body_contention(self):
        self.sim.accept(self.out,self.s,self.s);n=len(self.sim.events)
        self.assertIn('duplicate_request',self.sim.accept(self.out,self.s,self.s)['errors'])
        s=self.sim.snapshot('r2');s.update(decisions=activities('work'),claims=self.s['claims'])
        self.assertIn('body_busy',self.sim.accept(self.out,s,s)['errors']);self.assertEqual(n,len(self.sim.events))

    def test_voice_pause_does_not_create_second_intent(self):
        self.sim.accept(self.out,self.s,self.s)
        for phase in ['traveling','arrived','observing','paused_for_voice']:self.sim.transition(phase)
        chain=self.sim.active['chain_id'];self.assertTrue(self.sim.active['arrived'])
        s=self.sim.snapshot('voice');s['voice_active']=True
        with patch('urllib.request.build_opener',side_effect=AssertionError('should not call')):
            result=decide(s)
        self.assertFalse(result['model_called']);self.sim.transition('observing');self.sim.transition('completed')
        self.assertEqual({e['chain_id']for e in self.sim.events if 'chain_id'in e},{chain})
        self.assertEqual(sum(e['kind']=='completed'for e in self.sim.events),1)
        self.assertEqual(self.sim.replay()['presentation_cue'],'idle')

    def test_interrupted_before_arrival_cannot_complete(self):
        self.sim.accept(self.out,self.s,self.s);self.sim.transition('traveling')
        with self.assertRaises(ValueError):self.sim.transition('completed')
        e=self.sim.transition('interrupted_by_work');self.assertFalse(e['arrived']);self.assertIsNone(self.sim.active)

    def test_focus_keeps_evidence_and_resources(self):
        before=deepcopy(self.sim.resources);s=self.s
        s['focus_candidates']=[dict(id='mark',label='标记',certainty='hypothesis',source_ids=['work'])]
        out=dict(self.out,focus_id='mark');p=self.sim.accept(out,s,s)
        self.assertEqual(self.sim.focus['certainty'],'hypothesis')
        self.assertEqual(self.sim.resources,before);self.assertEqual(p['resource_mutations'],[])
        view=self.sim.replay();view['focus']['certainty']='fact'
        self.assertEqual(self.sim.focus['certainty'],'hypothesis')

    def test_discovery_never_executes_action(self):
        self.s['trigger']='discovery'
        self.assertFalse(resolve(self.out,self.s)['decision_accepted'])
        self.assertTrue(resolve(dict(self.out,decision_id='NONE'),self.s)['decision_accepted'])

    def test_unrelated_completion_does_not_erase_pending(self):
        self.sim.accept(self.out,self.s,self.s);self.sim.transition('traveling');self.sim.transition('interrupted_by_work')
        s=self.sim.snapshot('different');s.update(claims=self.s['claims'],decisions=activities('work'))
        self.sim.accept(dict(self.out,decision_id='rest'),s,s)
        for phase in ['traveling','arrived','observing','completed']:self.sim.transition(phase)
        self.assertEqual(self.sim.pending['action']['intent'],'watch_window')
        s=self.sim.snapshot('return');s.update(claims=self.s['claims'],decisions=activities('work',self.sim.pending['action']))
        self.sim.accept(dict(self.out,decision_id='resume'),s,s)
        for phase in ['traveling','arrived','observing','completed']:self.sim.transition(phase)
        self.assertIsNone(self.sim.pending)


if __name__=='__main__':unittest.main()
