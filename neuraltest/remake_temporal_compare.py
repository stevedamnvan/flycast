#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Fail-closed frozen-source comparison; never declares a perceptual winner."""
import argparse
import hashlib
import json
from pathlib import Path
import re

import numpy as np
from PIL import Image, ImageDraw


def require(condition, detail):
    if not condition:
        raise ValueError(detail)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rgba(path):
    with Image.open(path) as image:
        result = np.asarray(image.convert("RGBA"))
    require(result.shape == (480, 640, 4), (path, result.shape))
    return result


def captures(root):
    result = {}
    for path in root.glob("frame-*/preview.json"):
        record = json.loads(path.read_text())
        frame = record["source_frame"]
        require(frame not in result, (root, "duplicate source", frame))
        result[frame] = (path.parent, record)
    return result


def validate_log(path, first, last, temporal):
    text = path.read_text(errors="replace")
    rows = [(int(f), int(reset)) for f, reset in re.findall(
        r"Remake async neural evaluation: source=(\d+).*?accepted=1 reset=(\d+)", text)]
    rows = [row for row in rows if row[0] <= last]
    expected = [(f, int(not temporal or f == first)) for f in range(first, last + 1)]
    require(rows == expected, (path, "accepted/reset sequence", rows))
    presents = set((int(f), int(current)) for f, current in re.findall(
        r"Remake preview present: source=(\d+) current=(\d+) kind=remake-evaluated completed=1 hresult=0", text))
    return presents


def metrics(output, source):
    difference = output - source
    gradients = [np.mean(np.abs(np.diff(output, axis=axis) - np.diff(source, axis=axis))) for axis in (1, 2)]
    dark = np.max(source, axis=-1) <= 16
    return {
        "source_rgb_mae": float(np.mean(np.abs(difference))),
        "gradient_rgb_mae": float(np.mean(gradients)),
        "mean_channel_drift": np.mean(difference, axis=(0, 1, 2)).tolist(),
        "saturation_range_drift": float(np.mean(np.ptp(output, axis=-1) - np.ptp(source, axis=-1))),
        "source_black_rgb_drift": float(np.mean(difference[dark])) if dark.any() else None,
        "temporal_rgb_delta": float(np.mean(np.abs(np.diff(output, axis=0)))),
        "source_relative_temporal_residual": float(np.mean(np.abs(np.diff(output, axis=0) - np.diff(source, axis=0)))),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("reset", "temporal", "reset-log", "temporal-log", "out"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--first-evaluation", type=int, required=True)
    parser.add_argument("--first-capture", type=int, required=True)
    parser.add_argument("--count", type=int, required=True)
    parser.add_argument("--reset-label", default="Remix + NR: reset each frame")
    parser.add_argument("--temporal-label", default="Remix + NR: geometry history")
    args = parser.parse_args()
    require(2 <= args.count <= 30 and args.out.is_absolute() and not args.out.exists(), "invalid count/output target")
    frames = list(range(args.first_capture, args.first_capture + args.count))
    lanes = [captures(args.reset), captures(args.temporal)]
    require(all(sorted(lane) == frames for lane in lanes), "capture sequence differs")
    presents = [validate_log(args.reset_log, args.first_evaluation, frames[-1], False),
                validate_log(args.temporal_log, args.first_evaluation, frames[-1], True)]
    inputs = ("remake-view.bin", "remake-return.bgra", "remake-return-depth.f32",
              "native-effect-identity.bin", "native-alpha-exclusions.bin",
              "original-native.png", "original-overlay-mask.png")
    report = {"frames": frames, "source_hashes": {}, "external_output_provenance": "not established by this comparison",
              "lane_labels": {"reset": args.reset_label, "temporal": args.temporal_label},
              "performance_eligible": False, "winner": None,
              "scope": "same frozen source, different reconstruction guidance/history; not an identical-NGX-input settings sweep"}
    sources, outputs, previews = [], [[], []], []
    changed = 0
    for frame in frames:
        paths = [lane[frame][0] for lane in lanes]
        report["source_hashes"][frame] = {}
        for name in inputs:
            hashes = [digest(path / name) for path in paths]
            require(hashes[0] == hashes[1], (frame, "source mismatch", name))
            report["source_hashes"][frame][name] = hashes[0]
        source = rgba(paths[0] / "returned-remix.png")
        sources.append(source[:, :, :3])
        finals = []
        for lane_index, path in enumerate(paths):
            record = lanes[lane_index][frame][1]
            require(record["replay_original_frame"] == frame, (frame, "original frame mismatch"))
            require((frame, record["current_frame"]) in presents[lane_index], (frame, "missing Present"))
            native = rgba(path / "original-native.png")
            mask = rgba(path / "original-overlay-mask.png")[:, :, 0] >= 128
            final = rgba(path / "composited-remix.png")
            evaluated = rgba(path / "evaluated-remix.png")
            require(mask.any() and (~mask).any(), (frame, "empty HUD or world"))
            require(np.array_equal(final, np.where(mask[:, :, None], native, evaluated)), (frame, "composition mismatch"))
            require(np.array_equal(final[:, :, :3], rgba(path / "flycast-pre-osd-backbuffer.png")[:, :, :3]), (frame, "backbuffer mismatch"))
            outputs[lane_index].append(rgba(path / "neural-before-native-effects.png")[:, :, :3])
            finals.append(final)
        changed += not np.array_equal(outputs[0][-1], outputs[1][-1])
        panel = Image.new("RGB", (1920, 504), "#181818")
        draw = ImageDraw.Draw(panel)
        for x, (label, picture) in enumerate(zip(("Native PVR", args.reset_label, args.temporal_label),
                                                (rgba(paths[0] / "original-native.png"), *finals))):
            draw.text((x * 640 + 8, 5), f"{label} | source {frame}", fill="white")
            panel.paste(Image.fromarray(picture), (x * 640, 24))
        previews.append(panel)
    source = np.asarray(sources, dtype=np.float32)
    report["metrics_before_native_effects"] = {name: metrics(np.asarray(output, dtype=np.float32), source)
                                              for name, output in zip(("reset", "temporal"), outputs)}
    report["changed_neural_frames"] = changed
    report["hud_mismatch_pixels"] = 0
    report["log_sha256"] = {"reset": digest(args.reset_log), "temporal": digest(args.temporal_log)}
    report["visual_note"] = "GIF is a quantized 5x-slow preview; original capture PNGs remain authoritative. Metrics are components, not a quality verdict."
    args.out.mkdir(parents=True, exist_ok=False)
    (args.out / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    previews[0].save(args.out / "moving-comparison-slow.gif", save_all=True, append_images=previews[1:], duration=80, loop=0, optimize=False)
    previews[len(previews) // 2].save(args.out / "comparison-midpoint.png")
    print(json.dumps({key: report[key] for key in ("frames", "changed_neural_frames", "metrics_before_native_effects", "winner")}, indent=2))


if __name__ == "__main__":
    main()
