"""Separate float input quantization from actual C++ camera arithmetic."""
import math
import subprocess
import numpy as np
from transform_store_inspect import require


def rebase(mesh, fixed_origin):
    """Translate both geometry and camera by one caller-owned sequence origin."""
    offset=np.asarray(fixed_origin,dtype=float)
    require(offset.shape==(3,) and np.isfinite(offset).all(),'fixed origin')
    camera=dict(mesh['harness_camera'])
    camera['position']=(np.asarray(camera['position'])-offset).tolist()
    vertices=[dict(v,position=(np.asarray(v['position'])-offset).tolist()) for v in mesh['vertices']]
    return dict(mesh,harness_camera=camera,vertices=vertices,fixed_origin=offset.tolist(),
                renderable_by_remix_adapter=False)


def inspect(mesh, executable):
    require(mesh['coordinate_space']=='reflected-selected-source-anchor','converted mesh required')
    camera=mesh['harness_camera']
    points=np.asarray([v['position'] for v in mesh['vertices']],dtype=float)
    require(0<len(points)<=65536 and np.isfinite(points).all(),'point bound/finite')
    origin=np.asarray(camera['position'])
    axes=np.asarray([camera[k] for k in ('right','up','forward')])
    lens=np.asarray([camera['fovY'],camera['aspect']])
    def project(p,o,a,l):
        v=(p-o)@a.T
        require(np.all(v[:,2]>0),'positive depth')
        t=math.tan(math.radians(l[0])/2)
        return np.column_stack((320+320*v[:,0]/(v[:,2]*t*l[1]),240-240*v[:,1]/(v[:,2]*t)))
    def quantize(v):
        return v.astype(np.float32).astype(float)
    reference=project(points,origin,axes,lens)
    stages={
        'origin_float_only':project(points,quantize(origin),axes,lens),
        'axes_float_only':project(points,origin,quantize(axes),lens),
        'lens_float_only':project(points,origin,axes,quantize(lens)),
        'source_float_only':project(quantize(points),origin,axes,lens),
        'camera_float_only':project(points,quantize(origin),quantize(axes),quantize(lens)),
        'all_float_inputs_double_arithmetic':project(quantize(points),quantize(origin),quantize(axes),quantize(lens)),
    }
    values=[*origin,*axes.flatten(),*lens,len(points),*points.flatten()]
    output=subprocess.check_output([str(executable)],input=' '.join(map(str,values)),text=True,timeout=30)
    actual=np.asarray([[float(x) for x in row.split()] for row in output.splitlines()])
    require(actual.shape==(len(points),3) and np.isfinite(actual).all(),'C++ output shape')
    stages['actual_cpp_float']=actual[:,:2]*[640,480]
    result={}
    for name,screen in stages.items():
        errors=np.max(np.abs(screen-reference),axis=1)
        result[name]=dict(maximum_pixels=float(max(errors)),over_001=int(sum(errors>=.001)),worst_index=int(np.argmax(errors)))
    return dict(vertices=len(points),stages=result,renderable_by_remix_adapter=False,
                strict_captured_gate_unchanged=True)
