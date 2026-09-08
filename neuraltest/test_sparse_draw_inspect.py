import copy
import unittest
from compact_draw_inspect import selection,inspect
from compact_consumer_inspect import encode
from test_compact_consumer_inspect import record


class SparseDrawTests(unittest.TestCase):
    def test_explicit_background_partition(self):
        from remaining_draw_inspect import packet_domain
        full=((0,(0,1,2,3)),)+tuple((i+1,tuple(range(4+i*57,4+(i+1)*57))) for i in range(15))+((16,tuple(range(859,924))),)
        self.assertEqual(len(selection(packet_domain(full))),920)
        with self.assertRaisesRegex(ValueError,'explicit background domain'):
            packet_domain(((0,(0,1,2,4)),)+full[1:])
        with self.assertRaisesRegex(ValueError,'remaining packet domain'):
            packet_domain(full[:-1])

    def test_sparse_geometry_and_gap_controls(self):
        domain=((2,(1,3,5)),(9,(8,12,20)))
        rows=[];frames=[]
        selected=sorted(selection(domain).items())
        for f in range(3):
            for i,(vertex,draw) in enumerate(selected):
                r=record(f*6+i+1);r.update(vertex=vertex,draw=draw,ordinal=1781+f,generation=10+f,
                    cycle=1000,ta_offset=i*32,copy_destination=0x200000000+i*32,decoder_pointer=0x200000000+i*32)
                rows.append(r)
            scene=dict(frame_id=1782+f,git_sha='synthetic',game_id='T1401N',
                vertices=[record()['after'][1:4] for _ in range(21)],indices=[1,3,5,8,12,20],
                draws=[dict(list=0,ordinal=d,range_space='indices',first=i*3,count=3) for i,d in enumerate((2,9))])
            manifest=dict(frame_id=1782+f,git_sha='synthetic',game_id='T1401N',
                producer_identity=dict(available=True,clock='sh4-scheduler-cycles',epoch=3,ordinal=1781+f,cycle=2000))
            frames.append((scene,manifest))
        self.assertTrue(inspect(encode(rows),frames,domain)['consumer_scene_binding'])
        bad=copy.deepcopy(rows);bad[1]['vertex']=2
        with self.assertRaisesRegex(ValueError,'consumer vertex/draw'):inspect(encode(bad),frames,domain)
        bad=copy.deepcopy(frames);bad[1][0]['indices'][1]=2
        with self.assertRaisesRegex(ValueError,'selected vertex domain'):inspect(encode(rows),bad,domain)

    def test_bounds_duplicates_and_order(self):
        self.assertEqual(len(selection(tuple((i,(i,)) for i in range(34)))),34)
        for d in (((1,(1,1)),),((1,(2,1)),),((1,(1,)),(2,(1,))),
                  ((1,(1,)),(1,(2,))),tuple((i,(i,)) for i in range(35)),
                  ((1,tuple(range(4096))),(2,(4096,)))):
            with self.subTest(domain=d[:2]),self.assertRaises(ValueError):selection(d)


if __name__=='__main__':unittest.main()
