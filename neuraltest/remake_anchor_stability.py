#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Measure anchored coordinate drift; never infer world/camera truth from it."""
import argparse
import hashlib
import json
import math
import struct
from pathlib import Path
from remake_material_compare import materials
from remake_temporal_compare import captures, require


def geometry(path):
    identity, _ = materials(path)  # Validate the bounded owned wire first.
    data = path.read_bytes()
    offset = 40
    def read(fmt):
        nonlocal offset
        result = struct.unpack_from('<' + fmt, data, offset)
        offset += struct.calcsize('<' + fmt)
        return result
    def skip_string():
        nonlocal offset
        length = read('I')[0]
        offset += length
    for _ in range(3):
        skip_string()
    camera = read('19f')
    require(all(math.isfinite(v) for v in camera), 'nonfinite camera')
    for _ in range(read('I')[0]):
        skip_string()
    buckets = {}
    for _ in range(read('I')[0]):
        mesh_id = read('Q')[0]
        state = read('IIII')
        texture = read('QQQQ')
        length = read('I')[0]
        offset += length
        vertices = [read('8fI') for _ in range(read('I')[0])]
        indices = read(str(read('I')[0]) + 'I')
        require(all(math.isfinite(x) for v in vertices for x in v[:8]), 'nonfinite vertex')
        # No draw-ordinal identity. Repeated topology/UV/texture buckets are
        # ambiguous and excluded, rather than greedily assigning false matches.
        signature = hashlib.sha256(repr((state, texture, indices,
            [(v[6], v[7]) for v in vertices])).encode()).hexdigest()
        buckets.setdefault(signature, []).append((mesh_id, [v[:3] for v in vertices]))
    require(offset == len(data), 'trailing data')
    return identity, camera, buckets


def compare(left, right):
    rows = []
    ambiguous = 0
    for key in sorted(left.keys() & right.keys()):
        if len(left[key]) != 1 or len(right[key]) != 1:
            ambiguous += 1
            continue
        a, b = left[key][0], right[key][0]
        require(len(a[1]) == len(b[1]), 'topology mismatch')
        distances = [math.dist(x, y) for x, y in zip(a[1], b[1])]
        if distances:
            rows.append(dict(signature=key, mesh_ids=[a[0], b[0]], vertices=len(distances),
                             rms=math.sqrt(sum(d*d for d in distances)/len(distances)),
                             maximum=max(distances)))
    return rows, ambiguous


def summarize(reports):
    tracks = {}
    for frame in reports:
        for row in frame['matched']:
            track = tracks.setdefault(row['signature'], dict(samples=0, maximum=0,
                vertices=row['vertices'], first_mesh=row['mesh_ids'][0]))
            track['samples'] += 1
            track['maximum'] = max(track['maximum'], row['maximum'])
    return tracks


def view_coordinates(camera, buckets):
    result = {}
    for key, meshes in buckets.items():
        converted = []
        for mesh, points in meshes:
            positions = []
            for point in points:
                delta = [point[i]-camera[4+i] for i in range(3)]
                positions.append(tuple(sum(delta[i]*camera[7+axis*3+i] for i in range(3))
                                       for axis in range(3)))
            converted.append((mesh, positions))
        result[key] = converted
    return result


def draw_map(first, tracks, path):
    # Diagnostic point plot, not a rendered scene or visibility classifier.
    from PIL import Image, ImageDraw
    image = Image.new('RGB', (640, 510), (20, 20, 24))
    draw = ImageDraw.Draw(image)
    camera = first[1]
    t = math.tan(math.radians(camera[0]) / 2)
    for signature, meshes in first[2].items():
        if len(meshes) != 1 or signature not in tracks:
            continue
        track = tracks[signature]
        color = (40, 230, 130) if track['maximum'] < 0.001 else (255, 100, 90)
        for point in meshes[0][1]:
            delta = [point[i] - camera[4+i] for i in range(3)]
            x, y, z = [sum(delta[i]*camera[7+axis*3+i] for i in range(3)) for axis in range(3)]
            if z <= 0:
                continue
            px, py = 640*(.5+x/(2*z*t*camera[1])), 480*(.5-y/(2*z*t))
            if 0 <= px < 640 and 0 <= py < 480:
                draw.ellipse((px-1, py-1, px+1, py+1), fill=color)
    draw.text((8, 486), 'Green: drift <0.001; red: larger. Points, no occlusion. Not semantic labels.', fill='white')
    require(not path.exists(), 'refuse map overwrite')
    image.save(path)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('captures', type=Path)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--map', type=Path)
    a = p.parse_args()
    frames = captures(a.captures)
    require(2 <= len(frames) <= 360, 'bounded sequence required')
    first = None
    first_view = None
    reports = []
    for frame in sorted(frames):
        current = geometry(frames[frame][0] / 'remake-view.bin')
        require(current[0][0] == frame, 'frame mismatch')
        if first is None:
            first = current
            first_view = view_coordinates(first[1], first[2])
        rows, ambiguous = compare(first[2], current[2])
        wrong, _ = compare(first_view, view_coordinates(current[1], current[2]))
        reports.append(dict(frame=frame, camera=list(current[1]),
                            matched=rows, camera_ignored=wrong, ambiguous_buckets=ambiguous))
    tracks = summarize(reports)
    report = dict(scope='anchored coordinate drift only; no arena classification or camera acceptance',
                  baseline_frame=first[0][0], frames=reports, tracks=tracks,
                  camera_ignored_tracks=summarize([dict(matched=r['camera_ignored']) for r in reports]))
    with a.out.open('x') as stream:
        json.dump(report, stream)
    if a.map:
        draw_map(first, tracks, a.map)
    print(json.dumps(dict(frames=len(reports), baseline=first[0][0],
                         last_matched=len(reports[-1]['matched']), output=str(a.out))))


if __name__ == '__main__':
    main()
