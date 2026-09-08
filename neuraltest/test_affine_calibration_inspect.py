import unittest
import numpy as np
from affine_calibration_inspect import inspect
from camera_calibration_inspect import analyze


class AffineCalibrationTests(unittest.TestCase):
    def test_rigid_witness_with_shear_preserves_projection(self):
        k=np.diag([600.,550.,1.,1.]);shear=np.eye(4);shear[0,1]=.2
        matrices=[k,k@shear]
        with self.assertRaises(ValueError):analyze(matrices)
        result=inspect(matrices)
        self.assertEqual(result['normalized_calibration'],[600.,550.])
        self.assertEqual(result['general_affine_indices'],[1])
        point=np.array([1.,2.,4.,1.]);q=matrices[1]@point
        reconstructed=np.linalg.solve(k,matrices[1])@point
        np.testing.assert_allclose((k@reconstructed)[:2]/reconstructed[2],q[:2]/q[2])
        with self.assertRaises(ValueError):inspect(matrices,[1200.,550.])

    def test_missing_witness_and_malformed_affine_reject(self):
        shear=np.eye(4);shear[0,1]=.2
        with self.assertRaisesRegex(ValueError,'no rigid'):inspect([shear])
        for bad in (np.zeros((4,4)),np.full((4,4),np.nan)):
            with self.assertRaises(ValueError):inspect([np.eye(4),bad])


if __name__=='__main__':unittest.main()
