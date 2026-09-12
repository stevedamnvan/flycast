import tempfile
import unittest
from pathlib import Path
from remix_capture_mcp import capture_destination, displacement_request


class CaptureDestinationTests(unittest.TestCase):
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

    def test_invalid_sources_are_rejected(self):
        with self.assertRaises(ValueError):
            capture_destination('relative.usda', 'capture.usd')


if __name__ == '__main__':
    unittest.main()
