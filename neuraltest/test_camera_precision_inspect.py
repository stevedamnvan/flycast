import unittest
from unittest.mock import patch
from camera_precision_inspect import inspect,rebase
import numpy as np


class PrecisionTests(unittest.TestCase):
    def test_fixed_rebase_preserves_relative_positions(self):
        m=self.fixture();m['harness_camera']['position']=[4,5,6]
        b=rebase(m,[2,3,4])
        np.testing.assert_array_equal(np.array(b['vertices'][0]['position'])-b['harness_camera']['position'],
                                      np.array(m['vertices'][0]['position'])-m['harness_camera']['position'])
        self.assertEqual(m['harness_camera']['position'],[4,5,6])
        m['harness_camera']['position']=[5,7,9]
        later=rebase(m,[2,3,4])
        np.testing.assert_array_equal(np.array(later['harness_camera']['position'])-b['harness_camera']['position'],[1,2,3])
        self.assertFalse(b['renderable_by_remix_adapter'])

    def test_bad_fixed_origin(self):
        with self.assertRaises(Exception):rebase(self.fixture(),[float('nan'),0,0])
    def fixture(self):
        return dict(coordinate_space='reflected-selected-source-anchor',
                    harness_camera=dict(position=[0,0,0],right=[1,0,0],up=[0,1,0],forward=[0,0,1],fovY=90,aspect=1),
                    vertices=[dict(position=[-1,-1,2])])

    @patch('camera_precision_inspect.subprocess.check_output',return_value='.25 .75 2\n')
    def test_golden(self,run):
        r=inspect(self.fixture(),'probe')
        self.assertTrue(all(v['maximum_pixels']<1e-10 for v in r['stages'].values()))
        self.assertFalse(r['renderable_by_remix_adapter'])
        self.assertEqual(run.call_args.kwargs['timeout'],30)

    @patch('camera_precision_inspect.subprocess.check_output',return_value='.3 .75 2\n')
    def test_wrong_projection(self,run):
        r=inspect(self.fixture(),'probe')
        self.assertEqual(r['stages']['actual_cpp_float']['over_001'],1)

    @patch('camera_precision_inspect.subprocess.check_output',return_value='nan .75 2\n')
    def test_invalid_output(self,run):
        with self.assertRaises(Exception):
            inspect(self.fixture(),'probe')
