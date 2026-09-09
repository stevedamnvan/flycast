import unittest
import struct
import io
from PIL import Image
from source_dds import encode


class DdsTests(unittest.TestCase):
    def test_header_and_exact_payload(self):
        levels=[dict(width=2,height=2,rgba=bytes(range(16))),dict(width=1,height=1,rgba=b'RGBA')]
        d=encode(levels,srgb=False)
        self.assertEqual(len(d),168)
        self.assertEqual(d[:4],b'DDS ')
        self.assertEqual(struct.unpack_from('<I',d,4)[0],124)
        self.assertEqual(struct.unpack_from('<5I',d,128),(28,3,0,1,0))
        self.assertEqual(d[148:],bytes(range(16))+b'RGBA')
        self.assertEqual(struct.unpack_from('<I',encode(levels,srgb=True),128)[0],29)
        for srgb in (False,True):
            decoded=Image.open(io.BytesIO(encode(levels,srgb=srgb)))
            self.assertEqual(decoded.convert('RGBA').tobytes(),levels[0]['rgba'])
    def test_invalid_chain(self):
        with self.assertRaises(ValueError):encode([],srgb=False)
        with self.assertRaises(ValueError):encode([dict(width=1,height=1,rgba=b'abc')],srgb=False)
        with self.assertRaises(ValueError):encode([dict(width=1,height=1,rgba=b'abcd')]*2,srgb=False)
