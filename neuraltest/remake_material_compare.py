#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Compare exact material payloads in Flycast-owned v4 packets, not camera poses."""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from remake_temporal_compare import captures, require


def materials(path, exact_geometry=False):
    data = path.read_bytes()
    require(len(data) <= 72*1024*1024, 'packet bound')
    offset = 0
    def take(n):
        nonlocal offset
        require(0 <= n <= len(data)-offset, 'truncated packet')
        value = data[offset:offset+n]
        offset += n
        return value
    def read(fmt):
        return struct.unpack('<'+fmt, take(struct.calcsize('<'+fmt)))
    def count(bound):
        n = read('I')[0]
        require(n <= bound, 'count bound')
        return n
    require(read('II') == (0x56524346, 4), 'requires owned v4 packet')
    frame, epoch, producer, cycle = read('QQQQ')
    require(take(count(64)) == b'T1401N', 'game mismatch')
    take(count(64));take(count(128))
    camera = take(16+60)  # Lens, anchored camera basis/position and origin.
    for _ in range(count(64)):
        take(count(256))
    result = {}
    texture_bytes = vertices = indices = 0
    for _ in range(count(128)):
        mesh = read('Q')[0]
        state = read('IIII')  # Sampler, cutout reference, blend, known texture.
        identity = read('QQQQ')
        length = count(64*1024*1024-texture_bytes);texture_bytes += length
        dds = take(length)
        require(len(dds) >= 148 and dds[:4] == b'DDS ', 'material DDS missing')
        require(mesh not in result, 'duplicate mesh')
        result[mesh] = (state, identity, hashlib.sha256(dds).hexdigest())
        n = count(65536-vertices);vertices += n;vertex_data = take(n*36)
        n = count(262144-indices);indices += n;index_data = take(n*4)
        if exact_geometry:
            lengths = struct.pack('<II', len(vertex_data), len(index_data))
            result[mesh] += (hashlib.sha256(camera+lengths+vertex_data+index_data).hexdigest(),)
    require(offset == len(data), 'trailing packet bytes')
    return (frame, epoch, producer, cycle), result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('baseline', type=Path);p.add_argument('candidate', type=Path)
    p.add_argument('--exact-geometry', action='store_true',
                   help='Also require byte-exact camera, vertices and indices; no anchor tolerance')
    a = p.parse_args()
    lanes = [captures(a.baseline), captures(a.candidate)]
    frames = sorted(set(lanes[0]) & set(lanes[1]))
    require(1 <= len(frames) <= 360, 'bounded overlap required')
    total = 0
    for frame in frames:
        left, right = [materials(lane[frame][0]/'remake-view.bin', a.exact_geometry) for lane in lanes]
        require(left[0] == right[0] and left[0][0] == frame, (frame, 'source identity mismatch'))
        require(left[1] == right[1], (frame, 'material/state/generation or requested exact geometry mismatch'))
        total += len(left[1])
    print(json.dumps(dict(matched_frames=len(frames), matched_mesh_materials=total,
                         unmatched=[sorted(set(lane)-set(frames)) for lane in lanes],
                         exact_geometry=a.exact_geometry,
                         scope='exact material/state/generation and producer identity; camera/geometry only when requested; not output equality')))


if __name__ == '__main__':
    main()
