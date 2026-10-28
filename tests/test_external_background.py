"""Storage migration contract: preserve voice assets, bounds, alignment, repeat updates."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('spring_pack', ROOT/'tools/pack-spring-background.py')
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)


class MigrationTest(unittest.TestCase):
    def test_preserves_all_files_and_aligns_native_pixels(self):
        files = {'index.json': (b'{"version":1}', 0, 0),
                 'srmodels.bin': (b'voice-model' * 17, 0, 0),
                 'font.bin': (bytes(range(256)), 0, 0),
                 'happy.png': (b'png-bytes', 24, 32)}
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            base, output, pixels = tmp/'base', tmp/'assets', tmp/'pixels'
            base.write_bytes(pack.codec().pack_assets_entries(files))
            pixels.write_bytes(bytes(range(256)) * 1800)
            report = pack.build(base, output, pixels)
            decoded = pack.codec().read_assets_entries(output)
            for key, value in files.items():
                self.assertEqual(decoded[key], value)
            self.assertEqual(report['preserved_files'], 4)
            blob = output.read_bytes()
            count = struct.unpack_from('<I', blob)[0]
            for i in range(count):
                _, size, offset, _, _ = struct.unpack_from('<32sIIHH', blob, 12+44*i)
                start = 12+44*count+offset+2
                self.assertEqual(start % 4, 0)
                self.assertLessEqual(start+size, len(blob))
            # Repacking a newer game image replaces the entry instead of appending copies.
            pixels.write_bytes(b'\x5a' * 460800)
            second = tmp/'second'
            pack.build(output, second, pixels)
            self.assertEqual(len(pack.codec().read_assets_entries(second)), count)
            self.assertEqual(second.stat().st_size, len(blob))

    def test_rejects_corrupt_base_and_partition_overflow(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)/'bad'
            good = bytearray(pack.pack({'index.json': (b'{}', 0, 0)}))
            good[-1] ^= 1
            base.write_bytes(good)
            with self.assertRaises(ValueError):
                pack.build(base, Path(tmp)/'out')
        with self.assertRaises(ValueError):
            pack.pack({'index.json': (b'x'*pack.PARTITION_SIZE, 0, 0)})


if __name__ == '__main__':
    unittest.main()
