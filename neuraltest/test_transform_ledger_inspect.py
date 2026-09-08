import unittest
import zlib
from transform_ledger_inspect import decode,COMPRESSED_LIMIT


class TransformLedgerTests(unittest.TestCase):
    def test_exact_events_and_negative_markers_preserved(self):
        raw=b'FC067_CT_BLOCK generation=10 slot=920 kind=0\nFC067_CT_REJECT reason=wrong-writer\n'
        self.assertEqual(decode(zlib.compress(raw)),raw.decode())
    def test_truncated_trailing_corrupt_and_concatenated(self):
        data=zlib.compress(b'event\n')
        for bad in (data[:-1],data+b'x',data+data,b'not zlib',b'\0'*(COMPRESSED_LIMIT+1)):
            with self.subTest(),self.assertRaises(ValueError):decode(bad)
    def test_expansion_limit_and_line_shape(self):
        data=zlib.compress(b'x'*4095+b'\n')
        self.assertEqual(len(decode(data,4096)),4096)
        with self.assertRaises(ValueError):decode(data,4095)
        for raw in (b'no terminator',b'bad\0line\n',b'\xff\n'):
            with self.subTest(),self.assertRaises(ValueError):decode(zlib.compress(raw))


if __name__=='__main__':unittest.main()
