import struct
import unittest
from effect_material_inspect import inspect


class BytesInput:
    def __init__(self, words):
        self.data = struct.pack('<%dI' % len(words), *words)
    def read_bytes(self):
        return self.data


class CensusTests(unittest.TestCase):
    def fixture(self):
        # One partially transparent pixel from draw7; all other pixels empty.
        body = [1, 1, 0, 23, 0, 100, 0, 8, 640 * 480, 1, 0,
                1, 0x12345680, 0x3f000000, 7 << 17, (4 << 29) | (5 << 26), 0xffffffff]
        body += [0] * (640 * 480 - 1)
        return [0x31494645, len(body)] + body

    def test_truth(self):
        result = inspect(BytesInput(self.fixture()))
        self.assertEqual(result['source_ordinal'], 23)
        draw, = result['draws']
        self.assertEqual((draw['ordinal'], draw['alpha_partial'], draw['alpha_min'], draw['bounds']), (7, 1, 128, [0, 0, 0, 0]))
        self.assertEqual((draw['source_factor'], draw['destination_factor']), (4, 5))
        self.assertFalse(result['material_or_promotion_proven'])

    def test_rejections(self):
        for kind in ('magic', 'length', 'truncated', 'trailing', 'layers'):
            with self.subTest(kind=kind):
                w = self.fixture()
                if kind == 'magic': w[0] = 0
                if kind == 'length': w[1] += 1
                if kind == 'truncated': w.pop(); w[1] -= 1
                if kind == 'trailing': w.append(0); w[1] += 1
                if kind == 'layers': w[13] = 9
                with self.assertRaises(ValueError): inspect(BytesInput(w))


if __name__ == '__main__':
    unittest.main()
