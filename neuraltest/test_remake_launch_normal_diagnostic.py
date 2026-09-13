import unittest
from types import SimpleNamespace
from remake_launch import normal_diagnostic_warmup
class NormalDiagnosticTests(unittest.TestCase):
 def args(self,**kw):
  d=dict(diagnostic_warmup=5900,diagnostic_fill=None,cpu_timing=True,normal_effects=True,renderer='dx11',managed_session=True,anchored_light=True,manual_input=False)
  d.update(kw);return SimpleNamespace(**d)
 def test_late_normal_diagnostic(self):
  self.assertEqual(normal_diagnostic_warmup(self.args()),5900)
 def test_default_unchanged(self):
  self.assertEqual(normal_diagnostic_warmup(SimpleNamespace()),0)
 def test_reject_mixed_or_unbounded_modes(self):
  for change in [dict(diagnostic_warmup=2099),dict(diagnostic_warmup=10001),dict(cpu_timing=False),dict(normal_effects=False),dict(renderer='dx11-oit'),dict(managed_session=False),dict(anchored_light=False),dict(manual_input=True),dict(capture_frames=3),dict(capture_warmup=5900),dict(capture_preroll=30),dict(benchmark_warmup=5900),dict(benchmark_fill='0 0 1 0.3'),dict(scene_fill='0 0 1 0.3'),dict(locked_input_root='archive'),dict(diagnostic_warmup=0,diagnostic_fill='0 0 1 0.3')]:
   with self.subTest(change=change),self.assertRaises(ValueError):normal_diagnostic_warmup(self.args(**change))
if __name__=='__main__':unittest.main()
