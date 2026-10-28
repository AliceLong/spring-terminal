"""Bounded private log of numerical diagnostics; no speech text or audio payloads."""
import json
import os
import time
from pathlib import Path


class AudioDiagnosticsLog:
    def __init__(self, path, max_bytes=2*1024*1024):
        self.path = Path(path)
        self.max_bytes = max_bytes

    def write(self, snapshot):
        runtime = snapshot.get('runtime', {})
        audio = runtime.get('audio')
        if not audio:
            return  # Old firmware is unknown, never zero-filled as "no underruns".
        self.path.parent.mkdir(parents=True, exist_ok=True)
        if self.path.exists() and self.path.stat().st_size >= self.max_bytes:
            for i in range(2, 0, -1):
                source = self.path.with_name(self.path.name + (f'.{i}' if i > 0 else ''))
                if source.exists():
                    os.replace(source, self.path.with_name(self.path.name+f'.{i+1}'))
            os.replace(self.path, self.path.with_name(self.path.name+'.1'))
        # Explicit allowlist: the snapshot also contains dialogue-related world fields.
        keys = ('tts_active', 'turn', 'packets', 'queue_rejected', 'decode_errors', 'played_frames',
                'pcm_wait_count', 'pcm_wait_total_us', 'pcm_wait_max_us', 'wait_encoded_pending',
                'packet_gap_max_us', 'decode_max_us', 'write_max_us', 'late_writes', 'pcm_peak',
                'full_scale_samples', 'encoded_queue', 'pcm_queue', 'decoding', 'writing')
        row = dict(received_epoch=time.time(), uptime_ms=snapshot.get('uptime_ms'),
                   background_storage=runtime.get('background_storage'),
                   internal_free=runtime.get('internal_free'), psram_free=runtime.get('psram_free'),
                   audio={key: audio[key] for key in keys if key in audio})
        for key in ('psram_min_free', 'psram_largest_free', 'internal_min_free',
                    'background_bytes', 'background_allocations', 'background_ready'):
            if key in runtime:
                row[key] = runtime[key]
        collection = runtime.get('collection')
        if isinstance(collection, dict):
            allowed = ('practice_only', 'enabled', 'active', 'particles', 'frames', 'batches',
                       'hits', 'misses', 'ignored_shakes', 'cancels', 'max_update_us',
                       'pixel_flash_bytes')
            row['collection'] = {key: collection[key] for key in allowed
                                 if key in collection and isinstance(collection[key], (int, bool))}
        with os.fdopen(os.open(self.path, os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0o600), 'a') as file:
            file.write(json.dumps(row, ensure_ascii=False)+'\n')
