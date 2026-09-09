"""Diagnostic comparison of public Remix depth with the synthetic camera.
Reads only the fixed 640x480 RGBA32F capture; never declares a gate passed.
"""
import json
import math
import struct
import sys
from pathlib import Path

path = Path(sys.argv[1])
if path.stat().st_size != 640*480*16:
    raise ValueError('expected bounded RGBA32F')
with path.open('rb') as stream:
    raw = stream.read(640*480*16+1)
if len(raw) != 640*480*16:
    raise ValueError('size changed')
counts = dict(near=0, far=0, background=0, invalid=0)
errors = []
interior_errors = []
wrong_order_errors = []
intersection = union = 0
def expected_at(x, y, reverse=False):
    for size in ((2, 1) if reverse else (1, 2)):
        z = 2*size
        world_x = .5 + (x+.5-320)*z/240
        world_y = (240-y-.5)*z/240
        if -size <= world_y <= size and abs(world_x) <= (size-world_y)/2:
            return 100/99.9 - .1*100/(99.9*z)
    return 1.

for y in range(480):
    for x in range(640):
        observed = struct.unpack_from('<f', raw, (y*640+x)*16)[0]
        expected = 1.
        for size in (1, 2):
            # Ray intersection with each synthetic triangle's constant-Z plane.
            z = 2*size
            world_x = .5 + (x+.5-320)*z/240
            world_y = (240-y-.5)*z/240
            if -size <= world_y <= size and abs(world_x) <= (size-world_y)/2:
                expected = 100/99.9 - .1*100/(99.9*z)
                break
        if not math.isfinite(observed):
            counts['invalid'] += 1
            continue
        counts['near' if observed < .96 else 'far' if observed < .99 else 'background'] += 1
        seen, truth = observed < .99, expected < .99
        intersection += seen and truth
        union += seen or truth
        if seen and truth:
            errors.append(abs(observed-expected))
            # Report separately, never replace the all-pixel residual above.
            interior = all(expected_at(x+dx, y+dy) == expected
                           for dx in (-2, 0, 2) for dy in (-2, 0, 2))
            if interior:
                interior_errors.append(abs(observed-expected))
                wrong_order_errors.append(abs(observed-expected_at(x,y,True)))
def mean(values):
    return sum(values)/len(values) if values else None

print(json.dumps(dict(counts=counts, coverage_iou=intersection/union if union else None,
                     matched_mae=mean(errors),
                     matched_max=max(errors, default=None),
                     interior_pixels=len(interior_errors),
                     interior_mae=mean(interior_errors),
                     interior_max=max(interior_errors, default=None),
                     wrong_order_interior_mae=mean(wrong_order_errors),
                     diagnostic_only=True), sort_keys=True))
