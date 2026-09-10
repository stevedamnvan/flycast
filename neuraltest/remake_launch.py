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
import mmap
import struct


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
    if args.managed_session:
        env['FLYCAST_REMAKE_MANAGED_SESSION'] = '1'
    host = [str(paths['harness']), 'performance', '--game', str(paths['game']),
            '--flycast', str(paths['flycast']), '--out', str(out/'host'),
            '--frames', '1200', '--warmup', '2100', '--lane', 'dlss5', '--api', 'd3d11on12',
            '--renderer', 'dx11-oit', '--input-replay', 'no' if args.manual_input else 'yes', '--render-height', '480',
            '--remake-evidence', 'none', '--timeout-ms', '180000']
    helper = [str(paths['helper']), '--runtime', str(paths['runtime']), '--frames', '660',
              '--live-channel-async', channel, '--assets', str(out), '--clips', '0.1', '2501',
              '--return-d3d9-scene-memory-depth', str(out/'unused-return.bmp')]
    reinit = getattr(args, 'renderer_reinit_after', 0)
    if not 0 <= reinit <= 10000:
        raise ValueError('Renderer restart frame must be 0..10000')
    if reinit:
        host.extend(['--renderer-reinit-after', str(reinit)])
    if args.anchored_light:
        helper.append('--scene-light-anchor')
    return paths, out, env, host, helper


def expected_retirement(code, log):
    return code == 0 or (code == 11 and any(reason in log for reason in (
        'live source failed: live channel bounded receive timeout',
        'live source failed: channel-closed')))


def orderly_host_shutdown(host_code, helper_code, log):
    return (host_code == 0 and helper_code == 11 and
            'live source failed: channel-closed' in log and
            ' published=1 ' in log)


def managed_run(out, env, host, helper, record, children):
    root = env['FLYCAST_REMAKE_ASYNC_CHANNEL']
    # Only the renderer increments generation. Mapping creation is launcher-owned.
    with mmap.mmap(-1, 16, tagname='Local\\FlycastRemake-'+root+'-control') as control:
        if control[:] != bytes(16):
            raise ValueError('Session controller name already owned')
        control[:] = struct.pack('<IIII', 0x534d5246, 1, 0, os.getpid())
        logs = []
        current = None
        generation = 0
        record['sessions'] = []
        try:
            pub = (out/'publisher.log').open('x'); logs.append(pub)
            process = subprocess.Popen(host, cwd=Path(__file__).resolve().parent.parent,
                env=env, stdout=pub, stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW)
            children.append(process)
            deadline = time.monotonic()+210
            while process.poll() is None:
                if time.monotonic() >= deadline:
                    raise TimeoutError('Managed session deadline')
                requested = struct.unpack_from('<I', control, 8)[0]
                if requested != generation:
                    if requested != generation+1 or requested > 8:
                        raise ValueError('Invalid session generation advance')
                    if current is not None:
                        # Retired publisher no longer sends. Let the helper's
                        # bounded receive timeout unwind its GPU resources.
                        current.wait(timeout=8)
                        record['sessions'][-1].update(exit_code=current.returncode, superseded=True)
                        old_log = (out/('consumer-g'+str(generation)+'.log')).read_text(errors='replace')
                        if not expected_retirement(current.returncode, old_log):
                            raise RuntimeError('Superseded helper failed unexpectedly; refusing to hide failure')
                    command = helper.copy()
                    command[command.index('--live-channel-async')+1] = root+'-g'+str(requested)
                    log = (out/('consumer-g'+str(requested)+'.log')).open('x'); logs.append(log)
                    current = subprocess.Popen(command, cwd=Path(__file__).resolve().parent.parent,
                        env=env, stdout=log, stderr=subprocess.STDOUT,
                        creationflags=subprocess.CREATE_NO_WINDOW)
                    children.append(current)
                    record['sessions'].append(dict(generation=requested, command=command, superseded=False))
                    generation = requested
                time.sleep(.05)
            if current is not None:
                current.wait(timeout=max(1, deadline-time.monotonic()))
                record['sessions'][-1]['exit_code'] = current.returncode
            record['exit_codes'] = [process.returncode, current.returncode if current else 1]
            final_log = (out/('consumer-g'+str(generation)+'.log')).read_text(errors='replace') if current else ''
            record['orderly_host_shutdown'] = orderly_host_shutdown(*record['exit_codes'], final_log)
        finally:
            for log in logs:
                log.close()


def archive_logs(host_directory, out):
    for name in ('flycast.log', 'ReShade.log'):
        source = host_directory/name
        if source.exists():
            target = out/('previous-'+name)
            if target.exists():
                raise ValueError('Previous log archive already exists')
            shutil.copy2(source, target)
            if source.read_bytes() != target.read_bytes():
                raise ValueError('Previous log backup mismatch')
            source.unlink()  # Exact named log only; verified recoverable copy is retained.


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('flycast', 'harness', 'helper', 'runtime', 'game', 'out'):
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--anchored-light', action='store_true')
    p.add_argument('--manual-input', action='store_true',
                   help='Use player input instead of scripted replay; still a bounded test session')
    p.add_argument('--managed-session', action='store_true',
                   help='Experimental fresh helper/channel on renderer restart; maximum eight generations')
    p.add_argument('--renderer-reinit-after', type=int, default=0,
                   help='Developer-only restart injection at main frame 1..10000')
    p.add_argument('--run', action='store_true', help='Actually launch; default is read-only preflight')
    args = p.parse_args()
    paths, out, env, host, helper = prepare(args)
    # Do not hash or read the supplied third-party runtime internally.
    record = dict(host=host, helper=helper, anchored_light=args.anchored_light,
                  manual_input=args.manual_input,
                  managed_session=args.managed_session,
                  scope='diagnostic anchored scene, not recovered world camera',
                  external_configuration_modified=False, external_provenance_verified=False,
                  executable_hashes={k: hashlib.sha256(paths[k].read_bytes()).hexdigest()
                                     for k in ('flycast', 'harness', 'helper')})
    if not args.run:
        print(json.dumps(record, indent=2))
        return 0
    out.mkdir(parents=True, exist_ok=False)
    # Preserve only known log files; never move configurations or runtime files.
    archive_logs(paths['flycast'].parent, out)
    children = []
    try:
        if args.managed_session:
            managed_run(out, env, host, helper, record, children)
        else:
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
    return 0 if (record.get('exit_codes') == [0, 0] or record.get('orderly_host_shutdown', False)) and not forced else 1


if __name__ == '__main__':
    raise SystemExit(main())
