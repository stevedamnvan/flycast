import copy
import unittest
from compact_draw_inspect import inspect, selection, DEFAULT_DOMAIN, NEXT_DOMAIN, FOUR_DOMAIN
from compact_consumer_inspect import encode
from test_compact_consumer_inspect import record


def fixture(domain=DEFAULT_DOMAIN):
    rows=[];frames=[]
    selected=[(draw,v) for draw,first,end in domain for v in range(first,end)]
    count=len(selected)
    for f in range(3):
        for v,(draw,vertex) in enumerate(selected):
            r=record(f*count+v+1)
            r.update(ordinal=1781+f,cycle=1000+f,generation=10+f,vertex=vertex,draw=draw,
                     ta_offset=v*32,copy_destination=0x200000000+v*32,decoder_pointer=0x200000000+v*32)
            rows.append(r)
        scene=dict(frame_id=1782+f,game_id='T1401N',git_sha='synthetic',
            vertices=[record()['after'][1:4] for _ in range(max(end for _,_,end in domain))],
            indices=[],draws=[])
        for draw,first,end in domain:
            scene['draws'].append(dict(list=0,ordinal=draw,range_space='indices',
                                      first=len(scene['indices']),count=end-first))
            scene['indices'].extend(range(first,end))
        manifest=dict(frame_id=1782+f,game_id='T1401N',git_sha='synthetic',
            producer_identity=dict(available=True,clock='sh4-scheduler-cycles',epoch=3,ordinal=1781+f,cycle=2000+f))
        frames.append((scene,manifest))
    return rows,frames


class CompactDrawTests(unittest.TestCase):
    def test_four_noncontiguous_draws_and_gap_controls(self):
        rows,frames=fixture(FOUR_DOMAIN)
        result=inspect(encode(rows),frames,FOUR_DOMAIN)
        self.assertEqual(len(rows),10593)
        self.assertEqual([f['vertices'] for f in result['frames']],[3531]*3)
        for index,gap in ((597,1420),(1628,5196),(2971,7272)):
            bad=copy.deepcopy(rows);bad[index]['vertex']=gap
            with self.subTest(index=index),self.assertRaisesRegex(ValueError,'consumer vertex/draw'):
                inspect(encode(bad),frames,FOUR_DOMAIN)

    def test_two_draw_batch_binding(self):
        rows,frames=fixture(NEXT_DOMAIN)
        result=inspect(encode(rows),frames,NEXT_DOMAIN)
        self.assertEqual(len(rows),10176)
        self.assertEqual(len(encode(rows)),1628176)
        self.assertEqual([f['vertices'] for f in result['frames']],[3392]*3)
        self.assertFalse(result['original_transform_proven'])
        for index in (1845,1846,3391,10175):
            bad=copy.deepcopy(rows);bad[index]['draw']=277
            with self.subTest(index=index),self.assertRaisesRegex(ValueError,'consumer vertex/draw'):
                inspect(encode(bad),frames,NEXT_DOMAIN)

    def test_batch_bounds_and_scene_ownership(self):
        for domain in ((),((1,0,4097),),((1,0,2049),(2,2049,4097)),
                       ((1,0,3),(2,2,5)),((1,-1,3),)):
            with self.subTest(domain=domain),self.assertRaises(ValueError):selection(domain)
        rows,frames=fixture(NEXT_DOMAIN)
        bad=copy.deepcopy(frames);bad[1][0]['draws'][1]['ordinal']=1503
        with self.assertRaisesRegex(ValueError,'draw identity'):
            inspect(encode(rows),bad,NEXT_DOMAIN)

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
