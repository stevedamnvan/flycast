"""Diagnostic triangle affine transport, not recovered bones or GPU proof."""
import math
import struct


def sub(a, b):
    return [x-y for x, y in zip(a, b)]


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def cross(a, b):
    return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]


def basis(points):
    if len(points) != 3 or any(len(p) != 3 or not all(math.isfinite(v) for v in p) for p in points):
        raise ValueError('finite triangle required')
    u, v = sub(points[1], points[0]), sub(points[2], points[0])
    n = cross(u, v)
    area = math.sqrt(dot(n, n))
    scale = math.sqrt(dot(u, u)*dot(v, v))
    if not math.isfinite(scale) or not scale or area <= scale*1e-8:
        raise ValueError('degenerate or ill-conditioned triangle')
    return [u, v, [x/area for x in n]]


def transform(first, second):
    """Map two edges and unit normal; return float32 row-major affine matrix.

    One such transform per split triangle needs <=256 triangles per instance.
    This deliberately does not solve changing UVs, colors, or correspondence.
    """
    a, b = basis(first), basis(second)
    det = dot(a[0], cross(a[1], a[2]))
    inverse = [[v/det for v in cross(a[1], a[2])],
               [v/det for v in cross(a[2], a[0])],
               [v/det for v in cross(a[0], a[1])]]
    result = []
    for row in range(3):
        linear = [sum(b[k][row]*inverse[k][col] for k in range(3)) for col in range(3)]
        result.extend(linear + [second[0][row]-dot(linear, first[0])])
    try:
        result = list(struct.unpack('<12f', struct.pack('<12f', *result)))
    except (OverflowError, struct.error) as error:
        raise ValueError('unrepresentable public transform') from error
    if not all(math.isfinite(v) for v in result):
        raise ValueError('unrepresentable public transform')
    return result


def apply(matrix, point, direction=False):
    return [dot(matrix[r*4:r*4+3], point) + (0 if direction else matrix[r*4+3]) for r in range(3)]


def inspect(first, second):
    matrix = transform(first, second)
    normal = apply(matrix, basis(first)[2], True)
    length = math.sqrt(dot(normal, normal))
    return dict(matrix=matrix,
                max_position_error=max(math.dist(apply(matrix, p), q) for p, q in zip(first, second)),
                normal_error=math.dist([v/length for v in normal], basis(second)[2]),
                unchanged_position_error=max(math.dist(p, q) for p, q in zip(first, second)),
                diagnostic_only=True, gpu_proven=False)


if __name__ == '__main__':
    import argparse
    import json
    from pathlib import Path
    from retained_topology_inspect import inspect as topology_inspect
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('first', type=Path)
    parser.add_argument('second', type=Path)
    args = parser.parse_args()
    first, second = [json.loads(p.read_text()) for p in (args.first, args.second)]
    topology = topology_inspect(first, second)
    if not all(m['topology_equal'] for m in topology['meshes']):
        raise ValueError('source topology differs')
    previous = {m['source_draw']: m for m in first['meshes']}
    rows = []
    for mesh in second['meshes']:
        before = previous[mesh['source_draw']]
        indices = mesh['indices']
        if len(indices) % 3:
            raise ValueError('triangle list required')
        results, rejected = [], 0
        for start in range(0, len(indices), 3):
            ids = indices[start:start+3]
            try:
                results.append(inspect([before['vertices'][i]['position'] for i in ids],
                                       [mesh['vertices'][i]['position'] for i in ids]))
            except ValueError:
                rejected += 1
        rows.append(dict(draw=mesh['source_draw'], triangles=len(results), rejected=rejected,
                         bone_batches=(len(indices)//3+255)//256,
                         **{key: max((r[key] for r in results), default=None) for key in
                            ('max_position_error', 'normal_error', 'unchanged_position_error')}))
    print(json.dumps(dict(diagnostic_only=True, gpu_proven=False,
                          dynamic_attributes_solved=False, meshes=rows), sort_keys=True))
