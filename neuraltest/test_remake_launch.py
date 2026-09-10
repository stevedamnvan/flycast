# SPDX-License-Identifier: GPL-2.0-or-later
import argparse
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from remake_launch import prepare


class LaunchPreflightTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        fixture = Path(__file__).resolve()
        self.args = argparse.Namespace(**{k: fixture for k in
            ('flycast', 'harness', 'helper', 'runtime', 'game')},
            out=Path(self.temp.name)/'new', anchored_light=False, manual_input=False)

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
