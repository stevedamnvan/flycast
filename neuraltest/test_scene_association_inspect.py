"""Synthetic association controls, not extra game/camera observations."""
import copy
import struct
import unittest

from scene_association_inspect import associate, bind_frame


def fixture():
    bits = lambda v: struct.unpack('<I', struct.pack('<f', v))[0]
    vertices = [[bits(x), bits(y), bits(1.)] for x, y in ((10., 20.), (15., 20.), (10., 25.))]
    draw = dict(list=2, ordinal=0, first=0, count=3, range_space='vertices',
                naomi2=False, pcw=0, isp=0, tcw=5, tsp=0, tileclip=0,
                tcw1=0, tsp1=0, texture=dict(upload_generation=1, palette_hash=None, rtt_generation=0), texture1=None)
    material = copy.deepcopy(draw)
    material.update(ordinal=1, first=3, count=0)
    scene = dict(schema='flycast-pvr-scene-v2', frame_id=7, game_id='fixture',
                 viewport_bits=list(map(bits, (2/640,0,0,0,0,-2/480,0,0,0,0,1,0,-1,1,0,1))),
                 framebuffer_size=[640, 480], vertices=vertices, indices=[0, 1, 2],
                 draws=[draw, material], sorted_triangles=[dict(poly_index=1, first=0, count=3)])
    snapshot = dict(epoch='9', context='00100000', frame='7', vertex='0', written='1', rtt='0', vertices='3', indices='3')
    witness = dict(context=0x100000, vertex=0, ta=dict(packet=[0]+vertices[0]))
    return scene, snapshot, witness, 9


class AssociationTests(unittest.TestCase):
    def test_merged_material_is_not_source_identity(self):
        result = associate(*fixture())
        self.assertEqual((result['source_draw'], result['material_state_draw']), (0, 1))
        for actual, expected in zip(result['screen_bounds'], [10., 20., 15., 25.]):
            self.assertAlmostEqual(actual, expected, places=4)
        self.assertEqual(result['visibility'], 'unproven-alpha-depth-occlusion')

    def test_wrong_frame_epoch_context(self):
        for key, value in (('frame', '8'), ('epoch', '10'), ('context', '00100020'), ('written', '0')):
            s, log, w, epoch = fixture()
            log[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                associate(s, log, w, epoch)

    def test_wrong_vertex_or_position(self):
        for mutate in (lambda s: s['vertices'][0].__setitem__(0, 0),
                       lambda s: s['draws'][0].update(first=1)):
            s, log, w, epoch = fixture(); mutate(s)
            with self.assertRaises(ValueError):
                associate(s, log, w, epoch)

    def test_wrong_topology_or_range_space(self):
        for mutate in (lambda s: s['indices'].__setitem__(2, 1),
                       lambda s: s['draws'][0].update(range_space='indices'),
                       lambda s: s['draws'][0].update(count=4)):
            s, log, w, epoch = fixture(); mutate(s)
            with self.assertRaises(ValueError):
                associate(s, log, w, epoch)

    def test_wrong_material_and_generation(self):
        for mutate in (lambda s: s['draws'][1].update(tcw=6),
                       lambda s: s['draws'][1]['texture'].update(upload_generation=2),
                       lambda s: s['sorted_triangles'][0].update(poly_index=99)):
            s, log, w, epoch = fixture(); mutate(s)
            with self.assertRaises(ValueError):
                associate(s, log, w, epoch)
        s, log, w, epoch = fixture()
        s['draws'][0]['isp'] = 2 << 27
        s['draws'][1]['isp'] = 3 << 27
        with self.assertRaisesRegex(ValueError, 'material'):
            associate(s, log, w, epoch)

    def test_ambiguous_source_and_duplicate_gpu_primitive(self):
        for mutate in (lambda s: s['draws'][1].update(first=0, count=3),
                       lambda s: s['sorted_triangles'].append(copy.deepcopy(s['sorted_triangles'][0]))):
            s, log, w, epoch = fixture(); mutate(s)
            with self.assertRaises(ValueError):
                associate(s, log, w, epoch)

    def test_offscreen_is_not_visible(self):
        s, log, w, epoch = fixture()
        for v in s['vertices']:
            v[1] = struct.unpack('<I', struct.pack('<f', -20.))[0]
        w['ta']['packet'][2] = s['vertices'][0][1]
        self.assertTrue(associate(s, log, w, epoch)['outside_viewport_bounds'])

    def test_unsupported_viewport(self):
        s, log, w, epoch = fixture()
        s['viewport_bits'][3] = 1065353216
        with self.assertRaisesRegex(ValueError, 'viewport'):
            associate(s, log, w, epoch)

    def test_matched_frame_and_material_controls(self):
        s, log, w, epoch = fixture()
        row = associate(s, log, w, epoch)
        s['git_sha'] = 'probe'
        captured = copy.deepcopy(s); captured['git_sha'] = 'restored'
        manifest = dict(frame_id=7, game_id='fixture', git_sha='restored')
        materials = dict(manifest, schema='flycast-source-materials-v1', scene_sha='restored', assets=[{}],
                         bindings=[dict(list=2, ordinal=i, slot=0, asset=0, tcw=5, upload_generation=1,
                                        palette_hash=None, rtt_generation=0) for i in (0, 1)])
        self.assertEqual(bind_frame(s, captured, manifest, materials, [row])[0]['asset'], 0)
        for mutation in (lambda c, f, m: c.update(frame_id=8),
                         lambda c, f, m: c['draws'][0].update(count=3.0),
                         lambda c, f, m: f.update(frame_id=8),
                         lambda c, f, m: m.update(git_sha='wrong'),
                         lambda c, f, m: m['bindings'][0].update(upload_generation=2),
                         lambda c, f, m: m['bindings'][1].update(asset=9)):
            c, f, m = copy.deepcopy((captured, manifest, materials)); mutation(c, f, m)
            with self.assertRaises(ValueError):
                bind_frame(s, c, f, m, [row])


if __name__ == '__main__':
    unittest.main()
