"""Independent projection arithmetic tests, not an opaque-game-geometry proof."""
import struct
import unittest

from combat_path_inspect import projection, words


def bits(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


class CombatProjectionTests(unittest.TestCase):
    def test_analytic_projection(self):
        result = projection(list(map(bits, (8., 6., 2., 1.))), bits(1.), list(map(bits, (320., 240.))), 3)
        self.assertEqual(result, list(map(bits, (324., 243., .5))))

    def test_depth_scale_is_separate_from_xy(self):
        args = list(map(bits, (8., 6., 2., 1.))), list(map(bits, (320., 240.)))
        a, b = [projection(args[0], bits(scale), args[1], 3) for scale in (1., 2.)]
        self.assertEqual(a[:2], b[:2])
        self.assertNotEqual(a[2], b[2])

    def test_wrong_offset_sign_changes_xy(self):
        source = list(map(bits, (8., 6., 2., 1.)))
        positive = projection(source, bits(1.), list(map(bits, (320., 240.))), 3)
        wrong = projection(source, bits(1.), list(map(bits, (-320., 240.))), 3)
        self.assertNotEqual(positive[0], wrong[0])
        self.assertEqual(positive[1:], wrong[1:])

    def test_captured_separate_rounding_golden(self):
        self.assertEqual(projection([0x453a5930,0x44f3bd67,0x41c51ec0,0x3f800000],
                                    0x3f733333,[0x43a00000,0x43700000],3),
                         [0x43dc80af,0x439f9171,0x3d1dec06])

    def test_invalid_contract_and_zero_depth(self):
        for source in ([0,0,0,bits(1.)], [0,0,bits(1.),0], [0,0,bits(1.)]):
            with self.assertRaises(ValueError):
                projection(source, bits(1.), [0,0], 3)

    def test_word_bounds(self):
        self.assertEqual(words('00000000,ffffffff'), [0,0xffffffff])
        for value in ('100000000', '-1'):
            with self.assertRaises(ValueError):
                words(value)


if __name__ == '__main__':
    unittest.main()
