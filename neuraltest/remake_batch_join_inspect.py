"""Join same-frame diagnostic batches, preserving camera uncertainty and assets."""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
from prepare_remake_endpoint import bounded_json


def combine(a, b):
    if a.get('schema') != 'flycast-prepared-remake-scene-v1':
        raise ValueError('prepared scene required')
    for key in ('schema', 'frame_id', 'game_id', 'git_sha', 'coordinate_space',
                'fixed_origin', 'material_semantic'):
        if a[key] != b[key]:
            raise ValueError('source identity mismatch: ' + key)
    if a['renderable_by_remix_adapter'] is not False or b['renderable_by_remix_adapter'] is not False:
        raise ValueError('diagnostic inputs required')
    ca, cb = a['camera'], b['camera']
    if set(ca) != set(cb):
        raise ValueError('camera fields mismatch')
    for key in ca:
        x, y = ca[key], cb[key]
        xs, ys = (x, y) if isinstance(x, list) else ([x], [y])
        if len(xs) != len(ys):
            raise ValueError('camera shape mismatch')
        for v, w in zip(xs, ys):
            if type(v) in (int, float) and type(w) in (int, float):
                if not math.isfinite(v) or not math.isfinite(w) or abs(v-w) > 1e-12:
                    raise ValueError('camera differs beyond diagnostic roundoff')
            elif v != w:
                raise ValueError('camera metadata mismatch')
    result = copy.deepcopy(a)
    meshes = a['meshes'] + b['meshes']
    ids = [m['source_draw'] for m in meshes]
    if not 0 < len(meshes) <= 128 or len(ids) != len(set(ids)):
        raise ValueError('duplicate draw or mesh bound')
    if sum(len(m['vertices']) for m in meshes) > 65536 or sum(len(m['indices']) for m in meshes) > 262144:
        raise ValueError('aggregate geometry bound')
    result['meshes'] = copy.deepcopy(sorted(meshes, key=lambda m: m['source_draw']))
    for key, asset in b['source_assets'].items():
        if key in result['source_assets'] and result['source_assets'][key] != asset:
            raise ValueError('conflicting asset identity')
        result['source_assets'][key] = copy.deepcopy(asset)
    result['omissions'] = list(dict.fromkeys(a['omissions'] + b['omissions']))
    result['strict_reprojection_pass'] = a['strict_reprojection_pass'] and b['strict_reprojection_pass']
    result['batch_join'] = dict(diagnostic_only=True, cameras=[ca, cb],
                              camera_absolute_roundoff_bound=1e-12,
                              complete_scene=False)
    return result


def publish(a, aroot, b, broot, output):
    result = combine(a, b)
    return publish_verified(result, ((a,aroot),(b,broot)), output)


def publish_verified(result, inputs, output):
    # Verify every input publication before creating any output, including shared assets.
    verified = {}
    total = 0
    for scene, root in inputs:
        for key, asset in scene['source_assets'].items():
            name = 'asset-' + key + '.dds'
            if not key.isdecimal() or asset['file'] != name:
                raise ValueError('fixed asset path required')
            path = root/name
            if path.stat().st_size > 64*1024*1024:
                raise ValueError('asset bound')
            with path.open('rb') as stream:
                data = stream.read(64*1024*1024+1)
            if len(data) != asset['bytes'] or hashlib.sha256(data).hexdigest() != asset['sha256']:
                raise ValueError('asset bytes mismatch')
            if name not in verified:
                total += len(data)
                if total > 64*1024*1024:
                    raise ValueError('aggregate asset bound')
                verified[name] = data
    output.mkdir(exist_ok=False)
    (output/'assets').mkdir()
    for name, data in verified.items():
        with (output/'assets'/name).open('xb') as stream:
            stream.write(data)
    with (output/'scene.json').open('x', encoding='utf-8') as stream:
        json.dump(result, stream, sort_keys=True)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('first', 'first_assets', 'second', 'second_assets', 'output'):
        parser.add_argument(key, type=Path)
    args = parser.parse_args()
    result = publish(bounded_json(args.first, 32*1024*1024), args.first_assets,
                     bounded_json(args.second, 32*1024*1024), args.second_assets, args.output)
    print(json.dumps(dict(meshes=len(result['meshes']), assets=len(result['source_assets']),
                          complete_scene=False, runtime_loaded=False)))
