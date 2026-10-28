#!/usr/bin/env python3
"""Summarize real scene-loop counters, without confusing SPI tiles with frames."""
import argparse
import json
import re
import statistics
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('log', type=Path)
p.add_argument('--output', type=Path)
args = p.parse_args()
pattern = re.compile(r'\((\d+)\) DeskGapScene: sim=(\d+) ui=(\d+) audio_ui=(\d+).*?state=(\w+) creature=.*?gap_ms=(\d+) refresh_ms=(\d+).*?psram=(\d+)')
rows = [(int(t),int(sim),int(ui),int(audio),state,int(gap),int(refresh),int(psram))
        for t,sim,ui,audio,state,gap,refresh,psram in pattern.findall(args.log.read_text(errors='replace'))]
groups = {}
for a,b in zip(rows, rows[1:]):
    delta = (b[0]-a[0])/1000
    if a[4] != b[4] or not 1.5 <= delta <= 3.5 or b[1] < a[1] or b[2] < a[2]:
        continue  # boot boundaries, missing log spans, mixed audio states
    g = groups.setdefault(b[4],{'seconds':0,'ui_updates':0,'simulation_ticks':0,'refresh_ms':[],'gap_ms':[],'psram':[]})
    g['seconds'] += delta
    g['ui_updates'] += b[2]-a[2]
    g['simulation_ticks'] += b[1]-a[1]
    g['refresh_ms'].append(b[6]);g['gap_ms'].append(b[5]);g['psram'].append(b[7])
result = {}
for state,g in groups.items():
    result[state] = {'sample_seconds':round(g['seconds'],2),'ui_updates_per_second':round(g['ui_updates']/g['seconds'],2),
                     'simulation_ticks_per_second':round(g['simulation_ticks']/g['seconds'],2),
                     'median_window_max_refresh_ms':statistics.median(g['refresh_ms']),
                     'max_window_refresh_ms':max(g['refresh_ms']), 'max_ui_gap_ms':max(g['gap_ms']),
                     'min_free_psram':min(g['psram'])}
data={'states':result,'encode_drop_warnings':args.log.read_text(errors='replace').count('Encode queue is full'),
      'notes':'UI timer update rate, not panel refresh rate. refresh_ms is each 2-second window maximum. State transitions excluded; UI remains dependent on subtitle/HUD updates.'}
text=json.dumps(data,ensure_ascii=False,indent=2)
if args.output:args.output.write_text(text+'\n')
print(text)
