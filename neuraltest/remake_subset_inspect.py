"""Create an explicitly incomplete diagnostic subset; never change source assets."""
import argparse
import copy
import json
from pathlib import Path
from prepare_remake_endpoint import bounded_json


def subset(scene, excluded):
    if scene.get('schema') != 'flycast-prepared-remake-scene-v1':
        raise ValueError('prepared scene required')
    meshes = scene['meshes']
    if not 0 < len(meshes) <= 128:
        raise ValueError('mesh bound')
    ids = [m['source_draw'] for m in meshes]
    if len(set(ids)) != len(ids) or not excluded or not set(excluded) <= set(ids):
        raise ValueError('unique existing draw exclusions required')
    retained = [m for m in meshes if m['source_draw'] not in excluded]
    if not retained:
        raise ValueError('empty subset')
    result = copy.deepcopy(scene)
    result['meshes'] = copy.deepcopy(retained)
    result['omissions'].append('diagnostic exclusion of source draws: ' +
                              ','.join(map(str, sorted(excluded))))
    result['diagnostic_excluded_draws'] = sorted(excluded)
    result['renderable_by_remix_adapter'] = False
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--exclude', type=int, nargs='+', required=True)
    args = parser.parse_args()
    result = subset(bounded_json(args.source, 32*1024*1024), args.exclude)
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, sort_keys=True)
    print(json.dumps(dict(meshes=len(result['meshes']), excluded=args.exclude,
                         diagnostic_only=True)))
