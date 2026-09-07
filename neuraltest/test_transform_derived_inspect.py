"""Parser/mutation controls; synthetic fixtures do not execute the guest."""
from pathlib import Path
import unittest
from unittest.mock import patch

from transform_derived_inspect import inspect_derived


def fixture(negative=False):
    lines = []
    values = [(0x8c070c74, 0x3f800000, 0x4163a016, 0, 0, 0x3d8ff4b2),
              (0x8c070c88, 0x3f851eb8, 0x3d8ff4b2, 0, 0, 0x3d95b6cd),
              (0x8c070c9a, 0x43a00000, 0x457dcb43, 0x3d8ff4b2, 1, 0x44175b90),
              (0x8c070ca2, 0x43700000, 0xc50958f1, 0x3d8ff4b2, 1, 0x42ab1024)]
    for i, (pc, a, b, c, fused, value) in enumerate(values):
        shadow = a ^ (0x80000000 if negative and i == 2 else 0)
        lines += [f'FC067_ARITH_IN pc={pc:08x} a={a:08x} b={b:08x} c={c:08x} '
                  f'oracle_a={shadow:08x} fused={fused} mxcsr=0000fffd',
                  f'FC067_ARITH_OUT pc={pc:08x} value={value:08x}']
        if i:
            storepc, address = ((0x8c070c92, 0x8c00f304), (0x8c070c9c, 0x8c00f2fc),
                                (0x8c070ca4, 0x8c00f300))[i-1]
            lines.append(f'FC067_DERIVED_STORE pc={storepc:08x} address={address:08x} '
                         f'value={value:08x} stored={value:08x} ram=1 exact=1')
    lines.append('FC067_DERIVED_END generation=2 candidate=true ta_lineage=unknown camera=unknown')
    return '\n'.join(lines)


class DerivedParserTests(unittest.TestCase):
    def inspect(self, text, negative=False):
        source = (0x8c00f2fc, [0x457dcb43, 0xc50958f1, 0x4163a016, 0x3f800000])
        with patch('transform_derived_inspect.inspect_span', return_value=(source, 0x8c8b6d00, '3d95b6cd')), \
                patch('transform_derived_inspect.load_session', return_value=text):
            return inspect_derived(Path('synthetic'), negative)

    def test_positive_negative(self):
        self.assertEqual(self.inspect(fixture())['rejected'], [False]*4)
        self.assertEqual(self.inspect(fixture(True), True)['rejected'], [False, False, True, False])

    def test_wrong_fields(self):
        changes = [('mxcsr=0000fffd', 'mxcsr=00009ffd'), ('fused=1', 'fused=0'),
                   ('stored=44175b90', 'stored=44175b91'), ('address=8c00f304', 'address=8c00f308'),
                   ('camera=unknown', 'camera=proven'), ('generation=2', 'generation=1'),
                   ('b=4163a016', 'b=4163a017'), ('oracle_a=43a00000', 'oracle_a=c3a00000'),
                   ('ram=1', 'ram=0'), ('value=42ab1024', 'value=42ab1025')]
        for old, new in changes:
            with self.subTest(old=old), self.assertRaises(ValueError):
                self.inspect(fixture().replace(old, new))

    def test_order_rejected(self):
        lines = fixture().splitlines()
        lines[3], lines[4] = lines[4], lines[3]
        with self.assertRaises(ValueError):
            self.inspect('\n'.join(lines))

    def test_missing_negative_rejected(self):
        with self.assertRaises(ValueError):
            self.inspect(fixture(), True)


if __name__ == '__main__':
    unittest.main()
