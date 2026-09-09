"""Read-only analytic coverage diagnostic for the bounded synthetic Remix scene.

This is not a gameplay, depth-order, or temporal acceptance gate. CLI reads a
640x480 top-down 32-bit BMP written by remake-runtime-smoke. JSON is stdout only.
"""
import argparse
import json
import math
import struct
from pathlib import Path


def coverage(px, py, camera_x, apex_offset=0):
    for size in (1, 2):
        z = 2 * size
        vertices = [(-size, -size), (size, -size), (apex_offset, size)]
        points = [(320 + (x-camera_x)*240/z, 240-y*240/z)
                  for x, y in vertices]
        signs = []
        for i in range(3):
            a, b = points[i], points[(i+1) % 3]
            signs.append((b[0]-a[0])*(py-a[1])-(b[1]-a[1])*(px-a[0]))
        if min(signs) >= 0 or max(signs) <= 0:
            return True
    return False


def inspect(path, camera_x, normals, apex_offset=0):
    if not math.isfinite(camera_x) or not math.isfinite(apex_offset):
        raise ValueError('finite camera and apex offset required')
    path = Path(path)
    if path.stat().st_size != 54 + 640*480*4:
        raise ValueError('expected bounded harness BMP')
    with path.open('rb') as stream:
        data = stream.read(55 + 640*480*4)
    if len(data) != 54 + 640*480*4 or data[:2] != b'BM':
        raise ValueError('expected bounded harness BMP')
    if struct.unpack_from('<iiHHI', data, 18) != (640, -480, 1, 32, 0):
        raise ValueError('unexpected BMP format')
    if struct.unpack_from('<I', data, 10)[0] != 54:
        raise ValueError('unexpected pixel offset')
    expected = observed = intersection = 0
    for y in range(480):
        for x in range(640):
            b, g, r = data[54+(y*640+x)*4:57+(y*640+x)*4]
            seen = max(b, g, r)-min(b, g, r) > 30 if normals else min(b, g, r) > 200
            truth = coverage(x+.5, y+.5, camera_x, apex_offset)
            expected += truth
            observed += seen
            intersection += truth and seen
    union = expected + observed - intersection
    return dict(camera_x=camera_x, apex_offset=apex_offset, expected_pixels=expected,
                observed_pixels=observed, intersection_pixels=intersection,
                iou=intersection/union if union else 0,
                mode='normal-visualization' if normals else 'bright-final-color',
                diagnostic_only=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bmp')
    parser.add_argument('--camera-x', type=float, default=.5)
    parser.add_argument('--apex-offset', type=float, default=0)
    parser.add_argument('--normals', action='store_true')
    args = parser.parse_args()
    if not math.isfinite(args.camera_x) or not math.isfinite(args.apex_offset):
        parser.error('finite camera and apex offset required')
    print(json.dumps(inspect(args.bmp, args.camera_x, args.normals,
                             args.apex_offset), sort_keys=True))
