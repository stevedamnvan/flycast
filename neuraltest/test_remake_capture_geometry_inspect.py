import struct
import tempfile
import unittest
from pathlib import Path

from remake_capture_geometry_check import coverage, inspect


class CaptureGeometryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / 'test.bmp'

    def image(self, lit):
        data = bytearray(54 + 640*480*4)
        data[:2] = b'BM'
        struct.pack_into('<I', data, 10, 54)
        struct.pack_into('<iiHHI', data, 18, 640, -480, 1, 32, 0)
        if lit:
            for y in range(480):
                for x in range(640):
                    if coverage(x+.5, y+.5, .5):
                        data[54+(y*640+x)*4:58+(y*640+x)*4] = b'\xff'*4
        self.path.write_bytes(data)

    def test_exact_and_wrong_camera(self):
        self.image(True)
        exact = inspect(self.path, .5, False)
        self.assertEqual(exact['iou'], 1)
        self.assertEqual(exact['expected_pixels'], 35550)
        self.assertLess(inspect(self.path, -.5, False)['iou'], .31)

    def test_black_is_not_geometry(self):
        self.image(False)
        self.assertEqual(inspect(self.path, .5, False)['iou'], 0)

    def test_reject_header_and_size(self):
        self.path.write_bytes(b'BM')
        with self.assertRaises(ValueError):
            inspect(self.path, .5, False)
        self.image(False)
        with self.path.open('r+b') as stream:
            stream.write(b'XX')
        with self.assertRaises(ValueError):
            inspect(self.path, .5, False)

    def test_nonfinite_camera_rejected(self):
        with self.assertRaises(ValueError):
            inspect(self.path, float('nan'), False)

    def test_analytic_landmarks(self):
        self.assertTrue(coverage(260, 240, .5))
        self.assertFalse(coverage(500, 240, .5))
        self.assertFalse(coverage(260, 100, .5))


if __name__ == '__main__':
    unittest.main()
