"""Candidate source-set continuity; does not establish object or world identity."""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
from producer_ancestry_inspect import inspect as ancestry
from transform_ledger_inspect import decode
from transform_store_inspect import require
import numpy as np
from binary32_oracle import fraction


def normalized_rigidity(deltas,calibration):
    require(len(calibration)==2 and all(np.isfinite(v) and v>0 for v in calibration),'relative calibration')
    k=np.diag([*calibration,1.,1.])
    normalized=[np.linalg.solve(k,np.asarray(d))@k for d in deltas]
    errors=[float(np.max(np.abs(d[:3,:3]@d[:3,:3].T-np.eye(3)))) for d in normalized]
    determinants=[float(np.linalg.det(d[:3,:3])) for d in normalized]
    return dict(maximum_orthogonality_error=max(errors),determinants=determinants,
                rigid_under_existing_tolerance=max(errors)<1e-6 and all(d>0 for d in determinants),
                normalized_relative_transforms=[d.tolist() for d in normalized])


def relative_motion(matrix_words,point_words):
    require(len(matrix_words)==3 and len(point_words)>=4,'relative motion sample shape')
    matrices=[]
    for words in matrix_words:
        require(len(words)==16,'relative matrix shape')
        m=np.array([float(fraction(w)) for w in words]).reshape(4,4).T
        require(tuple(m[3]) in ((0,0,0,0),(0,0,0,1)),'relative fourth row')
        m[3]=[0,0,0,1] # Explicit affine XYZ analysis, not captured W.
        require(abs(np.linalg.det(m[:3,:3]))>1e-12,'singular relative matrix')
        matrices.append(m)
    points=np.array([[float(fraction(w)) for w in p] for p in point_words])
    require(points.ndim==2 and points.shape[1]==4,'relative point shape')
    singular=np.linalg.svd(points,compute_uv=False)
    deltas=[m@np.linalg.inv(matrices[0]) for m in matrices[1:]]
    return dict(homogeneous_source_rank=int(np.linalg.matrix_rank(points)),source_singular_values=singular.tolist(),
                affine_xyz_relative_transforms=[d.tolist() for d in deltas],world_identity_proven=False)


def groups(transforms):
    require(0<len(transforms)<=19341,'transform bound')
    grouped=defaultdict(Counter)
    for identity,record in transforms:
        point=tuple(record['point_words']);matrix=tuple(record['matrix_words'])
        require(len(point)==4 and len(matrix)==16,'transform shape')
        grouped[identity[1],matrix][point]+=1
    result=defaultdict(list)
    for (site,matrix),points in grouped.items():
        # Multiplicity is retained; a repeated point is not an independent witness.
        signature=tuple(sorted(points.items()))
        result[site,signature].append(matrix)
    return result


def compare(frames):
    require(len(frames)==3,'three sample frames required')
    inventories=[groups(frame) for frame in frames]
    common=set(inventories[0]).intersection(*[set(x) for x in inventories[1:]])
    rows=[]
    for key in common:
        matrices=[inventory[key] for inventory in inventories]
        rows.append(dict(site=key[0],source_set_sha256=hashlib.sha256(repr(key[1]).encode()).hexdigest(),
            unique_source_points=len(key[1]),executions=sum(count for _,count in key[1]),
            matrices_per_frame=[len(m) for m in matrices],
            unambiguous_matrix_per_frame=all(len(m)==1 for m in matrices),
            matrix_words_unchanged=all(set(m)==set(matrices[0]) for m in matrices[1:])))
    return dict(common_source_sets=sorted(rows,key=lambda r:(r['site'],r['source_set_sha256'])),
        source_set_counts=[len(x) for x in inventories],world_identity_proven=False,
        limitations=['same source set can be reused by distinct objects','inventory includes unselected executions',
                     'no static arena semantic classification','model/view split remains ambiguous'])


def divided_motion(frames):
    inventories=[groups(frame) for frame in frames]
    common=set(inventories[0]).intersection(*[set(x) for x in inventories[1:]])
    candidates=sorted((k for k in common if k[0]=='8c03a9ea' and all(len(g[k])==1 for g in inventories)),
                      key=lambda k:(-len(k[1]),repr(k)))
    require(len(candidates)==2,'expected two divided source sets')
    reports=[relative_motion([g[k][0] for g in inventories],[point for point,count in k[1]]) for k in candidates]
    for report in reports:
        report['calibrated_rigidity']=normalized_rigidity(report['affine_xyz_relative_transforms'],[614.7144309686947,565.5372185498106])
    difference=float(np.max(np.abs(np.array(reports[0]['affine_xyz_relative_transforms'])-
                                   np.array(reports[1]['affine_xyz_relative_transforms']))))
    return dict(candidates=reports,maximum_relative_transform_difference=difference,
                source_point_counts=[len(k[1]) for k in candidates],world_identity_proven=False)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('ledgers',type=Path,nargs=3)
    p.add_argument('--summary',action='store_true')
    p.add_argument('--divided-motion',action='store_true')
    a=p.parse_args();frames=[ancestry(decode(path.read_bytes()),True)['transform_inputs'] for path in a.ledgers]
    result=compare(frames)
    if a.divided_motion:result['divided_motion']=divided_motion(frames)
    if a.summary:
        rows=result.pop('common_source_sets')
        result.update(common_source_sets=len(rows),ambiguous_source_sets=sum(not r['unambiguous_matrix_per_frame'] for r in rows),
                      largest_candidates=sorted(rows,key=lambda r:r['unique_source_points'],reverse=True)[:3])
    print(json.dumps(result,sort_keys=True))
