import unittest
from types import SimpleNamespace
from remake_launch import capture_evaluation_start

class CapturePrerollTests(unittest.TestCase):
    def args(self, **kw):
        values=dict(capture_start_source=5300,capture_preroll=300,capture_frames=300,
                    capture_warmup=5000,effect_identity=True,managed_session=True,
                    manual_input=False,locked_input_root=None)
        values.update(kw)
        return SimpleNamespace(**values)
    def test_requested_preview_boundary_is_preserved(self):
        args=self.args()
        self.assertEqual(capture_evaluation_start(args),5000)
        self.assertEqual(args.capture_start_source,5300)
        self.assertEqual(args.capture_frames,300)
    def test_default_has_no_behavior_change(self):
        self.assertEqual(capture_evaluation_start(self.args(capture_preroll=0)),5300)
    def test_unsafe_or_unavailable_preroll_rejected(self):
        for change in [dict(capture_preroll=-1),dict(capture_preroll=301),
                       dict(capture_warmup=5300),dict(capture_frames=0),
                       dict(effect_identity=False),dict(managed_session=False),
                       dict(manual_input=True),dict(locked_input_root='archive')]:
            with self.subTest(change=change), self.assertRaises(ValueError):
                capture_evaluation_start(self.args(**change))
if __name__=='__main__': unittest.main()
