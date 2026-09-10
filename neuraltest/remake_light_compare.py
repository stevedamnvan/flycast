#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Compare authored-light captures, preserving unmatched frames and stage labels."""
import argparse
import json
import re
import numpy as np
from PIL import Image, ImageDraw
from pathlib import Path
from remake_temporal_compare import captures, digest, rgba, require


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('baseline', 'candidate', 'baseline-log', 'candidate-log', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--remix-neural', action='store_true',
                   help='Require identical returned pixels and explicit Remix-only baseline; not external provenance')
    p.add_argument('--public-neural', action='store_true',
                   help='Compare hooks-disabled returned DLAA to combined, requiring exact source/effect inputs')
    a = p.parse_args()
    require(not(a.remix_neural and a.public_neural), 'choose one comparison mode')
    if a.public_neural:
        require('SAFE MODE: EnableHooks=0, all hooks off (no NR)' in
                (a.baseline_log.parent/'ReShade.log').read_text(errors='replace'),
                'public baseline host must report hooks disabled')
    require(a.out.is_absolute() and not a.out.exists(), 'new absolute output required')
    lanes = [captures(a.baseline), captures(a.candidate)]
    frames = sorted(set(lanes[0]) & set(lanes[1]))
    require(2 <= len(frames) <= 360, 'bounded overlapping sequence required')
    presents = []
    for log in (a.baseline_log, a.candidate_log):
        presents.append({tuple(map(int, row)) for row in re.findall(
            r'Remake preview present: source=(\d+) current=(\d+) kind=remake-evaluated completed=1 hresult=0',
            log.read_text(errors='replace'))})
    report = dict(frames=frames, unmatched=[sorted(set(lane)-set(frames)) for lane in lanes],
                  gaps=[(x, y) for x, y in zip(frames, frames[1:]) if y != x+1],
                  scope='matched scene packets, not matched temporal histories or NGX inputs',
                  external_output_provenance='not established by this comparison',
                  performance_eligible=False, winner=None, samples=[])
    panels = []
    for frame in frames:
        paths = [lane[frame][0] for lane in lanes]
        hashes = {}
        for name in ('remake-view.bin', 'original-native.png', 'original-overlay-mask.png'):
            values = [digest(path / name) for path in paths]
            require(values[0] == values[1], (frame, 'source mismatch', name))
            hashes[name] = values[0]
        if a.remix_neural or a.public_neural:
            require((lanes[0][frame][1].get('neural_evaluation_skipped') is True) == a.remix_neural,
                    (frame, 'baseline evaluation mode mismatch'))
            require(lanes[1][frame][1].get('neural_evaluation_skipped') is not True,
                    (frame, 'candidate cannot be Remix-only'))
            values = [digest(path/'returned-remix.png') for path in paths]
            require(values[0] == values[1], (frame, 'returned Remix input mismatch'))
            hashes['returned-remix.png'] = values[0]
            for name in ('remake-return-depth.f32','native-effect-identity.bin','native-alpha-exclusions.bin'):
                values=[digest(path/name) for path in paths]
                require(values[0]==values[1], (frame,'source/effect mismatch',name))
                hashes[name]=values[0]
        pictures = [rgba(paths[0] / 'original-native.png')]
        returned = []
        for i, path in enumerate(paths):
            record = lanes[i][frame][1]
            require((frame, record['current_frame']) in presents[i], (frame, 'missing Present'))
            native = rgba(path / 'original-native.png')
            mask = rgba(path / 'original-overlay-mask.png')[:, :, 0] >= 128
            final = rgba(path / 'composited-remix.png')
            evaluated = rgba(path / 'evaluated-remix.png')
            require(mask.any() and (~mask).any(), (frame, 'empty classification'))
            require(np.array_equal(final, np.where(mask[:, :, None], native, evaluated)),
                    (frame, 'composition mismatch'))
            require(np.array_equal(final[:, :, :3], rgba(path / 'flycast-pre-osd-backbuffer.png')[:, :, :3]),
                    (frame, 'backbuffer mismatch'))
            returned.append(rgba(path / 'returned-remix.png')[:, :, :3].astype(np.float32))
            pictures.append(final)
        report['samples'].append(dict(frame=frame, source_hashes=hashes,
            returned_rgb_mae=float(np.mean(np.abs(returned[1]-returned[0]))),
            returned_mean_channel_delta=np.mean(returned[1]-returned[0], axis=(0, 1)).tolist()))
        panel = Image.new('RGB', (1920, 504), '#181818')
        draw = ImageDraw.Draw(panel)
        labels = ('Native PVR', 'Remix only: native effects/HUD', 'Combined experimental: native effects/HUD') if a.remix_neural else (
            'Native PVR', 'Combined baseline light', 'Combined candidate light')
        if a.public_neural:
            labels=('Native PVR','Remix + public DLAA: hooks disabled','Combined experimental: external hooks enabled')
        for i, (label, picture) in enumerate(zip(labels, pictures)):
            draw.text((i*640+8, 5), f'{label} | source {frame}', fill='white')
            panel.paste(Image.fromarray(picture), (i*640, 24))
        panels.append(panel)
    report['log_sha256'] = [digest(a.baseline_log), digest(a.candidate_log)]
    report['hud_mismatch_pixels'] = 0
    report['note'] = 'Quantized slowed GIF; source-frame gaps retained in playback duration. Original PNGs authoritative.'
    a.out.mkdir(parents=True, exist_ok=False)
    (a.out / 'comparison.json').write_text(json.dumps(report, indent=2)+'\n')
    durations = [80*(y-x) for x, y in zip(frames, frames[1:])] + [80]
    panels[0].save(a.out / 'moving-comparison-slow.gif', save_all=True,
                   append_images=panels[1:], duration=durations, loop=0, optimize=False)
    panels[len(panels)//2].save(a.out / 'comparison-midpoint.png')
    print(json.dumps({key: report[key] for key in ('frames', 'gaps', 'unmatched', 'winner')}))


if __name__ == '__main__':
    main()
