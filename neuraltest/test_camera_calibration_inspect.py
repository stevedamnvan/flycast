import unittest
import numpy as np
from camera_calibration_inspect import analyze


class CalibrationTests(unittest.TestCase):
    def test_full_domain_bound_and_last_matrix_control(self):
        matrices=[np.diag([600.,500.,1.,1.]) for _ in range(144)]
        self.assertEqual(analyze(matrices,full_draw=True)['matrix_count'],144)
        with self.assertRaises(ValueError):analyze(matrices+[matrices[0]],full_draw=True)
        matrices[-1][0,0]=1200
        with self.assertRaises(ValueError):analyze(matrices,full_draw=True)
    def test_shared_rigid_and_uniform_scale(self):
        a=np.diag([600.,500.,1.,1.]);a[:3,3]=[3,4,5]
        r=np.array([[0.,-1,0,0],[1,0,0,0],[0,0,1,0],[0,0,0,1]])
        b=a@r;b[:3,:]*=2
        result=analyze([a,b])
        self.assertEqual(result['normalized_calibration'],[600.,500.])
        self.assertFalse(result['physical_scale_known'])
        self.assertFalse(result['whole_scene_coverage'])
    def test_false_common_calibration(self):
        a=np.diag([600.,500.,1.,1.]);b=a.copy();b[0,0]*=2
        with self.assertRaises(ValueError):analyze([a,b])
        with self.assertRaises(ValueError):analyze([a],[1200.,500.])
    def test_bad_shape_shear_and_bounds(self):
        with self.assertRaises(ValueError):analyze([])
        with self.assertRaises(ValueError):analyze([np.eye(4)]*22)
        a=np.eye(4);a[0,1]=.1
        with self.assertRaises(ValueError):analyze([a])
        with self.assertRaises(ValueError):analyze([np.eye(4)],[float('nan'),1])


if __name__=='__main__':unittest.main()
