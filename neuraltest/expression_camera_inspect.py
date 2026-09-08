"""Extract pre-division coordinates from verified expressions; no world camera."""
import math
from binary32_oracle import fraction
from ordered_float_expression import evaluate
from transform_store_inspect import require


def outside_common_plane(points, rectangle=(0.,0.,640.,480.)):
    """Sufficient rejection only; False does not establish visible coverage."""
    require(len(points)==3 and all(len(p)==2 and all(math.isfinite(v) for v in p) for p in points),'triangle shape')
    left,top,right,bottom=rectangle
    require(left<right and top<bottom,'rectangle shape')
    return (all(p[0]<left for p in points) or all(p[0]>right for p in points)
            or all(p[1]<top for p in points) or all(p[1]>bottom for p in points))


def recover(nodes, calibration):
    require(len(nodes)==3 and len(calibration)==2 and all(math.isfinite(v) and v>0 for v in calibration),'camera contract shape')
    x,y,depth=nodes
    require(x[0]==y[0]=='fadd' and x[1][0]==y[1][0]=='fmul','projection expression shape')
    reciprocal=x[1][2]
    require(reciprocal==y[1][2] and reciprocal[0]=='fdiv' and evaluate(reciprocal[1])==0x3f800000,'shared unit reciprocal')
    require(evaluate(x[2])==0x43a00000 and evaluate(y[2])==0x43700000,'observed screen center')
    if depth==reciprocal:scale=1.
    else:
        require(depth[0]=='fmul' and depth[1]==reciprocal,'depth expression')
        scale=float(fraction(evaluate(depth[2])))
    pre=[float(fraction(evaluate(n))) for n in (x[1][1],y[1][1],reciprocal[2])]
    require(all(math.isfinite(v) for v in pre) and pre[2]>0,'unsupported preprojection depth')
    sx,sy=calibration
    view=[pre[0]/sx,-pre[1]/sy,pre[2]]
    projected=[320+sx*view[0]/view[2],240-sy*view[1]/view[2]]
    observed=[float(fraction(evaluate(n))) for n in nodes[:2]]
    return dict(position=view,predivision_position=pre,observed_screen_position=observed,
                mathematical_screen_position=projected,
                maximum_reprojection_error_pixels=max(abs(a-b) for a,b in zip(projected,observed)),
                observed_depth_scale=scale,coordinate_space='calibrated-camera-relative',world_camera_recovered=False)
