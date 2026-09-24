"""Find Remix sky texture hashes for one stage (LOG1185 method).

Inputs: a saved remake-view.bin for the stage, and a Remix capture textures
folder written while rendering that packet (remake_offline_render.py with
DXVK_RTX_CAPTURE_ENABLE_ON_FRAME). Sky candidates are meshes whose vertices are
all unlit white and at least 30 percent of the far plane from the camera.
White meshes at 10-30 percent of the far plane are listed as REVIEW (look at
the native frame; add by hand only if it is clearly distant scenery).
A texture also used by any nearer mesh is reported as SHARED and must not be
tagged (it would turn that nearer mesh into sky).

Prints a ready rtx.skyBoxTextures line with the safe hashes.
"""
import argparse
import hashlib
import struct
from pathlib import Path

import numpy as np


def dds_payload(data):
    offset = 148 if data[84:88] == b'DX10' else 128
    return data[offset:]


def load_packet(path):
    data = Path(path).read_bytes()
    off = 0

    def take(n):
        nonlocal off
        if n < 0 or off+n > len(data):
            raise ValueError('truncated packet')
        value = data[off:off+n]
        off += n
        return value

    def read(fmt):
        return struct.unpack('<'+fmt, take(struct.calcsize('<'+fmt)))

    magic, version = read('II')
    if magic != 0x56524346 or version not in (4, 5):
        raise ValueError('requires a v4/v5 remake packet')
    read('QQQQ')
    take(read('I')[0]); take(read('I')[0]); take(read('I')[0])
    camera = np.frombuffer(take(16+60), dtype='<f4')
    for _ in range(read('I')[0]):
        take(read('I')[0])
    meshes = []
    for _ in range(read('I')[0]):
        mesh_id = read('Q')[0]
        read('IIII'); read('QQQQ')
        if version >= 5:
            read('I')
        dds = take(read('I')[0])
        n = read('I')[0]
        raw = take(n*36)
        take(read('I')[0]*4)
        pos = np.frombuffer(raw, dtype='<f4').reshape(n, 9)[:, :3]
        col = np.frombuffer(raw, dtype=np.uint8).reshape(n, 36)[:, 32:35]
        meshes.append(dict(id=mesh_id, dds=dds, pos=pos, col=col))
    return camera, meshes


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('packet', type=Path, help='remake-view.bin')
    ap.add_argument('--textures', type=Path, required=True, help='rtx-remix/captures/textures folder')
    a = ap.parse_args()
    camera, meshes = load_packet(a.packet)
    far, cam = float(camera[3]), camera[4:7]
    runtime = {}
    for p in a.textures.glob('*.dds'):
        runtime.setdefault(hashlib.sha256(dds_payload(p.read_bytes())).hexdigest(), p.stem.upper())
    sky, near_textures = {}, set()
    for m in meshes:
        key = hashlib.sha256(dds_payload(m['dds'])).hexdigest() if len(m['dds']) > 128 else None
        dmin = float(np.linalg.norm(m['pos']-cam, axis=1).min()) if len(m['pos']) else 0.0
        white = bool((m['col'] >= 250).all())
        if white and dmin >= 0.1*far:
            sky.setdefault(key, []).append((m['id'], dmin))
        if not (white and dmin >= 0.3*far):
            near_textures.add(key)
    safe = []
    print(f'far plane {far:g}; {len(meshes)} meshes; {len(sky)} sky texture(s)')
    for key, rows in sky.items():
        rh = runtime.get(key)
        close = min(d for _, d in rows) < 0.3*far
        state = ('NO-CAPTURE-MATCH' if rh is None else 'REVIEW-closer-than-0.3-far' if close
                 else 'SHARED-with-near-mesh' if key in near_textures else 'ok')
        print(f"  {rh or '?':16s} {state:22s} meshes={[f'{i:X}' for i, _ in rows]} min_distance={min(d for _, d in rows):.0f}")
        if state == 'ok':
            safe.append('0x'+rh)
    print('rtx.skyBoxTextures = ' + ', '.join(safe) if safe else 'no safe sky textures found')


if __name__ == '__main__':
    main()
