#!/usr/bin/env python3
"""Observe live device facts for a bounded interval; never sends device commands."""
import argparse
import json
from pathlib import Path
import time
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=float, default=300)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    samples, errors = [], []
    until = time.monotonic()+args.seconds
    while time.monotonic() < until:
        try:
            data = json.load(opener.open('http://127.0.0.1:8766/api/state', timeout=3))
            if data['connection']['live']:
                snap = data['snapshot']
                if not samples or snap['uptime_ms'] != samples[-1]['uptime_ms']:
                    samples.append(snap)
        except (OSError, ValueError, KeyError) as exc:
            errors.append(type(exc).__name__)
        time.sleep(2)
    failures = []
    if len(samples) < 6:
        failures.append('Too few complete live samples')
    previous_uptime = -1
    for snap in samples:
        r = snap['runtime']
        if snap['uptime_ms'] <= previous_uptime:
            failures.append('Device restarted during observation')
        previous_uptime = snap['uptime_ms']
        if snap['memory']['world_id'] != 'normal-b4200200887481cd':
            failures.append('Saved world identity changed')
        if not snap['world']['storage_available']:
            failures.append('Save storage unavailable')
        if (r.get('background_storage') != 'psram' or not r.get('background_ready') or
                r.get('background_bytes') != 460800 or r.get('background_allocations') != 1):
            failures.append('Background policy or once-only allocation violated')
        if r['psram_free'] < 2*1024*1024:
            failures.append('Observed PSRAM free below 2 MiB')
    report = dict(status='NUMERICAL_CHECKS_PASSED' if not failures else 'NEEDS_INVESTIGATION',
                  observation_seconds=args.seconds, sample_count=len(samples),
                  failures=sorted(set(failures)), connection_errors=errors)
    if samples:
        first, last = samples[0]['runtime'], samples[-1]['runtime']
        report.update(psram_free_start=first['psram_free'], psram_free_end=last['psram_free'],
                      psram_free_lowest_sample=min(s['runtime']['psram_free'] for s in samples),
                      internal_free_lowest_sample=min(s['runtime']['internal_free'] for s in samples),
                      psram_largest_block_end=last.get('psram_largest_free'),
                      background_allocation_counts=sorted(set(s['runtime'].get('background_allocations') for s in samples)),
                      tts_starts_during_window=last['audio']['turn']-first['audio']['turn'],
                      observed_audio_end=last['audio'])
        if report['tts_starts_during_window'] == 0:
            report['voice_verification'] = 'No TTS observed; numerical checks do not verify speech'
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n')
    args.output.chmod(0o600)
    raw = args.output.with_suffix('.samples.json')
    raw.write_text(json.dumps(samples, ensure_ascii=False))
    raw.chmod(0o600)
    print(json.dumps(report, ensure_ascii=False, indent=2), flush=True)


if __name__ == '__main__':
    main()
