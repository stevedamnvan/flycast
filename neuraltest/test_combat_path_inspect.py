"""Independent projection arithmetic tests, not an opaque-game-geometry proof."""
import struct
import unittest
from unittest.mock import patch

from combat_path_inspect import projection, words, verify


def bits(v):
    return struct.unpack('<I', struct.pack('<f', v))[0]


class CombatProjectionTests(unittest.TestCase):
    def test_negative_control_targets_buffer_not_diagnostic(self):
        result = dict(output_base='8ce6e460', projected_words=['43dc80af','439f9171','3d1dec06'])
        diagnostic = 'FC067_WATCH_BEGIN words=43dc80af,439f9171,3d1dec06'
        buffer = 'FC067_COMBAT_BUFFER step=3 address=8ce6e460 words=43dc80af,439f9171,3d1dec06'
        session = '\n'.join([diagnostic, buffer, 'step=3 block=8c0610d2',
                             'fdiv f3.1 <- f3.0, f2.0', 'writem  <- r5.3, f0.2',
                             'reason=eight-block-bound'])
        observed = []
        def inspect(text):
            observed.append(text)
            if text == session:
                return dict(result)
            if buffer not in text or any(s not in text for s in
                    ('step=3 block=8c0610d2','fdiv f3.1 <- f3.0, f2.0',
                     'writem  <- r5.3, f0.2','reason=eight-block-bound')):
                raise ValueError('falsified evidence')
            return dict(result)
        with patch('combat_path_inspect.load_session', return_value=session), \
             patch('combat_path_inspect.inspect_session', side_effect=inspect), \
             patch('combat_path_inspect.compare_native', return_value=54):
            self.assertEqual(verify(None, None)['rejected_offline_controls'], 5)
        self.assertIn(diagnostic, observed[-1])
        self.assertNotIn(buffer, observed[-1])

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
