"""Check ordinary-feed receipts and retained images; never certifies presentation."""
import argparse
import json
import re
from pathlib import Path

import numpy as np
from PIL import Image


def inspect(publisher, consumer, prefix, require_overlays=False):
    published = {}
    for match in re.finditer(r"Remake async publish: frame=(\d+) producer=(\d+) sequence=(\d+) bytes=(\d+) digest=(\d+) capture=false wait=false presentation=false", publisher):
        frame, producer, sequence, size, digest = map(int, match.groups())
        if sequence in published:
            raise ValueError("duplicate publication sequence")
        published[sequence] = (frame, producer, size, digest)
    received = []
    for match in re.finditer(r"live_receive sequence=(\d+) frame=(\d+) producer=(\d+) bytes=(\d+) digest=(\d+) saved_packets_read=false", consumer):
        sequence, frame, producer, size, digest = map(int, match.groups())
        if published.get(sequence) != (frame, producer, size, digest):
            raise ValueError("sender/receiver receipt mismatch")
        if received and (sequence <= received[-1][0] or producer <= received[-1][2]):
            raise ValueError("non-forward received source")
        received.append((sequence, frame, producer))
    if not received:
        raise ValueError("no matched ordinary sources")
    returned = {(int(a), int(b)) for a, b in re.findall(
        r"live_return sequence=(\d+) frame=(\d+) published=1 depth_values=307200 error= presentation_proven=false", consumer)}
    busy = {(int(a), int(b)) for a, b in re.findall(
        r"live_return sequence=(\d+) frame=(\d+) published=0 depth_values=307200 error=return-busy presentation_proven=false", consumer)}
    observed = {(sequence, frame) for sequence, frame, _ in received}
    if returned & busy or not (returned | busy) <= observed:
        raise ValueError("conflicting or unobserved consumer return")
    for sequence, frame, _ in received:
        if (sequence, frame) not in returned | busy:
            raise ValueError("missing paired return or explicit busy-drop")
        path = Path(f"{prefix}.frame-{frame}.bmp")
        with Image.open(path) as image:
            rgb = np.asarray(image.convert("RGB"))
            if image.size != (640, 480) or not np.any(rgb):
                raise ValueError("empty or wrong-size color")
        depth = np.fromfile(str(path) + ".depth.rgba32f", dtype="<f4")
        if depth.size != 640 * 480 * 4:
            raise ValueError("wrong-size depth")
        depth = depth.reshape(-1, 4)[:, 0]
        if not np.isfinite(depth).all() or not ((depth >= 0) & (depth <= 1)).all() or not np.any(depth):
            raise ValueError("invalid or empty depth")
    ages = []
    retained = set()
    overlays = {(int(sequence), int(frame)) for frame, sequence in re.findall(
        r"Remake async overlay retained: frame=(\d+) sequence=(\d+) original_native=true original_mask=true presentation=false", publisher)}
    for match in re.finditer(r"Remake async return: source=(\d+) producer=(\d+) sequence=(\d+) current=(\d+) retained=1 presentation=false", publisher):
        frame, producer, sequence, current = map(int, match.groups())
        if published.get(sequence, ())[:2] != (frame, producer) or not 0 <= current - frame <= 8:
            raise ValueError("retained identity/age mismatch")
        if (sequence, frame) not in returned:
            raise ValueError("retained without consumer return")
        if require_overlays and (sequence, frame) not in overlays:
            raise ValueError("retained without matching original overlays")
        if sequence in retained:
            raise ValueError("duplicate retained source")
        retained.add(sequence)
        ages.append(current - frame)
    if not ages:
        raise ValueError("no bounded returned pairs retained")
    return {"matched_source_frames": len(received), "published_frames": len(published),
            "paired_images_checked": len(received), "published_returns": len(returned),
            "busy_return_drops": len(busy), "retained_pairs": len(ages),
            "published_returns_not_retained": len(returned) - len(ages),
            "retained_age_min": min(ages), "retained_age_max": max(ages),
            "original_overlay_receipts_required": require_overlays,
            "source_gaps": sum(b[2] != a[2] + 1 for a, b in zip(received, received[1:])),
            "capture_gated": False, "producer_wait": False, "presentation_proven": False,
            "combined_dlss5_proven": False, "temporal_quality_proven": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--publisher", type=Path, required=True)
    parser.add_argument("--consumer", type=Path, required=True)
    parser.add_argument("--prefix", type=Path, required=True)
    parser.add_argument("--require-overlays", action="store_true")
    args = parser.parse_args()
    print(json.dumps(inspect(args.publisher.read_text(errors="replace"),
                             args.consumer.read_text(errors="replace"), args.prefix, args.require_overlays), indent=2))
