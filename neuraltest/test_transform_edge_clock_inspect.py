"""A scheduler boundary needs an exact successor stamp, not a loose tolerance."""
import re
import unittest
from initial_edge_inspect import inspect_edge as supply
from factor_edge_inspect import inspect_edge as pred
from test_initial_edge_inspect import fixture as supply_fixture
from test_factor_edge_inspect import fixture as pred_fixture


class TransformEdgeClockTests(unittest.TestCase):
    def test_exact_next_stamp_and_reversed_or_wrong_controls(self):
        for inspect,fixture,tag,cycle in [(supply,supply_fixture,'SUPPLY',7602512960),
                                         (pred,pred_fixture,'PRED',7602640640)]:
            original=fixture()
            def edge_at(value):
                return re.sub(r'(FC067_'+tag+r'_EDGE [^\n]*cycle=)\d+',
                              lambda m:m[1]+str(value),original)
            changed=edge_at(cycle+448)
            with self.subTest(tag=tag):
                inspect(changed,next_cycle=str(cycle+448))
                with self.assertRaises(ValueError): inspect(changed)
                with self.assertRaises(ValueError): inspect(changed,next_cycle=str(cycle+449))
                with self.assertRaises(ValueError): inspect(edge_at(cycle-1),next_cycle=str(cycle-1))


if __name__=='__main__': unittest.main()
