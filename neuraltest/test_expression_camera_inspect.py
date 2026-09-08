import unittest
import struct
from expression_camera_inspect import recover, outside_common_plane


def fixture():
    one=('literal',0x3f800000);reciprocal=('fdiv',one,one)
    return [('fadd',('fmul',one,reciprocal),('literal',0x43a00000)),
            ('fadd',('fmul',one,reciprocal),('literal',0x43700000)),reciprocal]


class ExpressionCameraTests(unittest.TestCase):
    def test_common_plane_is_not_individual_offscreen_test(self):
        self.assertTrue(outside_common_plane([[4000,100],[4100,200],[8900,300]]))
        self.assertFalse(outside_common_plane([[-100,100],[740,100],[320,600]]))
        self.assertFalse(outside_common_plane([[640,100],[740,100],[740,200]]))

    def test_offscreen_rounding_does_not_relax_tolerance(self):
        def literal(value):return ('literal',struct.unpack('<I',struct.pack('<f',value))[0])
        reciprocal=('fdiv',literal(1),literal(.8955900073051453))
        nodes=[('fadd',('fmul',literal(value),reciprocal),literal(center))
               for value,center in ((7700.06103515625,320),(1787.73828125,240))]
        nodes.append(('fmul',reciprocal,literal(.949999988079071)))
        r=recover(nodes,[614.7144309686947,565.5372185498106])
        self.assertEqual(r['observed_screen_position'],[8917.7509765625,2236.15673828125])
        self.assertGreater(r['maximum_reprojection_error_pixels'],.001)
        self.assertAlmostEqual(r['maximum_reprojection_error_pixels'],.0013122620966896648,places=10)

    def test_predivision_depth_not_scaled_depth(self):
        nodes=fixture();nodes[2]=('fmul',nodes[2],('literal',0x3f000000))
        r=recover(nodes,[2.,4.])
        self.assertEqual(r['position'],[.5,-.25,1.])
        self.assertEqual(r['observed_depth_scale'],.5)
        self.assertEqual(r['maximum_reprojection_error_pixels'],0)

    def test_wrong_center(self):
        nodes=fixture();nodes[0]=('fadd',nodes[0][1],('literal',0))
        with self.assertRaisesRegex(ValueError,'screen center'):recover(nodes,[2.,4.])
