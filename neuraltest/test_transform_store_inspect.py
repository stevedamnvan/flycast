"""Parser failure controls; these do not execute SH4 or prove camera semantics."""
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from transform_store_inspect import inspect_capture


def fixture(negative=False):
    lines = ['DX11 Context initializing',
             'FC067_FTRV_SITE pc=8c070c3c cycles=6000468096 '
             'input=00000000,00000000,00000000,3f800000 '
             'output=00000001,00000002,00000003,00000004 lineage=uncorrelated']
    lines += [f'FC067_FTRV_MATRIX pc=8c070c3c column={i} '
              'bits=00000000,00000000,00000000,00000000 meaning=unknown' for i in range(4)]
    for ordinal in range(4):
        index = 3 - ordinal
        value = index + 1
        expected = value ^ (1 if negative and index == 0 else 0)
        lines.append(f'FC067_FTRV_STORE pc={0x8c070c3e+ordinal*2:08x} '
                     f'address={0x8c00f2fc+index*4:08x} index={index} '
                     f'value={value:08x} expected={expected:08x} ram=1 '
                     f'stored={value:08x} ordered=1 '
                     f'exact={0 if negative and index == 0 else 1} ta_lineage=unknown')
    return '\n'.join(lines)


class StoreParserTests(unittest.TestCase):
    def inspect(self, text, negative=False):
        with patch.object(Path, 'stat', return_value=SimpleNamespace(st_size=len(text))), \
                patch.object(Path, 'read_text', return_value=text):
            return inspect_capture(Path('synthetic'), negative)

    def test_positive_and_negative(self):
        self.assertEqual(self.inspect(fixture()), self.inspect(fixture(True), True))

    def test_old_launch_ignored(self):
        self.assertEqual(self.inspect('DX11 Context initializing\noverflow=true\n' + fixture()),
                         self.inspect(fixture()))

    def test_invalid_last_launch_not_rescued_by_old(self):
        with self.assertRaises(ValueError):
            self.inspect(fixture() + '\nDX11 Context initializing\n')

    def test_wrong_controls(self):
        replacements = [
            ('cycles=6000468096', 'cycles=1'),
            ('column=2', 'column=1'),
            ('meaning=unknown', 'meaning=camera'),
            ('ta_lineage=unknown', 'ta_lineage=proven'),
            ('pc=8c070c40', 'pc=8c070c42'),
            ('address=8c00f300', 'address=8c00f310'),
            ('stored=00000004', 'stored=00000005'),
            ('ordered=1', 'ordered=0'),
            ('index=3', 'index=0'),
            ('input=00000000', 'input=invalid'),
        ]
        for original, changed in replacements:
            with self.subTest(original=original), self.assertRaises(ValueError):
                self.inspect(fixture().replace(original, changed))

    def test_overflow_rejected(self):
        with self.assertRaises(ValueError):
            self.inspect(fixture() + '\noverflow=true')

    def test_missing_negative_rejected(self):
        with self.assertRaises(ValueError):
            self.inspect(fixture(), True)


if __name__ == '__main__':
    unittest.main()
