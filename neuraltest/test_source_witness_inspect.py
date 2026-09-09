import unittest
import numpy as np
from source_witness_inspect import floats


class WitnessBitTests(unittest.TestCase):
    def test_float_bits_preserve_nonunit_w(self):
        np.testing.assert_array_equal(floats([0x3f800000, 0x40000000]), [1.0, 2.0])

    def test_signed_zero_survives_decode(self):
        self.assertTrue(np.signbit(floats([0x80000000])[0]))


if __name__ == "__main__":
    unittest.main()
