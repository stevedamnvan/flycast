"""Synthetic copied-context parser controls, not emulated rendering."""
from pathlib import Path
import unittest
from unittest.mock import patch

from transform_context_inspect import inspect_context


def fixture(negative=False):
    return ('FC067_PACKET_COPY context=00100000 offset=64 bytes=32 exact=1 generation=1\n'
            f'FC067_PACKET_DECODE child=00100000 root=00100000 expected_context={0x100020 if negative else 0x100000:08x} '
            f'context_exact={0 if negative else 1} packet_exact=1 vertex=7 '
            'xyz=3f800000,40000000,40400000 xyz_exact=1 generation=1')


class ContextParserTests(unittest.TestCase):
    def inspect(self, text, negative=False):
        with patch('transform_context_inspect.inspect_ta', return_value=dict(packet=[0, 0x3f800000, 0x40000000, 0x40400000])), \
                patch('transform_context_inspect.load_session', return_value=text):
            return inspect_context(Path('synthetic'), negative)

    def test_positive_negative(self):
        self.assertEqual(self.inspect(fixture()), self.inspect(fixture(True), True))

    def test_missing_negative(self):
        with self.assertRaises(ValueError):
            self.inspect(fixture(), True)

    def test_invalid_fields(self):
        changes = [('root=00100000', 'root=00100020'), ('child=00100000', 'child=00100020'),
                   ('offset=64', 'offset=65'), ('bytes=32', 'bytes=28'), ('vertex=7', 'vertex=65536'),
                   ('packet_exact=1', 'packet_exact=0'), ('xyz_exact=1', 'xyz_exact=0'),
                   ('xyz=3f800000', 'xyz=3f800001'), ('expected_context=00100000', 'expected_context=00100020'),
                   ('generation=1', 'generation=2')]
        for old, new in changes:
            with self.subTest(old=old), self.assertRaises(ValueError):
                self.inspect(fixture().replace(old, new))

    def test_invalidation(self):
        with self.assertRaises(ValueError):
            self.inspect(fixture() + '\nFC067_PACKET_STOP reason=context-recycle')

    def test_reordering(self):
        with self.assertRaises(ValueError):
            self.inspect('\n'.join(reversed(fixture().splitlines())))


if __name__ == '__main__':
    unittest.main()
