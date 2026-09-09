import copy
import unittest
import tempfile
import hashlib
from pathlib import Path
from remake_batch_join_inspect import combine, publish


class JoinTests(unittest.TestCase):
    def test_publication_hash_and_create_only(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory); assets=root/'assets';assets.mkdir()
            (assets/'asset-0.dds').write_bytes(b'fixture')
            a=dict(schema='flycast-prepared-remake-scene-v1',frame_id=1,game_id='fixture',
                   git_sha='fixture',coordinate_space='sample',fixed_origin=[0,0,0],
                   material_semantic='source',renderable_by_remix_adapter=False,
                   camera={},meshes=[dict(source_draw=1,vertices=[],indices=[])],
                   source_assets={'0':dict(file='asset-0.dds',bytes=7,
                       sha256=hashlib.sha256(b'fixture').hexdigest())},
                   omissions=['unknown'],strict_reprojection_pass=False)
            b=copy.deepcopy(a);b['meshes'][0]['source_draw']=2
            publish(a,assets,b,assets,root/'result')
            self.assertEqual((root/'result/assets/asset-0.dds').read_bytes(),b'fixture')
            with self.assertRaises(FileExistsError):
                publish(a,assets,b,assets,root/'result')
            (assets/'asset-0.dds').write_bytes(b'changed')
            with self.assertRaises(ValueError):
                publish(a,assets,b,assets,root/'bad')
            self.assertFalse((root/'bad').exists())

    def test_join_and_mutations(self):
        a = dict(schema='flycast-prepared-remake-scene-v1', frame_id=1,
                 game_id='fixture', git_sha='fixture', coordinate_space='sample',
                 fixed_origin=[0,0,0], material_semantic='source',
                 renderable_by_remix_adapter=False, camera=dict(position=[0.,0.,0.]),
                 meshes=[dict(source_draw=1,vertices=[],indices=[])], source_assets={'0':{}},
                 omissions=['unknown'], strict_reprojection_pass=False)
        b = copy.deepcopy(a)
        b['meshes'][0]['source_draw'] = 2
        b['camera']['position'][0] = 1e-16
        result = combine(a,b)
        self.assertEqual(len(result['meshes']),2)
        self.assertFalse(result['strict_reprojection_pass'])
        self.assertFalse(result['batch_join']['complete_scene'])
        self.assertEqual(len(a['meshes']),1)
        for key,value in [('frame_id',2),('game_id','wrong'),('git_sha','wrong'),
                          ('fixed_origin',[1,0,0]),('meshes',a['meshes']),
                          ('source_assets',{'0':{'conflict':True}}),
                          ('camera',dict(position=[1e-6,0,0])),
                          ('camera',dict(position=[float('nan'),0,0]))]:
            bad=copy.deepcopy(b);bad[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):
                combine(a,bad)


if __name__=='__main__':
    unittest.main()
