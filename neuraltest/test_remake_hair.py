"""Tests for remake_packet.py and remake_hair.py (run from neuraltest/: python -m unittest test_remake_hair)."""
import unittest

import numpy as np

import remake_hair as rh
import remake_packet as rp


def small_texture(size=8, colour=(200, 150, 100, 255)):
    rgba = np.zeros((size, size, 4), np.uint8)
    rgba[:] = colour
    return rp.dds_rgba(size, size, rh.mip_chain(rgba, 96))


def grid_packet(offset=(0, 0, 0), rotate=0.0, shuffle=False):
    """Packet with one 'hair' mesh: a bumpy 4x4 grid on a known atlas and UV box."""
    dds = small_texture()
    c, s = np.cos(rotate), np.sin(rotate)
    r = np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])
    tris = []
    for i in range(4):
        for j in range(4):
            q = [(i, j), (i + 1, j), (i, j + 1), (i + 1, j), (i + 1, j + 1), (i, j + 1)]
            tris += [q[:3], q[3:]]
    if shuffle:
        tris = [tris[k] for k in np.random.default_rng(3).permutation(len(tris))]
    v = np.zeros(len(tris) * 3, rp.VERTEX)
    k = 0
    for t in tris:
        for (i, j) in t:
            p = np.array([i * 0.1, j * 0.1, 0.02 * np.sin(i + 2 * j)])
            v[k]['pos'] = r @ p + offset
            v[k]['nrm'] = r @ [0, 0, 1]
            v[k]['uv'] = [0.2 + 0.1 * i, 0.2 + 0.1 * j]
            v[k]['col'] = 255
            k += 1
    mesh = rp.Mesh(id=5, tsp=0x947024ed, alpha_ref=256, alpha_blend=1, known=1, texture=(9, 1, 0, 0), mode=0,
                   dds=dds, vertices=v, indices=np.arange(len(v), dtype='<u4'))
    pose = ((0, 0, -2), (1, 0, 0), (0, 1, 0), (0, 0, 1), (0, 0, 0))
    return rp.Packet(4, 7, (1, 2, 3), 'T1401N', 'sha', rp.ANCHORED, (45.0, 1.25, 0.1, 100.0), pose, [], [mesh])


def style_for(p):
    return dict(size=10, regions=[(rh.sha(p.meshes[0].dds)[:16], (0, 0, 10, 10))])


class PacketTests(unittest.TestCase):
    def test_roundtrip_bytes(self):
        p = grid_packet()
        b = rp.write(p)
        self.assertEqual(rp.write(rp.read(b)), b)
        self.assertEqual(rp.read(b).version, 4)

    def test_dds_contract_layout(self):
        d = small_texture(16)
        self.assertEqual(d[:4], b'DDS ')
        levels = rp.dds_levels(d)
        self.assertEqual([l.shape[:2] for l in levels], [(16, 16), (8, 8), (4, 4), (2, 2), (1, 1)])
        self.assertEqual(len(d), 148 + 4 * (256 + 64 + 16 + 4 + 1))

    def test_mip_chain_keeps_alpha_coverage(self):
        rgba = np.zeros((64, 64, 4), np.uint8)
        rgba[:, ::4, 3] = 255  # thin strands, 25% coverage
        levels = rh.mip_chain(rgba, 96)
        self.assertEqual((levels[0][..., 3] >= 96).mean(), 0.25)
        # Plain box mips would drop to alpha 64 (below the threshold) by level 2 and
        # the strands would vanish; rescaled mips never lose coverage.
        for level in levels[1:]:
            self.assertGreaterEqual((level[..., 3] >= 96).mean(), 0.25)


