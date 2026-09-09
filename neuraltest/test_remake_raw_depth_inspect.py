import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest


class RawDepthTests(unittest.TestCase):
    def test_empty_invalid_and_bad_size(self):
        script = Path(__file__).with_name('remake_raw_depth_check.py')
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'raw'
            for value, invalid in ((1., 0), (float('nan'), 307200)):
                path.write_bytes(struct.pack('<ffff', value, 0., 0., 1.) * 307200)
                run = subprocess.run([sys.executable, str(script), str(path)],
                                     capture_output=True, text=True, timeout=20)
                self.assertEqual(run.returncode, 0, run.stderr)
                result = json.loads(run.stdout)
                self.assertEqual(result['counts']['invalid'], invalid)
                self.assertIsNone(result['matched_mae'])
            path.write_bytes(b'bad')
            run = subprocess.run([sys.executable, str(script), str(path)],
                                 capture_output=True, text=True, timeout=20)
            self.assertNotEqual(run.returncode, 0)


if __name__ == '__main__':
    unittest.main()
