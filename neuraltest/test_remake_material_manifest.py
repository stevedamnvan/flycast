#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Synthetic packet/capture/mod fixtures for the material manifest (no game data)."""
import hashlib
import json
import struct
import tempfile
import unittest
from pathlib import Path

import remake_material_manifest as manifest


def dds(payload, dx10=True):
    header = b'DDS ' + struct.pack('<I', 124) + struct.pack('<III', 0x1007, 4, 4) + b'\0' * (128 - 20)
    if dx10:
        header = header[:84] + b'DX10' + header[88:] + struct.pack('<IIIII', 28, 3, 0, 1, 0)
    return header + payload


def packet(meshes, version=4):
    out = bytearray(struct.pack('<II', 0x56524346, version) + struct.pack('<QQQQ', 2601, 1, 2, 3))
    out += struct.pack('<I', 6) + b'T1401N' + struct.pack('<I', 0) + struct.pack('<I', 0) + b'\0' * 76 + struct.pack('<I', 0)
    out += struct.pack('<I', len(meshes))
    for mesh_id, tsp, alpha_ref, blend, identity, data in meshes:
        out += struct.pack('<Q', mesh_id) + struct.pack('<IIII', tsp, alpha_ref, blend, 1) + struct.pack('<QQQQ', *identity)
        if version >= 5:
            out += struct.pack('<I', 0)
        out += struct.pack('<I', len(data)) + data + struct.pack('<I', 1) + b'\0' * 36 + struct.pack('<I', 3) + b'\0' * 12
    return bytes(out)


class ManifestTest(unittest.TestCase):
    def setUp(self):
        self.dir = Path(tempfile.mkdtemp())
        self.payload_a = bytes([10, 20, 30, 255] * 16)
        self.payload_b = bytes([1, 2, 3, 0, 4, 5, 6, 255] * 8)
        (self.dir / 'textures').mkdir()
        (self.dir / 'textures' / 'AAAA000000000001.dds').write_bytes(dds(self.payload_a, dx10=False))
        (self.dir / 'textures' / 'BBBB000000000002.dds').write_bytes(dds(self.payload_b, dx10=False))
        project = self.dir / 'project'; (project / 'assets' / 'ingested').mkdir(parents=True); (project / 'layers').mkdir()
        for name in ('AAAA000000000001_diffuse.a.rtex.dds', 'AAAA000000000001_roughness.r.rtex.dds'):
            (project / 'assets' / 'ingested' / name).write_bytes(b'x')
        (project / 'mod.usda').write_text('#usda 1.0\n(\n    subLayers = [\n        @./layers/draft.usda@\n    ]\n)\n\nover "RootNode"\n{\n}\n', encoding='utf-8')
        (project / 'layers' / 'draft.usda').write_text(
            '#usda 1.0\n(\n)\nover "RootNode"\n{\n    over "Looks"\n    {\n'
            '        def Material "mat_AAAA000000000001"\n        {\n            def Shader "Shader"\n            {\n'
            '                asset inputs:diffuse_texture = @../assets/ingested/AAAA000000000001_diffuse.a.rtex.dds@\n'
            '                asset inputs:normalmap_texture = @../assets/ingested/AAAA000000000001_roughness.r.rtex.dds@\n'
            '                asset inputs:reflectionroughness_texture = @../assets/ingested/missing.r.rtex.dds@\n'
            '            }\n        }\n    }\n}\n', encoding='utf-8')
        self.project = project

    def test_join_usage_and_binding_checks(self):
        p = self.dir / 'remake-view.bin'
        p.write_bytes(packet([(1, 7, 256, 0, (100, 1, 0, 0), dds(self.payload_a)),
                              (2, 7, 128, 1, (100, 1, 0, 0), dds(self.payload_a)),
                              (3, 9, 256, 0, (200, 1, 5, 0), dds(self.payload_b))]))
        out = self.dir / 'manifest.json'
        m = manifest.build(p, self.dir / 'textures', self.project, out)
        self.assertEqual((m['matched_materials'], m['unmatched_packet_textures'], m['captured_not_in_packet']), (2, [], []))
        a = m['materials']['AAAA000000000001']
        self.assertEqual(a['helper_content_digest'], manifest.fnv1a(dds(self.payload_a)))
        self.assertEqual(a['usage'], dict(meshes=2, vertices=2, cutout=True, alpha_blend=True, tsp=[7]))
        self.assertEqual(a['source']['alpha'], 'opaque')
        self.assertEqual(m['materials']['BBBB000000000002']['source']['alpha'], 'binary')
        self.assertTrue(a['replacement']['diffuse_texture']['slot_type_ok'])
        self.assertEqual(m['wrong_slot_bindings'], [['AAAA000000000001', 'normalmap_texture']])
        self.assertEqual(m['missing_replacements'], [['AAAA000000000001', 'reflectionroughness_texture']])
        self.assertEqual(json.loads(out.read_text(encoding='utf-8'))['source_frame'], 2601)

    def test_stronger_untyped_override_preserves_weaker_slots(self):
        root = self.project / 'mod.usda'
        root.write_text(root.read_text().replace(
            '@./layers/draft.usda@',
            '@./layers/refined.usda@, @./layers/draft.usda@'))
        (self.project / 'layers' / 'refined.usda').write_text(
            'over "RootNode"\n{\n    over "Looks"\n    {\n'
            '        over "mat_AAAA000000000001"\n        {\n'
            '            over "Shader"\n            {\n'
            '                asset inputs:reflectionroughness_texture = @../assets/refined.r.rtex.dds@\n'
            '            }\n        }\n    }\n}\n')
        maps = manifest.mod_layers(root)['AAAA000000000001']
        self.assertEqual(maps['reflectionroughness_texture'], 'assets/refined.r.rtex.dds')
        self.assertEqual(maps['diffuse_texture'],
                         'assets/ingested/AAAA000000000001_diffuse.a.rtex.dds')
        self.assertEqual(len(maps), 3)

    def test_unmatched_and_bad_packets(self):
        p = self.dir / 'remake-view.bin'
        p.write_bytes(packet([(1, 7, 256, 0, (100, 1, 0, 0), dds(bytes(64)))]))
        m = manifest.build(p, self.dir / 'textures', None, self.dir / 'm.json')
        self.assertEqual(len(m['unmatched_packet_textures']), 1)
        self.assertEqual(m['captured_not_in_packet'], ['AAAA000000000001', 'BBBB000000000002'])
        p.write_bytes(packet([], version=3))
        with self.assertRaises(ValueError):
            manifest.build(p, self.dir / 'textures', None, self.dir / 'm.json')
        p.write_bytes(packet([]) + b'\0')
        with self.assertRaises(ValueError):
            manifest.build(p, self.dir / 'textures', None, self.dir / 'm.json')


if __name__ == '__main__':
    unittest.main()
