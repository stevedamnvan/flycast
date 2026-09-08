import copy
import unittest
from calibrated_scene_inspect import build_frame


def fixture():
    words=[0x3f800000,0x40000000,0x3f800000]
    scene=dict(schema='flycast-pvr-scene-v2',frame_id=2,game_id='synthetic',
               vertices=[words[:] for i in range(6)],indices=[0,1,2,0xffffffff,2,3,4,5],
               draws=[dict(list=0,ordinal=7,first=0,count=8,texture=dict(upload_generation=4))])
    samples=[dict(producer=dict(ordinal=1),vertex=i,draw=7,final_words=words[:],preprojection_record_words=words[:]) for i in range(5)]
    return scene,samples,dict(normalized_calibration=[1.,1.],screen_center=[0,0])


class SceneTests(unittest.TestCase):
    def test_full_draw_and_late_vertex_controls(self):
        scene,samples,contract=fixture()
        scene['vertices']=[scene['vertices'][0][:] for _ in range(146)]
        scene['indices']=list(range(4,146))
        scene['draws']=[dict(list=0,ordinal=1,first=0,count=142)]
        samples=[dict(samples[0],vertex=v,draw=1) for v in range(4,146)]
        result=build_frame(scene,samples,contract,True)
        self.assertEqual(result['supported_triangles'],140)
        self.assertEqual(result['omitted_opaque_triangles'],0)
        self.assertEqual(result['triangles'][1]['vertices'],[6,5,7])
        self.assertFalse(result['complete_scene'])
        for bad in (samples[:-1],samples[:-1]+[dict(samples[-1],draw=2)],
                    samples[:-1]+[dict(samples[-1],preprojection_record_words=[0,0,0])]):
            with self.subTest(),self.assertRaises(ValueError):build_frame(scene,bad,contract,True)
    def test_partial_primitives_and_restart(self):
        result=build_frame(*fixture())
        self.assertEqual([t['vertices'] for t in result['triangles']],[[0,1,2],[2,3,4]])
        self.assertEqual(result['total_opaque_index_triangles'],3)
        self.assertEqual(result['omitted_opaque_triangles'],1)
        self.assertFalse(result['complete_scene'])
        self.assertFalse(result['renderable_by_remix_adapter'])
        self.assertIsNone(result['vertices'][0]['normal'])
        self.assertEqual(result['triangles'][0]['original_draw']['texture']['upload_generation'],4)
    def test_identity_rejections(self):
        for field in ('frame','vertex','words','range'):
            scene,samples,contract=fixture()
            if field=='frame':scene['frame_id']=3
            elif field=='vertex':samples[0]['vertex']=90
            elif field=='words':samples[0]['final_words'][0]^=1
            else:scene['draws'][0]['count']=90
            with self.subTest(field=field),self.assertRaises(ValueError):build_frame(scene,samples,contract)
    def test_unverified_draw_not_promoted(self):
        scene,samples,contract=fixture()
        for s in samples:s['draw']=8
        with self.assertRaises(ValueError):build_frame(scene,samples,contract)
    def test_wrong_projection_and_depth(self):
        scene,samples,contract=fixture();contract['screen_center']=[1,0]
        with self.assertRaises(ValueError):build_frame(scene,samples,contract)
        scene,samples,contract=fixture();samples[0]['preprojection_record_words'][2]=0
        with self.assertRaises(ValueError):build_frame(scene,samples,contract)


if __name__=='__main__':unittest.main()
