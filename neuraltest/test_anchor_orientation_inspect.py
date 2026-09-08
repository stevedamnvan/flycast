import unittest
import numpy as np
from anchor_orientation_inspect import convert


class OrientationTests(unittest.TestCase):
    def test_reflection_winding_and_projection(self):
        mesh=dict(coordinate_space='selected-source-anchor',anchored_camera=dict(source_to_normalized_view=np.eye(4).tolist()),
                  vertices=[dict(position=p) for p in ([0,0,2],[1,0,2],[0,1,2])],triangles=[dict(vertices=[0,1,2])],strict_reprojection_pass=False)
        r=convert(mesh,[320,240])
        self.assertEqual(r['vertices'][2]['position'],[0,-1,2])
        self.assertEqual(r['triangles'][0]['vertices'],[0,2,1])
        self.assertEqual(r['maximum_conversion_pixel_error'],0)
        self.assertAlmostEqual(r['harness_camera']['aspect'],1)
        self.assertFalse(r['strict_reprojection_pass'])
        mesh['anchored_camera']['source_to_normalized_view'][1][1]=-1
        with self.assertRaisesRegex(ValueError,'reflected'):convert(mesh,[320,240])
