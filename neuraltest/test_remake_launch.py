# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch, MagicMock
import remake_launch
from remake_launch import prepare, expected_retirement, orderly_host_shutdown, archive_logs


class LaunchPreflightTests(unittest.TestCase):
    def test_isolated_output_routes_only_helper_and_preserves_preflight(self):
        for managed, run, isolated in ((False, True, True), (True, True, True),
                                       (False, False, True), (False, True, False)):
            with self.subTest(managed=managed, run=run, isolated=isolated), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                binary = root/'input.exe'
                binary.write_bytes(b'test executable')
                out = root/'new-run'
                argv = ['remake_launch.py']
                for name in ('flycast', 'harness', 'helper', 'runtime', 'game'):
                    argv.extend(['--'+name, str(binary)])
                argv.extend(['--out', str(out)])
                if isolated: argv.append('--isolated-runtime-output')
                if managed: argv.append('--managed-session')
                if run: argv.append('--run')
                host = ['host', '--timeout-ms', '1000']
                helper = ['helper']
                process = MagicMock(returncode=0)
                process.poll.return_value = 0
                def managed_stub(out, env, host, helper, record, children, cwd):
                    self.assertEqual(cwd, out/'runtime-output')
                    self.assertTrue(cwd.is_dir())
                    record['exit_codes'] = [0, 0]
                paths = {name: binary for name in ('flycast', 'harness', 'helper', 'runtime', 'game')}
                with patch('sys.argv', argv), patch('builtins.print'), \
                     patch('remake_launch.prepare', return_value=(paths, out, {}, host, helper)), \
                     patch('remake_launch.archive_logs'), \
                     patch('remake_launch.managed_run', side_effect=managed_stub), \
                     patch('remake_launch.subprocess.Popen', return_value=process) as launch:
                    self.assertEqual(remake_launch.main(), 0)
                if not run:
                    self.assertFalse(out.exists())
                    launch.assert_not_called()
                elif not managed:
                    repo = Path(remake_launch.__file__).resolve().parent.parent
                    self.assertEqual(launch.call_args_list[0].kwargs['cwd'], repo)
                    self.assertEqual(launch.call_args_list[1].kwargs['cwd'], out/'runtime-output' if isolated else repo)

    def test_late_benchmark_preserves_capture_free_route(self):
        self.args.benchmark_warmup = 5900
        self.args.anchored_light = self.args.managed_session = True
        self.args.benchmark_fill = '0 0 1 0.3'
        _, _, env, host, helper = prepare(self.args)
        self.assertEqual(host[host.index('--warmup')+1], '5900')
        self.assertEqual(host[host.index('--frames')+1], '1200')
        self.assertEqual(host[host.index('--timeout-ms')+1], '420000')
        self.assertEqual(host[host.index('--remake-evidence')+1], 'none')
        self.assertEqual(helper[helper.index('--scene-fill')+1], '0 0 1 0.3')
        self.assertNotIn('--diagnostic-capture-budget', helper)
        self.assertNotIn('FLYCAST_REMAKE_COMPARE_REMIX_ONLY', env)

    def test_late_benchmark_rejects_mixed_or_unbounded_modes(self):
        self.args.benchmark_warmup = 5900
        self.args.anchored_light = self.args.managed_session = True
        for name, value in [('manual_input', True), ('managed_session', False),
                            ('anchored_light', False), ('capture_frames', 3),
                            ('capture_warmup', 5900), ('cpu_timing', True),
                            ('benchmark_warmup', -1), ('benchmark_warmup', 2099),
                            ('benchmark_warmup', 10001)]:
            with self.subTest(name=name, value=value):
                old = getattr(self.args, name, None)
                setattr(self.args, name, value)
                with self.assertRaisesRegex(ValueError, 'Benchmark warmup'):
                    prepare(self.args)
                if old is None:
                    delattr(self.args, name)
                else:
                    setattr(self.args, name, old)
        self.args.benchmark_fill = '0 0 1 0.3'
        self.args.benchmark_warmup = 0
        with self.assertRaisesRegex(ValueError, 'Benchmark fill'):
            prepare(self.args)
        self.args.benchmark_warmup = 5900
        for invalid in ('0 0 0 1', 'nan 0 1 1', '0 0 1 4'):
            self.args.benchmark_fill = invalid
            with self.assertRaises(ValueError):
                prepare(self.args)
        self.args.benchmark_fill = '0 0 1 0.3'
        self.args.scene_fill = '0 0 1 0.3'
        with self.assertRaisesRegex(ValueError, 'Benchmark fill'):
            prepare(self.args)

    def test_scene_fill_is_bounded_capture_only(self):
        self.args.scene_fill = '0 0 1 0.3'
        with self.assertRaisesRegex(ValueError, 'Scene fill requires'):
            prepare(self.args)
        self.args.capture_frames = 3
        self.args.anchored_light = True
        helper = prepare(self.args)[4]
        self.assertEqual(helper[helper.index('--scene-fill')+1], '0 0 1 0.3')
        for value in ('0 0 1', '0 0 0 1', '0 0 1 4', 'nan 0 1 1', '0 0 1 -1'):
            self.args.scene_fill = value
            with self.assertRaisesRegex(ValueError, 'Scene fill requires'):
                prepare(self.args)
        self.args.scene_fill = '0 0 1 0.3'
        self.args.temple_light_rig = True
        with self.assertRaisesRegex(ValueError, 'Scene fill requires'):
            prepare(self.args)

    def test_late_capture_warmup_cannot_change_performance_defaults(self):
        host = prepare(self.args)[3]
        self.assertEqual(host[host.index('--warmup')+1], '2100')
        self.args.capture_warmup = 5900
        with self.assertRaisesRegex(ValueError, 'Capture warmup requires'):
            prepare(self.args)
        self.args.capture_frames = 3
        host = prepare(self.args)[3]
        self.assertEqual(host[host.index('--warmup')+1], '5900')
        self.assertEqual(host[host.index('--frames')+1], '1200')
        self.assertEqual(host[host.index('--timeout-ms')+1], '420000')
        for invalid in (-1, 2099, 10001):
            self.args.capture_warmup = invalid
            with self.assertRaisesRegex(ValueError, 'Capture warmup requires'):
                prepare(self.args)

    def test_realtime_audio_requires_explicit_request(self):
        with patch.dict(os.environ, {'FLYCAST_AUTOMATION_REALTIME_AUDIO': '1'}):
            self.assertNotIn('FLYCAST_AUTOMATION_REALTIME_AUDIO', prepare(self.args)[2])
            self.args.realtime_audio = True
            self.assertEqual(prepare(self.args)[2]['FLYCAST_AUTOMATION_REALTIME_AUDIO'], '1')

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

    def test_selective_diagnostic_requires_instrumentation(self):
        self.args.anchored_light = True
        self.args.cpu_timing = True
        self.args.diagnostic_selective_resource_refresh = True
        env = prepare(self.args)[2]
        self.assertEqual(env['FLYCAST_REMAKE_SELECTIVE_RESOURCE_REFRESH'], '1')
        self.assertEqual(env['FLYCAST_REMAKE_CPU_TIMING'], '1')
        for field, value in [('anchored_light', False), ('cpu_timing', False),
                             ('capture_frames', 1), ('manual_input', True),
                             ('benchmark_selective_resource_refresh', True),
                             ('selective_resource_refresh', True)]:
            previous = getattr(self.args, field, False)
            setattr(self.args, field, value)
            with self.subTest(field=field), self.assertRaises(ValueError):
                prepare(self.args)
            setattr(self.args, field, previous)

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
