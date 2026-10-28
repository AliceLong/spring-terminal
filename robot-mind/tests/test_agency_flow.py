import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from device_bridge import context, encode
from director import resolve
from evaluate_agency_flow import SCENARIOS, fixture, replay


class AgencyFlowTests(unittest.TestCase):
    def setUp(self):
        self.snapshot = fixture()
        self.ctx = context(self.snapshot)
        # Deterministic contract test; this is NOT reported as a real LLM choice.
        proposal = resolve(dict(decision_id="3:0", focus_id="KEEP", bubble="",
                                social_id="NONE"), self.ctx)
        self.assertEqual(proposal["execution_status"], "wish_proposed")
        self.packet = encode(self.snapshot, proposal)

    def test_full_chain_uses_real_core_and_shared_query(self):
        flow = replay(self.packet, "approve")
        steps = {s["step"]: s for s in flow["steps"]}
        for label in ("proposed_without_movement", "opened_without_approval"):
            self.assertFalse(steps[label]["life_active"])
            self.assertEqual(steps[label]["query"]["agency"]["wish"]["action_sequence"], 0)
        requested = steps["approved_actual_request_created"]
        completed = steps["completed_with_queryable_evidence"]
        wish = completed["query"]["agency"]["wish"]
        self.assertTrue(wish["arrived"])
        self.assertEqual(wish["status"], "completed")
        self.assertEqual(wish["action_sequence"], requested["life_sequence"])
        self.assertEqual(wish, completed["query"]["agency"]["history"][-1])
        self.assertTrue(flow["resources_unchanged"])
        self.assertFalse(flow["hardware"])

    def test_consent_and_interruption_matrix(self):
        for case in SCENARIOS:
            with self.subTest(case=case):
                flow = replay(self.packet, case)
                self.assertTrue(flow["assertions_pass"])
                self.assertTrue(flow["resources_unchanged"])
                last = flow["steps"][-1]
                wish = last["query"]["agency"]["wish"]
                if case.startswith("interrupt_") or case == "restart_executing":
                    self.assertEqual(wish["status"], "interrupted")
                    self.assertEqual(wish["arrived"], case == "interrupt_after_arrival")
                if case in ("same_turn", "wrong_session", "wrong_token", "story_busy",
                            "stage_regressed", "work_before_approval"):
                    self.assertFalse(last["life_active"])
                    self.assertEqual(wish["status"], "presented")
                    self.assertEqual(wish["approved_game_second"], 0)
                if case in ("defer", "decline", "restart_pending"):
                    self.assertFalse(last["life_active"])
                    self.assertEqual(wish["action_sequence"], 0)
                    self.assertNotEqual(wish["status"], "completed")


if __name__ == "__main__":
    unittest.main()
