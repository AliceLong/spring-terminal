"""Replay real local model wishes through the firmware Core on the host.

No serial connection, monitor commands, NVS, cloud role updates, or game save I/O.
Voice turns and the renderer's arrival signal are explicitly simulated inputs.
"""
import argparse
import json
import subprocess
from pathlib import Path

from device_bridge import context, encode
from director import decide

ROOT = Path(__file__).resolve().parents[1]
RUNNER = ROOT / "tests/.build/agency_flow_runner"
SCENARIOS = (
    "approve", "defer", "decline", "same_turn", "wrong_session", "wrong_token",
    "work_before_approval", "story_busy", "stage_regressed", "restart_pending",
    "interrupt_before_arrival", "interrupt_after_arrival", "restart_executing",
)


def fixture():
    return json.loads(subprocess.check_output([str(RUNNER), "fixture"], cwd=ROOT, text=True))


def replay(packet, scenario):
    if scenario not in SCENARIOS:
        raise ValueError("unknown scenario")
    result = subprocess.run(
        [str(RUNNER), scenario], cwd=ROOT, input=packet, capture_output=True, check=True,
    )
    return json.loads(result.stdout)


def evaluate(output, runs=3):
    if not RUNNER.exists():
        raise SystemExit("Run bash tools/test-communication.sh to build the isolated Core runner.")
    rows = []
    for trial in range(runs):
        snapshot = fixture()
        ctx = context(snapshot)
        row = {"trial": trial + 1, "real_model_call": True, "simulated_world": True,
               "sent_to_device": False, "flows": []}
        try:
            result = decide(ctx)
            proposal = result["proposal"]
            row.update(output=result["output"], seconds=result["seconds"], proposal=proposal,
                       policy_sha256=result["policy_sha256"])
            row["protocol_pass"] = bool(proposal["decision_accepted"])
            action = proposal.get("action")
            if not row["protocol_pass"]:
                row["coverage"] = "invalid_model_decision"
            elif not action or action.get("execution_mode") != "ASK_PERMISSION":
                # NONE/routine is legal; never substitute a handcrafted personal wish.
                row["coverage"] = "model_did_not_choose_a_personal_wish"
            else:
                packet = encode(snapshot, proposal)
                row["packet"] = packet.decode().strip()
                row["flows"] = [replay(packet, case) for case in SCENARIOS]
                row["coverage"] = "personal_wish_full_flow"
        except Exception as exc:
            row.update(error=type(exc).__name__, protocol_pass=False, coverage="error")
        rows.append(row)
    covered = [r for r in rows if r["coverage"] == "personal_wish_full_flow"]
    summary = {
        "real_model_calls": runs,
        "valid_model_decisions": sum(r["protocol_pass"] for r in rows),
        "actual_model_wishes": len(covered),
        "core_flow_replays": sum(len(r["flows"]) for r in covered),
        "cases_per_wish": len(SCENARIOS),
        "all_observed_flows_pass": bool(covered) and all(
            f["assertions_pass"] for r in covered for f in r["flows"]
        ),
        "scope": "Real local LLM -> wire parser/guard -> real firmware Core -> real query JSON. "
                 "Host-only; voice consent, renderer arrival and restart callbacks simulated. "
                 "Not a hardware, cloud speech, audio or personality acceptance result.",
    }
    target = ROOT / output
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps({"summary": summary, "rows": rows}, ensure_ascii=False,
                                 indent=2) + "\n")
    print(json.dumps(summary, ensure_ascii=False))
    return summary


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default="evidence/v56/agency-flow-model-evaluation.json")
    parser.add_argument("--runs", type=int, default=3)
    args = parser.parse_args()
    if not 1 <= args.runs <= 20:
        parser.error("runs must be 1..20")
    summary = evaluate(args.output, args.runs)
    raise SystemExit(0 if summary["all_observed_flows_pass"] and
                     summary["valid_model_decisions"] == args.runs else 1)
