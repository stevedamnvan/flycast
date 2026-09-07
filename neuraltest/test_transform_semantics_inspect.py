"""Independent synthetic projection/factorization controls."""
import struct
import unittest
import numpy as np

from transform_semantics_inspect import analyze, factor


def word(value):
    return struct.unpack('!I', struct.pack('!f', value))[0]


class SemanticsTests(unittest.TestCase):
    def fixture(self):
        rows = [[600, 0, 0, 100], [0, 600, 0, 50], [0, 0, 1, 10], [0, 0, 0, 1]]
        columns = [[word(v) for v in c] for c in zip(*rows)]
        return columns, list(map(word, [0.25, 0.5, 1, 1])), list(map(word, [250, 350, 11, 1]))

    def test_analytic_projection_and_ambiguity(self):
        result = analyze(*self.fixture())
        np.testing.assert_allclose(result['algebraic_projection'], [320+250/11, 240+350/11])
        self.assertEqual(result['axis_scales'], [600, 600, 1])
        self.assertFalse(result['unique_model_view'])
        self.assertLess(result['equivalent_decomposition_error'], 1e-9)

    def test_wrong_output_rejected(self):
        columns, vector, output = self.fixture()
        output[0] ^= 1
        with self.assertRaises(ValueError):
            analyze(columns, vector, output)

    def test_wrong_layout_rejected(self):
        columns, vector, output = self.fixture()
        with self.assertRaises(ValueError):
            analyze(list(map(list, zip(*columns))), vector, output)

    def test_unsupported_factorizations(self):
        matrices = []
        for row, col, value in ((0, 1, .2), (3, 2, 1), (0, 0, 0), (0, 0, float('nan'))):
            matrix = np.eye(4)
            matrix[row, col] = value
            matrices.append(matrix)
        for matrix in matrices:
            with self.subTest(matrix=matrix), self.assertRaises(ValueError):
                factor(matrix)


if __name__ == '__main__':
    unittest.main()
