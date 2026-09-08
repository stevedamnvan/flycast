"""Independent synthetic projection/factorization controls."""
import struct
import unittest
import numpy as np

from transform_semantics_inspect import analyze, analyze_opaque, factor


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


class OpaqueSemanticsTests(unittest.TestCase):
    def fixture(self, w=1):
        rows = [[600, 0, 0, 100], [0, 500, 0, 50], [0, 0, 1, 8], [0, 0, 0, 1]]
        columns = [[word(v) for v in c] for c in zip(*rows)]
        vector = list(map(word, [.25, .5, 0, w]))
        if w == 1:
            output = list(map(word, [250, 300, 8, 1]))
            decoded = list(map(word, [351.25, 277.5, .125]))
        else:
            self.assertEqual(w, 2)
            output = list(map(word, [350, 350, 16, 2]))
            decoded = list(map(word, [341.875, 261.875, .0625]))
        return columns, vector, output, list(map(word, [320, 240])), decoded

    def test_independent_opaque_goldens(self):
        result = analyze_opaque(*self.fixture())
        self.assertEqual(result['normalized_projection_scales'], [600, 500])
        self.assertEqual(result['camera_relative_point'], [250/600, .6, 8])
        self.assertEqual(result['reciprocal_depth_scale_word'], '3f800000')
        self.assertTrue(result['wrong_translucent_depth_scale_rejected'])
        self.assertEqual((result['observed_points'], result['synthetic_probe_points']), (1, 3))
        for flag in ('shared_game_camera', 'usable_camera_contract', 'production_enabled'):
            self.assertIs(result[flag], False)

    def test_nonunit_w_is_preserved(self):
        result = analyze_opaque(*self.fixture(2))
        self.assertEqual(result['actual_w_word'], '40000000')
        self.assertTrue(result['nonunit_w_control_rejected'])
        args = list(self.fixture(2))
        args[1][3] = word(1)
        with self.assertRaises(ValueError):
            analyze_opaque(*args)

    def test_wrong_depth_or_viewport_or_pixel(self):
        for index in range(3):
            args = list(self.fixture())
            args[4][index] ^= 1
            with self.subTest(index=index), self.assertRaises(ValueError):
                analyze_opaque(*args)
        args = list(self.fixture())
        args[3][0] = word(321)
        with self.assertRaisesRegex(ValueError, 'viewport center'):
            analyze_opaque(*args)

    def test_split_arithmetic_is_not_replaced_by_fused(self):
        # Independent boundary: the rounded negative product is -64, while
        # adding it before rounding lies just below 256. Toward-zero differs.
        x = -100.00000762939453
        rows = [[600, 0, 0, x], [0, 500, 0, 50], [0, 0, 1, 1.5625], [0, 0, 0, 1]]
        args = ([[word(v) for v in c] for c in zip(*rows)],
                list(map(word, [0, 0, 0, 1])), list(map(word, [x, 50, 1.5625, 1])),
                list(map(word, [320, 240])), [word(256), word(272)-1, 0x3f23d70a])
        result = analyze_opaque(*args)
        self.assertEqual(result['decoded_xyz_words'][0], '43800000')
        args[4][0] = word(256)-1
        with self.assertRaisesRegex(ValueError, 'split projection'):
            analyze_opaque(*args)


if __name__ == '__main__':
    unittest.main()
