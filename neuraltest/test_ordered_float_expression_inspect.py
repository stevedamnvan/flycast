import unittest
from ordered_float_expression import evaluate


class OrderedExpressionTests(unittest.TestCase):
    def test_order_changes_rounding(self):
        a=('literal',0x4b800000)  # 2**24
        b=('literal',0xcb800000)
        c=('literal',0x3f800000)
        self.assertEqual(evaluate(('fadd',('fadd',a,b),c)),0x3f800000)
        self.assertEqual(evaluate(('fadd',('fadd',a,c),b)),0)

    def test_unknown_and_zero_divisor(self):
        for node in (None,('fdiv',('literal',0x3f800000),('literal',0))):
            with self.assertRaises(ValueError):evaluate(node)
