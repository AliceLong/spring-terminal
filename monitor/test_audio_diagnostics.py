import json
from pathlib import Path
import tempfile
import unittest
from audio_diagnostics import AudioDiagnosticsLog


class AudioDiagnosticsTests(unittest.TestCase):
    def test_bounded_numeric_log_and_old_firmware(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)/'audio.jsonl'
            log = AudioDiagnosticsLog(path, max_bytes=1)
            log.write({'runtime': {}})
            self.assertFalse(path.exists())
            snapshot = {'uptime_ms': 1000, 'world': {'secret': 'dialogue'}, 'runtime': {
                'background_storage': 'psram', 'internal_free': 80000,
                'audio': {'turn': 1, 'pcm_wait_count': 2, 'transcript': 'must not be recorded'}}}
            for _ in range(6):
                log.write(snapshot)
            self.assertEqual(len(list(Path(tmp).iterdir())), 4)
            self.assertEqual(path.stat().st_mode & 0o777, 0o600)
            row = json.loads(path.read_text())
            self.assertEqual(row['audio'], {'turn': 1, 'pcm_wait_count': 2})
            self.assertNotIn('dialogue', path.read_text())
            self.assertNotIn('transcript', path.read_text())
            snapshot['runtime']['collection'] = {'enabled': 1, 'active': 0, 'hits': 6,
                                                 'label': 'must not be recorded',
                                                 'batches': 'invalid string'}
            log.write(snapshot)
            row = json.loads(path.read_text())
            self.assertEqual(row['collection'], {'enabled': 1, 'active': 0, 'hits': 6})
