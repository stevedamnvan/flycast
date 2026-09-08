import unittest

import test_reverse_copy_inspect as copy_fixture
from sq_writer_inspect import inspect, apply_store, check_controls


class SqWriterTests(unittest.TestCase):
    def setUp(self):
        f = copy_fixture.ReverseCopyTests()
        f.setUp()
        self.scene, self.manifest = f.scene, f.manifest
        packet = f.session.split('words=')[1].splitlines()[0].split(',')
        pcs = ['8c03cc40','8c03cc82','8c03cc84','8c03cc86','8c03cc8c','8c03cc96','8c03cc8e','8c03cc90']
        self.session = ('FC067_SQ_BEGIN cycle=7602643776 generation=1 boundary=first-observed-store\n'
            + ''.join(f'FC067_SQ_STORE event={i+1} generation=1 cycle=7602643776 pc={pcs[i]} '
                      f'address={0xe0000020+4*i:08x} offset={4*i} size=4 expected={int(w,16):016x} '
                      f'actual={int(w,16):016x} exact=1\n' for i,w in enumerate(packet))
            + 'FC067_SQ_FLUSH event=9 generation=1 cycle=7602643776 address=e0000020 coverage=ffffffff exact=1 target=1\n'
            + ''.join(f'FC067_SQ_WORD index={i} value={w} writers={i+1},{i+1},{i+1},{i+1}\n' for i,w in enumerate(packet))
            + 'FC067_SQ_STOP reason=target-covered events=9\n' + f.session)

    def test_supported(self):
        result = inspect(self.session, self.scene, self.manifest)
        self.assertEqual(result['known_bytes'],32)
        self.assertFalse(result['position_calculation_provenance'])

    def test_controls(self):
        self.assertEqual(check_controls(self.session, self.scene, self.manifest),5)

    def test_missing_and_order(self):
        lines = self.session.splitlines()
        for session in ['\n'.join(lines[1:]), '\n'.join(lines[:2]+lines[3:]),
                        '\n'.join(lines[:1]+[lines[2],lines[1]]+lines[3:])]:
            with self.subTest(session=session), self.assertRaises(ValueError):
                inspect(session,self.scene,self.manifest)

    def test_false_last_writer(self):
        with self.assertRaises(ValueError):
            inspect(self.session.replace('writers=2,2,2,2','writers=1,1,1,1'),self.scene,self.manifest)

    def test_wrong_word(self):
        with self.assertRaises(ValueError):
            inspect(self.session.replace('actual=00000000e0000000','actual=00000000e0000001'),self.scene,self.manifest)

    def test_wide_alias_and_overwrite(self):
        known, writers = [None]*32, [None]*32
        row = dict(address='e3000020',offset='0',size='8',pc='8c000002',
                   expected='0807060504030201',actual='0807060504030201',exact='1')
        apply_store(known,writers,row,1)
        self.assertEqual(known[:8],list(range(1,9)))
        row.update(address='e1000024',offset='4',size='4',expected='ffffffff',actual='ffffffff')
        apply_store(known,writers,row,2)
        self.assertEqual(writers[:8],[1]*4+[2]*4)
        self.assertEqual(known[:8],[1,2,3,4]+[255]*4)

    def test_flush_preserves_last_writer(self):
        marker = 'FC067_SQ_FLUSH event=9 generation=1'
        intermediate = 'FC067_SQ_FLUSH event=9 generation=1 cycle=7602643776 address=e0000060 coverage=ffffffff exact=1 target=0\n'
        session = self.session.replace(marker,intermediate+'FC067_SQ_FLUSH event=10 generation=2')
        session = session.replace('target-covered events=9','target-covered events=10')
        self.assertEqual(inspect(session,self.scene,self.manifest)['known_bytes'],32)

    def test_cap_reset_and_width(self):
        for old,new in [('target-covered','cap-or-clock-reset'), ('generation=1','generation=0'),
                        ('size=4','size=2'), ('address=e0000020 offset=0','address=e000003f offset=31'),
                        ('STORE event=1','STORE event=257')]:
            with self.subTest(new=new), self.assertRaises(ValueError):
                inspect(self.session.replace(old,new,1),self.scene,self.manifest)


if __name__ == '__main__':
    unittest.main()
