import unittest
import copy
from material_inspect import decode
from scene_material_binding_inspect import bind,vertex_attributes,draw_state,selected_source_color,sample_linear_repeat,effective_globals


class SourceMaterialTests(unittest.TestCase):
    def test_global_provenance_negatives(self):
        s=dict(frame_id=1,game_id='fixture',git_sha='fixture')
        m=dict(s,scene_sha='fixture',shader_globals=dict(provenance='native-pixel-constant-upload',
               cpu_snapshot_source_bytes_unchanged=True,fog_enabled=True,
               fog_color_vertex=[0,0,0],fog_color_ram=[0,0,0],clamp_min=[0]*4,clamp_max=[1]*4))
        self.assertFalse(effective_globals(s,m,dict(tcw=0))['bump_mapping'])
        self.assertTrue(effective_globals(s,m,dict(tcw=4<<27))['bump_mapping'])
        for field,value in [('provenance','unknown'),('cpu_snapshot_source_bytes_unchanged',False),
                            ('fog_enabled',1),('fog_color_vertex',[float('nan'),0,0]),('clamp_max',[1])]:
            bad=copy.deepcopy(m);bad['shader_globals'][field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):effective_globals(s,bad,dict(tcw=0))
        for field in ('frame_id','game_id','git_sha','scene_sha'):
            bad=copy.deepcopy(m);bad[field]='wrong'
            with self.subTest(field=field),self.assertRaises(ValueError):effective_globals(s,bad,dict(tcw=0))
    def test_explicit_mip_repeat_sampling(self):
        level=dict(width=2,height=1,rgba=bytes([255,0,0,255,0,0,255,255]))
        self.assertEqual(sample_linear_repeat(level,[.25,.5]),[1,0,0,1])
        self.assertEqual(sample_linear_repeat(level,[-.75,2.5]),[1,0,0,1])
        self.assertEqual(sample_linear_repeat(level,[0,.5]),[.5,0,.5,1])
        self.assertEqual(sample_linear_repeat(level,[1,.5]),[.5,0,.5,1])
        with self.assertRaises(ValueError):sample_linear_repeat(level,[float('nan'),0])
    def test_selected_equation_and_missing_fog(self):
        s=draw_state(dict(tsp=(3<<6)|(1<<22)|(1<<19),pcw=4))
        args=([.5]*4,[.5]*4,[.25,.25,.25,.5],s)
        self.assertEqual(selected_source_color(*args,dict(fog_enabled=True,fog_color_vertex=[1,0,0],bump_mapping=False)),[.75,.25,.25,1])
        self.assertEqual(selected_source_color(*args,dict(fog_enabled=False,fog_color_vertex=[1,0,0],bump_mapping=False)),[.5,.5,.5,1])
        with self.assertRaises(ValueError):selected_source_color(*args,None)
        s['color_clamp']=1
        g=dict(fog_enabled=False,fog_color_vertex=[1,0,0],bump_mapping=False,
               clamp_min=[0]*4,clamp_max=[1]*4)
        self.assertEqual(selected_source_color(*args,g),[.5,.5,.5,1])
        g['clamp_max']=[.5]*4
        with self.assertRaises(ValueError):selected_source_color(*args,g)
    def test_state_bit_positions(self):
        for name,bit in [('clamp_v',15),('clamp_u',16),('flip_v',17),('flip_u',18),
                         ('ignore_texture_alpha',19),('use_alpha',20),('color_clamp',21)]:
            s=draw_state(dict(tsp=1<<bit,pcw=0))
            self.assertEqual(s[name],1)
            self.assertEqual(s['fog_control'],0)
        s=draw_state(dict(tsp=(3<<22)|(2<<13)|(3<<6),pcw=14))
        self.assertEqual((s['fog_control'],s['filter_mode'],s['shading']),(3,2,3))
        self.assertEqual((s['offset'],s['texture'],s['gouraud']),(1,1,1))
    def test_native_vertex_channels_and_uv(self):
        s=dict(vertex_layout=['x_bits','y_bits','z_bits','u_bits','v_bits','color_rgba_bytes','offset_rgba_bytes',
                              'u1_bits','v1_bits','color1_rgba_bytes','offset1_rgba_bytes'],
               vertices=[[0,0,0,0x3f800000,0xc0000000,[3,2,1,4],[7,6,5,8],0,0,[0]*4,[0]*4]])
        r=vertex_attributes(s,[0])[0]
        self.assertEqual(r['uv'],[1,-2])
        self.assertEqual(r['color_rgba'],[1,2,3,4])
        self.assertEqual(r['offset_rgba'],[5,6,7,8])
        self.assertIsNone(r['normal'])
        s['vertices'][0][3]=0x7fc00000
        with self.assertRaises(ValueError):vertex_attributes(s,[0])
    def test_channel_goldens(self):
        self.assertEqual(decode(85,bytes([0,248]),1,1).tobytes(),bytes([255,0,0,255]))
        self.assertEqual(decode(86,bytes([0,252]),1,1).tobytes(),bytes([255,0,0,255]))
        self.assertEqual(decode(87,bytes([3,2,1,4]),1,1).tobytes(),bytes([1,2,3,4]))

    def test_identity_and_duplicate_rejection(self):
        s=dict(schema='flycast-pvr-scene-v2',frame_id=1,game_id='fixture',git_sha='fixture',
               draws=[dict(list=0,ordinal=1,texture=None,texture1=None,pcw=123,tsp=0)])
        m=dict(schema='flycast-source-materials-v1',frame_id=1,game_id='fixture',git_sha='fixture',
               scene_sha='fixture',assets=[],bindings=[dict(list=0,ordinal=1,slot=i,asset=None) for i in (0,1)])
        r=bind(s,m,[1]);self.assertEqual(r['draws'][0]['original_draw']['pcw'],123)
        self.assertFalse(r['physical_albedo_proven'])
        m['bindings'].append(dict(m['bindings'][0]))
        with self.assertRaises(ValueError):bind(s,m,[1])
