"""Package existing offline preview PNGs; never labels them Remix/DLSS output."""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageDraw

p = argparse.ArgumentParser()
p.add_argument("preview", type=Path)
a = p.parse_args()
meta = json.loads((a.preview / "preview.json").read_text())
assert meta["remix"] is False and meta["dlss5"] is False
frames = []
for frame in range(meta["first_frame"], meta["first_frame"] + meta["frames"]):
    folder = a.preview / f"frame-{frame:06d}"
    canvas = Image.new("RGB", (1280, 352), "#171b21")
    draw = ImageDraw.Draw(canvas)
    draw.text((12, 9), "NATIVE PVR - matched source", fill="white")
    draw.text((652, 9), "CAMERA-RELATIVE LIGHTING APPROXIMATION - NOT Remix / DLSS 5", fill="#ffcf80")
    for x, name in [(0, "native.png"), (640, "approximation.png")]:
        with Image.open(folder / name) as image:
            assert image.size == (640, 480)
            canvas.paste(image.crop((0, 144, 640, 432)).convert("RGB"), (x, 36))
    draw.text((12, 333), f"Frame {frame} | world crop | assumed FOV {meta['assumed_fov_degrees']} deg | face normals + baked source color | no path tracing", fill="white")
    frames.append(canvas)
assert frames
target = a.preview / "moving-comparison.gif"
if target.exists():
    raise SystemExit("Output already exists; refusing to replace it")
# Captures are consecutive emulated 60 Hz frames. Also expose slowed scrutiny.
# GIF uses centiseconds: 20/10/20 ms totals 50 ms per three frames (60 Hz).
frames[0].save(target, save_all=True, append_images=frames[1:],
               duration=[(20, 10, 20)[i % 3] for i in range(len(frames))], loop=0)
frames[0].save(a.preview / "moving-comparison-slow.gif", save_all=True,
               append_images=frames[1:], duration=100, loop=0)
for index in sorted({0, len(frames)//2, len(frames)-1}):
    frames[index].save(a.preview / f"comparison-{index:02d}.png")
print(f"Wrote {len(frames)} matched frames; 60 Hz average and 100 ms playback; approximation only")
