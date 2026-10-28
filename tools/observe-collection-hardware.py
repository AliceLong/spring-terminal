"""Read-only numerical sampling. Never changes device mode, voice, resources or storage."""
import argparse
import json
import re
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def summarize(rows):
    def group(active):
        selected = [r for r in rows if bool(r.get("collection", {}).get("active")) == active]
        if not selected:
            return {"samples": 0}
        keys = ("internal_free", "psram_free", "internal_min_free", "psram_min_free")
        return dict(samples=len(selected),
                    minima={k: min(r[k] for r in selected if isinstance(r.get(k), int))
                            for k in keys if any(isinstance(r.get(k), int) for r in selected)},
                    max_collection_update_us=max(r.get("collection", {}).get("max_update_us", 0)
                                                 for r in selected),
                    particles=max(r.get("collection", {}).get("particles", 0) for r in selected),
                    tts_samples=sum(r.get("audio", {}).get("tts_active", False) for r in selected))
    return dict(samples=len(rows), idle_or_no_collection=group(False), active_collection=group(True),
                device_audio_listening_test_required=True,
                note="Software counters and GUI refresh cycles do not prove clean audible output.")


def run(seconds, output, log):
    op = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    end = time.monotonic() + seconds
    rows = []
    seen = set()
    while time.monotonic() < end:
        try:
            data = json.load(op.open("http://127.0.0.1:8766/api/state", timeout=3))
            s = data.get("snapshot") or {}
            if data.get("connection", {}).get("live") and s.get("uptime_ms") not in seen:
                seen.add(s["uptime_ms"])
                runtime = s["runtime"]
                keys = ("voice", "internal_free", "psram_free", "internal_min_free", "psram_min_free",
                        "psram_largest_free", "background_storage", "collection", "audio")
                rows.append(dict(uptime_ms=s["uptime_ms"], mode=s["world"]["mode"],
                                 **{key: runtime[key] for key in keys if key in runtime}))
        except (OSError, ValueError, KeyError):
            pass  # Missing data is unavailable, never reported as zero failures.
        time.sleep(min(1, max(0, end - time.monotonic())))
    perf = []
    path = ROOT / log
    if path.exists() and rows:
        first, last = rows[0]["uptime_ms"], rows[-1]["uptime_ms"]
        for line in path.read_text(errors="replace").splitlines():
            if "UI_PERF " not in line:
                continue
            stamp = re.search(r"\((\d+)\)", line)
            fields = {k: int(v) for k, v in re.findall(r"(\w+)=(\d+)", line)}
            if stamp and first <= int(stamp[1]) <= last and fields.get("interval_us"):
                perf.append(dict(uptime_ms=int(stamp[1]), **fields,
                                 lvgl_refresh_cycles_per_second=round(
                                     fields.get("refreshes", 0) * 1000000 / fields["interval_us"], 2)))
    summary = summarize(rows)
    target = ROOT / output
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(dict(summary=summary, samples=rows, ui_performance=perf),
                                 ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(summary, ensure_ascii=False))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=int, default=90)
    parser.add_argument("--output", default="evidence/harvest-v56/hardware-observation.json")
    parser.add_argument("--log", default="evidence/harvest-v56/hardware.log")
    args = parser.parse_args()
    if not 1 <= args.seconds <= 300:
        parser.error("seconds must be 1..300")
    run(args.seconds, args.output, args.log)
