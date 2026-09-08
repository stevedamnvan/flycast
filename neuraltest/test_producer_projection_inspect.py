import unittest
from producer_projection_inspect import project


class ProducerProjectionTests(unittest.TestCase):
    def test_z_divide_preserves_zero_w(self):
        matrix=[0]*16
        for i in (0,5,10): matrix[i]=0x3f800000
        point=[0x3f800000,0x3f800000,0x40000000,0x3f800000]
        constants={20:0x3f800000,24:0,25:0}
        self.assertEqual(project(point,matrix,0x3f800000,constants),[0x3f000000]*3)
        with self.assertRaisesRegex(ValueError,'projection denominator'):
            project(point,matrix,0x3f800000,constants,3)
        self.assertEqual(matrix[15],0)
