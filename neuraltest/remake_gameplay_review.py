#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Review bounded native/returned/combined gameplay; retain failed frames visibly."""
import argparse
import json
import re
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw
from remake_temporal_compare import captures, digest, rgba, require


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('captures', 'log', 'out'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    require(args.out.is_absolute() and not args.out.exists(), 'new absolute output required')
    rows = captures(args.captures)
    require(1 <= len(rows) <= 360, 'bounded nonempty capture required')
    frames = sorted(rows)
    presents = {tuple(map(int, row)) for row in re.findall(
        r'Remake preview present: source=(\d+) current=(\d+) kind=remake-evaluated completed=1 hresult=0',
        args.log.read_text(errors='replace'))}
    samples, panels = [], []
    for frame in frames:
        path, record = rows[frame]
        native = rgba(path/'original-native.png')
        returned = rgba(path/'returned-remix.png')
        final = rgba(path/'composited-remix.png')
        evaluated = rgba(path/'evaluated-remix.png')
        mask = rgba(path/'original-overlay-mask.png')[:, :, 0] >= 128
        expected = np.where(mask[:, :, None], native, evaluated)
        sample = dict(source=frame, current=record['current_frame'], protected_pixels=int(mask.sum()),
            hud_mismatches=int(np.count_nonzero(np.any(final[mask] != native[mask], axis=1))),
            composition_mismatches=int(np.count_nonzero(np.any(final != expected, axis=2))),
            backbuffer_mismatches=int(np.count_nonzero(np.any(final[:, :, :3] !=
                rgba(path/'flycast-pre-osd-backbuffer.png')[:, :, :3], axis=2))),
            completed_present=(frame, record['current_frame']) in presents,
            native_sha256=digest(path/'original-native.png'), final_sha256=digest(path/'composited-remix.png'))
        sample['passed'] = bool(mask.any() and (~mask).any() and sample['completed_present'] and
                                not sample['composition_mismatches'] and not sample['backbuffer_mismatches'])
        samples.append(sample)
        panel = Image.new('RGB', (1920, 520), '#181818')
        draw = ImageDraw.Draw(panel)
        for i, (label, pixels) in enumerate(zip(('Native PVR', 'Returned Remix scene: before neural/effects/HUD',
                                                'Remix only: effects/HUD, neural skipped' if record.get('neural_evaluation_skipped') else
                                                'Combined experimental: effects/HUD composed'), (native, returned, final))):
            draw.text((i*640+8, 4), label, fill='white')
            panel.paste(Image.fromarray(pixels), (i*640, 24))
        draw.text((8, 506), f"Source {frame} | protected {sample['protected_pixels']} | " +
                  ('composition checks pass; visual quality unjudged' if sample['passed'] else 'CORRECTIONS REQUIRED'),
                  fill='white' if sample['passed'] else '#ff6060')
        panels.append(panel)
    report = dict(samples=samples, gaps=[(x,y) for x,y in zip(frames,frames[1:]) if y!=x+1],
                  scope='nonempty HUD and composition checks; not complete scene coverage or temporal quality',
                  external_provenance='not re-established by this review', performance_eligible=False,
                  winner=None, log_sha256=digest(args.log),
                  passed=all(sample['passed'] for sample in samples),
                  playback='quantized slowed GIF; gaps held according to source-frame delta; original PNGs authoritative')
    args.out.mkdir(parents=True, exist_ok=False)
    (args.out/'review.json').write_text(json.dumps(report, indent=2)+'\n')
    panels[0].save(args.out/'moving-review-slow.gif', save_all=True, append_images=panels[1:],
                   duration=[80*(y-x) for x,y in zip(frames,frames[1:])]+[80], loop=0, optimize=False)
    for index in sorted({0, len(panels)//2, len(panels)-1}):
        panels[index].save(args.out/f'review-{frames[index]}.png')
    print(json.dumps(dict(frames=len(samples), first=frames[0], last=frames[-1], gaps=report['gaps'],
                         failed=[s['source'] for s in samples if not s['passed']])))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
