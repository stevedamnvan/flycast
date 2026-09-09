import copy
import unittest
from compose_remake_inspect import compose


class CompositionTests(unittest.TestCase):
    def test_provenance_and_conflicts(self):
        base=dict(schema='flycast-prepared-remake-scene-v1',frame_id=1,game_id='fixture',
                  git_sha='reference',coordinate_space='reflected-selected-source-anchor',
                  fixed_origin=[0,0,0],material_semantic='source',renderable_by_remix_adapter=False,
                  camera={},meshes=[dict(source_draw=1,vertices=[],indices=[])],source_assets={},
                  omissions=['unknown'],strict_reprojection_pass=False)
        source=copy.deepcopy(base)
        source.update(git_sha='original',coordinate_space='diagnostic-camera-embedded-anchor',
                      meshes=[dict(source_draw=2,vertices=[],indices=[])],
                      embedding_provenance=dict(source_git_sha='original',reference_git_sha='reference',
                          recovered_world_transform=False,source_coordinate_space='calibrated-camera-relative'),
                      capture_equivalence=dict(source_git_sha='original',reference_git_sha='reference',
                          frame_id=1,game_id='fixture',diagnostic_content_equivalence=True,
                          draws=[2],scene_content_sha256='a'*64,asset_sha256={}))
        result=compose(base,[source])
        self.assertEqual(result['coordinate_space'],'mixed-diagnostic-anchor')
        self.assertEqual([g['source_git_sha'] for g in result['source_groups']],['reference','original'])
        self.assertEqual(source['git_sha'],'original')
        self.assertFalse(result['renderable_by_remix_adapter'])
        for key,value in [('source_git_sha','wrong'),('reference_git_sha','wrong'),
                          ('diagnostic_content_equivalence',False),('draws',[3]),('frame_id',2)]:
            bad=copy.deepcopy(source);bad['capture_equivalence'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):compose(base,[bad])
        bad=copy.deepcopy(source);bad['embedding_provenance']['recovered_world_transform']=True
        with self.assertRaises(ValueError):compose(base,[bad])
        with self.assertRaises(ValueError):compose(base,[source,source])


if __name__=='__main__':unittest.main()
