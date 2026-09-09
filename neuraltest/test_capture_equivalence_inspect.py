import unittest
from capture_equivalence_inspect import scene_equivalence


class EquivalenceTests(unittest.TestCase):
    def test_revision_is_not_content_authority(self):
        a=dict(schema='flycast-pvr-scene-v2',git_sha='old',frame_id=1,game_id='fixture',vertices=[[1,2,3]])
        b=dict(a,git_sha='new')
        result=scene_equivalence(a,b)
        self.assertEqual(result['source_git_sha'],'old')
        self.assertEqual(result['reference_git_sha'],'new')
        for key,value in [('frame_id',2),('vertices',[[1,2,4]]),('game_id','wrong'),('extra',1)]:
            with self.subTest(key=key),self.assertRaises(ValueError):
                scene_equivalence(a,dict(b,**{key:value}))


if __name__=='__main__':unittest.main()
