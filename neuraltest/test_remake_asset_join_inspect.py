import copy
import unittest
from unittest.mock import patch
from expression_mesh_inspect import build
from test_expression_mesh_inspect import fixture
from remake_asset_join_inspect import join


class JoinTests(unittest.TestCase):
    def test_join_preserves_exclusions_and_rejects_wrong_source(self):
        scene,rows,contract=fixture();mesh=build(scene,rows,contract)
        with patch('remake_asset_join_inspect.verify_published',return_value=dict(assets={},bindings=[])):
            result=join(mesh,scene,{},None,None)
            self.assertEqual(result['omissions'],mesh['omissions'])
            self.assertEqual(len(result['draw_packets']),1)
            self.assertFalse(result['renderable_by_remix_adapter'])
            for key in ('frame_id','game_id','git_sha'):
                bad=copy.deepcopy(mesh);bad[key]='wrong'
                with self.assertRaises(ValueError):join(bad,scene,{},None,None)
            bad=copy.deepcopy(mesh);bad['vertices'][0]['original_vertex']=[1,2,3]
            with self.assertRaisesRegex(ValueError,'attributes'):join(bad,scene,{},None,None)
            bad=copy.deepcopy(mesh);bad['triangles'][0]['vertices']=[0,2,1]
            with self.assertRaisesRegex(ValueError,'topology'):join(bad,scene,{},None,None)
            bad['winding_reversed']=True
            self.assertEqual(len(join(bad,scene,{},None,None)['draw_packets']),1)
