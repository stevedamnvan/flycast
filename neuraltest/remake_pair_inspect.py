"""Verify archived paired return bytes against same-frame consumer readbacks.

This checks transport only, not guidance semantics or presentation.
"""
import argparse
import json
import struct
from pathlib import Path


def inspect(root, prefix):
    rows = []
    for folder in sorted(root.glob("frame-*")):
        receipt = json.loads((folder / "remake-return.json").read_text())
        frame = receipt["frame"]
        bmp_path = Path(str(prefix) + f".frame-{frame}.bmp")
        bmp = bmp_path.read_bytes()
        offset = struct.unpack_from("<I", bmp, 10)[0]
        width, height = struct.unpack_from("<ii", bmp, 18)
        if bmp[:2] != b"BM" or (width, abs(height)) != (640, 480):
            raise ValueError("unexpected consumer bitmap")
        pixels = bmp[offset:]
        if height > 0:
            pixels = b"".join(pixels[y*2560:(y+1)*2560] for y in reversed(range(480)))
        rgba = Path(str(bmp_path) + ".depth.rgba32f").read_bytes()
        if len(rgba) != 640 * 480 * 16:
            raise ValueError("unexpected consumer depth extent")
        red = b"".join(rgba[i:i+4] for i in range(0, len(rgba), 16))
        color = (folder / "remake-return.bgra").read_bytes()
        depth = (folder / "remake-return-depth.f32").read_bytes()
        passed = (len(color) == 640*480*4 and color == pixels and depth == red
                  and receipt["depth_values"] == 640*480
                  and receipt["prepared_before_composite"] is True)
        rows.append({"frame": frame, "color_exact": color == pixels,
                     "depth_exact": depth == red, "passed": passed,
                     "presentation_proven": False})
    if not rows:
        raise ValueError("no paired captures")
    return rows


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    parser.add_argument("prefix", type=Path)
    args = parser.parse_args()
    rows = inspect(args.root, args.prefix)
    print(json.dumps(rows, indent=2))
    raise SystemExit(0 if all(row["passed"] for row in rows) else 1)
