"""Read-only diagnostic: can two prepared snapshots share immutable attributes?

Source slot equality is necessary, not proof of object identity or a skeleton.
No artifacts are rewritten and no rendering acceptance is inferred.
"""
import argparse
import json
import math
from pathlib import Path


def inspect(first, second):
    if (first['game_id'] != second['game_id'] or
            second['frame_id'] != first['frame_id'] + 1 or
            first['coordinate_space'] != second['coordinate_space'] or
            first['fixed_origin'] != second['fixed_origin']):
        raise ValueError('incompatible snapshot sequence')
    def meshes(scene):
        result = {m['source_draw']: m for m in scene['meshes']}
        if len(result) != len(scene['meshes']):
            raise ValueError('duplicate source draw')
        return result
    a, b = meshes(first), meshes(second)
    if a.keys() != b.keys():
        raise ValueError('changed draw domain')
    rows = []
    for draw in sorted(a):
        x, y = a[draw], b[draw]
        xv, yv = x['vertices'], y['vertices']
        topology = (x['indices'] == y['indices'] and len(xv) == len(yv)
                    and [v['source_vertex'] for v in xv] ==
                    [v['source_vertex'] for v in yv])
        row = dict(draw=draw, vertices=len(xv), topology_equal=topology,
                   bindings_equal=x['source_bindings'] == y['source_bindings'])
        if topology:
            movement, normal_delta = [], []
            uv_changes = color_changes = offset_changes = 0
            color_deltas = set()
            for old, new in zip(xv, yv):
                values = old['position'] + new['position'] + old['normal'] + new['normal']
                if len(values) != 12 or not all(math.isfinite(v) for v in values):
                    raise ValueError('invalid geometry')
                movement.append(math.dist(old['position'], new['position']))
                normal_delta.append(math.dist(old['normal'], new['normal']))
                # The current adapter imports base color, not PVR offset color.
                uv_changes += old['original_vertex'][3:5] != new['original_vertex'][3:5]
                before, after = old['original_vertex'][5], new['original_vertex'][5]
                if (not isinstance(before, list) or not isinstance(after, list) or
                        len(before) != 4 or len(after) != 4 or
                        any(type(v) is not int or not 0 <= v <= 255 for v in before + after)):
                    raise ValueError('invalid base color')
                color_changes += before != after
                color_deltas.add(tuple(v-u for u, v in zip(before, after)))
                offset_changes += old['original_vertex'][6] != new['original_vertex'][6]
            row.update(max_position_delta=max(movement, default=0),
                       max_normal_delta=max(normal_delta, default=0),
                       changed_uv_vertices=uv_changes, changed_color_vertices=color_changes,
                       changed_offset_color_vertices=offset_changes,
                       distinct_base_color_deltas=len(color_deltas),
                       immutable_attributes_equal=not uv_changes and not color_changes
                           and row['bindings_equal'])
        rows.append(row)
    return dict(diagnostic_only=True, identity_proven=False, rendered=False,
                frames=[first['frame_id'], second['frame_id']], meshes=rows)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('first', type=Path)
    parser.add_argument('second', type=Path)
    args = parser.parse_args()
    print(json.dumps(inspect(json.loads(args.first.read_text()),
                             json.loads(args.second.read_text())), sort_keys=True))
