import copy
import unittest
from unittest.mock import patch
from types import SimpleNamespace
from remake_scene_artifact_inspect import prepare
from remake_scene_artifact_inspect import diagnostic_clips


class PrepareTests(unittest.TestCase):
    def test_explicit_clips_preserve_unknown_source_and_detect_exclusion(self):
        artifact=dict(schema='flycast-prepared-remake-scene-v1',camera=dict(position=[0,0,0],forward=[0,0,1],
            nearPlane=None,farPlane=None),meshes=[dict(source_draw=1,vertices=[dict(position=[0,0,1]),dict(position=[0,0,103])])])
        before=copy.deepcopy(artifact)
        self.assertFalse(diagnostic_clips(artifact,.1,100)['encloses_submitted_vertices'])
        self.assertTrue(diagnostic_clips(artifact,.1,104)['encloses_submitted_vertices'])
        self.assertEqual(artifact,before)
        for near,far in [(0,100),(2,1),(.1,float('inf')),(True,100)]:
            with self.assertRaises(ValueError):diagnostic_clips(artifact,near,far)
    def fixture(self):
        vertices=[dict(source_vertex=i,position=p,original_vertex=['preserved',i]) for i,p in enumerate(
            ([0,0,1],[1,0,1],[0,1,1]))]
        return dict(source_publication_verified=True,coordinate_space='reflected-selected-source-anchor',
            winding_reversed=True,harness_camera={},fixed_origin=[0,0,0],frame_id=1,game_id='fixture',git_sha='fixture',
            source_assets={},omissions=['unknown clip'],strict_reprojection_pass=False,
            draw_packets=[dict(source_draw=1,vertices=vertices,triangles=[dict(vertices=[0,1,2])],source_bindings=[])])
    def test_preserves_unknowns_and_source_attributes(self):
        source=self.fixture();before=copy.deepcopy(source)
        with patch('remake_scene_artifact_inspect.subprocess.run',return_value=SimpleNamespace(stdout='3 0\n0 0 1\n')):
            result=prepare(source,'fixture')
        self.assertEqual(source,before)
        self.assertIsNone(result['camera']['nearPlane'])
        self.assertIsNone(result['camera']['farPlane'])
        self.assertFalse(result['renderable_by_remix_adapter'])
        self.assertEqual(result['meshes'][0]['vertices'][1]['original_vertex'],['preserved',1])
    def test_omitted_face_and_wrong_domain_reject(self):
        with patch('remake_scene_artifact_inspect.subprocess.run',return_value=SimpleNamespace(stdout='0 1\n')):
            with self.assertRaisesRegex(ValueError,'omissions'):prepare(self.fixture(),'fixture')
        source=self.fixture();source['coordinate_space']='pvr-projected'
        with self.assertRaisesRegex(ValueError,'anchor'):prepare(source,'fixture')
