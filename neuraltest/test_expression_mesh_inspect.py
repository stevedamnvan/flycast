import unittest
from expression_mesh_inspect import build


def fixture():
    scene=dict(schema='flycast-pvr-scene-v2',frame_id=1782,game_id='T1401N',git_sha='fixture',
               vertices=[[0,0,0]]*3,indices=[0,1,2],
               draws=[dict(list=0,ordinal=1,first=0,count=3,range_space='indices')])
    rows=[dict(vertex=i,draw=1,record=[1,4*i],error=0.,
               diagnostic=dict(position=[float(i),0.,1.],maximum_reprojection_error_pixels=0.)) for i in range(3)]
    return scene,rows,dict(doubled_calibration_rejected=True)


class ExpressionMeshTests(unittest.TestCase):
    def test_preserves_failing_geometry_without_acceptance(self):
        scene,rows,contract=fixture()
        rows[2]['error']=rows[2]['diagnostic']['maximum_reprojection_error_pixels']=.001
        mesh=build(scene,rows,contract)
        self.assertEqual(len(mesh['vertices']),3)
        self.assertEqual(len(mesh['triangles']),1)
        self.assertEqual(mesh['strict_reprojection_failures'],[2])
        self.assertFalse(mesh['strict_reprojection_pass'])
        self.assertFalse(mesh['renderable_by_remix_adapter'])

    def test_duplicate_and_missing_witness_reject(self):
        scene,rows,contract=fixture();rows[2]['vertex']=1
        with self.assertRaisesRegex(ValueError,'vertex identity'):build(scene,rows,contract)
        scene,rows,contract=fixture()
        with self.assertRaisesRegex(ValueError,'matrix witnesses'):build(scene,rows,{})
