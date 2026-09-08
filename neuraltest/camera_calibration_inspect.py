"""Shared scaled-axis calibration for already verified bounded transform samples."""
import argparse
import json
from pathlib import Path
import numpy as np
from binary32_oracle import fraction
from camera_transform_inspect import inspect_capture as inspect_transforms
from transform_semantics_inspect import factor
from transform_store_inspect import load_session,require
from transform_span_inspect import records


def analyze(matrices, calibration=None, full_draw=False, matrix_limit=None):
    if matrix_limit is None:matrix_limit=144 if full_draw else 21
    require(type(matrix_limit) is int and 1<=matrix_limit<=19341,'matrix budget')
    require(1<=len(matrices)<=matrix_limit,'matrix bounds')
    normalized=[]; residuals=[]
    for matrix in matrices:
        k,_,residual=factor(np.asarray(matrix,dtype=float))
        scale=np.diag(k)[:3]
        normalized.append(scale[:2]/scale[2]);residuals.append(residual)
    reference=np.asarray(normalized[0] if calibration is None else calibration,dtype=float)
    require(reference.shape==(2,) and np.isfinite(reference).all() and (reference>0).all(),'calibration shape')
    error=float(np.max(np.abs(np.asarray(normalized)-reference)))
    # Existing shared-calibration work used0.001 pixel-scale tolerance. Preserve it.
    require(error<.001,'incompatible normalized calibration')
    k=np.diag([reference[0],reference[1],1.,1.])
    composites=[np.linalg.solve(k,np.asarray(m,dtype=float)) for m in matrices]
    require(max(float(np.max(np.abs(k@c-m))) for c,m in zip(composites,matrices))<1e-9,'decomposition error')
    return dict(normalized_calibration=reference.tolist(),maximum_scale_difference=error,
                maximum_orthogonality_residual=max(residuals),matrix_count=len(matrices),
                coordinate_contract='bounded-calibrated-camera-relative',physical_scale_known=False,
                world_camera_recovered=False,whole_scene_coverage=False,production_enabled=False)


def inspect_capture(path, full_draw=False):
    evidence=inspect_transforms(path,full_draw)
    session=load_session(path)
    offsets=[r for r in records(session,'FC067_CALC_INPUT') if r['reg'] in ('20','21')]
    require(len(offsets)==(288 if full_draw else 30) and all(int(r['value'],16)==(0x43a00000 if r['reg']=='20' else 0x43700000) for r in offsets),
            'shared screen center not verified')
    samples=evidence['samples']
    if full_draw:
        samples=list({(s['record_generation'],s['record_base']):s for s in samples}.values())
    words=[s['xf_matrix_words'] for s in samples]
    for part in session.split('FC067_EXTRA_FILTER ')[1:]:
        section=part.split('FC067_EXTRA_EXIT ')[0]
        live={int(r['reg']):int(r['value'],16) for r in records(section,'FC067_EXTRA_INPUT')}
        words.append([live[r] for r in range(32,48)])
    require(len(words)==(144 if full_draw else 21) and evidence['observations']==(426 if full_draw else 15),'verified sample coverage')
    matrices=[np.array([float(fraction(w)) for w in row]).reshape(4,4).T for row in words]
    result=analyze(matrices,full_draw=full_draw)
    # Negative shares the same predicate, not a separate permissive comparison.
    wrong=result['normalized_calibration'][:];wrong[0]*=2
    try: analyze(matrices,wrong,full_draw)
    except ValueError: pass
    else: raise ValueError('doubled-calibration control accepted')
    result.update(verified_samples=evidence['observations'],unsupported_slots=[] if full_draw else [3],wrong_scale_rejected=True,screen_center=[320,240],
                  source_axis_convention='record-positive-Y-projects-down',depth_contract='verified-unit-reciprocal-record-Z')
    return result


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('capture',type=Path)
    parser.add_argument('--full-draw',action='store_true')
    args=parser.parse_args()
    print(json.dumps(inspect_capture(args.capture,args.full_draw),indent=2))
