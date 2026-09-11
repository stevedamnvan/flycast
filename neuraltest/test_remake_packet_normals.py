#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Synthetic packet fixtures for the offline normal rewrite (D-225 weld rule)."""
import math
import struct
import tempfile
import unittest
from pathlib import Path

import remake_packet_normals as normals
from test_remake_material_manifest import dds

V = normals.VERTEX


def packet(vertices, version=4):
    """One mesh with the given expanded triangle list (tuples of 9 values)."""
    out = bytearray(struct.pack('<II', 0x56524346, version) + struct.pack('<QQQQ', 2601, 1, 2, 3))
    out += struct.pack('<I', 6) + b'T1401N' + struct.pack('<I', 0) + struct.pack('<I', 0) + b'\0' * 76 + struct.pack('<I', 0)
    out += struct.pack('<I', 1) + struct.pack('<Q', 1) + struct.pack('<IIII', 0, 256, 0, 1) + struct.pack('<QQQQ', 1, 1, 0, 0)
    if version >= 5:
        out += struct.pack('<I', 0)
    data = dds(bytes(64))
    out += struct.pack('<I', len(data)) + data + struct.pack('<I', len(vertices))
    for v in vertices:
        out += V.pack(*v)
    out += struct.pack('<I', len(vertices)) + b''.join(struct.pack('<I', i) for i in range(len(vertices)))
    return bytes(out)


def tri(p0, p1, p2, n, uv=(0.0, 0.0), color=0xffffffff):
    return [p + n + uv + (color,) for p in (p0, p1, p2)]


class PacketNormalsTest(unittest.TestCase):
    def setUp(self):
        self.dir = Path(tempfile.mkdtemp())

    def run_rewrite(self, vertices, **kw):
        src = self.dir / 'in.bin'; dst = self.dir / 'out.bin'
        src.write_bytes(packet(vertices)); dst.unlink(missing_ok=True)
        stats = normals.rewrite(src, dst, **kw)
        data = dst.read_bytes()
        self.assertEqual(len(data), len(src.read_bytes()))
        start = len(data) - 4 - len(vertices) * (V.size + 4)
        return stats, [V.unpack(data[start + i * V.size:start + (i + 1) * V.size]) for i in range(len(vertices))]

    def test_shared_vertex_averages_within_crease_and_keeps_positions(self):
        # Two triangles sharing the edge (1,0,0)-(0,1,0), faces tilted 30 degrees apart.
        n1 = (0.0, 0.0, 1.0); n2 = (0.5, 0.0, math.sqrt(0.75))
        verts = tri((0, 0, 0), (1, 0, 0), (0, 1, 0), n1) + tri((1, 0, 0), (1, 1, 0), (0, 1, 0), n2)
        stats, out = self.run_rewrite(verts)
        self.assertEqual((stats['triangles'], stats['groups_welded']), (2, 4))
        for i, v in enumerate(out):
            self.assertEqual(v[:3], verts[i][:3]); self.assertEqual(v[6:], verts[i][6:])
        expected = tuple(a / math.sqrt(sum(x * x for x in (n1[0] + n2[0], n1[1] + n2[1], n1[2] + n2[2]))) for a in (n1[0] + n2[0], n1[1] + n2[1], n1[2] + n2[2]))
        for shared in (1, 2, 3, 5):
            self.assertTrue(all(abs(a - b) < 1e-6 for a, b in zip(out[shared][3:6], expected)))
        self.assertTrue(all(abs(a - b) < 1e-6 for a, b in zip(out[0][3:6], n1)))
        self.assertTrue(all(abs(a - b) < 1e-6 for a, b in zip(out[4][3:6], n2)))

    def test_crease_and_attribute_qualification(self):
        # Same edge, faces 90 degrees apart: outside the 60 degree crease, no averaging.
        verts = tri((0, 0, 0), (1, 0, 0), (0, 1, 0), (0.0, 0.0, 1.0)) + tri((1, 0, 0), (1, 1, 0), (0, 1, 0), (1.0, 0.0, 0.0))
        _, out = self.run_rewrite(verts)
        self.assertEqual([v[3:6] for v in out], [v[3:6] for v in verts])
        # Coincident positions with a different colour are not one vertex: no weld.
        verts = tri((0, 0, 0), (1, 0, 0), (0, 1, 0), (0.0, 0.0, 1.0)) + tri((1, 0, 0), (1, 1, 0), (0, 1, 0), (0.5, 0.0, math.sqrt(0.75)), color=0xff0000ff)
        stats, out = self.run_rewrite(verts)
        self.assertEqual(stats['groups_welded'], 6)
        for a, b in zip(out, verts):
            self.assertTrue(all(abs(x - y) < 1e-6 for x, y in zip(a[3:6], b[3:6])))

    def test_refuses_non_flat_and_bad_packets(self):
        verts = tri((0, 0, 0), (1, 0, 0), (0, 1, 0), (0.0, 0.0, 1.0))
        verts[1] = verts[1][:3] + (1.0, 0.0, 0.0) + verts[1][6:]
        with self.assertRaises(ValueError):
            self.run_rewrite(verts)
        src = self.dir / 'bad.bin'; src.write_bytes(packet([], version=3))
        with self.assertRaises(ValueError):
            normals.rewrite(src, self.dir / 'o.bin')


if __name__ == '__main__':
    unittest.main()
