import unittest
from alternate_source_inspect import inspect


def fixture():
    lines=[]
    for n in (1781,1782,1783):
        lines.append(f'FC067_ALT_REJECT ordinal={n} offset=32 generation={n} context=00100000 path=2 address=e0000000 words=1,2,3,4,5,6,7,8 exact=1 executed_sq_pc=00001000 executed_sq_address=e0000000')
        lines.append(f'FC067_ALT_WRITER_REJECT ordinal={n} offset=32 writers=100,102,104 valid_mask=7 ram=00100000,00100004,00100008')
    return '\n'.join(lines)


class AlternateSourceTests(unittest.TestCase):
    def test_positive_scope(self):
        r=inspect(fixture(),fixture(),3)
        self.assertTrue(r['packet_parity'])
        self.assertFalse(r['original_transform_lineage'])
        self.assertFalse(r['independent_absolute_source_address_proof'])

    def test_address_value_generation_and_identity_controls(self):
        for old,new,reason in (
            ('00100004','00100008','source address structure'),
            ('words=1,2','words=1,9','packet words mismatch'),
            ('generation=1781','generation=1782','packet generation mismatch'),
            ('valid_mask=7','valid_mask=3','read/store validity'),
            ('executed_sq_pc=00001000','executed_sq_pc=00001002','packet executed_sq_pc mismatch')):
            with self.subTest(reason=reason),self.assertRaisesRegex(ValueError,reason):
                inspect(fixture().replace(old,new,1),fixture(),3)

    def test_duplicates_missing_and_rejection(self):
        s=fixture()
        for bad in (s+'\n'+s.splitlines()[0], '\n'.join(s.splitlines()[1:]),s+'\nFC067_DRAW_REJECT reason=overflow'):
            with self.assertRaises(ValueError):inspect(bad,s,3)

    def test_uniform_address_shift_is_not_absolute_proof(self):
        s=fixture().replace('ram=00100000,00100004,00100008','ram=00200000,00200004,00200008')
        self.assertFalse(inspect(s,fixture(),3)['independent_absolute_source_address_proof'])
