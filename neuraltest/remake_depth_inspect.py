"""Measure returned public depth against the supplied PVR view-space embedding.

This reports hypotheses, not NGX guidance acceptance or physical depth truth.
"""
import argparse
import json
import struct
from pathlib import Path
import numpy as np


def packet_projection(path):
    data = path.read_bytes()
    if len(data) > 72 * 1024 * 1024:
        raise ValueError("packet bound")
    offset = 0
    def read(fmt):
        nonlocal offset
        result = struct.unpack_from("<" + fmt, data, offset)
        offset += struct.calcsize("<" + fmt)
        return result
    def skip(n):
        nonlocal offset
        offset += n
        if offset > len(data):
            raise ValueError("truncated packet")
    if read("II") != (0x56524346, 1):
        raise ValueError("packet schema")
    frame, _, _, _ = read("QQQQ")
    for _ in range(3):
        skip(read("I")[0])
    _, _, near, far = read("ffff")
    for _ in range(read("I")[0]):
        skip(read("I")[0])
    ids = []
    for _ in range(read("I")[0]):
        ids.append(read("Q")[0])
        skip(40)
        skip(read("I")[0])
        skip(read("I")[0] * 36)
        skip(read("I")[0] * 4)
    if offset != len(data) or not 0 < near < far:
        raise ValueError("packet extent/projection")
    return frame, near, far, ids


def inspect(root, prefix, frame):
    folder = root / f"frame-{frame:06d}"
    returned = np.fromfile(str(prefix) + f".frame-{frame}.bmp.depth.rgba32f", dtype="<f4").reshape(480, 640, 4)
    native = np.fromfile(folder / "depth.f32", dtype="<f4").reshape(480, 640).astype(np.float64)
    raw = np.expm1(native * 34 * np.log(2)) / 100000
    expected = np.divide(.95, raw, out=np.full_like(raw, np.nan), where=raw > 0)
    # This is a candidate view-Z relation, not an assumed description of DEPTH.
    candidate = returned[:, :, 0].astype(np.float64)
    valid = np.isfinite(candidate) & (candidate > 0) & np.isfinite(expected) & (expected >= .1) & (expected <= 2501)
    channels = []
    for c in range(4):
        finite = returned[:, :, c][np.isfinite(returned[:, :, c])]
        channels.append({"finite": int(finite.size), "min": float(finite.min()) if finite.size else None,
                         "max": float(finite.max()) if finite.size else None})
    residual = np.abs(candidate[valid] - expected[valid])
    packet_frame, near, far, mesh_ids = packet_projection(folder / "remake-view.bin")
    if packet_frame != frame:
        raise ValueError("source frame mismatch")
    # Exact supplied D3D9 projection from remake_d3d9_scene.h, not a fitted model.
    ndc_expected = far / (far - near) - near * far / (far - near) / expected
    draw_ids = np.fromfile(folder / "draw-id.r16u", dtype="<u2").reshape(480, 640)
    overlap = valid & np.isin(draw_ids, mesh_ids)
    ndc_error = np.abs(candidate[overlap] - ndc_expected[overlap])
    reversed_error = np.abs(1 - candidate[overlap] - ndc_expected[overlap])
    return {"frame": frame, "channels": channels, "candidate_pixels": int(valid.sum()),
            "emitted_opaque_overlap_pixels": int(overlap.sum()),
            "supplied_projection_ndc_error_percentiles": np.percentile(ndc_error, [50, 90, 99]).tolist() if ndc_error.size else [],
            "wrong_reversed_ndc_error_percentiles": np.percentile(reversed_error, [50, 90, 99]).tolist() if reversed_error.size else [],
            "view_z_absolute_error_percentiles": np.percentile(residual, [50, 90, 99]).tolist() if residual.size else [],
            "view_z_ratio_percentiles": np.percentile(candidate[valid] / expected[valid], [10, 50, 90]).tolist() if residual.size else [],
            "guidance_semantics_proven": False, "note": "includes omitted/different surfaces; no fitted correction or acceptance"}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    parser.add_argument("prefix", type=Path)
    args = parser.parse_args()
    print(json.dumps([inspect(args.root, args.prefix, f) for f in range(1782, 1785)], indent=2, allow_nan=False))
