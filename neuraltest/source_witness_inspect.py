"""Explore captured coordinate relations; never grants reconstruction acceptance."""
import argparse
import json
from pathlib import Path

import numpy as np


def floats(bits):
    return np.asarray(bits, dtype=np.uint32).view(np.float32).astype(np.float64)


def load_frame(path):
    data = json.loads(path.read_text(encoding="utf-8"))
    if data["schema"] != 1 or data["scope"] != "observed-dependency-not-reconstruction":
        raise ValueError("unsupported witness scope")
    if not data["producer"][0] or not data["producer"][1]:
        raise ValueError("missing producer identity")
    transforms = {row["serial"]: row for row in data["transforms"]}
    if len(transforms) != len(data["transforms"]):
        raise ValueError("duplicate transform identity")
    output, projected = [], []
    for row in data["vertices"]:
        origins = row["origins"]
        if len(origins) != 3 or origins[0] is None or len(set(origins)) != 1:
            continue
        output.append(floats(transforms[origins[0]]["output_bits"]))
        projected.append(floats(row["xyz_bits"]))
    return data["frame"], np.asarray(output), np.asarray(projected)


def inspect(root):
    frames = [load_frame(path) for path in sorted(root.glob("frame-*/pvr-source-witness.json"))]
    if len(frames) < 2:
        raise ValueError("moving cross-frame witnesses required")
    report = {"scope": "empirical-relation-only", "reconstruction_accepted": False, "models": {}}
    report["fixed_float32_candidates"] = []
    for frame, source, target in frames:
        source, target = source.astype(np.float32), target.astype(np.float32)
        z = source[:, 2]
        inv = np.float32(1) / z
        candidates = {
            "reciprocal_then_multiply": np.column_stack((source[:, 0]*inv+np.float32(320), source[:, 1]*inv+np.float32(240), np.float32(0.95)*inv)),
            "direct_division": np.column_stack((source[:, 0]/z+np.float32(320), source[:, 1]/z+np.float32(240), np.float32(0.95)/z)),
        }
        for name, prediction in candidates.items():
            report["fixed_float32_candidates"].append({"frame": frame, "name": name,
                "vertices": len(target), "exact_xyz": (prediction.view(np.uint32)==target.view(np.uint32)).sum(axis=0).tolist(),
                "max_abs_xyz": np.max(np.abs(prediction.astype(float)-target),axis=0).tolist()})
    matrix_groups = {}
    for path in sorted(root.glob("frame-*/pvr-source-witness.json")):
        data = json.loads(path.read_text(encoding="utf-8"))
        for transform in data["transforms"]:
            matrix = floats(transform["matrix_bits"]).reshape((4,4),order="F")
            vector, output = floats(transform["input_bits"]), floats(transform["output_bits"])
            norms = np.linalg.norm(matrix[:3,:3],axis=1)
            if not np.isfinite(matrix).all() or np.any(norms==0):
                continue
            normalized = matrix[:3,:3]/norms[:,None]
            row = [norms[0]/norms[2],norms[1]/norms[2],np.max(np.abs(normalized@normalized.T-np.eye(3))),
                   np.max(np.abs(matrix@vector-output)),output[3]]
            matrix_groups.setdefault("%08x"%transform["pc"],[]).append(row)
    report["matrix_groups"] = {pc:{"observations":len(rows),"columns":["row0_row2_norm_ratio","row1_row2_norm_ratio","row_orthogonality_error","matrix_vector_max_error","output_w"],
                                   "min":np.min(rows,axis=0).tolist(),"median":np.median(rows,axis=0).tolist(),"max":np.max(rows,axis=0).tolist()}
                               for pc,rows in matrix_groups.items()}
    for group in report["matrix_groups"].values():
        fx, fy = group["median"][:2]
        group["candidate_at_640x480"] = {"vertical_fov_degrees": float(np.degrees(2*np.arctan(240/fy))),
                                         "projection_aspect": float((640/480)*fy/fx),
                                         "assumption": "orthogonal uniformly scaled source basis; not world-space proof"}
    for name, divisor in (("divide_by_z", 2), ("divide_by_w", 3)):
        prepared = []
        for frame, source, target in frames:
            if not len(source):
                raise ValueError("no complete common-origin vertices")
            valid = np.isfinite(source).all(axis=1) & np.isfinite(target).all(axis=1) & (np.abs(source[:, divisor]) > 1e-12)
            source, target = source[valid], target[valid]
            denominator = source[:, divisor]
            basis = np.column_stack((source[:, 0] / denominator, source[:, 1] / denominator, 1 / denominator, np.ones(len(source))))
            prepared.append((frame, basis, target))
        coefficients, _, rank, _ = np.linalg.lstsq(prepared[0][1], prepared[0][2], rcond=None)
        rows = []
        for frame, basis, target in prepared:
            error = basis @ coefficients - target
            wrong = np.roll(basis, 1, axis=0) @ coefficients - target
            rows.append({"frame": frame, "vertices": len(target), "rms_xyz": np.sqrt(np.mean(error**2, axis=0)).tolist(),
                         "max_abs_xyz": np.max(np.abs(error), axis=0).tolist(),
                         "wrong_row_rms_xyz": np.sqrt(np.mean(wrong**2, axis=0)).tolist()})
        report["models"][name] = {"fit_frame": prepared[0][0], "rank": int(rank), "coefficients": coefficients.tolist(), "frames": rows}
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    print(json.dumps(inspect(parser.parse_args().root), indent=2, allow_nan=False))
