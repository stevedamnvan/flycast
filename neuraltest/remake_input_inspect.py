"""Check the reset-only returned scene at the real neural GPU input boundary."""
import argparse
import json
from pathlib import Path
import numpy as np
from PIL import Image


def inspect(folder):
    metadata = json.loads((folder / "manifest.json").read_text())
    rgba = np.fromfile(folder / "remake-return.bgra", dtype=np.uint8).reshape(480, 640, 4)[:, :, [2, 1, 0, 3]]
    source = np.asarray(Image.open(folder / "source-color.png").convert("RGBA"))
    native = np.asarray(Image.open(folder / "native-pvr-color.png").convert("RGBA"))
    returned_depth = np.fromfile(folder / "remake-return-depth.f32", dtype="<f4")
    depth = np.fromfile(folder / "depth.f32", dtype="<f4")
    motion = np.fromfile(folder / "motion.rg16f", dtype="<f2")
    bias = np.asarray(Image.open(folder / "bias-mask.png").convert("RGBA"))[:, :, 0]
    confidence = np.asarray(Image.open(folder / "confidence.png").convert("RGBA"))[:, :, 0]
    ids = np.fromfile(folder / "draw-id.r16u", dtype="<u2")
    public = np.asarray(Image.open(folder / "public-dlaa-output.png").convert("RGBA"))
    final = np.asarray(Image.open(folder / "final-composited.png").convert("RGBA"))
    overlay = np.asarray(Image.open(folder / "overlay-classification.png").convert("RGBA"))[:, :, 0] >= 128
    checks = {
        "source_exact_returned": bool(np.array_equal(source, rgba)),
        "depth_exact_inverted_returned": bool(np.array_equal(depth, np.float32(1) - returned_depth)),
        "motion_zero": bool(np.all(motion == 0)), "bias_full": bool(np.all(bias == 255)),
        "confidence_zero": bool(np.all(confidence == 0)), "identity_zero": bool(np.all(ids == 0)),
        "source_differs_from_native": bool(np.any(source != native)),
        "explicit_contract": metadata["remake_input"] == "returned-scene-reset-only-inverted-projection-experiment",
        "reset": metadata["reset_history"] is True,
        "evaluation_accepted": metadata["evaluation_accepted"] is True,
        "public_output_exists": (folder / "public-dlaa-output.png").is_file(),
        "protected_final_exact_native": bool(np.array_equal(final[overlay], native[overlay])),
        "world_final_exact_public": bool(np.array_equal(final[~overlay], public[~overlay])),
    }
    return {"frame": folder.name, "checks": checks, "passed": all(checks.values()),
            "combined_dlss5_proven": False, "temporal_quality_proven": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    args = parser.parse_args()
    rows = [inspect(folder) for folder in sorted(args.root.glob("frame-*")) if folder.is_dir()]
    print(json.dumps(rows, indent=2))
    raise SystemExit(0 if rows and all(row["passed"] for row in rows) else 1)
