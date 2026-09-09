"""Diagnostic embedding of witnessed view coordinates, not recovered world space."""
import copy
import math
import numpy as np
from binary32_oracle import fraction
from transform_store_inspect import require


def embed(mesh, scene, reference):
    require(mesh.get('coordinate_space')=='calibrated-camera-relative','view mesh required')
    require(mesh['frame_id']==scene['frame_id']==reference['frame_id']
            and mesh['game_id']==scene['game_id']==reference['game_id'],'source frame/game')
    require(reference['renderable_by_remix_adapter'] is False
            and reference['camera']['accepted_game_camera'] is False,'diagnostic reference')
    vertices=mesh['vertices'];triangles=mesh['triangles']
    require(0<len(vertices)<=65536 and 0<len(triangles)<=262144//3,'geometry bound')
    require(len({v['source_vertex'] for v in vertices})==len(vertices),'unique vertices')
    c=reference['camera']; axes=np.asarray([c[k] for k in ('right','up','forward')],dtype=float)
    origin=np.asarray(c['position'],dtype=float)
    require(axes.shape==(3,3) and origin.shape==(3,) and np.isfinite(axes).all()
            and np.isfinite(origin).all(),'finite camera')
    require(np.max(np.abs(axes@axes.T-np.eye(3)))<1e-5
            and abs(np.linalg.det(axes)-1)<1e-5,'proper camera basis')
    require(math.isfinite(c['fovY']) and 0<c['fovY']<180
            and math.isfinite(c['aspect']) and c['aspect']>0,'camera lens')
    tan=math.tan(math.radians(c['fovY'])/2)
    inverse=np.linalg.inv(axes)
    points=[];maximum=0.
    for v in vertices:
        index=v['source_vertex']
        require(type(index) is int and 0<=index<len(scene['vertices'])
                and v['original_vertex']==scene['vertices'][index],'source vertex identity')
        view=np.asarray(v['position'],dtype=float)
        require(view.shape==(3,) and np.isfinite(view).all() and view[2]>0,'positive view geometry')
        position=origin+inverse@view
        reprojection=axes@(position-origin)
        actual=[320+320*reprojection[0]/(reprojection[2]*tan*c['aspect']),
                240-240*reprojection[1]/(reprojection[2]*tan)]
        expected=[float(fraction(w)) for w in v['original_vertex'][:2]]
        error=max(abs(x-y) for x,y in zip(actual,expected))
        require(math.isfinite(error) and error<.001,'source pixel reprojection')
        maximum=max(maximum,error)
        points.append(dict(v,position=position.tolist(),view_position=view.tolist()))
    result=copy.deepcopy(mesh)
    result.update(vertices=points,coordinate_space='diagnostic-camera-embedded-anchor',
                  strict_reprojection_pass=False,
                  git_sha=scene['git_sha'],fixed_origin=reference['fixed_origin'],
                  harness_camera=copy.deepcopy(c),maximum_embedding_pixel_error=maximum,
                  embedding_provenance=dict(source_coordinate_space=mesh['coordinate_space'],
                      source_git_sha=scene['git_sha'],reference_git_sha=reference['git_sha'],
                      recovered_world_transform=False),renderable_by_remix_adapter=False)
    result['omissions']=list(mesh['omissions'])+['diagnostic camera embedding is not world reconstruction']
    # build_frame negates source Y but retains original strip topology. Account
    # for that reflection here; the subsequent proper basis rotation adds none.
    result['triangles']=[dict(t,vertices=[t['vertices'][0],t['vertices'][2],t['vertices'][1]]) for t in triangles]
    result['winding_reversed']=True
    return result
