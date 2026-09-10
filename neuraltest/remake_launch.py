#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Bounded opt-in Soulcalibur Remix experiment; requires a user-prepared host."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import shutil
import time
import uuid


def prepare(args):
    paths = {key: Path(getattr(args, key)).resolve(strict=True)
             for key in ('flycast', 'harness', 'helper', 'runtime', 'game')}
    if not all(p.is_file() for p in paths.values()):
        raise ValueError('All input paths must name existing files')
    out = args.out.resolve()
    if out.exists():
        raise ValueError('Output must be a new directory')
    if any(out == p.parent or out in p.parents for p in paths.values()):
        raise ValueError('Output cannot contain an input')
    channel = 'fc067-launch-' + uuid.uuid4().hex[:16]
    env = os.environ.copy()
    # Avoid accidentally inheriting capture, stale-input or negative controls.
    for key in list(env):
        if key.startswith('FLYCAST_REMAKE_'):
            del env[key]
    enabled = ('TEMPORAL_RASTER', 'TEMPORAL_PREPARE', 'CAMERA_ANCHOR',
               'ALPHA_COMBINED', 'PUNCH_THROUGH', 'ASYNC_OIT', 'ASYNC_DIAGNOSTICS',
               'ESTIMATE_UNTRACED', 'ASYNC_PRESENT', 'ASYNC_NEURAL', 'NATIVE_EFFECTS')
    env.update({'FLYCAST_REMAKE_'+key: '1' for key in enabled})
    env.update(FLYCAST_NEURAL_SOURCE_OBSERVATION='1', FLYCAST_REMAKE_COLOR_CONSISTENCY='0',
               FLYCAST_REMAKE_EFFECT_IDENTITY='0', FLYCAST_REMAKE_ASYNC_CHANNEL=channel,
               FLYCAST_REMAKE_ASYNC_START_PRODUCER='0' if args.manual_input else '2090')
    host = [str(paths['harness']), 'performance', '--game', str(paths['game']),
            '--flycast', str(paths['flycast']), '--out', str(out/'host'),
            '--frames', '1200', '--warmup', '2100', '--lane', 'dlss5', '--api', 'd3d11on12',
            '--renderer', 'dx11-oit', '--input-replay', 'no' if args.manual_input else 'yes', '--render-height', '480',
            '--remake-evidence', 'none', '--timeout-ms', '180000']
    helper = [str(paths['helper']), '--runtime', str(paths['runtime']), '--frames', '660',
              '--live-channel-async', channel, '--assets', str(out), '--clips', '0.1', '2501',
              '--return-d3d9-scene-memory-depth', str(out/'unused-return.bmp')]
    if args.anchored_light:
        helper.append('--scene-light-anchor')
    return paths, out, env, host, helper


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('flycast', 'harness', 'helper', 'runtime', 'game', 'out'):
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--anchored-light', action='store_true')
    p.add_argument('--manual-input', action='store_true',
                   help='Use player input instead of scripted replay; still a bounded test session')
    p.add_argument('--run', action='store_true', help='Actually launch; default is read-only preflight')
    args = p.parse_args()
    paths, out, env, host, helper = prepare(args)
    # Do not hash or read the supplied third-party runtime internally.
    record = dict(host=host, helper=helper, anchored_light=args.anchored_light,
                  manual_input=args.manual_input,
                  scope='diagnostic anchored scene, not recovered world camera',
                  external_configuration_modified=False, external_provenance_verified=False,
                  executable_hashes={k: hashlib.sha256(paths[k].read_bytes()).hexdigest()
                                     for k in ('flycast', 'harness', 'helper')})
    if not args.run:
        print(json.dumps(record, indent=2))
        return 0
    out.mkdir(parents=True, exist_ok=False)
    # Preserve only known log files; never move configurations or runtime files.
    for name in ('flycast.log', 'ReShade.log'):
        source = paths['flycast'].parent/name
        if source.exists():
            target = out/('previous-'+name)
            shutil.copy2(source, target)
            if source.read_bytes() != target.read_bytes():
                raise ValueError('Previous log backup mismatch')
    children = []
    try:
        with (out/'publisher.log').open('x') as pub, (out/'consumer.log').open('x') as con:
            for command, log in ((host, pub), (helper, con)):
                children.append(subprocess.Popen(command, cwd=Path(__file__).resolve().parent.parent, env=env,
                    stdout=log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW))
            deadline = time.monotonic()+210
            for child in children:
                child.wait(timeout=max(1, deadline-time.monotonic()))
            record['exit_codes'] = [child.returncode for child in children]
    finally:
        forced = []
        for child in children:
            if child.poll() is None:
                child.terminate()
                child.wait(timeout=10)
                forced.append(child.pid)
        record['forced_children'] = forced
        (out/'launch.json').write_text(json.dumps(record, indent=2))
    return 0 if record.get('exit_codes') == [0, 0] and not forced else 1


if __name__ == '__main__':
    raise SystemExit(main())
