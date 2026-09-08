"""Selected source-basis continuity with wrong-frame projection diagnostics."""
import numpy as np
from binary32_oracle import fraction
from transform_store_inspect import require


def anchored_camera(rows,calibration):
    require(0<len(rows)<=4096,'anchor sample bound')
    require(len(calibration)==2 and all(np.isfinite(v) and v>0 for v in calibration),'anchor calibration')
    matrices=[]
    for words in {tuple(r['matrix_words']) for r in rows}:
        require(len(words)==16,'anchor matrix shape')
        m=np.array([float(fraction(w)) for w in words]).reshape(4,4).T
        require(tuple(m[3]) in ((0,0,0,0),(0,0,0,1)),'anchor fourth row')
        matrices.append(m[:3])
    require(all(np.array_equal(m,matrices[0]) for m in matrices),'inconsistent anchor XYZ matrices')
    composite=np.eye(4);composite[:3]=matrices[0]
    k=np.diag([*calibration,1.,1.]);view=np.linalg.solve(k,composite)
    rotation=view[:3,:3]
    residual=float(np.max(np.abs(rotation@rotation.T-np.eye(3))))
    require(residual<1e-6 and np.linalg.det(rotation)>0,'anchor not rigid at supplied calibration')
    pose=np.linalg.inv(view)
    require(np.max(np.abs(view@pose-np.eye(4)))<1e-9,'anchor inverse mismatch')
    require(np.max(np.abs(k@view-composite))<1e-9,'anchor decomposition mismatch')
    return dict(source_to_normalized_view=view.tolist(),normalized_view_to_source=pose.tolist(),
                camera_origin_in_source=pose[:3,3].tolist(),orthogonality_residual=residual,
                axes='captured normalized X/Y/Z; positive Y projects down',
                source_units='unknown physical scale',static_arena_semantics_proven=False,
                remix_coordinate_conversion_proven=False)


def anchor_mesh(mesh,camera):
    require(mesh['schema']=='flycast-expression-evidence-mesh-v1','anchor mesh schema')
    pose=np.asarray(camera['normalized_view_to_source']);view=np.asarray(camera['source_to_normalized_view'])
    require(pose.shape==view.shape==(4,4) and np.isfinite(pose).all() and np.isfinite(view).all(),'anchor pose shape')
    require(np.max(np.abs(view@pose-np.eye(4)))<1e-9,'anchor mesh inverse')
    points=[];error=0.
    for vertex in mesh['vertices']:
        x,y,z=vertex['position']
        # Evidence mesh is Y-up; captured normalized view used by anchor is Y-down.
        original=np.array([x,-y,z,1.]);source=pose@original;back=view@source
        error=max(error,float(np.max(np.abs(back-original))))
        points.append(dict(vertex,position=source[:3].tolist()))
    require(error<1e-9,'anchor mesh roundtrip')
    return dict(mesh,coordinate_space='selected-source-anchor',vertices=points,
                anchored_camera=camera,maximum_anchor_roundtrip_error=error,
                renderable_by_remix_adapter=False)


def project(row,matrix_words=None):
    words=row['matrix_words'] if matrix_words is None else matrix_words
    matrix=np.array([float(fraction(w)) for w in words]).reshape(4,4).T
    point=np.array([float(fraction(w)) for w in row['point_words']])
    p=matrix@point
    require(np.isfinite(p).all() and p[2]>0,'candidate projection depth')
    return p[:2]/p[2]+[320,240]


def compare(frames):
    require(len(frames)==3,'three selected frames')
    maps=[]
    for ordinal,rows in zip((1781,1782,1783),frames):
        require(0<len(rows)<=4096 and all(r['ordinal']==ordinal for r in rows),'selected frame identity')
        mapping={(r['draw'],r['vertex']):r for r in rows}
        require(len(mapping)==len(rows),'duplicate selected source')
        maps.append(mapping)
    require(set(maps[0])==set(maps[1])==set(maps[2]),'selected source topology')
    changed=[];errors=[]
    for key,first in maps[0].items():
        if any(m[key]['point_words']!=first['point_words'] for m in maps[1:]):changed.append(key)
        for mapping in maps[1:]:
            row=mapping[key]
            errors.append(float(np.max(np.abs(project(row)-project(row,first['matrix_words'])))))
    return dict(selected_vertices=len(maps[0]),source_word_changes=changed,
                wrong_first_frame_matrix_maximum_pixel_error=max(errors),
                wrong_first_frame_matrix_errors_over_threshold=sum(e>=.001 for e in errors),
                tested_later_frame_vertices=len(errors),world_camera_proven=False,
                comparison='mathematical XYZ projection; recorded rounding checked separately')
