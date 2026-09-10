# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from remake_launch import prepare, expected_retirement, orderly_host_shutdown, archive_logs


class LaunchPreflightTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        fixture = Path(__file__).resolve()
        self.args = argparse.Namespace(**{k: fixture for k in
            ('flycast', 'harness', 'helper', 'runtime', 'game')},
            out=Path(self.temp.name)/'new', anchored_light=False, manual_input=False, managed_session=False)

    def test_no_writes_and_explicit_opt_in(self):
        _, out, _, host, helper = prepare(self.args)
        self.assertFalse(out.exists())
        self.assertNotIn('--scene-light-anchor', helper)
        self.args.anchored_light = True
        self.assertIn('--scene-light-anchor', prepare(self.args)[4])
        self.assertEqual(host[host.index('--remake-evidence')+1], 'none')

    def test_inherited_controls_scrubbed_without_parent_mutation(self):
        with patch.dict(os.environ, {'FLYCAST_REMAKE_CPU_TIMING': '1',
                                    'FLYCAST_REMAKE_ASYNC_LOCKED_INPUT_ROOT': 'stale'}):
            env = prepare(self.args)[2]
            self.assertNotIn('FLYCAST_REMAKE_CPU_TIMING', env)
            self.assertNotIn('FLYCAST_REMAKE_ASYNC_LOCKED_INPUT_ROOT', env)
            self.assertEqual(os.environ['FLYCAST_REMAKE_CPU_TIMING'], '1')

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
        self.args.manual_input = True
        _, _, env, host, _ = prepare(self.args)
        self.assertEqual(host[host.index('--input-replay')+1], 'no')
        self.assertEqual(env['FLYCAST_REMAKE_ASYNC_START_PRODUCER'], '0')
        self.assertEqual(host[host.index('--timeout-ms')+1], '180000')


if __name__ == '__main__':
    unittest.main()
