"""Explicit source and view reflection; no runtime or complete-scene promotion."""
import math
import numpy as np
from transform_store_inspect import require


def convert(mesh,calibration):
    require(mesh['coordinate_space']=='selected-source-anchor','source anchor required')
    sx,sy=calibration
    require(all(math.isfinite(v) and v>0 for v in calibration),'orientation calibration')
    reflection=np.diag([1.,-1.,1.,1.])
    view=reflection@np.asarray(mesh['anchored_camera']['source_to_normalized_view'])@reflection
    require(np.linalg.det(view[:3,:3])>0,'reflected camera basis')
    pose=np.linalg.inv(view);origin=pose[:3,3]
    points=[dict(v,position=(reflection@np.array([*v['position'],1.]))[:3].tolist()) for v in mesh['vertices']]
    triangles=[dict(t,vertices=[t['vertices'][0],t['vertices'][2],t['vertices'][1]]) for t in mesh['triangles']]
    error=0.
    original_view=np.asarray(mesh['anchored_camera']['source_to_normalized_view'])
    for original,converted in zip(mesh['vertices'],points):
        p=original_view@np.array([*original['position'],1.])
        q=view[:3,:3]@(np.array(converted['position'])-origin)
        require(p[2]>0 and q[2]>0,'converted depth')
        expected=np.array([320+sx*p[0]/p[2],240+sy*p[1]/p[2]])
        actual=np.array([320+sx*q[0]/q[2],240-sy*q[1]/q[2]])
        error=max(error,float(np.max(np.abs(actual-expected))))
    require(error<1e-9,'converted projection mismatch')
    camera=dict(position=origin.tolist(),right=view[0,:3].tolist(),up=view[1,:3].tolist(),forward=view[2,:3].tolist(),
                fovY=math.degrees(2*math.atan(240/sy)),aspect=(640/480)*(sy/sx),near_far='not inferred')
    return dict(mesh,vertices=points,triangles=triangles,coordinate_space='reflected-selected-source-anchor',
                harness_camera=camera,maximum_conversion_pixel_error=error,
                winding_reversed=True,renderable_by_remix_adapter=False)
