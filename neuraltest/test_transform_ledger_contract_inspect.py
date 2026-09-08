import unittest
import zlib
from transform_ledger_contract_inspect import check_concurrent


class ConcurrentLedgerTests(unittest.TestCase):
    def test_ordered_interleaving(self):
        lines=[f'writer={writer} event={event}\n' for event in range(2000) for writer in range(8)]
        check_concurrent(zlib.compress(''.join(lines).encode()))
        for bad in (lines[:-1],lines[:1]+lines,lines[:1]+lines[:1]+lines[2:],
                    [lines[8]]+lines[1:8]+[lines[0]]+lines[9:]):
            with self.subTest(),self.assertRaises(ValueError):check_concurrent(zlib.compress(''.join(bad).encode()))


if __name__=='__main__':unittest.main()
