import math
import unittest
from triangle_deformation_inspect import inspect, transform, apply


class TriangleDeformationTests(unittest.TestCase):
    first = [[0, 0, 0], [1, 0, 0], [0, 1, 0]]

    def test_translation(self):
        second = [[p[0]+4, p[1]-3, p[2]+2] for p in self.first]
        result = inspect(self.first, second)
        self.assertEqual(result['max_position_error'], 0)
        self.assertEqual(result['normal_error'], 0)
        self.assertGreater(result['unchanged_position_error'], 5)

    def test_rotation_scale_shear_and_wrong_direction(self):
        second = [[2, 3, 4], [4, 3, 4], [3, 3, 7]]
        result = inspect(self.first, second)
        self.assertLess(result['max_position_error'], 1e-6)
        self.assertLess(result['normal_error'], 1e-6)
        wrong = transform(second, self.first)
        self.assertGreater(math.dist(apply(wrong, self.first[0]), second[0]), 1)

    def test_degenerate_and_nonfinite_rejected(self):
        for bad in ([[0, 0, 0]]*3, [[0, 0, 0], [1, 0, 0], [2, 0, 0]],
                    [[float('nan'), 0, 0], [1, 0, 0], [0, 1, 0]]):
            with self.assertRaises(ValueError):
                transform(self.first, bad)
            with self.assertRaises(ValueError):
                transform(bad, self.first)


if __name__ == '__main__':
    unittest.main()
