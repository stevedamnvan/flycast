import copy
import unittest
from compact_draw_inspect import inspect
from compact_consumer_inspect import encode
from test_compact_consumer_inspect import record


def fixture():
    rows=[];frames=[]
    for f in range(3):
        for v in range(2745):
            r=record(f*2745+v+1)
            r.update(ordinal=1781+f,cycle=1000+f,generation=10+f,vertex=1420+v,
                     ta_offset=v*32,copy_destination=0x200000000+v*32,decoder_pointer=0x200000000+v*32)
            rows.append(r)
        scene=dict(frame_id=1782+f,game_id='T1401N',git_sha='synthetic',
            vertices=[record()['after'][1:4] for _ in range(4165)],indices=list(range(1420,4165)),
            draws=[dict(list=0,ordinal=277,range_space='indices',first=0,count=2745)])
        manifest=dict(frame_id=1782+f,game_id='T1401N',git_sha='synthetic',
            producer_identity=dict(available=True,clock='sh4-scheduler-cycles',epoch=3,ordinal=1781+f,cycle=2000+f))
        frames.append((scene,manifest))
    return rows,frames


class CompactDrawTests(unittest.TestCase):
    def test_complete_geometry_binding_not_transform_proof(self):
        rows,frames=fixture();result=inspect(encode(rows),frames)
        self.assertEqual([f['vertices'] for f in result['frames']],[2745]*3)
        self.assertEqual([f['source_addresses'] for f in result['frames']],[1]*3)
        self.assertFalse(result['original_transform_proven'])
    def test_last_consumer_and_frame_controls(self):
        rows,frames=fixture()
        for key,value in [('draw',1),('vertex',4163),('cycle',9999),('generation',99),('ta_offset',0)]:
            bad=copy.deepcopy(rows);bad[-1][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):inspect(encode(bad),frames)
        bad=copy.deepcopy(frames);bad[-1][0]['vertices'][-1][0]^=1
        with self.assertRaises(ValueError):inspect(encode(rows),bad)


if __name__=='__main__':unittest.main()
