#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Rewrite the vertex normals of a saved v4/v5 view packet for a same-source A/B.

The exporter writes one flat normal per triangle (D-223 default). This tool
produces the smoothed variant offline from the very same packet so that the
standalone helper renders flat and smoothed geometry of one source frame under
identical lighting. It mirrors the exporter's level-2 rule (D-225): within one
exported mesh, vertices whose exported position, texture coordinate and colour
are bit-identical are the same logical vertex; each such group averages the
face normals within a 60 degree crease of the facet being shaded. Nothing else
in the packet changes. The input must be flat (all three vertices of a triangle
carry the same normal), which also guards against smoothing twice.
"""
import argparse
import math
import struct
from pathlib import Path

VERTEX = struct.Struct('<3f3fffI')
CREASE_COSINE = 0.5


def smooth_mesh(vertices):
    """vertices: list of (px,py,pz,nx,ny,nz,u,v,color) as an expanded triangle list."""
    groups = {}
    for i, v in enumerate(vertices):
        groups.setdefault((v[0], v[1], v[2], v[6], v[7], v[8]), []).append(i)
    out = list(vertices)
    for members in groups.values():
        for i in members:
            n = vertices[i][3:6]
            sx = sy = sz = 0.0
            for j in members:
                m = vertices[j][3:6]
                if n[0] * m[0] + n[1] * m[1] + n[2] * m[2] < CREASE_COSINE:
                    continue
                sx += m[0]; sy += m[1]; sz += m[2]
            length = math.sqrt(sx * sx + sy * sy + sz * sz)
            if length > 1e-12:
                out[i] = vertices[i][:3] + (sx / length, sy / length, sz / length) + vertices[i][6:]
    return out


def rewrite(source, target, require_flat=True):
    data = Path(source).read_bytes()
    out = bytearray(data)
    offset = 0

    def take(n):
        nonlocal offset
        if not 0 <= n <= len(data) - offset:
            raise ValueError('truncated packet')
        value = data[offset:offset + n]
        offset += n
        return value

    def read(fmt):
        return struct.unpack('<' + fmt, take(struct.calcsize('<' + fmt)))

    magic, version = read('II')
    if magic != 0x56524346 or version not in (4, 5):
        raise ValueError('requires an owned v4/v5 packet')
    read('QQQQ')
    take(read('I')[0]); take(read('I')[0]); take(read('I')[0]); take(16 + 60)
    for _ in range(read('I')[0]):
        take(read('I')[0])
    stats = dict(meshes=0, vertices=0, groups_welded=0, normals_changed=0, triangles=0)
    for _ in range(read('I')[0]):
        read('Q'); read('IIII'); read('QQQQ')
        if version >= 5:
            read('I')
        take(read('I')[0])
        count = read('I')[0]
        start = offset
        vertices = [VERTEX.unpack(take(VERTEX.size)) for _ in range(count)]
        if count % 3:
            raise ValueError('expanded triangle list expected')
        if require_flat:
            for t in range(0, count, 3):
                if not (vertices[t][3:6] == vertices[t + 1][3:6] == vertices[t + 2][3:6]):
                    raise ValueError('packet is not flat-shaded; refusing to smooth twice')
        smoothed = smooth_mesh(vertices)
        for i, v in enumerate(smoothed):
            out[start + i * VERTEX.size:start + (i + 1) * VERTEX.size] = VERTEX.pack(*v)
            stats['normals_changed'] += max(abs(a - b) for a, b in zip(v[3:6], vertices[i][3:6])) > 1e-4
        stats['groups_welded'] += sum(1 for g in {(v[0], v[1], v[2], v[6], v[7], v[8]) for v in vertices})
        stats['meshes'] += 1; stats['vertices'] += count; stats['triangles'] += count // 3
        take(read('I')[0] * 4)
    if offset != len(data):
        raise ValueError('trailing packet bytes')
    Path(target).write_bytes(bytes(out))
    return stats


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source', type=Path)
    p.add_argument('target', type=Path)
    a = p.parse_args()
    if a.target.exists():
        raise SystemExit('target exists; refusing to overwrite')
    print(rewrite(a.source, a.target))


if __name__ == '__main__':
    main()
