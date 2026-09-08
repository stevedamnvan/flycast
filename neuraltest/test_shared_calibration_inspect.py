"""Synthetic shared-calibration/epoch controls, not extra captured vertices."""
import copy
import unittest
import numpy as np

from shared_calibration_inspect import compare_samples


def fixtures():
    calibration = np.diag([600., 550., 1., 1.])
    samples = []
    for i in range(2):
        composite = np.eye(4)
        composite[:3, 3] = [i*2, -i, 10]
        samples.append(dict(epoch=7, first_cycle=1, calibration=calibration.copy(),
                            matrix=calibration @ composite, point=np.array([1., 2., 3., 1.]),
                            witness=dict(context=0x100000, vertex=i+1, packet_offset=64+32*i,
                                         ta=dict(cycles='100', count=16, destination=0x0c100040+32*i,
                                                 offset=64+32*i))))
    return samples


class SharedCalibrationTests(unittest.TestCase):
    def test_shared_calibration(self):
        self.assertLess(compare_samples(*fixtures()), 1e-9)

    def test_wrong_scale(self):
        first, second = fixtures()
        wrong = first['calibration'].copy()
        wrong[0, 0] *= 2
        with self.assertRaisesRegex(ValueError, 'reprojection'):
            compare_samples(first, second, wrong)

    def test_stale_epoch(self):
        first, second = fixtures()
        second['epoch'] += 1
        with self.assertRaisesRegex(ValueError, 'epoch'):
            compare_samples(first, second)

    def test_wrong_batch_or_context(self):
        for field, value in (('cycles', '101'), ('count', 15), ('destination', 0x0c200060)):
            first, second = fixtures()
            second['witness']['ta'][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                compare_samples(first, second)
        first, second = fixtures()
        second['witness']['context'] += 32
        with self.assertRaises(ValueError):
            compare_samples(first, second)

    def test_duplicate_instance(self):
        first, _ = fixtures()
        with self.assertRaises(ValueError):
            compare_samples(first, copy.deepcopy(first))


if __name__ == '__main__':
    unittest.main()
