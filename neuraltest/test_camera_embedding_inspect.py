import copy
import struct
import unittest
from camera_embedding_inspect import embed


class EmbeddingTests(unittest.TestCase):
    def test_projection_identity_and_mutations(self):
        words=lambda x:struct.unpack('<I',struct.pack('<f',x))[0]
        raw=[words(440),words(240),words(.5)]
        raws=[raw,[words(320),words(120),words(.5)],[words(320),words(240),words(.5)]]
        scene=dict(frame_id=1,game_id='fixture',git_sha='source',vertices=raws)
        mesh=dict(frame_id=1,game_id='fixture',coordinate_space='calibrated-camera-relative',
                  vertices=[dict(source_vertex=i,source_draw=1,original_vertex=raws[i],position=p)
                            for i,p in enumerate([[1,0,2],[0,1,2],[0,0,2]])],
                  triangles=[dict(vertices=[0,1,2])],omissions=['unknown'])
        reference=dict(frame_id=1,game_id='fixture',git_sha='reference',fixed_origin=[0,0,0],
                       renderable_by_remix_adapter=False,camera=dict(accepted_game_camera=False,
                       position=[3,4,5],right=[0,0,-1],up=[0,1,0],forward=[1,0,0],fovY=90,aspect=4/3))
        result=embed(mesh,scene,reference)
        self.assertEqual(result['vertices'][0]['position'],[5,4,4])
        self.assertTrue(result['winding_reversed'])
        self.assertEqual(result['triangles'][0]['vertices'],[0,2,1])
        self.assertEqual(mesh['triangles'][0]['vertices'],[0,1,2])
        self.assertFalse(result['embedding_provenance']['recovered_world_transform'])
        self.assertLess(result['maximum_embedding_pixel_error'],1e-10)
        for field,value in [('right',[0,0,1]),('fovY',60),('position',[float('nan'),0,0])]:
            bad=copy.deepcopy(reference);bad['camera'][field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):embed(mesh,scene,bad)
        bad=copy.deepcopy(scene);bad['vertices'][0][0]=words(441)
        with self.assertRaises(ValueError):embed(mesh,bad,reference)


if __name__=='__main__':unittest.main()