class HairTests(unittest.TestCase):
    def setUp(self):
        self.ref = grid_packet()
        self.style = style_for(self.ref)
        self.tris = rh.hair_triangles(self.ref, self.style)

    def test_finds_all_triangles(self):
        self.assertEqual(len(self.tris), 32)

    def test_correspondence_survives_shuffle_and_motion(self):
        cur = rh.hair_triangles(grid_packet(offset=(1, 2, 3), rotate=0.4, shuffle=True), self.style)
        match = rh.correspond(self.tris, cur)
        for i, j in enumerate(match):
            self.assertEqual(self.tris[i]['key'], cur[j]['key'])

    def binding(self, model, k=4):
        tris = np.array([t['pos'] for t in self.tris])
        return rh.make_binding(model['pos'], model['nrm'], tris, k=k, far=1.0), tris

    def test_closest_point_distance(self):
        tris = np.array([[[0, 0, 0], [1, 0, 0], [0, 1, 0]]], float)
        idx, bary, dist = rh.closest_on_triangles(np.array([[0.2, 0.2, 0.5], [2, 0, 0]]), tris)
        self.assertAlmostEqual(dist[0], 0.5)
        self.assertAlmostEqual(dist[1], 1.0)
        np.testing.assert_allclose(bary[1], [0, 1, 0], atol=1e-9)

    def test_deform_at_reference_is_identity(self):
        rng = np.random.default_rng(1)
        model = dict(pos=rng.uniform([0, 0, -0.05], [0.4, 0.4, 0.08], (200, 3)), nrm=np.tile([0, 0, 1.0], (200, 1)))
        for k in (1, 4):
            b, tris = self.binding(model, k)
            pos, nrm = rh.deform(b, tris, (np.eye(3), np.zeros(3)))
            np.testing.assert_allclose(pos, model['pos'], atol=1e-9)

    def test_blend_weights_normalised_nearest_first(self):
        rng = np.random.default_rng(5)
        model = dict(pos=rng.uniform([0, 0, 0], [0.4, 0.4, 0.05], (50, 3)), nrm=np.tile([0, 0, 1.0], (50, 1)))
        b, _ = self.binding(model)
        np.testing.assert_allclose(b['weight'].sum(1), 1)
        self.assertTrue((np.diff(b['weight'], axis=1) <= 1e-12).all())

    def test_deform_follows_rigid_motion(self):
        rng = np.random.default_rng(2)
        model = dict(pos=rng.uniform([0, 0, -0.05], [0.4, 0.4, 0.08], (200, 3)), nrm=np.tile([0, 0, 1.0], (200, 1)))
        b, _ = self.binding(model)
        cur_packet = grid_packet(offset=(1, 2, 3), rotate=0.4, shuffle=True)
        cur = rh.hair_triangles(cur_packet, self.style)
        match = rh.correspond(self.tris, cur)
        cur_tris = np.array([cur[j]['pos'] for j in match])
        pos, nrm = rh.deform(b, cur_tris, (np.eye(3), np.zeros(3)))
        c, s = np.cos(0.4), np.sin(0.4)
        r = np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])
        np.testing.assert_allclose(pos, model['pos'] @ r.T + [1, 2, 3], atol=1e-5)  # float32 packet
        np.testing.assert_allclose(nrm, np.tile(r @ [0, 0, 1.0], (200, 1)), atol=1e-5)

    def test_edited_packet_hides_and_adds(self):
        rng = np.random.default_rng(4)
        model = dict(pos=rng.uniform([0, 0, 0.01], [0.4, 0.4, 0.03], (30, 3)), nrm=np.tile([0, 0, 1.0], (30, 1)),
                     uv=rng.random((30, 2)), tri=np.arange(30).reshape(10, 3))
        b, tris = self.binding(model)
        b.update(ref_keys=np.array([repr(t['key']) for t in self.tris]), ref_pos=tris, uv=model['uv'], tri=model['tri'])
        p = rh.edited_packet(grid_packet(offset=(0.5, 0, 0)), b, small_texture(), 96, style=self.style)
        self.assertEqual(len(p.meshes), 1)  # the all-hair mesh is dropped, the new hair added
        new = p.meshes[0]
        self.assertEqual((new.alpha_ref, new.alpha_blend, len(new.indices)), (96, 0, 30))
        np.testing.assert_allclose(new.vertices['pos'], model['pos'] + [0.5, 0, 0], atol=1e-5)
        self.assertEqual(rp.write(rp.read(rp.write(p))), rp.write(p))


if __name__ == '__main__':
    unittest.main()
