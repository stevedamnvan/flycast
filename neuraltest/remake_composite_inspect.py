"""Inspect diagnostic returned-image composition; never certifies presentation."""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image


def inspect(root):
    receipt = json.loads((root / "remake-return.json").read_text())
    proof = json.loads((root / "remake-composite-native-proof.json").read_text())
    native = np.asarray(Image.open(root / "native-pvr-color.png").convert("RGBA"))
    mask = np.asarray(Image.open(root / "overlay-classification.png").convert("RGBA"))[:, :, 0] >= 128
    result = np.asarray(Image.open(root / "remake-protected-composite.png").convert("RGBA"))
    returned = np.frombuffer((root / "remake-return.bgra").read_bytes(), dtype=np.uint8).reshape(480, 640, 4)[:, :, [2, 1, 0, 3]]
    if native.shape != returned.shape or result.shape != returned.shape or mask.shape != (480, 640):
        raise ValueError("dimensions do not describe the exact content rectangle")
    protected_mismatch = int(np.count_nonzero(np.any(result[mask] != native[mask], axis=1)))
    world_mismatch = int(np.count_nonzero(np.any(result[~mask] != returned[~mask], axis=1)))
    wrong_no_overlay = int(np.count_nonzero(np.any(returned[mask] != native[mask], axis=1)))
    wrong_native = int(np.count_nonzero(np.any(native[~mask] != returned[~mask], axis=1)))
    return {"frame": receipt["frame"], "protected_pixels": int(mask.sum()),
            "protected_mismatch": protected_mismatch, "returned_scene_mismatch": world_mismatch,
            "wrong_no_overlay_mismatch": wrong_no_overlay, "wrong_native_only_mismatch": wrong_native,
            "native_targets_unchanged": proof["native_targets_unchanged"],
            "passed": bool(proof["native_targets_unchanged"] is True and proof["frame"] == receipt["frame"]
                           and mask.any() and (~mask).any() and not protected_mismatch and not world_mismatch
                           and wrong_no_overlay and wrong_native), "presentation_proven": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    args = parser.parse_args()
    rows = [inspect(path) for path in sorted(args.root.glob("frame-*")) if path.is_dir()]
    print(json.dumps(rows, indent=2))
    raise SystemExit(0 if rows and all(row["passed"] for row in rows) else 1)
