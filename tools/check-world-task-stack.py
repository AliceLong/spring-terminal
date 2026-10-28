#!/usr/bin/env python3
"""Reject oversized compiled world-worker frames before hardware deployment.

This checks target ELF prologues, not host sanitizer stacks. It is a conservative
regression guard; the device's stack high-water measurement is still required.
"""
import argparse
import json
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("elf")
parser.add_argument("--objdump", default="xtensa-esp-elf-objdump")
args = parser.parse_args()
disassembly = subprocess.check_output([args.objdump, "-d", "-C", args.elf], text=True)
frames = {}
for match in re.finditer(
    r"^[0-9a-f]+ <([^\n]+)>:\n[0-9a-f]+:\s+\w+\s+entry\s+a1, (0x[0-9a-f]+|\d+)",
    disassembly, re.MULTILINE,
):
    frames[match[1]] = int(match[2], 0)

worker = "desk_gap::(anonymous namespace)::Worker(void*)"
restore_names = [
    "desk_gap::(anonymous namespace)::LoadGame(unsigned long, bool, spring::Core&)",
    "spring::Core::RestoreStory(spring::StorySave const&)",
    "spring::Core::RestoreArchive(std::array<unsigned long, 2636u> const&)",
    "spring::Core::RestoreFull(std::array<unsigned long, 384u> const&)",
    "spring::Core::Restore(std::array<unsigned long, 128u> const&, bool)",
]
missing = [name for name in [worker, *restore_names] if name not in frames]
if missing:
    raise SystemExit("Cannot verify target stack frames: " + ", ".join(missing))
selected = {name: frames[name] for name in [worker, *restore_names]}
# Leave space for nested restore/checkpoint frames and library calls. The sum
# documents known restore frames, not an exact runtime call-chain bound.
known_sum = sum(selected.values())
accepted = frames[worker] <= 4096 and known_sum + 4096 <= 24576
print(json.dumps({
    "accepted": accepted,
    "task_stack_bytes": 24576,
    "worker_frame_budget": 4096,
    "known_restore_frames": selected,
    "known_frames_sum": known_sum,
    "runtime_high_water_required": True,
}, indent=2))
raise SystemExit(0 if accepted else 1)
