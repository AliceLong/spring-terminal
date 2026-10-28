#!/usr/bin/env python3
"""Summarize diagnostic counters, without copying conversational transcripts."""
import argparse
import json
import re
import statistics
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('log', type=Path)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
samples = []
for line in args.log.read_text(errors='replace').splitlines():
    if 'DeskGapScene: sim=' not in line:
        continue
    record = dict(re.findall(r'([a-z_]+)=([^\s]+)', line))
    for key in record:
        if record[key].isdigit(): record[key] = int(record[key])
    record['uptime_ms'] = int(re.search(r'\((\d+)\)', line).group(1))
    samples.append(record)

summary = {'log': str(args.log), 'sample_count': len(samples),
           'warmup_excluded_ms': 10000, 'states': {}}
for state in ('idle', 'listening', 'speaking'):
    rows = [r for r in samples if r['state'] == state and r['uptime_ms'] >= 10000]
    if not rows: continue
    rates = []
    for previous, current in zip(samples, samples[1:]):
        delta = current['uptime_ms'] - previous['uptime_ms']
        if (previous['state'] == current['state'] == state and
            previous['uptime_ms'] >= 10000 and delta > 0 and current['ui'] >= previous['ui']):
            rates.append((current['ui'] - previous['ui']) * 1000 / delta)
    summary['states'][state] = {
        'sample_count': len(rows),
        'median_ui_updates_per_second': round(statistics.median(rates), 2) if rates else None,
        'max_ui_callback_gap_ms': max(r['gap_ms'] for r in rows),
        'max_refresh_cycle_ms': max(r['refresh_ms'] for r in rows),
        'min_free_internal_bytes': min(r['sram'] for r in rows),
        'min_free_psram_bytes': min(r['psram'] for r in rows),
        'creature_states': sorted(set(r['creature'] for r in rows)),
        'last_sim_counter': rows[-1]['sim'], 'last_ui_counter': rows[-1]['ui'],
        'last_flush_counter': rows[-1]['flush'],
    }
summary['panic_seen'] = any(token in args.log.read_text(errors='replace')
                            for token in ('Guru Meditation', 'assert failed', 'Brownout detector'))
args.output.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + '\n')
print(json.dumps(summary, ensure_ascii=False, indent=2))
