# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from remake_launch import prepare, expected_retirement, orderly_host_shutdown, archive_logs


class LaunchPreflightTests(unittest.TestCase):
    def test_selective_refresh_requires_capture_and_anchored_lights(self):
        with patch.dict(os.environ, {'FLYCAST_REMAKE_SELECTIVE_RESOURCE_REFRESH': '1'}):
            self.assertNotIn('FLYCAST_REMAKE_SELECTIVE_RESOURCE_REFRESH', prepare(self.args)[2])
        self.args.selective_resource_refresh = True
        with self.assertRaisesRegex(ValueError, 'requires bounded capture and anchored light'):
            prepare(self.args)
        self.args.capture_frames = 30
        with self.assertRaisesRegex(ValueError, 'requires bounded capture and anchored light'):
            prepare(self.args)
        self.args.anchored_light = True
        self.assertEqual(prepare(self.args)[2]['FLYCAST_REMAKE_SELECTIVE_RESOURCE_REFRESH'], '1')

    def test_shading_motion_requires_explicit_capture_and_colour_check(self):
        with patch.dict(os.environ, {'FLYCAST_REMAKE_SHADING_AWARE_MOTION': '1',
                                    'FLYCAST_REMAKE_COLOR_CONSISTENCY': '1'}):
            env = prepare(self.args)[2]
            self.assertNotIn('FLYCAST_REMAKE_SHADING_AWARE_MOTION', env)
            self.assertEqual(env['FLYCAST_REMAKE_COLOR_CONSISTENCY'], '0')
        self.args.shading_aware_motion = True
        with self.assertRaisesRegex(ValueError, 'requires bounded combined capture'):
            prepare(self.args)
        self.args.capture_frames = 30
        env = prepare(self.args)[2]
        self.assertEqual(env['FLYCAST_REMAKE_SHADING_AWARE_MOTION'], '1')
        self.assertEqual(env['FLYCAST_REMAKE_COLOR_CONSISTENCY'], '1')
        self.args.remix_only = True
        with self.assertRaisesRegex(ValueError, 'requires bounded combined capture'):
            prepare(self.args)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        fixture = Path(__file__).resolve()
        self.args = argparse.Namespace(**{k: fixture for k in
            ('flycast', 'harness', 'helper', 'runtime', 'game')},
            out=Path(self.temp.name)/'new', anchored_light=False, manual_input=False, managed_session=False)

    def test_normal_effect_proof_requires_explicit_diagnostic_route(self):
        with patch.dict(os.environ, {'FLYCAST_REMAKE_NORMAL_EFFECT_PROOF': '1'}):
            self.assertNotIn('FLYCAST_REMAKE_NORMAL_EFFECT_PROOF', prepare(self.args)[2])
        self.args.normal_effect_proof = True
        with self.assertRaisesRegex(ValueError, 'requires dx11'):
            prepare(self.args)
        self.args.renderer = 'dx11'
        with self.assertRaisesRegex(ValueError, 'diagnostic CPU timing'):
            prepare(self.args)
        self.args.cpu_timing = True
        env = prepare(self.args)[2]
        self.assertEqual(env['FLYCAST_REMAKE_NORMAL_EFFECT_PROOF'], '1')
        self.assertEqual(Path(env['FLYCAST_REMAKE_NORMAL_EFFECT_PROOF_ROOT']), self.args.out/'normal-effects')

    def test_normal_effects_cannot_enter_performance_route_implicitly(self):
        with patch.dict(os.environ, {'FLYCAST_REMAKE_NORMAL_EFFECTS': '1'}):
            self.assertNotIn('FLYCAST_REMAKE_NORMAL_EFFECTS', prepare(self.args)[2])
        self.args.normal_effects = True
        with self.assertRaisesRegex(ValueError, 'requires dx11'):
            prepare(self.args)
        self.args.renderer = 'dx11'
        with self.assertRaisesRegex(ValueError, 'diagnostic CPU timing'):
            prepare(self.args)
        self.args.cpu_timing = True
        self.assertEqual(prepare(self.args)[2]['FLYCAST_REMAKE_NORMAL_EFFECTS'], '1')

    def test_no_writes_and_explicit_opt_in(self):
        _, out, _, host, helper = prepare(self.args)
        self.assertFalse(out.exists())
        self.assertNotIn('--scene-light-anchor', helper)
        self.args.anchored_light = True
        self.assertIn('--scene-light-anchor', prepare(self.args)[4])
        self.assertEqual(host[host.index('--remake-evidence')+1], 'none')

    def test_depth_format_controls_require_explicit_diagnostic_selection(self):
        with patch.dict(os.environ, {'FLYCAST_REMAKE_VERIFY_DEPTH_FORMAT': '1',
                                    'FLYCAST_REMAKE_DEPTH_RGBA32F': '1'}):
            env = prepare(self.args)[2]
            self.assertNotIn('FLYCAST_REMAKE_VERIFY_DEPTH_FORMAT', env)
            self.assertNotIn('FLYCAST_REMAKE_DEPTH_RGBA32F', env)
        self.args.verify_depth_format = True
        with self.assertRaisesRegex(ValueError, 'requires CPU timing'):
            prepare(self.args)
        self.args.cpu_timing = True
        self.assertEqual(prepare(self.args)[2]['FLYCAST_REMAKE_VERIFY_DEPTH_FORMAT'], '1')
        self.args.depth_rgba32f = True
        with self.assertRaisesRegex(ValueError, 'R32F selection'):
            prepare(self.args)
        self.args.verify_depth_format = False
        self.assertEqual(prepare(self.args)[2]['FLYCAST_REMAKE_DEPTH_RGBA32F'], '1')

    def test_inherited_controls_scrubbed_without_parent_mutation(self):
        with patch.dict(os.environ, {'FLYCAST_REMAKE_CPU_TIMING': '1',
                                    'FLYCAST_REMAKE_ASYNC_LOCKED_INPUT_ROOT': 'stale'}):
            env = prepare(self.args)[2]
            self.assertNotIn('FLYCAST_REMAKE_CPU_TIMING', env)
            self.assertNotIn('FLYCAST_REMAKE_ASYNC_LOCKED_INPUT_ROOT', env)
            self.assertEqual(os.environ['FLYCAST_REMAKE_CPU_TIMING'], '1')

    def test_temple_rig_requires_anchor_and_preserves_argument_order(self):
        self.assertNotIn('--temple-light-rig', prepare(self.args)[4])
        self.args.temple_light_rig = True
        with self.assertRaisesRegex(ValueError, 'requires anchored'):
            prepare(self.args)
        self.args.anchored_light = True
        helper = prepare(self.args)[4]
        self.assertEqual(helper[-4:], ['--scene-light-radiance', '1', '--temple-light-rig', '--scene-light-anchor'])

    def test_extent_is_explicit_and_shared_with_native_rendering(self):
        _, _, env, host, _ = prepare(self.args)
        self.assertEqual(env['FLYCAST_REMAKE_OUTPUT_SIZE'], '640x480')
        self.args.output_size = '1280x960'
        _, _, env, host, _ = prepare(self.args)
        self.assertEqual(env['FLYCAST_REMAKE_OUTPUT_SIZE'], '1280x960')
        self.assertEqual(host[host.index('--render-height')+1], '960')
        self.args.output_size = '1280x480'
        with self.assertRaisesRegex(ValueError, 'Unsupported'):
            prepare(self.args)

    def test_existing_output_rejected(self):
        self.args.out = Path(self.temp.name)
        with self.assertRaises(ValueError):
            prepare(self.args)

    def test_missing_input_rejected(self):
        self.args.runtime = Path(self.temp.name)/'missing'
        with self.assertRaises(FileNotFoundError):
            prepare(self.args)

    def test_unique_channels(self):
        self.assertNotEqual(prepare(self.args)[2]['FLYCAST_REMAKE_ASYNC_CHANNEL'],
                            prepare(self.args)[2]['FLYCAST_REMAKE_ASYNC_CHANNEL'])

    def test_archive_preserves_exact_logs_and_config(self):
        root=Path(self.temp.name);out=root/'archive';out.mkdir()
        (root/'flycast.log').write_bytes(b'old\r\nlog\x00')
        (root/'reshade.ini').write_bytes(b'user-owned')
        archive_logs(root,out)
        self.assertFalse((root/'flycast.log').exists())
        self.assertEqual((out/'previous-flycast.log').read_bytes(), b'old\r\nlog\x00')
        self.assertEqual((root/'reshade.ini').read_bytes(), b'user-owned')

    def test_failed_archive_keeps_original(self):
        root=Path(self.temp.name);out=root/'archive';out.mkdir()
        source=root/'flycast.log';source.write_bytes(b'keep')
        with patch('remake_launch.shutil.copy2', side_effect=OSError('copy failed')):
            with self.assertRaises(OSError):
                archive_logs(root,out)
        self.assertEqual(source.read_bytes(), b'keep')

    def test_superseded_crash_is_not_success(self):
        self.assertTrue(expected_retirement(0, ''))
        self.assertTrue(expected_retirement(11, 'live source failed: live channel bounded receive timeout'))
        self.assertTrue(expected_retirement(11, 'live source failed: channel-closed'))
        self.assertFalse(expected_retirement(11, 'live source continuity rejected'))
        self.assertFalse(expected_retirement(124, 'live source failed: live channel bounded receive timeout'))
        self.assertFalse(expected_retirement(-1, ''))

    def test_managed_mode_is_explicit(self):
        self.assertNotIn('FLYCAST_REMAKE_MANAGED_SESSION', prepare(self.args)[2])
        self.args.managed_session = True
        self.assertEqual(prepare(self.args)[2]['FLYCAST_REMAKE_MANAGED_SESSION'], '1')
        self.assertIn('--session-worker',prepare(self.args)[4])
        self.args.renderer_reinit_after = 10001
        with self.assertRaises(ValueError):
            prepare(self.args)

    def test_capture_is_bounded_and_separate_from_performance(self):
        default=prepare(self.args)
        self.assertNotIn('FLYCAST_REMAKE_MOVING_CAPTURE',default[2])
        self.args.capture_frames=120;self.args.capture_start_source=2700
        _,_,env,host,helper=prepare(self.args)
        self.assertEqual(env['FLYCAST_REMAKE_PREVIEW_CAPTURE_FRAMES'],'120')
        self.assertEqual(env['FLYCAST_REMAKE_PREVIEW_START_SOURCE'],'2700')
        self.assertEqual(host[host.index('--timeout-ms')+1],'420000')
        self.assertEqual(helper[-1],'--diagnostic-capture-budget')
        self.args.capture_frames=301
        with self.assertRaises(ValueError):
            prepare(self.args)

    def test_orderly_host_shutdown_requires_delivery_and_clean_host(self):
        log = 'live_return sequence=1 published=1 depth_values=307200\nlive source failed: channel-closed'
        self.assertTrue(orderly_host_shutdown(0, 11, log))
        self.assertFalse(orderly_host_shutdown(1, 11, log))
        self.assertFalse(orderly_host_shutdown(0, 124, log))
        self.assertFalse(orderly_host_shutdown(0, 11, 'live source failed: channel-closed'))
        self.assertFalse(orderly_host_shutdown(0, 11, ' published=1 live source failed: live channel bounded receive timeout'))

    def test_manual_input_is_explicit_and_not_replay_gated(self):
        default = prepare(self.args)
        self.assertEqual(default[3][default[3].index('--input-replay')+1], 'yes')
        self.assertEqual(default[3][default[3].index('--renderer')+1], 'dx11-oit')
        self.args.renderer = 'dx11'
        self.assertEqual(prepare(self.args)[3][prepare(self.args)[3].index('--renderer')+1], 'dx11')
        self.args.renderer = 'dx11-oit'
        self.assertEqual(default[3][default[3].index('--timeout-ms')+1], '180000')
        self.assertNotIn('FLYCAST_REMAKE_CPU_TIMING', default[2])
        self.assertEqual(default[2]['FLYCAST_REMAKE_ALPHA_COMBINED'], '1')
        self.assertNotIn('FLYCAST_REMAKE_OPAQUE_ALPHA_ONE', default[2])
        self.args.alpha_combined_off = True; self.args.opaque_alpha_one = True
        ab = prepare(self.args)[2]
        self.assertEqual(ab['FLYCAST_REMAKE_ALPHA_COMBINED'], '0')
        self.assertEqual(ab['FLYCAST_REMAKE_OPAQUE_ALPHA_ONE'], '1')
        self.args.alpha_combined_off = False; self.args.opaque_alpha_one = False
        self.assertNotIn('FLYCAST_REMAKE_ALPHA_CUTOUT', prepare(self.args)[2])
        self.args.alpha_cutout = True
        cut = prepare(self.args)[2]
        self.assertEqual(cut['FLYCAST_REMAKE_ALPHA_CUTOUT'], '1')
        self.assertEqual(cut['FLYCAST_REMAKE_ALPHA_COMBINED'], '1')
        self.args.alpha_combined_off = True
        with self.assertRaises(ValueError):
            prepare(self.args)
        self.args.alpha_combined_off = False; self.args.alpha_cutout = False
        self.assertNotIn('FLYCAST_REMAKE_CURVED_EXPORT', prepare(self.args)[2])
        self.args.curved_export = True
        with self.assertRaises(ValueError):
            prepare(self.args)
        self.args.smooth_normals_weld = True
        self.assertEqual(prepare(self.args)[2]['FLYCAST_REMAKE_CURVED_EXPORT'], '1')
        self.args.curved_export = False; self.args.smooth_normals_weld = False
        self.assertNotIn('FLYCAST_REMAKE_SMOOTH_NORMALS', prepare(self.args)[2])
        self.args.smooth_normals = True
        self.assertEqual(prepare(self.args)[2]['FLYCAST_REMAKE_SMOOTH_NORMALS'], '1')
        self.args.smooth_normals = False
        self.args.smooth_normals_weld = True
        self.assertEqual(prepare(self.args)[2]['FLYCAST_REMAKE_SMOOTH_NORMALS'], '2')
        self.args.smooth_normals_weld = False
        self.args.cpu_timing = True
        self.assertEqual(prepare(self.args)[2]['FLYCAST_REMAKE_CPU_TIMING'], '1')
        self.assertNotIn('FLYCAST_REMAKE_HOOK_CYCLES', prepare(self.args)[2])
        self.args.hook_cycles = True
        self.assertEqual(prepare(self.args)[2]['FLYCAST_REMAKE_HOOK_CYCLES'], '1')
        self.args.hook_cycles = False
        self.assertNotIn('FLYCAST_REMAKE_HOOK_ATTRIBUTION', prepare(self.args)[2])
        self.args.hook_attribution = True
        self.assertEqual(prepare(self.args)[2]['FLYCAST_REMAKE_HOOK_ATTRIBUTION'], '1')
        self.args.cpu_timing = False
        self.args.hook_cycles = True
        self.assertNotIn('FLYCAST_REMAKE_HOOK_CYCLES', prepare(self.args)[2])
        self.assertNotIn('FLYCAST_REMAKE_HOOK_ATTRIBUTION', prepare(self.args)[2])
        self.args.hook_cycles = False
        self.args.hook_attribution = False
        self.assertNotIn('FLYCAST_REMAKE_GUEST_FRAME_DIGEST', prepare(self.args)[2])
        self.args.guest_frame_digest = True
        self.assertEqual(prepare(self.args)[2]['FLYCAST_REMAKE_GUEST_FRAME_DIGEST'], '1')
        self.args.guest_frame_digest = False
        self.assertNotIn('FLYCAST_REMAKE_OBSERVATION_SCOPE', prepare(self.args)[2])
        self.args.observation_scope = 'narrow'
        self.assertEqual(prepare(self.args)[2]['FLYCAST_REMAKE_OBSERVATION_SCOPE'], 'narrow')
        self.args.observation_scope = 'full'
        self.assertNotIn('FLYCAST_REMAKE_OBSERVATION_SCOPE', prepare(self.args)[2])
        self.args.scope_gates = 'off'
        with self.assertRaises(ValueError):
            prepare(self.args)
        self.args.observation_scope = 'narrow'; self.args.scope_parts = 'none'
        env = prepare(self.args)[2]
        self.assertEqual((env['FLYCAST_REMAKE_OBSERVATION_SCOPE_GATES'], env['FLYCAST_REMAKE_OBSERVATION_SCOPE_PARTS']), ('off', 'none'))
        self.args.scope_gates = None; self.args.scope_parts = None; self.args.observation_scope = 'full'
        self.assertNotIn('FLYCAST_REMAKE_FRAME_BUDGET_MS', default[2])
        self.args.frame_budget_ms = 4.0
        self.assertEqual(prepare(self.args)[2]['FLYCAST_REMAKE_FRAME_BUDGET_MS'], '4.0')
        self.args.frame_budget_ms = 0.0
        with self.assertRaises(ValueError):
            prepare(self.args)
        self.args.frame_budget_ms = None
        self.assertNotIn('DXVK_RTX_CONFIG_FILE', default[2])
        conf = Path(self.temp.name)/'variant.conf'
        conf.write_text('rtx.pathMaxBounces = 1\n')
        self.args.consumer_config = conf
        self.assertEqual(prepare(self.args)[2]['DXVK_RTX_CONFIG_FILE'], str(conf.resolve()))
        self.assertEqual(conf.read_text(), 'rtx.pathMaxBounces = 1\n')
        self.args.consumer_config = Path(self.temp.name)/'missing.conf'
        with self.assertRaises(FileNotFoundError):
            prepare(self.args)
        self.args.consumer_config = Path(__file__).resolve()
        with self.assertRaises(ValueError):
            prepare(self.args)
        self.args.consumer_config = None
        self.assertNotIn('--source-wait-seconds', default[4])
        self.args.manual_input = True
        _, _, env, host, helper = prepare(self.args)
        self.assertEqual(host[host.index('--input-replay')+1], 'no')
        self.assertEqual(env['FLYCAST_REMAKE_ASYNC_START_PRODUCER'], '0')
        self.assertEqual(host[host.index('--timeout-ms')+1], '420000')
        self.assertEqual(host[host.index('--warmup')+1], '3000')
        self.assertEqual(host[host.index('--frames')+1], '9000')
        self.assertEqual(helper[-2:], ['--source-wait-seconds', '180'])
        self.assertEqual(helper.count('--diagnostic-capture-budget'), 1)
        self.assertLess(helper.index('--diagnostic-capture-budget'), helper.index('--source-wait-seconds'))

    def test_selective_benchmark_is_explicit_and_capture_free(self):
        self.args.anchored_light = True
        self.args.benchmark_selective_resource_refresh = True
        self.assertEqual(prepare(self.args)[2]['FLYCAST_REMAKE_SELECTIVE_RESOURCE_REFRESH'], '1')
        for field, value in [('anchored_light', False), ('capture_frames', 1),
                             ('manual_input', True), ('cpu_timing', True),
                             ('selective_resource_refresh', True),
                             ('scope_gates', 'off'), ('scope_parts', 'none')]:
            existed = hasattr(self.args, field)
            previous = getattr(self.args, field, None)
            setattr(self.args, field, value)
            with self.subTest(field=field), self.assertRaises(ValueError):
                prepare(self.args)
            if existed:
                setattr(self.args, field, previous)
            else:
                delattr(self.args, field)
        self.args.benchmark_selective_resource_refresh = False
        self.assertNotIn('FLYCAST_REMAKE_SELECTIVE_RESOURCE_REFRESH', prepare(self.args)[2])

    def test_remix_only_requires_capture(self):
        self.args.remix_only = True
        with self.assertRaises(ValueError):
            prepare(self.args)
        self.args.capture_frames = 120
        self.assertEqual(prepare(self.args)[2]['FLYCAST_REMAKE_COMPARE_REMIX_ONLY'], '1')

    def test_exact_effects_capture_is_bounded(self):
        self.args.effect_identity = True
        for count,start in [(0,2700),(31,2700),(30,0)]:
            self.args.capture_frames=count;self.args.capture_start_source=start
            with self.assertRaises(ValueError):
                prepare(self.args)
        self.args.capture_frames=30;self.args.capture_start_source=2700
        env=prepare(self.args)[2]
        self.assertEqual(env['FLYCAST_REMAKE_EFFECT_IDENTITY'],'1')
        self.assertEqual(env['FLYCAST_REMAKE_COMPARE_START_FRAME'],'2700')
        self.args.locked_input_root=self.args.game.parent
        with self.assertRaises(ValueError):
            prepare(self.args)

    def test_returned_dlaa_is_distinct(self):
        self.args.returned_dlaa=True
        with self.assertRaises(ValueError):
            prepare(self.args)
        self.args.capture_frames=30
        host=prepare(self.args)[3]
        self.assertEqual(host[host.index('--lane')+1],'dlaa')
        self.args.remix_only=True
        with self.assertRaises(ValueError):
            prepare(self.args)

    def test_extended_effects_are_explicit(self):
        self.args.effect_identity=True;self.args.capture_frames=300;self.args.capture_start_source=2400
        with self.assertRaises(ValueError):
            prepare(self.args)
        self.args.extended_effect_capture=True
        env=prepare(self.args)[2]
        self.assertEqual(env['FLYCAST_REMAKE_EXTENDED_EFFECT_CAPTURE'],'1')
        self.assertEqual(env['FLYCAST_REMAKE_COMPARE_END_FRAME'],'2699')
        self.args.capture_start_source=10000000
        with self.assertRaises(ValueError):
            prepare(self.args)


if __name__ == '__main__':
    unittest.main()
