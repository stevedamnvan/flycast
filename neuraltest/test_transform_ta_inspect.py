"""Synthetic association-parser controls, not emulated TA execution."""
from pathlib import Path
import unittest
from unittest.mock import patch

from transform_ta_inspect import inspect_ta


DERIVED = dict(source=(0x0c100000,), actual=[0, 0x40400000, 0x3f800000, 0x40000000])


def fixture(negative=False):
    xyz = (0x3f800000, 0x40000000, 0x40400000)
    lines = ['FC067_G2_BEGIN base=0c100000 bytes=12 generation=2 coverage=shil-cpu-only']
    for ordinal, (pc, i, event) in enumerate(zip((0x8c070cb4, 0x8c070ea0, 0x8c070ea4, 0x8c070ea8),
                                               (2, 0, 1, 2), (33, 58, 60, 62))):
        lines += [f'FC067_G2_ACCESS event={event} pc={pc:08x} address={0x0c100000+4*i:08x} '
                  f'size=4 write=0 bytes={xyz[i]:016x}',
                  f'FC067_G2_READ pc={pc:08x} value={xyz[i]:016x} expected={xyz[i]:016x} exact=1']
        if ordinal:
            lines.append(f'FC067_G2_STORE pc={pc+2:08x} address={0xe0100404+4*i:08x} '
                         f'value={xyz[i]:08x} stored={xyz[i]:08x} ram=0 sq=1 exact=1')
    lines += ['FC067_G2_STORE context=unrelated',
              'FC067_G2_FLUSH address=e0100400 area=3 destination=0c100400 ram_exact=1 '
              'words=e0000000,3f800000,40000000,40400000,00000000,00000000,ffffffff,00000000',
              'FC067_G3_BEGIN base=0c100400 bytes=32 source=sq-flush coverage=shil-sq-ta-entry']
    lines += ['FC067_G2_STORE context=unrelated']*5
    lines += ['FC067_G2_FLUSH context=unrelated',
              'FC067_G2_ACCESS event=381 pc=8c070c40 address=0c100008 size=4 write=1 bytes=0000000040400000',
              'FC067_G2_STOP reason=overwrite events=381 pc=8c070c40 ta_lineage=unknown',
              f'FC067_G3_TA_BULK address=00000000 count=100 packet_offset=64 expected_offset={96 if negative else 64} '
              f'association_exact={0 if negative else 1} base=0c100400 exact=1 poly=1 events=1234 cycles=100 start=0 '
              'words=e0000000,3f800000,40000000,40400000,00000000,00000000,ffffffff,00000000']
    for i, reg in enumerate(('r2.1', 'r3.1', 'r2.2')):
        suffix = '' if i == 0 else f', {4*i}'
        lines += [f'FC067_SPAN_SHIL pc={0x8c070ea0+4*i:08x} op=readm {reg} <- r4.0{suffix}',
                  f'FC067_SPAN_SHIL pc={0x8c070ea2+4*i:08x} op=writem  <- r14.0, {reg}, {4*(i+1)}']
    lines.append('FC067_SPAN_SHIL pc=8c070eb0 op=pref  <- r14.0')
    return '\n'.join(lines)


class TaParserTests(unittest.TestCase):
    def inspect(self, text, negative=False, **kwargs):
        with patch('transform_ta_inspect.inspect_derived', return_value=DERIVED), \
                patch('transform_ta_inspect.load_session', return_value=text):
            return inspect_ta(Path('synthetic'), negative, **kwargs)

    def test_explicit_additional_event_profile(self):
        text = fixture()
        for before, after in ((58, 48), (60, 50), (62, 52), (381, 377)):
            text = text.replace(f'event={before} ', f'event={after} ')
            text = text.replace(f'events={before} ', f'events={after} ')
        self.inspect(text, access_events=(33, 48, 50, 52, 377))
        with self.assertRaisesRegex(ValueError, 'consumer address/event'):
            self.inspect(text)

    def test_positive_negative(self):
        self.assertEqual(self.inspect(fixture()), self.inspect(fixture(True), True))

    def test_missing_negative(self):
        with self.assertRaises(ValueError):
            self.inspect(fixture(), True)

    def test_invalid_fields(self):
        changes = [('count=100', 'count=2'), ('packet_offset=64', 'packet_offset=65'),
                   ('area=3', 'area=4'), ('ram_exact=1', 'ram_exact=0'),
                   ('poly=1', 'poly=0'), ('r2.2, 12', 'r2.1, 12'),
                   ('events=1234', 'events=5000001'), ('cycles=100', 'cycles=200000001'),
                   ('base=0c100400 exact=1', 'base=0c100420 exact=1'),
                   ('association_exact=1', 'association_exact=0')]
        for old, new in changes:
            with self.subTest(old=old), self.assertRaises(ValueError):
                self.inspect(fixture().replace(old, new))

    def test_invalidated_generation(self):
        with self.assertRaises(ValueError):
            self.inspect(fixture() + '\nFC067_G3_STOP reason=overwrite')

    def test_reordered_copy(self):
        lines = fixture().splitlines()
        lines[4], lines[5] = lines[5], lines[4]
        with self.assertRaises(ValueError):
            self.inspect('\n'.join(lines))


if __name__ == '__main__':
    unittest.main()
