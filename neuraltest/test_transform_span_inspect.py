"""Synthetic parser controls only; not SH4, DMA, TA or camera tests."""
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from test_transform_store_inspect import fixture as source_fixture
from transform_span_inspect import inspect_span


def fixture(negative=False):
    lines = [source_fixture(), 'FC067_SPAN_BEGIN base=8c00f2fc bytes=16 sequence=1 coverage=shil-cpu-access-only']
    for ordinal, (pc, index) in enumerate(zip((0x8c070c4a, 0x8c070c52, 0x8c070c5a, 0x8c070c64), (0, 1, 2, 2))):
        value = index + 1
        mutated = negative and ordinal == 0
        lines += [f'FC067_SPAN_ACCESS event={ordinal*2+1} pc={pc:08x} '
                  f'address={0x8c00f2fc+4*index:08x} size=4 write=0 bytes={value:016x} phase=before-access',
                  f'FC067_SPAN_READ_RESULT pc={pc:08x} value={value:016x} '
                  f'expected={value ^ int(mutated):016x} exact={0 if mutated else 1}']
        if ordinal < 3:
            lines.append(f'FC067_SPAN_CONSUMER_STORE pc={pc+4:08x} address={0x8c800000+4*index:08x} '
                         f'value={value:08x} stored={value:08x} ram=1 exact=1 copy=1 source_match=1')
    lines += ['FC067_SPAN_ACCESS event=10 pc=8c070c92 address=8c00f304 size=4 write=1 bytes=0000000000000003 phase=before-access',
              'FC067_SPAN_STOP reason=overwriting-store pc=8c070c92 events=10 ta_lineage=unknown',
              'FC067_SPAN_CONSUMER_STORE pc=8c070c92 address=8c00f304 value=00000099 stored=00000099 ram=1 exact=1 copy=0 source_match=0']
    for i in range(3):
        lines += [f'FC067_SPAN_SHIL pc={0x8c070c4a+8*i:08x} op=readm f3.{i+1} <- r4.0',
                  f'FC067_SPAN_SHIL pc={0x8c070c4e+8*i:08x} op=writem  <- r10.0, f3.{i+1}, {32+4*i}']
    return '\n'.join(lines)


class SpanParserTests(unittest.TestCase):
    def inspect(self, text, negative=False):
        with patch.object(Path, 'stat', return_value=SimpleNamespace(st_size=len(text))), \
                patch.object(Path, 'read_text', return_value=text):
            return inspect_span(Path('synthetic'), negative)

    def test_positive_negative(self):
        self.assertEqual(self.inspect(fixture()), self.inspect(fixture(True), True))

    def test_old_launch_ignored(self):
        self.assertEqual(self.inspect(fixture(True) + '\n' + fixture()), self.inspect(fixture()))

    def test_wrong_fields(self):
        changes = [('event=3', 'event=2'), ('address=8c800004', 'address=8c800010'),
                   ('source_match=1', 'source_match=0'), ('ta_lineage=unknown', 'ta_lineage=proven'),
                   ('reason=overwriting-store', 'reason=event-cap'), ('bytes=16', 'bytes=32'),
                   ('stored=00000099', 'stored=00000098'), ('f3.2, 36', 'f3.1, 36'),
                   ('value=0000000000000002', 'value=0000000000000001'),
                   ('coverage=shil-cpu-access-only', 'coverage=all-access')]
        for original, wrong in changes:
            with self.subTest(original=original), self.assertRaises(ValueError):
                self.inspect(fixture().replace(original, wrong))

    def test_reordered_copy_rejected(self):
        lines = fixture().splitlines()
        index = next(i for i, line in enumerate(lines) if 'FC067_SPAN_CONSUMER_STORE ' in line)
        lines[index-1], lines[index] = lines[index], lines[index-1]
        with self.assertRaises(ValueError):
            self.inspect('\n'.join(lines))

    def test_missing_negative_rejected(self):
        with self.assertRaises(ValueError):
            self.inspect(fixture(), True)


if __name__ == '__main__':
    unittest.main()
