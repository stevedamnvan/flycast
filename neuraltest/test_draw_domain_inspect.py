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


if __name__=='__main__':unittest.main()
