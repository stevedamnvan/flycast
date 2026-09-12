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
    output_size = getattr(args, 'output_size', '640x480')
    if output_size not in ('640x480', '1280x960'):
        raise ValueError('Unsupported remake output size')
    env = os.environ.copy()
    # Avoid accidentally inheriting capture, stale-input or negative controls.
    for key in list(env):
        if key.startswith('FLYCAST_REMAKE_'):
            del env[key]
    env['FLYCAST_REMAKE_OUTPUT_SIZE'] = output_size
    if getattr(args, 'normal_effects', False):
        if getattr(args, 'renderer', 'dx11-oit') != 'dx11' or not getattr(args, 'cpu_timing', False):
            raise ValueError('Normal effects prototype requires dx11 and diagnostic CPU timing')
        env['FLYCAST_REMAKE_NORMAL_EFFECTS'] = '1'
    if getattr(args, 'normal_effect_proof', False):
        if getattr(args, 'renderer', 'dx11-oit') != 'dx11' or not getattr(args, 'cpu_timing', False):
            raise ValueError('Normal effects proof requires dx11 and diagnostic CPU timing')
        env['FLYCAST_REMAKE_NORMAL_EFFECT_PROOF'] = '1'
        env['FLYCAST_REMAKE_NORMAL_EFFECT_PROOF_ROOT'] = str(out / 'normal-effects')

    enabled = ('TEMPORAL_RASTER', 'TEMPORAL_PREPARE', 'CAMERA_ANCHOR',
               'ALPHA_COMBINED', 'PUNCH_THROUGH', 'ASYNC_OIT', 'ASYNC_DIAGNOSTICS',
               'ESTIMATE_UNTRACED', 'ASYNC_PRESENT', 'ASYNC_NEURAL', 'NATIVE_EFFECTS')
    env.update({'FLYCAST_REMAKE_'+key: '1' for key in enabled})
    env.update(FLYCAST_NEURAL_SOURCE_OBSERVATION='1', FLYCAST_REMAKE_COLOR_CONSISTENCY='0',
               FLYCAST_REMAKE_EFFECT_IDENTITY='0', FLYCAST_REMAKE_ASYNC_CHANNEL=channel,
               FLYCAST_REMAKE_ASYNC_START_PRODUCER='0' if args.manual_input else '2090')
    if args.managed_session:
        env['FLYCAST_REMAKE_MANAGED_SESSION'] = '1'
    # D-220 translucency A/B (diagnostic controls, both default off/unchanged).
    if getattr(args, 'alpha_combined_off', False):
        env['FLYCAST_REMAKE_ALPHA_COMBINED'] = '0'
    if getattr(args, 'opaque_alpha_one', False):
        env['FLYCAST_REMAKE_OPAQUE_ALPHA_ONE'] = '1'
    # D-240 hair option 1 (experimental, default off): promoted alpha draws with
    # cutout texture alpha travel as alpha-tested cutouts; needs alpha ownership.
    if getattr(args, 'alpha_cutout', False):
        if getattr(args, 'alpha_combined_off', False):
            raise ValueError('--alpha-cutout requires the promoted alpha surfaces (not --alpha-combined-off)')
        env['FLYCAST_REMAKE_ALPHA_CUTOUT'] = '1'
    # D-241 candidate (experimental, default off): curved PN-triangle export; needs smoothed normals.
    if getattr(args, 'curved_export', False):
        if not (getattr(args, 'smooth_normals', False) or getattr(args, 'smooth_normals_weld', False)):
            raise ValueError('--curved-export requires --smooth-normals or --smooth-normals-weld')
        env['FLYCAST_REMAKE_CURVED_EXPORT'] = '1'
    # D-223 experimental shading option (default off): smoothed export normals.
    if getattr(args, 'smooth_normals', False):
        env['FLYCAST_REMAKE_SMOOTH_NORMALS'] = '1'
    # D-225 level 2: also weld bit-identical resubmitted vertices within a draw.
    if getattr(args, 'smooth_normals_weld', False):
        env['FLYCAST_REMAKE_SMOOTH_NORMALS'] = '2'
    host = [str(paths['harness']), 'performance', '--game', str(paths['game']),
            '--flycast', str(paths['flycast']), '--out', str(out/'host'),
            '--frames', '1200', '--warmup', '2100', '--lane', 'dlss5', '--api', 'd3d11on12',
            '--renderer', getattr(args, 'renderer', 'dx11-oit'), '--input-replay', 'no' if args.manual_input else 'yes', '--render-height', '480',
            '--remake-evidence', 'none', '--timeout-ms', '180000']
    helper = [str(paths['helper']), '--runtime', str(paths['runtime']), '--frames', '660',
              '--live-channel-async', channel, '--assets', str(out), '--clips', '0.1', '2501',
              '--return-d3d9-scene-memory-depth', str(out/'unused-return.bmp')]
    host[host.index('--render-height')+1] = output_size.split('x')[1]
    reinit = getattr(args, 'renderer_reinit_after', 0)
    if not 0 <= reinit <= 10000:
        raise ValueError('Renderer restart frame must be 0..10000')
    if reinit:
        host.extend(['--renderer-reinit-after', str(reinit)])
    if getattr(args, 'temple_light_rig', False):
        if not args.anchored_light:
            raise ValueError('Temple light rig requires anchored light')
        helper.extend(['--scene-light-radiance', '1', '--temple-light-rig'])
    if args.anchored_light:
        helper.append('--scene-light-anchor')
    if args.managed_session:
        helper.append('--session-worker')
    capture_frames = getattr(args, 'capture_frames', 0)
    benchmark_reuse = getattr(args, 'benchmark_selective_resource_refresh', False)
    if benchmark_reuse:
        if (not args.anchored_light or capture_frames or args.manual_input
                or getattr(args, 'cpu_timing', False)
                or getattr(args, 'selective_resource_refresh', False)
                or getattr(args, 'scope_gates', None) or getattr(args, 'scope_parts', None)):
            raise ValueError('Selective reuse benchmark requires anchored capture-free automatic timing without diagnostic scopes')
        env['FLYCAST_REMAKE_SELECTIVE_RESOURCE_REFRESH'] = '1'
    if getattr(args, 'selective_resource_refresh', False):
        if not capture_frames or not args.anchored_light:
            raise ValueError('Selective resource refresh requires bounded capture and anchored light')
        env['FLYCAST_REMAKE_SELECTIVE_RESOURCE_REFRESH'] = '1'
    if getattr(args, 'shading_aware_motion', False):
        if not capture_frames or getattr(args, 'remix_only', False):
            raise ValueError('Shading-aware motion requires bounded combined capture')
        env['FLYCAST_REMAKE_SHADING_AWARE_MOTION'] = '1'
        env['FLYCAST_REMAKE_COLOR_CONSISTENCY'] = '1'
    capture_start = getattr(args, 'capture_start_source', 0)
    remix_only = getattr(args, 'remix_only', False)
    if remix_only and not capture_frames:
        raise ValueError('Remix-only comparison requires bounded image capture')
    if remix_only:
        env['FLYCAST_REMAKE_COMPARE_REMIX_ONLY'] = '1'
    returned_dlaa = getattr(args, 'returned_dlaa', False)
    if returned_dlaa:
        if remix_only or not capture_frames:
            raise ValueError('Returned DLAA requires capture and cannot skip neural evaluation')
        host[host.index('--lane')+1] = 'dlaa'
    effect_identity = getattr(args, 'effect_identity', False)
    locked = getattr(args, 'locked_input_root', None)
    extended = getattr(args, 'extended_effect_capture', False)
    if extended and not (effect_identity or locked):
        raise ValueError('Extended effects limit requires exact effect capture or replay')
    if effect_identity or locked:
        if not 1 <= capture_frames <= (300 if extended else 30) or capture_start <= 0:
            raise ValueError('Exact effects comparison exceeds explicit bound or lacks source start')
        if capture_start+capture_frames-1 > 10000000:
            raise ValueError('Comparison end exceeds source bound')
        env['FLYCAST_REMAKE_EFFECT_IDENTITY'] = '1'
        env['FLYCAST_REMAKE_COMPARE_START_FRAME'] = str(capture_start)
        env['FLYCAST_REMAKE_COMPARE_END_FRAME'] = str(capture_start+capture_frames-1)
        if extended:
            env['FLYCAST_REMAKE_EXTENDED_EFFECT_CAPTURE'] = '1'
    if locked:
        locked = Path(locked).resolve(strict=True)
        if not locked.is_dir() or not any(locked.glob('*/native-effect-identity.bin')):
            raise ValueError('Locked source requires captured native effect identity')
        env['FLYCAST_REMAKE_ASYNC_LOCKED_INPUT_ROOT'] = str(locked)
    if not 0 <= capture_frames <= 300 or not 0 <= capture_start <= 10000000:
        raise ValueError('Capture bounds exceeded')
    if capture_frames:
        env.update(FLYCAST_REMAKE_MOVING_CAPTURE='1',
                   FLYCAST_REMAKE_PREVIEW_CAPTURE=str(out/'captures'),
                   FLYCAST_REMAKE_PREVIEW_CAPTURE_FRAMES=str(capture_frames),
                   FLYCAST_REMAKE_PREVIEW_START_SOURCE=str(capture_start))
        host[host.index('--timeout-ms')+1] = '420000'
        helper.append('--diagnostic-capture-budget')
    if getattr(args, 'cpu_timing', False):
        # Bounded per-stage CPU scope logging (600 samples per stage) for
        # locating host-side cost; a diagnostic run, never performance evidence.
        env['FLYCAST_REMAKE_CPU_TIMING'] = '1'
        if getattr(args, 'hook_cycles', False):
            # Per-hook time-stamp-counter accounting on the emulation thread
            # (D-218); inflates the emulated frame period by several ms.
            env['FLYCAST_REMAKE_HOOK_CYCLES'] = '1'
        if getattr(args, 'hook_attribution', False):
            # Per-block hook attribution (D-240 groundwork): which guest code the
            # hooks run from and which feeds complete observations; counting only.
            env['FLYCAST_REMAKE_HOOK_ATTRIBUTION'] = '1'
    if getattr(args, 'depth_rgba32f', False):
        env['FLYCAST_REMAKE_DEPTH_RGBA32F'] = '1'
    if getattr(args, 'guest_frame_digest', False):
        # Per-frame hash of the raw TA bytes with the producer cycle: a guest-only
        # signal for matching scenes across runs (LOG907); diagnostic only.
        env['FLYCAST_REMAKE_GUEST_FRAME_DIGEST'] = '1'
    # D-240 narrowed observation scope (experimental, default full): hooks run
    # only in guest code regions that fed complete observations, discovered at runtime.
    if getattr(args, 'observation_scope', 'full') == 'narrow':
        env['FLYCAST_REMAKE_OBSERVATION_SCOPE'] = 'narrow'
        # Diagnostic knobs (attribution of a behaviour difference to one hook
        # site or bookkeeping part); never a performance or acceptance setting.
        if getattr(args, 'scope_gates', None):
            env['FLYCAST_REMAKE_OBSERVATION_SCOPE_GATES'] = args.scope_gates
        if getattr(args, 'scope_parts', None):
            env['FLYCAST_REMAKE_OBSERVATION_SCOPE_PARTS'] = args.scope_parts
    elif getattr(args, 'scope_gates', None) or getattr(args, 'scope_parts', None):
        raise ValueError('--scope-gates/--scope-parts require --observation-scope narrow')
    if getattr(args, 'verify_depth_format', False):
        if not getattr(args, 'cpu_timing', False) or getattr(args, 'depth_rgba32f', False):
            raise ValueError('Depth format verification requires CPU timing and R32F selection')
        env['FLYCAST_REMAKE_VERIFY_DEPTH_FORMAT'] = '1'
    budget = getattr(args, 'frame_budget_ms', None)
    if budget is not None:
        # Render-thread budget for the remake lane (D-216): the feed skips and
        # the evaluation defers explicitly when the budget cannot cover them.
        if not (0 < budget < 1000):
            raise ValueError('Frame budget must be between 0 and 1000 ms')
        env['FLYCAST_REMAKE_FRAME_BUDGET_MS'] = repr(float(budget))
    consumer = getattr(args, 'consumer_config', None)
    if consumer is not None:
        # An explicit, user-authored consumer configuration (an rtx.conf) handed
        # to the consumer through its documented DXVK_RTX_CONFIG_FILE override.
        # The launcher never writes, moves or edits a runtime configuration; a
        # run with this option is evidence for that configuration only.
        consumer = Path(consumer).resolve(strict=True)
        if not consumer.is_file() or consumer.suffix != '.conf':
            raise ValueError('Consumer configuration must be an existing .conf file')
        if out == consumer.parent or out in consumer.parents:
            raise ValueError('Output cannot contain an input')
        env['DXVK_RTX_CONFIG_FILE'] = str(consumer)
    if args.manual_input:
        # A player must boot, reach a fight and play through cuts: 12000 emulated
        # frames (4:50 to over 5:00 observed), a 420 second host bound, the helper's
        # 420 second session-worker budget and a 180 second first-source wait. Never
        # performance evidence. Trailing helper option order is fixed.
        host[host.index('--warmup')+1] = '3000'
        host[host.index('--frames')+1] = '9000'
        host[host.index('--timeout-ms')+1] = '420000'
        if '--diagnostic-capture-budget' not in helper:
            helper.append('--diagnostic-capture-budget')
        helper.extend(['--source-wait-seconds', '180'])
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
            deadline = time.monotonic()+int(host[host.index('--timeout-ms')+1])/1000+30
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
    p.add_argument('--temple-light-rig', action='store_true', help='Opt-in authored warm key/cool fill, requires --anchored-light')
    p.add_argument('--output-size', choices=['640x480','1280x960'], default='640x480', help='Opt-in matching host/helper/neural extent; fresh session required')
    p.add_argument('--manual-input', action='store_true',
                   help='Use player input instead of scripted replay; still a bounded test session')
    p.add_argument('--managed-session', action='store_true',
                   help='Experimental fresh helper/channel on renderer restart; maximum eight generations')
    p.add_argument('--renderer-reinit-after', type=int, default=0,
                   help='Developer-only restart injection at main frame 1..10000')
    p.add_argument('--capture-frames', type=int, default=0,
                   help='Developer image capture 1..300; excludes this run from performance evidence')
    p.add_argument('--capture-start-source', type=int, default=0)
    p.add_argument('--remix-only', action='store_true',
                   help='Capture-only comparison: skip neural evaluation, preserve native effects and HUD')
    p.add_argument('--returned-dlaa', action='store_true',
                   help='DLAA on returned Remix, not native-PVR DLAA; requires a supplied hooks-disabled host')
    p.add_argument('--effect-identity', action='store_true',
                   help='Synchronous exact-effects archive, at most30 frames; never performance evidence')
    p.add_argument('--extended-effect-capture', action='store_true',
                   help='Explicit300-frame exact-effects diagnostic ceiling; watchdogs unchanged')
    p.add_argument('--locked-input-root', type=Path,
                   help='Replay existing source-qualified returned pixels; exact effect identity required')
    p.add_argument('--cpu-timing', action='store_true',
                   help='Log bounded per-stage host CPU timing; diagnostic, never performance evidence')
    p.add_argument('--depth-rgba32f', action='store_true',
                   help='Force the helper RGBA32F depth reference path')
    p.add_argument('--verify-depth-format', action='store_true',
                   help='With CPU timing, compare both depth formats from eight identical completed frames')
    p.add_argument('--normal-effects', action='store_true', help='Experimental normal effects composition; requires dx11 and cpu-timing')
    p.add_argument('--normal-effect-proof', action='store_true', help='Diagnostic-only bounded native effects replay comparison; requires dx11 and cpu-timing')
    p.add_argument('--renderer', choices=['dx11-oit', 'dx11'], default='dx11-oit',
                   help='Host renderer route for the run (the 600-frame gate asks for both); default dx11-oit')
    p.add_argument('--alpha-combined-off', action='store_true',
                   help='A/B control: disable the promoted alpha surfaces (FLYCAST_REMAKE_ALPHA_COMBINED=0)')
    p.add_argument('--opaque-alpha-one', action='store_true',
                   help='A/B control: the helper draws opaque meshes with alpha one (FLYCAST_REMAKE_OPAQUE_ALPHA_ONE=1)')
    p.add_argument('--curved-export', action='store_true',
                   help='Experimental (D-241): curved PN-triangle export of the smoothed geometry (FLYCAST_REMAKE_CURVED_EXPORT=1); requires smoothed normals')
    p.add_argument('--shading-aware-motion', action='store_true',
                   help='Diagnostic capture: match geometry across RGB shading changes with mandatory returned-colour rejection')
    p.add_argument('--selective-resource-refresh', action='store_true',
                   help='Diagnostic anchored capture: retain strictly compatible mesh resources when other slots change')
    p.add_argument('--benchmark-selective-resource-refresh', action='store_true',
                   help='Bounded automatic capture-free benchmark of the same strict anchored resource reuse; defaults unchanged')
    p.add_argument('--alpha-cutout', action='store_true',
                   help='Experimental (D-240): promoted alpha draws with cutout texture alpha travel as alpha-tested cutouts (FLYCAST_REMAKE_ALPHA_CUTOUT=1); the rest stay native')
    p.add_argument('--smooth-normals-weld', action='store_true',
                   help='Experimental: --smooth-normals plus welding of bit-identical resubmitted vertices within a draw (level 2)')
    p.add_argument('--smooth-normals', action='store_true',
                   help='Experimental: average exported face normals per source vertex (60 degree crease); default off')
    p.add_argument('--hook-cycles', action='store_true',
                   help='With --cpu-timing: per-hook cycle accounting on the emulation thread (D-218); diagnostic only')
    p.add_argument('--observation-scope', choices=['full', 'narrow'], default='full',
                   help='Experimental (D-240): narrow the source observation hooks to the guest code regions that fed complete observations, discovered at runtime; default full')
    p.add_argument('--scope-gates', default=None,
                   help='Diagnostic with --observation-scope narrow: comma list of hook sites that carry the gate (entry,arith,read,store,sq,ftrv,after,load); "off" gates nothing')
    p.add_argument('--scope-parts', default=None,
                   help='Diagnostic with --observation-scope narrow: comma list of bookkeeping parts that run (note,ta,ctrl,reg); "none" runs nothing')
    p.add_argument('--guest-frame-digest', action='store_true',
                   help='Log a per-frame hash of the raw TA bytes with the producer cycle (guest-only scene identity); diagnostic only')
    p.add_argument('--hook-attribution', action='store_true',
                   help='With --cpu-timing: per-block source hook attribution report every 120 frames (D-240 groundwork); diagnostic only')
    p.add_argument('--frame-budget-ms', type=float, default=None,
                   help='Render-thread remake budget per emulated frame in ms (D-216); default unlimited')
    p.add_argument('--consumer-config', type=Path, default=None,
                   help='User-authored consumer rtx.conf passed by DXVK_RTX_CONFIG_FILE; never written by the launcher')
    p.add_argument('--run', action='store_true', help='Actually launch; default is read-only preflight')
    args = p.parse_args()
    paths, out, env, host, helper = prepare(args)
    # Do not hash or read the supplied third-party runtime internally.
    record = dict(host=host, helper=helper, anchored_light=args.anchored_light,
                  temple_light_rig=args.temple_light_rig,
                  output_size=args.output_size,
                  manual_input=args.manual_input,
                  managed_session=args.managed_session,
                  comparison_lane='remix-only' if args.remix_only else 'returned-dlaa-requested' if args.returned_dlaa else 'combined-experimental',
                  locked_input_root=str(args.locked_input_root) if args.locked_input_root else None,
                  effect_identity=args.effect_identity or bool(args.locked_input_root),
                  capture_frames=args.capture_frames,
                  capture_start_source=args.capture_start_source,
                  extended_effect_capture=args.extended_effect_capture,
                  comparison_end_source=(args.capture_start_source+args.capture_frames-1)
                      if args.effect_identity or args.locked_input_root else None,
                  cpu_timing=args.cpu_timing,
                  depth_rgba32f=args.depth_rgba32f,
                  verify_depth_format=args.verify_depth_format,
                  hook_cycles=args.cpu_timing and args.hook_cycles,
                  hook_attribution=args.cpu_timing and args.hook_attribution,
                  guest_frame_digest=args.guest_frame_digest,
                  observation_scope=args.observation_scope,
                  scope_gates=args.scope_gates, scope_parts=args.scope_parts,
                  renderer=args.renderer,
                  alpha_combined_off=args.alpha_combined_off, opaque_alpha_one=args.opaque_alpha_one,
                  alpha_cutout=args.alpha_cutout, curved_export=args.curved_export,
                  shading_aware_motion=args.shading_aware_motion,
                  selective_resource_refresh=args.selective_resource_refresh,
                  benchmark_selective_resource_refresh=args.benchmark_selective_resource_refresh,
                  smooth_normals=args.smooth_normals, smooth_normals_weld=args.smooth_normals_weld,
                  frame_budget_ms=args.frame_budget_ms,
                  consumer_config=str(args.consumer_config.resolve()) if args.consumer_config else None,
                  consumer_config_sha256=hashlib.sha256(args.consumer_config.read_bytes()).hexdigest()
                      if args.consumer_config else None,
                  performance_eligible=args.capture_frames == 0 and not args.manual_input and not args.cpu_timing,
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
            deadline = time.monotonic()+int(host[host.index('--timeout-ms')+1])/1000+30
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
