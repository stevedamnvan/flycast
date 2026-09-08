import unittest
import copy
from background_geometry_inspect import construct,bits,f,inspect


class BackgroundGeometryTests(unittest.TestCase):
    def test_capture_binding_and_mutations(self):
        lines=[];scenes={};raw=[bits(0.)]*9
        result=construct(raw,bits(1.),0,0)
        for n in (1781,1782,1783):
            lines.append(f'FC067_BGP_FRAME_INPUT ordinal={n} generation={n} context=1234 texture=0 hscale=0 depth=3f800000 xyz='+','.join(f'{w:08x}' for w in raw))
            lines.append(f'FC067_BGP_FRAME_BIND ordinal={n} generation={n} context=1234')
            for i,xyz in enumerate(result):lines.append(f'FC067_BGP_FRAME_OUTPUT ordinal={n} vertex={i} xyz='+','.join(f'{w:08x}' for w in xyz))
            scenes[n]=dict(frame_id=n+1,game_id='T1401N',vertices=copy.deepcopy(result))
        text='\n'.join(lines)
        self.assertTrue(inspect(text,scenes)['geometry_construction_proven'])
        for bad,reason in (
            (text.replace('depth=3f800000','depth=40000000',1),'background arithmetic'),
            (text.replace('hscale=0','hscale=1',1),'background arithmetic'),
            (text.replace('generation=1781','generation=1782',1),'background queue binding'),
            (text+'\n'+lines[0],'duplicate background input'),
            ('\n'.join(lines[1:]),'complete background records')):
            with self.subTest(reason=reason),self.assertRaisesRegex(ValueError,reason):inspect(bad,scenes)
        changed=copy.deepcopy(scenes);changed[1781]['vertices'][3][0]^=1
        with self.assertRaisesRegex(ValueError,'background scene binding'):inspect(text,changed)

    def test_untextured_scale_and_depth_floor(self):
        r=construct([bits(0.)]*9,bits(0.),0,1)
        self.assertEqual([f(w) for w in r[0]][:2],[-512.,0.])
        self.assertEqual([f(w) for w in r[3]][:2],[1792.,480.])
        self.assertEqual(r[0][2],bits(1e-11))

    def test_textured_corner_and_scale_controls(self):
        raw=[bits(v) for v in (10.,20.,1.,30.,20.,1.,30.,40.,1.)]
        r=construct(raw,bits(1.),1,0)
        self.assertEqual([f(w) for w in r[2]][:2],[-246.,40.])
        self.assertEqual([f(w) for w in r[3]][:2],[286.,40.])
        self.assertNotEqual(r,construct(raw,bits(1.),1,1))
        self.assertNotEqual(r,construct(raw,bits(2.),1,0))

    def test_invalid_inputs(self):
        for raw in ([0]*8,[bits(float('nan'))]*9):
            with self.assertRaises(ValueError):construct(raw,bits(1.),0,0)
