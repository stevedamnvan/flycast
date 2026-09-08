"""Relocate independent synthetic seam fixtures without loosening their math checks."""
import re
import unittest

import initial_edge_inspect as supply
import initial_record_inspect as initial
import factor_edge_inspect as pred
import record_calc_inspect as calc
from test_initial_edge_inspect import fixture as supply_fixture
from test_initial_record_inspect import fixture as initial_fixture
from test_factor_edge_inspect import fixture as pred_fixture
from test_record_calc_inspect import fixture as calc_fixture


def relocate(text, base, cycle):
    def address(match):
        value=int(match[0],16)
        return f'{base+value-0x8ce74250:x}' if 0x8ce74250<=value<=0x8ce74270 else match[0]
    text=re.sub(r'\b[0-9a-f]{8}\b',address,text)
    return re.sub(r'cycle=\d+',f'cycle={cycle}',text)


class TransformTargetTests(unittest.TestCase):
    def test_six_record_targets_and_three_generations(self):
        bases=(0x8ce74250,0x8ce74230,0x8ce74240,0x8ce6e460,0x8ce6e470,0x8ce6e480)
        for generation in range(1790,1793):
            for base in bases:
                cycle=str(generation*1000)
                with self.subTest(base=base,generation=generation):
                    text=relocate(supply_fixture(),base,cycle).replace('generation=1 ',f'generation={generation} ')
                    result=supply.inspect_edge(text,base,cycle,str(generation))
                    self.assertEqual(result['transformed_words'][:3],[0x41200000,0x40a00000,0x41a80000])
                    self.assertFalse(result['world_camera_recovered'])
                    initial.inspect_block(relocate(initial_fixture(),base,cycle),base,cycle)
                    pred.inspect_edge(relocate(pred_fixture(),base,cycle),base,cycle)
                    calc.inspect_block(relocate(calc_fixture(),base,cycle),base,cycle)

    def test_wrong_target_rejected_by_all_seams(self):
        base=0x8ce6e460; cycle='123456'
        for fixture,check in ((supply_fixture,supply.inspect_edge),(initial_fixture,initial.inspect_block),
                              (pred_fixture,pred.inspect_edge),(calc_fixture,calc.inspect_block)):
            text=relocate(fixture(),base,cycle)
            with self.subTest(check=check),self.assertRaises(ValueError): check(text,base+16,cycle)
            with self.subTest(check=check),self.assertRaises(ValueError): check(text,base,'123457')

    def test_wrong_generation_and_nonunit_w_preserved(self):
        base=0x8ce74230; cycle='123456'
        text=relocate(supply_fixture(identity=True,w=0x3f800001),base,cycle)
        result=supply.inspect_edge(text,base,cycle)
        self.assertEqual(result['source_point_words'][3],0x3f800001)
        self.assertEqual(result['transformed_words'][3],0x3f800001)
        with self.assertRaisesRegex(ValueError,'edge identity'):
            supply.inspect_edge(text,base,cycle,'2')


if __name__=='__main__': unittest.main()
