"""Create a diagnostic endpoint from independently witnessed frame lineage.

Reuses existing strict inspectors; never promotes camera/complete-scene status.
Writes only a new output directory containing verified assets and scene JSON.
"""
import argparse
import json
import math
from pathlib import Path
from ancestry_scene_inspect import inspect
from selected_basis_inspect import anchored_camera, anchor_mesh
from anchor_orientation_inspect import convert
from camera_precision_inspect import rebase
from source_dds import publish_capture
from remake_asset_join_inspect import join
from remake_scene_artifact_inspect import prepare


def bounded_json(path, limit):
    if path.stat().st_size > limit:
        raise ValueError('JSON input exceeds bound')
    with path.open('rb') as stream:
        data = stream.read(limit+1)
    if len(data) > limit:
        raise ValueError('JSON input grew beyond bound')
    return json.loads(data)


def validate_reference(reference):
    if reference.get('schema') != 'flycast-prepared-remake-scene-v1':
        raise ValueError('prepared reference required')
    origin = reference.get('fixed_origin')
    if not isinstance(origin,list) or len(origin)!=3 or not all(
            type(x) in (int,float) and math.isfinite(x) for x in origin):
        raise ValueError('finite shared origin required')
    if reference.get('frame_id') != 1782 or reference.get('game_id') != 'T1401N':
        raise ValueError('H source frame required')
    if not isinstance(reference.get('git_sha'),str) or not reference['git_sha']:
        raise ValueError('source SHA required')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('capture', 'tape', 'ledger', 'topology', 'reference', 'normal_executable', 'output'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--ordinal', type=int, choices=(1781,1782,1783), required=True)
    parser.add_argument('--calibration', type=float, nargs=2, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('output must be new')
    reference = bounded_json(args.reference,32*1024*1024)
    validate_reference(reference)
    evidence = inspect(args.capture,args.tape,args.ledger,args.topology,
                       args.calibration,ordinal=args.ordinal,source_details=True)
    camera = anchored_camera(evidence['selected_divided_sources'],args.calibration)
    mesh = rebase(convert(anchor_mesh(evidence['expression_mesh'],camera),args.calibration),
                  reference['fixed_origin'])
    frame = args.capture / f'frame-{args.ordinal+1:06d}'
    scene = bounded_json(frame/'pvr-scene.json',64*1024*1024)
    materials = bounded_json(frame/'materials'/'manifest.json',16*1024*1024)
    if scene['game_id'] != reference['game_id'] or scene['git_sha'] != reference['git_sha']:
        raise ValueError('reference source identity mismatch')
    draws = sorted({v['source_draw'] for v in mesh['vertices']})
    args.output.mkdir(exist_ok=False)
    assets = args.output/'assets'
    publish_capture(scene,materials,draws,frame/'materials',assets)
    joined = join(mesh,scene,materials,frame/'materials',assets)
    prepared = prepare(joined,args.normal_executable)
    with (args.output/'scene.json').open('x',encoding='utf-8') as stream:
        json.dump(prepared,stream,sort_keys=True)
    print(json.dumps(dict(frame=prepared['frame_id'],meshes=len(prepared['meshes']),
                         camera=prepared['camera'],omissions=prepared['omissions'],
                         runtime_loaded=False,accepted_game_camera=False)))


if __name__ == '__main__':
    main()
