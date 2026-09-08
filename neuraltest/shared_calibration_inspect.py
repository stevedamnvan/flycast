"""Two linked transform instances in one replay context, not all-scene calibration."""
import argparse
import copy
import json
from pathlib import Path
import numpy as np

from binary32_oracle import fraction
from transform_context_inspect import inspect_context
from transform_semantics_inspect import analyze, project
from transform_span_inspect import records
from transform_store_inspect import compare_native, load_session, require


def load_sample(path, different):
    # Original fixture still supplies the discriminating wrong-rounding control.
    # The second reciprocal happens to agree for nearest and toward-zero. Both
    # retain exact actual arithmetic checks; no sign/scale/output test is skipped.
    events = (33, 48, 50, 52, 377) if different else (33, 58, 60, 62, 381)
    witness = inspect_context(path, require_rounding_control=not different, access_events=events)
    session = load_session(path)
    rows = [records(session, 'FC067_CALIB_'+tag) for tag in ('SOURCE', 'COPY', 'DECODE')]
    require(all(len(r) == 1 for r in rows), 'expected one calibration source/copy/decode')
    source, copied, decoded = [r[0] for r in rows]
    epochs = [int(r['epoch']) for r in (source, copied, decoded)]
    require(epochs[0] > 0 and len(set(epochs)) == 1, 'stale source/copy/decode epoch')
    require(int(copied['context'], 16) == int(decoded['child'], 16)
            == int(decoded['root'], 16) == witness['context'], 'calibration context differs')
    require(source['different'] == str(int(different)), 'wrong selection lane')
    require(1 <= int(source['candidates']) <= 200000, 'selection outside candidate bound')
    actual = witness['ta']['derived']['source']
    require(int(source['cycles']) == actual[2]
            and 0 <= actual[2] - int(source['first_cycle']) <= 20000000, 'selection outside cycle bound')
    if not different:
        require(source['candidates'] == '1' and int(source['first_cycle']) == actual[2], 'not original sample')
    columns = [[int(v, 16) for v in c.split(',')] for c in actual[4]]
    vector = [int(v, 16) for v in actual[3].split(',')]
    semantics = analyze(columns, vector, actual[1])
    matrix = np.array([[float(fraction(v)) for v in c] for c in columns]).T
    point = np.array([float(fraction(v)) for v in vector])
    scales = np.array(semantics['axis_scales'])
    calibration = np.diag([scales[0]/scales[2], scales[1]/scales[2], 1., 1.])
    return dict(witness=witness, epoch=epochs[0], first_cycle=int(source['first_cycle']),
                matrix=matrix, point=point, calibration=calibration, semantics=semantics)


def compare_samples(first, second, calibration=None):
    require(first['epoch'] == second['epoch'], 'stale or different context epoch')
    a, b = first['witness'], second['witness']
    require(a['context'] == b['context'] and first['first_cycle'] == second['first_cycle'],
            'different context or replay selection origin')
    require(a['vertex'] != b['vertex'] and not np.array_equal(first['matrix'], second['matrix']),
            'not distinct linked transform instances')
    require(a['ta']['cycles'] == b['ta']['cycles'] and a['ta']['count'] == b['ta']['count']
            and a['ta']['destination'] - a['ta']['offset'] == b['ta']['destination'] - b['ta']['offset'],
            'not the same bounded bulk transfer')
    require(b['packet_offset']-a['packet_offset'] == b['ta']['offset']-a['ta']['offset'],
            'copied packet spacing differs')
    calibration = first['calibration'] if calibration is None else calibration
    require(np.isfinite(calibration).all(), 'nonfinite shared calibration')
    errors = []
    for sample in (first, second):
        composite = np.linalg.solve(sample['calibration'], sample['matrix'])
        # One observed point and three analytic local perturbations per instance.
        for delta in ([0,0,0,0], [.125,0,0,0], [0,.125,0,0], [0,0,.125,0]):
            p = sample['point'] + delta
            errors.append(float(np.max(np.abs(project(calibration @ composite, p)-project(sample['matrix'], p)))))
    require(max(errors) < 0.001, 'shared calibration exceeds reprojection tolerance')
    return max(errors)


def verify(original, different, baseline):
    first, second = load_sample(original, False), load_sample(different, True)
    error = compare_samples(first, second)
    wrong_scale = first['calibration'].copy()
    wrong_scale[0,0] *= 2
    try:
        compare_samples(first, second, wrong_scale)
    except ValueError:
        pass
    else:
        raise ValueError('wrong shared scale was accepted')
    stale = copy.copy(second)
    stale['epoch'] += 1
    try:
        compare_samples(first, stale)
    except ValueError:
        pass
    else:
        raise ValueError('stale epoch was accepted')
    return dict(scope='two-linked-transform-instances-one-replay-context', epoch=first['epoch'],
                context=f"{first['witness']['context']:08x}",
                vertices=[s['witness']['vertex'] for s in (first, second)],
                normalized_projection_scales=[np.diag(s['calibration'])[:2].tolist() for s in (first, second)],
                max_shared_projection_error=error, observed_points=2, synthetic_probe_points=6,
                reciprocal_depth_scale_word='3f851eb8', screen_offset=[320, 240],
                wrong_scale_rejected=True, stale_epoch_rejected=True, controls='offline parsed-input mutations',
                native_plane_comparisons=compare_native(original, different, baseline),
                whole_scene_calibration='unproven', world_camera='unproven')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('original', 'different', 'baseline'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(verify(args.original, args.different, args.baseline), indent=2))
