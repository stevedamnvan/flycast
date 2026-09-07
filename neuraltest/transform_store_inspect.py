"""Verify the bounded FC-067 x64 transform-to-RAM diagnostic, not camera recovery."""
import argparse
import json
from pathlib import Path
import re

import numpy as np
from PIL import Image

PLANES = ('native-pvr-color', 'source-color', 'depth', 'motion', 'bias-mask',
          'confidence', 'draw-id', 'overlay-classification', 'final-composited')
SITE = 0x8C070C3C


def require(condition, message):
    if not condition:
        raise ValueError(message)


def load_session(path):
    log = path / 'execution.log'
    require(log.stat().st_size <= 16 * 1024 * 1024, 'log exceeds 16 MiB bound')
    text = log.read_text(encoding='utf-8')
    require('DX11 Context initializing' in text, 'missing launch boundary')
    return text.rsplit('DX11 Context initializing', 1)[1]


def inspect_capture(path, negative):
    session = load_session(path)
    require('overflow=true' not in session, 'incomplete site observation')
    sites = [line for line in session.splitlines()
             if f'FC067_FTRV_SITE pc={SITE:08x} ' in line]
    require(len(sites) == 1, 'expected one target execution sample in final launch')
    site_fields = dict(re.findall(r'(\w+)=([^ ]+)', sites[0]))
    cycles = int(site_fields['cycles'])
    require(6_000_000_000 <= cycles < 8_000_000_000, 'sample outside diagnostic window')
    columns = [dict(re.findall(r'(\w+)=([^ ]+)', line)) for line in session.splitlines()
               if f'FC067_FTRV_MATRIX pc={SITE:08x} ' in line]
    require([c['column'] for c in columns] == ['0', '1', '2', '3'], 'missing matrix columns')
    for vector in [site_fields['input']] + [c['bits'] for c in columns]:
        require(re.fullmatch(r'[0-9a-f]{8}(,[0-9a-f]{8}){3}', vector) is not None,
                'invalid source vector/matrix bits')
    require(site_fields['lineage'] == 'uncorrelated', 'unsupported site provenance upgrade')
    require(all(c['meaning'] == 'unknown' for c in columns), 'unsupported matrix meaning')
    output_match = re.search(r'output=([0-9a-f,]+) ', sites[0])
    require(output_match is not None, 'missing transform output bits')
    output = [int(word, 16) for word in output_match[1].split(',')]
    require(len(output) == 4, 'invalid output vector')
    records = [dict(re.findall(r'(\w+)=([^ ]+)', line))
               for line in session.splitlines() if 'FC067_FTRV_STORE ' in line]
    require(len(records) == 4, 'expected four stores in final launch')
    base = int(records[-1]['address'], 16)
    require(base & 3 == 0 and base & 0x1C000000 == 0x0C000000,
            'store destination is not aligned guest RAM')
    for ordinal, record in enumerate(records):
        index = 3 - ordinal
        require(int(record['index']) == index, 'wrong component order')
        require(int(record['pc'], 16) == SITE + 2 + ordinal * 2, 'wrong store PC')
        require(int(record['address'], 16) == base + index * 4, 'wrong store address')
        value = int(record['value'], 16)
        require(value == output[index] == int(record['stored'], 16),
                'executed output and committed RAM disagree')
        expected = output[index] ^ (1 if negative and index == 0 else 0)
        require(int(record['expected'], 16) == expected, 'wrong control expectation')
        require(record['ram'] == record['ordered'] == '1', 'invalid RAM/order witness')
        require(record['exact'] == ('0' if negative and index == 0 else '1'),
                'wrong checker disposition')
        require(record['ta_lineage'] == 'unknown', 'unsupported provenance upgrade')
    return base, output, cycles, site_fields['input'], [c['bits'] for c in columns]


def compare_native(positive, negative, baseline):
    frames = sorted(p.name for p in baseline.glob('frame-*') if p.is_dir())
    require(len(frames) == 3, 'expected bounded three-frame baseline')
    comparisons = 0
    for capture in (positive, negative):
        require(sorted(p.name for p in capture.glob('frame-*') if p.is_dir()) == frames,
                'capture frame identities differ')
        for frame in frames:
            for plane in PLANES:
                with Image.open(baseline / frame / (plane + '.png')) as reference:
                    with Image.open(capture / frame / (plane + '.png')) as actual:
                        require(np.array_equal(np.asarray(reference), np.asarray(actual)),
                                f'image mismatch: {capture.name}/{frame}/{plane}')
                comparisons += 1
    return comparisons


def verify(positive, negative, baseline):
    good = inspect_capture(positive, False)
    bad = inspect_capture(negative, True)
    require(good == bad, 'positive and negative executed store differ')
    comparisons = compare_native(positive, negative, baseline)
    return {'scope': 'one-executed-transform-to-guest-ram', 'stores_exact': 4,
            'wrong_expected_word_rejected': True, 'native_plane_comparisons': comparisons,
            'ta_lineage': 'unknown', 'camera_recovered': False}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--positive', required=True, type=Path)
    parser.add_argument('--negative', required=True, type=Path)
    parser.add_argument('--baseline', required=True, type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args.positive, args.negative, args.baseline), sort_keys=True))
    except (ValueError, OSError, KeyError) as error:
        parser.exit(1, f'transform-store evidence rejected: {error}\n')
