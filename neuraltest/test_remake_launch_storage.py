import unittest
from unittest.mock import patch
from types import SimpleNamespace
from remake_launch import capture_storage_preflight

class CaptureStorageTests(unittest.TestCase):
    def test_insufficient_space_rejects_before_launch(self):
        with patch('remake_launch.shutil.disk_usage', return_value=SimpleNamespace(free=0)):
            with self.assertRaisesRegex(ValueError, 'Capture storage preflight'):
                capture_storage_preflight('C:/nonexistent-capture-test/output', 300, '640x480')

    def test_estimate_scales_with_resolution_and_frames(self):
        with patch('remake_launch.shutil.disk_usage', return_value=SimpleNamespace(free=10**12)):
            low = capture_storage_preflight('C:/nonexistent-capture-test/output', 3, '640x480')
            high = capture_storage_preflight('C:/nonexistent-capture-test/output', 300, '1280x960')
            self.assertGreater(high['estimated_required_bytes'], low['estimated_required_bytes'])
            self.assertFalse(high['reservation'])

    def test_non_capture_run_has_no_storage_probe(self):
        with patch('remake_launch.shutil.disk_usage', side_effect=AssertionError('unexpected probe')):
            self.assertIsNone(capture_storage_preflight('C:/unused', 0, '640x480'))

if __name__ == '__main__':
    unittest.main()
