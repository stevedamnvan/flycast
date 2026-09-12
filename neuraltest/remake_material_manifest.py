#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Compact asset manifest for the Soulcalibur Remix pilot (BACKLOG pilot substep A).

Joins, for one saved v4/v5 packet (remake-view.bin), the helper's texture
identity (id/generation/palette/rtt, content digest as logged by
`texture_register`) to the runtime's material hash from a Remix USD capture
(textures/<HASH>.dds, matched by pixel payload), the source draw semantics
(cutout reference, alpha blend, TSP word) and the replacement maps a mod layer
binds (mod.usda). Runtime material hashes and helper content digests are
different identities and are kept in separate fields. Reads only; writes the
manifest JSON to the path given. Not a task tracker and not a quality judge.
"""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path


def fnv1a(data):
    h = 14695981039346656037
    for b in data:
        h ^= b
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return '%016x' % h


def dds_payload(data):
    """Pixel payload after the 128-byte DDS header plus a DX10 chunk when present."""
    if len(data) < 128 or data[:4] != b'DDS ':
        raise ValueError('not a DDS file')
    height, width = struct.unpack('<II', data[12:20])
    offset = 148 if data[84:88] == b'DX10' else 128
    fmt = struct.unpack('<I', data[128:132])[0] if offset == 148 else None
    return width, height, fmt, data[offset:]


def alpha_summary(payload, width, height):
    """Alpha channel histogram class of an RGBA8 payload (source semantics hint)."""
    n = width * height
    if len(payload) < n * 4:
        return 'unknown'
    opaque = sum(1 for i in range(3, n * 4, 4 * 16) if payload[i] == 255)
    zero = sum(1 for i in range(3, n * 4, 4 * 16) if payload[i] == 0)
    sampled = len(range(3, n * 4, 4 * 16))
    if opaque == sampled:
        return 'opaque'
    if opaque + zero == sampled:
        return 'binary'
    return 'graded'


def packet_meshes(path):
    data = Path(path).read_bytes()
    offset = 0

    def take(n):
        nonlocal offset
        if not 0 <= n <= len(data) - offset:
            raise ValueError('truncated packet')
        value = data[offset:offset + n]
        offset += n
        return value

    def read(fmt):
        return struct.unpack('<' + fmt, take(struct.calcsize('<' + fmt)))

    magic, version = read('II')
    if magic != 0x56524346 or version not in (4, 5):
        raise ValueError('requires an owned v4/v5 packet')
    frame = read('QQQQ')[0]
    if take(read('I')[0]) != b'T1401N':
        raise ValueError('game mismatch')
    take(read('I')[0]); take(read('I')[0])
    take(16 + 60)
    for _ in range(read('I')[0]):
        take(read('I')[0])
    meshes = []
    for _ in range(read('I')[0]):
        mesh_id = read('Q')[0]
        tsp, alpha_reference, alpha_blend, known = read('IIII')
        identity = read('QQQQ')
        if version >= 5:
            mode = read('I')[0]
        else:
            mode = 0
        dds = take(read('I')[0])
        vertex_count = read('I')[0]; take(vertex_count * 36)
        index_count = read('I')[0]; take(index_count * 4)
        meshes.append(dict(mesh=mesh_id, tsp=tsp, alpha_reference=alpha_reference,
                           alpha_blend=bool(alpha_blend), texture_known=bool(known),
                           texture_wire=mode, texture=identity, dds=dds,
                           vertices=vertex_count, indices=index_count))
    if offset != len(data):
        raise ValueError('trailing packet bytes')
    return frame, meshes


def capture_textures(directory):
    result = {}
    for path in sorted(Path(directory).glob('*.dds')):
        data = path.read_bytes()
        width, height, fmt, payload = dds_payload(data)
        result[path.stem.upper()] = dict(width=width, height=height, dxgi=fmt,
                                         payload_sha256=hashlib.sha256(payload).hexdigest(),
                                         bytes=len(data))
    return result


def mod_bindings(mod_path):
    """Replacement map paths per material hash from a typed Material/Shader layer."""
    text = Path(mod_path).read_text(encoding='utf-8')
    bindings = {}
    # Toolkit refinement layers author untyped `over` opinions, which must
    # participate before weaker typed definitions when resolving map slots.
    for block in re.finditer(r'(?:def Material|over) "mat_([0-9A-F]{16})"(.*?)\n        \}', text, re.S):
        maps = dict(re.findall(r'inputs:(\w+_texture) = @([^@]*)@', block.group(2)))
        bindings[block.group(1)] = maps
    return bindings


SLOT_SUFFIX = dict(diffuse_texture='.a.rtex.dds', normalmap_texture='.n.rtex.dds',
                   reflectionroughness_texture='.r.rtex.dds')


def mod_layers(mod_path):
    """Material bindings across a mod root and its sublayers (strongest first)."""
    root = Path(mod_path)
    text = root.read_text(encoding='utf-8')
    header = text.split('\n)', 1)[0]
    layers = [root] + [root.parent / rel for rel in re.findall(r'@([^@]+\.usda?)@', header)]
    bindings = {}
    for layer in layers:
        if layer.is_file():
            for h, maps in mod_bindings(layer).items():
                for slot, rel in maps.items():
                    relative = rel if layer == root else str((layer.parent / rel).resolve().relative_to(root.parent.resolve()))
                    bindings.setdefault(h, {}).setdefault(slot, relative.replace('\\', '/'))
    return bindings


def build(packet, capture_dir, project, out, capture_name=''):
    frame, meshes = packet_meshes(packet)
    captured = capture_textures(capture_dir)
    by_payload = {v['payload_sha256']: h for h, v in captured.items()}
    mod = mod_layers(Path(project) / 'mod.usda') if project else {}
    materials = {}
    unmatched = []
    for mesh in meshes:
        if not mesh['dds']:
            continue
        width, height, fmt, payload = dds_payload(mesh['dds'])
        key = hashlib.sha256(payload).hexdigest()
        digest = fnv1a(mesh['dds'])
        runtime_hash = by_payload.get(key)
        if runtime_hash is None:
            unmatched.append(dict(mesh=mesh['mesh'], content_digest=digest, texture_id=mesh['texture'][0]))
            continue
        entry = materials.setdefault(runtime_hash, dict(
            runtime_material_hash=runtime_hash,
            helper_content_digest=digest,
            helper_texture=dict(id=mesh['texture'][0], generation=mesh['texture'][1],
                                palette=mesh['texture'][2], rtt=mesh['texture'][3]),
            source=dict(width=width, height=height, dxgi=fmt,
                        alpha=alpha_summary(payload, width, height)),
            usage=dict(meshes=0, vertices=0, cutout=False, alpha_blend=False, tsp=set()),
            replacement={}))
        if entry['helper_content_digest'] != digest:
            entry.setdefault('conflicting_digests', []).append(digest)
        usage = entry['usage']
        usage['meshes'] += 1; usage['vertices'] += mesh['vertices']
        usage['cutout'] |= mesh['alpha_reference'] < 256
        usage['alpha_blend'] |= mesh['alpha_blend']
        usage['tsp'].add(mesh['tsp'])
    for runtime_hash, entry in materials.items():
        entry['usage']['tsp'] = sorted(entry['usage']['tsp'])
        maps = mod.get(runtime_hash, {})
        for slot, rel in maps.items():
            file = (Path(project) / rel).resolve() if project else None
            expected = SLOT_SUFFIX.get(slot)
            entry['replacement'][slot] = dict(
                path=rel, exists=bool(file and file.is_file()),
                sha256=hashlib.sha256(file.read_bytes()).hexdigest() if file and file.is_file() else None,
                # Ingested Toolkit maps carry a channel suffix (.a diffuse, .n normal, .r roughness);
                # a map whose suffix does not match its slot is a wrong binding.
                slot_type_ok=None if expected is None or not rel.endswith('.rtex.dds') else rel.endswith(expected))
    wrong = [[h, slot] for h, e in materials.items() for slot, r in e['replacement'].items() if r['slot_type_ok'] is False]
    missing = [[h, slot] for h, e in materials.items() for slot, r in e['replacement'].items() if not r['exists']]
    manifest = dict(
        schema='flycast-remix-material-manifest-v1',
        wrong_slot_bindings=wrong, missing_replacements=missing,
        source_frame=frame, capture=capture_name,
        captured_materials=len(captured),
        matched_materials=len(materials),
        unmatched_packet_textures=unmatched,
        captured_not_in_packet=sorted(set(captured) - set(materials)),
        identity_note='runtime_material_hash is the Remix runtime hash from the capture; '
                      'helper_content_digest is FNV-1a over the helper DDS bytes as logged by texture_register; '
                      'they are joined by identical pixel payload only',
        materials=dict(sorted(materials.items())))
    Path(out).write_text(json.dumps(manifest, indent=1, sort_keys=True) + '\n', encoding='utf-8')
    return manifest


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--packet', type=Path, required=True, help='saved remake-view.bin (v4/v5)')
    p.add_argument('--capture-textures', type=Path, required=True, help='rtx-remix/captures/textures')
    p.add_argument('--project', type=Path, default=None, help='Toolkit project dir holding mod.usda')
    p.add_argument('--capture-name', default='')
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    m = build(a.packet, a.capture_textures, a.project, a.out, a.capture_name)
    print(json.dumps(dict(source_frame=m['source_frame'], captured=m['captured_materials'],
                          matched=m['matched_materials'], unmatched=len(m['unmatched_packet_textures']),
                          captured_not_in_packet=m['captured_not_in_packet'])))


if __name__ == '__main__':
    main()
