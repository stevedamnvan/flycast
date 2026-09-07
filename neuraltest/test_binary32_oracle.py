"""Exact finite-normal arithmetic subset, not full floating-point emulation."""
from fractions import Fraction
import unittest

from binary32_oracle import evaluate, fraction, rounded_bits


class Binary32Tests(unittest.TestCase):
    def test_observed_chain(self):
        reciprocal = evaluate('div', 0x3f800000, 0x4163a016, 0, 3)
        self.assertEqual(reciprocal, 0x3d8ff4b2)
        self.assertEqual(evaluate('mul', 0x3f851eb8, reciprocal, 0, 3), 0x3d95b6cd)
        self.assertEqual(evaluate('madd', 0x43a00000, 0x457dcb43, reciprocal, 3, True), 0x44175b90)
        self.assertEqual(evaluate('madd', 0x43700000, 0xc50958f1, reciprocal, 3, True), 0x42ab1024)
        self.assertNotEqual(evaluate('div', 0x3f800000, 0x4163a016, 0, 0), reciprocal)

    def test_ties_even(self):
        for low, expected in ((0x3f800000, 0x3f800000), (0x3f800001, 0x3f800002)):
            self.assertEqual(rounded_bits((fraction(low) + fraction(low+1))/2, 0), expected)

    def test_directed_signs(self):
        midpoint = Fraction(1) + Fraction(1, 1 << 24)
        self.assertEqual([rounded_bits(midpoint, mode) for mode in (1, 2, 3)],
                         [0x3f800000, 0x3f800001, 0x3f800000])
        self.assertEqual([rounded_bits(-midpoint, mode) for mode in (1, 2, 3)],
                         [0xbf800001, 0xbf800000, 0xbf800000])

    def test_fused_is_distinct(self):
        args = ('madd', 0xbf800000, 0x3f800001, 0x3f7ffffe, 0)
        self.assertEqual(evaluate(*args, False), 0)
        self.assertEqual(evaluate(*args, True), 0xa8800000)

    def test_unsupported_rejected(self):
        for word in (1, 0x7f800000, 0x7fc00000, -1, 0x100000000):
            with self.subTest(word=word), self.assertRaises(ValueError):
                fraction(word)
        for value in (Fraction(1, 1 << 127), Fraction(1 << 128)):
            with self.assertRaises(ValueError):
                rounded_bits(value, 0)


if __name__ == '__main__':
    unittest.main()
