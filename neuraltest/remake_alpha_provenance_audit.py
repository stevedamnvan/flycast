"""Read-only exact-input post-effect external-output evidence audit."""
import json
import re
from pathlib import Path
import numpy as np
from PIL import Image

import argparse
parser = argparse.ArgumentParser(description=__doc__)
for name in ('evidence', 'repo', 'archive', 'marked', 'clean', 'off'):
    parser.add_argument('--' + name, required=True)
args = parser.parse_args()
root = Path(args.evidence)
repo = Path(args.repo)

def lane(tag):
    log = (repo / f'build-neural-automation/remake-effects-flycast-{tag}.log').read_text()
    mapping = dict(re.findall(r'locked input accepted: source=(\d+) original=(\d+)', log))
    effects = {int(a): int(b) for a, b in re.findall(r'effect replay matched: source=(\d+) original=(\d+)', log)}
    evidence = {}
    for row in re.findall(r'frame=(\d+) color_fnv64=(\w+) depth_fnv64=(\w+) motion_fnv64=(\w+) mask_fnv64=(\w+) returned_fnv64=(\w+)', log):
        source = int(row[0])
        assert row[0] in mapping and effects[source] == int(mapping[row[0]])
        evidence[effects[source]] = row[1:]
    presents = {tuple(map(int, r)) for r in re.findall(r'Remake preview present: source=(\d+) current=(\d+) kind=remake-evaluated completed=1 hresult=0', log)}
    markers = set(map(int, re.findall(r'DLSS 5 developer present evidence:.*?frame=(\d+).*?marker_pixels=1024/1024', log)))
    captures = {}
    for d in (root / f'fc067-effects-{tag}-composites').glob('frame-*'):
        p = json.loads((d / 'preview.json').read_text())
        assert (p['source_frame'], p['current_frame']) in presents
        assert p['native_effects_applied'] and p['replay_original_frame'] == effects[p['source_frame']]
        archive = next((root / f'fc067-effects-{args.archive}-composites').glob(f"frame-{p['replay_original_frame']}-*"))
        assert (d/'native-effect-identity.bin').read_bytes() == (archive/'native-effect-identity.bin').read_bytes()
        assert (d/'native-alpha-exclusions.bin').read_bytes() == (archive/'native-alpha-exclusions.bin').read_bytes(), 'alpha ownership mismatch'
        captures[p['replay_original_frame']] = (d, p)
    return evidence, captures, markers, log

def img(d, name):
    return np.array(Image.open(d/name).convert('RGBA'))

def fnv(data):
    h = 14695981039346656037
    for b in data:
        h = ((h ^ b) * 1099511628211) & ((1 << 64)-1)
    return f'{h:016X}'

marked, clean, off = [lane(tag) for tag in (args.marked, args.clean, args.off)]
common = sorted(set(marked[1]) & set(clean[1]) & set(off[0]))
assert common, 'no complete matched evidence'
assert not off[1], 'disabled external consumer unexpectedly produced evaluated captures'
changed, unchanged = [], []
for original in common:
    a, b, c = [x[0][original] for x in (marked, clean, off)]
    assert a[:4] == b[:4] == c[:4], ('neural input mismatch', original)
    assert a[4] == b[4], ('marked/clean pre-marker output mismatch', original)
    assert marked[1][original][1]['source_frame'] in marked[2], ('missing final marker', original)
    d, p = clean[1][original]
    before = img(d, 'neural-before-native-effects.png')
    assert fnv(before.tobytes()) == b[4], ('pre-effect output hash', original)
    evaluated = img(d, 'evaluated-remix.png')
    native = img(d, 'original-native.png')
    mask = img(d, 'original-overlay-mask.png')[:,:,0] >= 128
    composite = img(d, 'composited-remix.png')
    assert mask.any() and (~mask).any()
    assert np.array_equal(composite, np.where(mask[:,:,None], native, evaluated))
    assert np.array_equal(composite[:,:,:3], img(d,'flycast-pre-osd-backbuffer.png')[:,:,:3])
    assert np.array_equal(np.abs(evaluated[:,:,:3].astype(int)-before[:,:,:3].astype(int)),
                          img(d,'native-effects-absolute-difference.png')[:,:,:3])
    assert np.any(composite[~mask] != native[~mask]), 'native substitution not falsified'
    (changed if b[4] != c[4] else unchanged).append(original)

tuple_text = 'upscaling=OFF intensity=1.000000 global_tone=1.000000 diffuse_white_nits=203.000000 preset=0 style=0 enabled=ON'
for tag in (args.marked, args.clean):
    assert tuple_text in (repo/f'build-neural-automation/remake-effects-reshade-{tag}.log').read_text()
print(json.dumps({'matched_presented_frames':len(common), 'external_changed_originals':changed,
 'startup_or_unchanged_originals':unchanged, 'exact_scene_neural_inputs_and_effects':True,
 'marker_clean_output_hash_match':True, 'pre_effect_png_matches_returned_hash':True,
 'post_effect_hud_backbuffer_present_checks':True, 'active_tuple':tuple_text,
 'performance_eligible':False, 'full_pipeline_acceptance':False},indent=2))
