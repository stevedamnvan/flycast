"""Aggregate protocol checks; mocked arithmetic is not GPU/source evidence."""
import unittest
from unittest.mock import patch
import large_transform_arithmetic_inspect as checker
from test_camera_transform_inspect import fixture,INITIAL,FINAL


def protocol():
    text,_=fixture(True)
    for frame in range(3):
        generation=1790+frame
        tag=f'FC067_CT_FRAME generation={generation}'
        text=text.replace(tag,f'FC067_CT_PREFIX generation={generation} ordinal={1781+frame} copies=5069 cycle={1900+frame*1000}\n'+tag)
    return text


def run(text):
    def pred(section,base,*args):
        return dict(factor_word=FINAL[2],source_loads=[(base+4,INITIAL[1]),(base+8,INITIAL[2])],written_registers=[17,18,19])
    with patch.object(checker,'inspect_supply',return_value=dict(transformed_words=INITIAL,source_point_words=[],xf_matrix_words=[])), \
         patch.object(checker,'inspect_initial',return_value=dict(initial_xyz_words=INITIAL)), \
         patch.object(checker,'inspect_pred',side_effect=pred), \
         patch.object(checker,'inspect_calc',return_value=dict(final_words_xyz=FINAL,live_input_words={16:INITIAL[0],17:INITIAL[1],19:FINAL[2]})):
        return checker.inspect(text)


class LargeArithmeticTests(unittest.TestCase):
    def test_indexed_protocol(self):
        result=run(protocol())
        self.assertEqual(result['records'],144)
        self.assertEqual(result['reads'],1278)
        self.assertFalse(result['world_camera_recovered'])

    def test_protocol_mutations(self):
        text=protocol()
        for old,new in [('FC067_SUPPLY_OP index=0 synthetic=1','MISSING_DESCRIPTOR'),
                        ('writer=3 count=1','writer=2 count=1'),
                        ('pc=8c03c94e','pc=8c03c94c'),
                        ('cycle=1900','cycle=999'),
                        ('FC067_X_EDGE slot=0 generation=1790','FC067_X_EDGE slot=1 generation=1790'),
                        ('kind=3 base=1000','kind=3 base=1004'),
                        ('events=6 writers=6,6,6,6','events=6 writers=5,5,5,5')]:
            self.assertIn(old,text)
            with self.subTest(old=old),self.assertRaises(ValueError):run(text.replace(old,new,1))


if __name__=='__main__':unittest.main()
