"""Rigid calibration witnesses plus explicit general-affine contributions.

This does not establish object semantics or a unique physical/world camera.
"""
import numpy as np
from camera_calibration_inspect import analyze
from transform_semantics_inspect import factor
from transform_store_inspect import require


def inspect(matrices,calibration=None):
    require(1<=len(matrices)<=19341,'matrix budget')
    witnesses=[];affine=[];residuals=[]
    for index,value in enumerate(matrices):
        matrix=np.asarray(value,dtype=float)
        require(matrix.shape==(4,4) and np.isfinite(matrix).all()
                and np.array_equal(matrix[3],[0,0,0,1]),'affine matrix shape')
        scales=np.linalg.norm(matrix[:3,:3],axis=1)
        require(np.all(scales>0) and abs(np.linalg.det(matrix[:3,:3]))>1e-12,'degenerate affine matrix')
        try:factor(matrix)
        except ValueError:
            affine.append(index)
            axes=matrix[:3,:3]/scales[:,None]
            residuals.append(float(np.max(np.abs(axes@axes.T-np.eye(3)))))
        else:witnesses.append(index)
    require(witnesses,'no rigid calibration witness')
    contract=analyze([matrices[i] for i in witnesses],calibration,matrix_limit=19341)
    k=np.diag([*contract['normalized_calibration'],1,1])
    error=max(float(np.max(np.abs(k@np.linalg.solve(k,m)-m))) for m in matrices)
    require(error<1e-9,'affine decomposition mismatch')
    contract.update(total_contributions=len(matrices),rigid_witnesses=len(witnesses),
                    general_affine_contributions=len(affine),general_affine_indices=affine,
                    maximum_general_affine_orthogonality_residual=max(residuals,default=0),
                    maximum_affine_decomposition_error=error,
                    object_deformation_semantics_proven=False,
                    calibration_from_all_contributions=False)
    return contract
