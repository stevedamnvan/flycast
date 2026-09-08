import unittest
from draw_domain_inspect import inspect_scene


def fixture():
    return dict(frame_id=1,game_id='synthetic',vertices=[[0,0,0]]*6,
                indices=[0,1,2,3,0xffffffff,2,3,4],draws=[dict(list=0,ordinal=1,range_space='indices',first=0,count=8)])


class DomainTests(unittest.TestCase):
    def test_restart_and_winding(self):
        r=inspect_scene(fixture())
        self.assertEqual(r['triangles'],[[0,1,2],[2,1,3],[2,3,4]])
        self.assertEqual(r['unique_vertices'],5)
        self.assertFalse(r['original_transform_coverage_proven'])
    def test_invalid_indices_and_domains(self):
        for kind in ('range','index','duplicate','space'):
            s=fixture()
            if kind=='range':s['draws'][0]['count']=4097
            elif kind=='index':s['indices'][0]=100
            elif kind=='duplicate':s['draws'].append(s['draws'][0])
            else:s['draws'][0]['range_space']='vertices'
            with self.subTest(kind=kind),self.assertRaises(ValueError):inspect_scene(s)
    def test_vertex_budget(self):
        s=fixture();s['vertices']=[[0,0,0]]*257;s['indices']=list(range(257));s['draws'][0]['count']=257
        with self.assertRaises(ValueError):inspect_scene(s)
    def test_explicit_large_domain_and_bad_caps(self):
        s=fixture();s['vertices']=[[0,0,0]]*2745;s['indices']=list(range(2745));s['draws'][0]['count']=2745
        self.assertEqual(inspect_scene(s,vertex_limit=4096,index_limit=8192)['triangle_count'],2743)
        for vertices,indices in ((4097,8192),(4096,8193),(True,8192),(0,8192)):
            with self.subTest(),self.assertRaises(ValueError):inspect_scene(s,vertex_limit=vertices,index_limit=indices)
        s['vertices'][2744]=[0,0]
        with self.assertRaisesRegex(ValueError,'position words'):inspect_scene(s,vertex_limit=4096,index_limit=8192)


if __name__=='__main__':unittest.main()
