import json
import tempfile
import unittest
import struct
from pathlib import Path
from remix_capture_mcp import capture_destination, displacement_request, ingestion_request, surface_request, diffuse_binding_request, scalar_dds_mip_probe, scalar_dds_ingestion_request


class CaptureDestinationTests(unittest.TestCase):
    def test_scalar_mip_probe_preservation_and_corruption(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); project = root / 'review.usda'; project.write_text('project')
            header = bytearray(148); header[:4] = b'DDS '
            for offset, value in [(4, 124), (8, 0x21007), (12, 4), (16, 4), (28, 3), (76, 32), (80, 4), (108, 0x401008)]:
                struct.pack_into('<I', header, offset, value)
            header[84:88] = b'DX10'; struct.pack_into('<5I', header, 128, 80, 3, 0, 1, 0)
            source = root / 'source.dds'; source.write_bytes(header + bytes(range(24)))
            body = {'context_plugin': {'name': 'TextureImporter', 'data': {'input_files': [[str(source), 'METALLIC']],
                    'output_directory': str(root / 'assets/ingested/new')}}, 'executor': 1}
            request, _ = scalar_dds_ingestion_request(project, json.dumps(body), 'METALLIC')
            self.assertEqual(request['executor'], 0)
            with self.assertRaises(ValueError): scalar_dds_ingestion_request(project, json.dumps(body), 'ROUGHNESS')
            body['cleanup_input'] = True
            with self.assertRaises(ValueError): scalar_dds_ingestion_request(project, json.dumps(body), 'METALLIC')
            del body['cleanup_input']
            body['context_plugin']['data']['output_directory'] = str(root / 'escape')
            with self.assertRaises(ValueError): scalar_dds_ingestion_request(project, json.dumps(body), 'METALLIC')
            body['context_plugin']['data']['output_directory'] = str(root / 'assets/ingested/new')
            existing = root / 'assets/ingested/new'; existing.mkdir(parents=True); (existing / 'keep').write_text('keep')
            with self.assertRaises(ValueError): scalar_dds_ingestion_request(project, json.dumps(body), 'METALLIC')
            target = root / 'assets/ingested/probe/metal.m.rtex.dds'; target.parent.mkdir(parents=True)
            target.write_bytes(source.read_bytes()); Path(str(target) + '.meta').write_text('fixture metadata')
            result = scalar_dds_mip_probe(source, 'METALLIC', project, target)
            self.assertTrue(result['all_mips_byte_identical'])
            self.assertFalse(result['pipeline_preservation_proven'])
            changed = bytearray(target.read_bytes()); changed[-1] ^= 1; target.write_bytes(changed)
            self.assertFalse(scalar_dds_mip_probe(source, 'METALLIC', project, target)['all_mips_byte_identical'])
            for offset, value in [(28, 1), (128, 81), (140, 2)]:
                bad = bytearray(source.read_bytes()); struct.pack_into('<I', bad, offset, value); target.write_bytes(bad)
                with self.assertRaises(ValueError): scalar_dds_mip_probe(target, 'METALLIC')
            target.write_bytes(source.read_bytes()[:-1])
            with self.assertRaises(ValueError): scalar_dds_mip_probe(target, 'METALLIC')
            with self.assertRaises(ValueError): scalar_dds_mip_probe(source, 'METALLIC', project, source)

    def test_diffuse_binding_requires_isolated_ingested_target(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); project = root / 'review.usda'; project.write_text('project')
            (root / 'layers').mkdir(); (root / 'assets/ingested/faces').mkdir(parents=True)
            layer = root / 'layers/candidate.usda'; layer.write_text('candidate')
            texture = root / 'assets/ingested/faces/face.dds'; texture.write_bytes(b'dds')
            shader = '/RootNode/Looks/mat_91446E8A159E8C2F/Shader'
            with self.assertRaises(ValueError): diffuse_binding_request(project, layer, shader, texture)
            Path(str(texture)+'.meta').write_text('ingested')
            self.assertEqual(diffuse_binding_request(project, layer, shader, texture), (layer.resolve(), texture.resolve()))
            for name in ['mod.usda', 'layers/pbrify_reimagined.usda']:
                bad = root / name; bad.write_text('baseline')
                with self.assertRaises(ValueError): diffuse_binding_request(project, bad, shader, texture)
                self.assertEqual(bad.read_text(), 'baseline')
            outside = root / 'face.dds'; outside.write_bytes(b'dds'); Path(str(outside)+'.meta').write_text('meta')
            with self.assertRaises(ValueError): diffuse_binding_request(project, layer, shader, outside)

    def test_displacement_bounds_and_shader_scope(self):
        shader = '/RootNode/Looks/mat_145398E2FC5B2FEA/Shader'
        self.assertEqual(displacement_request(shader, 0, 0), {'displace_in': 0.0, 'displace_out': 0.0})
        self.assertEqual(displacement_request(shader), {})
        for bad in (True, -0.1, 0.201, 1.1, float('nan'), float('inf'), '0'):
            with self.assertRaises(ValueError):
                displacement_request(shader, bad, 0)
        for bad in ('/Other/Shader', shader + '/Child', 'relative', shader.replace('145398E2FC5B2FEA', 'bad')):
            with self.assertRaises(ValueError):
                displacement_request(bad, 0, 0)

    def test_surface_constants_reject_invalid_values(self):
        shader = '/RootNode/Looks/mat_0DCBE839C56F7DD2/Shader'
        self.assertEqual(surface_request(shader, .7, 0), {'reflection_roughness_constant': .7, 'metallic_constant': 0.0})
        for bad in (True, None, -0.01, 1.01, float('nan'), float('inf'), '0'):
            with self.assertRaises(ValueError): surface_request(shader, bad, 0)
            with self.assertRaises(ValueError): surface_request(shader, .7, bad)
        with self.assertRaises(ValueError): surface_request('/Other/Shader', .7, 0)

    def test_collision_is_rejected_without_changing_capture(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            project = root / 'review.usda'; project.write_text('project')
            (root / 'deps/captures').mkdir(parents=True)
            source = root / 'capture.usd'; source.write_bytes(b'capture source')
            _, target, digest = capture_destination(project, source)
            self.assertEqual(target.parent, (root / 'deps/captures').resolve())
            self.assertIn(digest[:16], target.name)
            target.write_text('existing')
            with self.assertRaises(ValueError):
                capture_destination(project, source)
            self.assertEqual(target.read_text(), 'existing')
            self.assertEqual(source.read_bytes(), b'capture source')

    def test_ingestion_rejects_escape_and_cache_collision(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); project = root / 'p.usda'; project.write_text('project')
            src = root / 'source.png'; src.write_bytes(b'png')
            output = root / 'assets/ingested/candidate'
            body = {'executor': 1, 'context_plugin': {'data': {
                'input_files': [[str(src), 'DIFFUSE']], 'output_directory': str(output)}}}
            self.assertEqual(ingestion_request(project, json.dumps(body))['executor'], 0)
            body['context_plugin']['data']['input_files'][0][1] = 'ROUGHNESS'
            with self.assertRaises(ValueError): ingestion_request(project, json.dumps(body))
            roughness = ingestion_request(project, json.dumps(body), 'ROUGHNESS')
            self.assertEqual(roughness['context_plugin']['data']['input_files'][0][1], 'ROUGHNESS')
            self.assertEqual(roughness['executor'], 0)
            body['context_plugin']['data']['input_files'][0][1] = 'METALLIC'
            metallic = ingestion_request(project, json.dumps(body), 'METALLIC')
            self.assertEqual(metallic['executor'], 0)
            self.assertEqual(metallic['context_plugin']['data']['input_files'][0][1], 'METALLIC')
            with self.assertRaises(ValueError): ingestion_request(project, json.dumps(body), 'ROUGHNESS')
            with self.assertRaises(ValueError): ingestion_request(project, json.dumps(body), 'NORMAL')
            body['context_plugin']['data']['input_files'][0][1] = 'DIFFUSE'
            output.mkdir(parents=True); (output / 'cached.dds').write_bytes(b'cached')
            with self.assertRaises(ValueError): ingestion_request(project, json.dumps(body))
            body['context_plugin']['data']['output_directory'] = str(root / 'outside')
            with self.assertRaises(ValueError): ingestion_request(project, json.dumps(body))
            self.assertEqual((output / 'cached.dds').read_bytes(), b'cached')

    def test_invalid_sources_are_rejected(self):
        with self.assertRaises(ValueError):
            capture_destination('relative.usda', 'capture.usd')


if __name__ == '__main__':
    unittest.main()
