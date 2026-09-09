"""Read-only census of retained native effect identities; not material acceptance."""
import argparse
import json
import struct
from pathlib import Path


def inspect(path):
    data = path.read_bytes()
    if len(data) < 48 or len(data) > 64 * 1024 * 1024 + 8 or len(data) % 4:
        raise ValueError("effect identity byte bound")
    words = struct.unpack("<%dI" % (len(data) // 4), data)
    if words[0] != 0x31494645 or words[1] != len(words) - 2:
        raise ValueError("effect identity header")
    w = words[2:]
    if w[0] != 1 or not 0 < w[7] <= 256 or w[8] != 640 * 480 or not 0 < w[9] <= 1026:
        raise ValueError("effect identity contract")
    cursor = 10 + w[9]
    draws = {}
    for pixel in range(w[8]):
        if cursor >= len(w):
            raise ValueError("truncated pixel")
        count = w[cursor]
        cursor += 1
        if count > w[7] or cursor + 5 * count > len(w):
            raise ValueError("fragment bound")
        for _ in range(count):
            color, depth, sequence, primary, secondary = w[cursor:cursor + 5]
            cursor += 5
            ordinal = (sequence & 0x3fffffff) >> 17
            key = (ordinal, primary, secondary)
            d = draws.setdefault(key, dict(ordinal=ordinal, primary=primary, secondary=secondary,
                fragments=0, alpha_min=255, alpha_max=0, alpha_zero=0, alpha_full=0, alpha_partial=0, bounds=[640, 480, -1, -1],
                source_factor=(primary >> 29) & 7, destination_factor=(primary >> 26) & 7,
                source_secondary=bool(primary & (1 << 25)), destination_secondary=bool(primary & (1 << 24))))
            alpha = color & 255
            d['fragments'] += 1
            d['alpha_zero' if alpha == 0 else 'alpha_full' if alpha == 255 else 'alpha_partial'] += 1
            d['alpha_min'] = min(d['alpha_min'], alpha)
            d['alpha_max'] = max(d['alpha_max'], alpha)
            x, y = pixel % 640, pixel // 640
            b = d['bounds']; b[:] = [min(b[0], x), min(b[1], y), max(b[2], x), max(b[3], y)]
    if cursor != len(w):
        raise ValueError("trailing identity words")
    return dict(source_ordinal=w[3] | (w[4] << 32), coverage_only=True,
                material_or_promotion_proven=False, draws=sorted(draws.values(), key=lambda d: d['ordinal']))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('identity', type=Path)
    args = parser.parse_args()
    print(json.dumps(inspect(args.identity), indent=2))
