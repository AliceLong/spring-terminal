import unittest
from test_observation import fixture
from device_bridge import context


class ChapterObservationTests(unittest.TestCase):
    def test_precondition_flags_are_not_completed_action_claims(self):
        snapshot = fixture('ordinary_idle')
        snapshot['runtime']['observation']['chapter_context'] = dict(
            scope='roadshow_fixture_and_live_events', beyond_work_observed=True,
            bench_rest_completed=True, observation_verified=False, seat_rest_verified=False)
        result = context(snapshot)
        self.assertNotIn('chapter-observation', [x['id'] for x in result['claims']])
        self.assertNotIn('chapter-seat', [x['id'] for x in result['claims']])

    def test_verified_experiences_reach_the_model_without_expanding_actions(self):
        snapshot = fixture('ordinary_idle')
        before = context(snapshot)['decisions']
        snapshot['runtime']['observation']['chapter_context'] = dict(
            scope='device_observation', observation_verified=True, seat_rest_verified=True)
        result = context(snapshot)
        self.assertEqual(result['decisions'], before)
        self.assertIn('chapter-observation', [x['id'] for x in result['claims']])
        self.assertIn('chapter-seat', [x['id'] for x in result['claims']])
        self.assertTrue(result['story_observation']['chapter_context']['seat_rest_verified'])


if __name__ == '__main__':
    unittest.main()
