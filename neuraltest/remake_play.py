"""Opt-in play session for the Soulcalibur remake lane.

Starts Flycast with the returned-scene lane and supervises one Remix helper per
session generation, without the test harness frame/time bounds, so a player can
use the combined image for as long as the game runs. This is a play route, not
evidence: nothing here is performance, provenance or appearance acceptance.

The workspace is a prepared host directory owned by the player (flycast.exe,
dxgi.dll/ReShade, the DLSS 5 add-on, nvngx DLLs, emu.cfg, data). The launcher
copies reshade-<look>.ini over reshade.ini inside that workspace and never
touches any other configuration.
"""
import argparse
import datetime
import mmap
import os
import shutil
import struct
import subprocess
import sys
import threading
import time
import uuid
from pathlib import Path

# World-space sun that matches the native drop-shadow direction on the shrine
# stage (lighting-fix-a) and the existing diagnostic fill.
DEFAULT_SUN = '-0.635885233 -0.730868090 0.247955248'
DEFAULT_FILL = '-0.487994879 -0.284207851 0.825279772 0.3'
ENABLED = ('TEMPORAL_RASTER', 'TEMPORAL_PREPARE', 'CAMERA_ANCHOR', 'ALPHA_COMBINED',
           'PUNCH_THROUGH', 'ASYNC_OIT', 'ESTIMATE_UNTRACED',
           'ASYNC_PRESENT', 'ASYNC_NEURAL', 'NATIVE_EFFECTS')
LOOKS = {'dlss5': 8, 'dlaa': 2}  # rend.NeuralMode per look


def unit(text, count):
    values = [float(v) for v in text.split()]
    if len(values) != count or abs(sum(v*v for v in values[:3])-1) > 1e-5:
        raise ValueError(f'expected unit XYZ{" and radiance" if count == 4 else ""}: {text!r}')
    return text


def other_flycast_running():
    out = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq flycast.exe', '/FO', 'CSV', '/NH'],
                         capture_output=True, text=True).stdout
    return 'flycast.exe' in out.lower()


def build(args, channel, logs):
    ws = args.workspace.resolve(strict=True)
    paths = dict(flycast=ws/'flycast.exe', helper=args.helper.resolve(strict=True),
                 runtime=args.runtime.resolve(strict=True), game=args.game.resolve(strict=True))
    for name, path in paths.items():
        if not path.is_file():
            raise ValueError(f'{name} is not a file: {path}')
    look_ini = ws/f'reshade-{args.look}.ini'
    if not look_ini.is_file():
        raise ValueError(f'missing {look_ini.name} in workspace')
    width, height = args.output_size.split('x')
    env = {k: v for k, v in os.environ.items() if not k.startswith(('FLYCAST_REMAKE_', 'DXVK_'))}
    env.update({'FLYCAST_REMAKE_'+key: '1' for key in ENABLED + (('ASYNC_DIAGNOSTICS',) if args.verbose_log else ())})
    env.update(FLYCAST_REMAKE_OUTPUT_SIZE=args.output_size, FLYCAST_NEURAL_SOURCE_OBSERVATION='1',
               FLYCAST_REMAKE_COLOR_CONSISTENCY='0', FLYCAST_REMAKE_EFFECT_IDENTITY='0',
               FLYCAST_REMAKE_ASYNC_CHANNEL=channel,
               FLYCAST_REMAKE_ASYNC_START_PRODUCER='2090' if args.scripted_input else '0',
               FLYCAST_REMAKE_MANAGED_SESSION='1', FLYCAST_REMAKE_SMOOTH_NORMALS='2',
               FLYCAST_REMAKE_OBSERVATION_SCOPE='narrow', FLYCAST_REMAKE_SELECTIVE_RESOURCE_REFRESH='1',
               FLYCAST_REMAKE_HELPER_PLAY='1',
               # Automation builds mute audio unless this explicit opt-in is present (LOG957).
               FLYCAST_AUTOMATION_REALTIME_AUDIO='1')
    if args.remix_config:
        env['DXVK_RTX_CONFIG_FILE'] = str(args.remix_config.resolve(strict=True))
    config = ','.join([
        'config:pvr.rend=6', f'config:rend.Resolution={height}', f'config:rend.NeuralMode={LOOKS[args.look]}',
        'config:rend.NeuralD3D12Surface=yes', 'config:rend.NeuralMatchOutputResolution=yes',
        'config:rend.NeuralCaptureFrames=0', 'config:rend.NeuralDlss5EvidenceCapture=no',
        f"record:replay_input={'yes' if args.scripted_input else 'no'}", 'log:LogToFile=yes', f"log:RENDERER={'yes' if args.verbose_log else 'no'}",
        f'window:width={width}', f'window:height={height}', 'window:maximized=no'])
    flycast = [str(paths['flycast']), '-config', config, str(paths['game'])]
    helper = [str(paths['helper']), '--runtime', str(paths['runtime']), '--frames', '660',
              '--live-channel-async', channel, '--assets', str(logs), '--clips', '0.1', '2501',
              '--return-d3d9-scene-memory-depth', str(logs/'unused-return.bmp')]
    if args.sun_radiance:
        helper += ['--scene-light-radiance', args.sun_radiance]
    helper += ['--scene-light-direction', unit(args.sun, 3)]
    if args.fill != 'none':
        helper += ['--scene-fill', unit(args.fill, 4)]
    helper += ['--scene-light-anchor', '--session-worker']
    return ws, look_ini, env, flycast, helper


PER_FRAME = ('live_return ', 'legacy_draw_cpu ', 'returned_depth ', 'capture_readback_hresult=')


def pump(stream, path, verbose):
    """Copy helper output to its log; without --verbose-log keep errors, session
    events and every 600th per-frame return as a periodic health sample."""
    returns = 0
    with open(path, 'w', encoding='utf-8', errors='replace') as log:
        for line in stream:
            if not verbose and line.startswith(PER_FRAME):
                if not line.startswith('live_return '):
                    continue
                returns += 1
                if returns % 600 != 1:
                    continue
            log.write(line)
            log.flush()


def supervise(ws, env, flycast, helper, logs, repo, verbose):
    root = env['FLYCAST_REMAKE_ASYNC_CHANNEL']
    with mmap.mmap(-1, 16, tagname='Local\\FlycastRemake-'+root+'-control') as control:
        if control[:] != bytes(16):
            raise ValueError('Session controller name already owned')
        control[:] = struct.pack('<IIII', 0x534d5246, 1, 0, os.getpid())
        game = subprocess.Popen(flycast, cwd=ws, env=env)
        (logs/'flycast.pid').write_text(str(game.pid))
        generation, current, reported = 0, None, False
        try:
            while game.poll() is None:
                requested = struct.unpack_from('<I', control, 8)[0]
                if requested != generation:
                    if current is not None and current.poll() is None:
                        try:
                            current.wait(timeout=8)  # retired publisher: bounded receive unwinds it
                        except subprocess.TimeoutExpired:
                            current.kill()
                    command = helper.copy()
                    command[command.index('--live-channel-async')+1] = f'{root}-g{requested}'
                    current = subprocess.Popen(command, cwd=repo, env=env, stdout=subprocess.PIPE,
                                               stderr=subprocess.STDOUT, text=True, errors='replace',
                                               creationflags=subprocess.CREATE_NO_WINDOW)
                    threading.Thread(target=pump, args=(current.stdout, logs/f'helper-g{requested}.log', verbose),
                                     daemon=True).start()
                    print(f'[play] Remix session {requested} started', flush=True)
                    generation, reported = requested, False
                elif current is not None and current.poll() is not None and not reported:
                    print(f'[play] Remix session {generation} ended (exit {current.returncode}); '
                          'native image until the next session starts. See '
                          f'{logs/f"helper-g{generation}.log"}', flush=True)
                    reported = True
                time.sleep(.05)
        finally:
            if current is not None and current.poll() is None:
                try:
                    current.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    current.kill()
    return game.returncode


def main():
    repo = Path(__file__).resolve().parent.parent
    p = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    p.add_argument('--workspace', type=Path, required=True, help='Prepared host directory with flycast.exe')
    p.add_argument('--helper', type=Path, required=True, help='remake-runtime-smoke.exe')
    p.add_argument('--runtime', type=Path, required=True, help='Remix runtime d3d9.dll')
    p.add_argument('--game', type=Path, required=True, help='Legal Soulcalibur disc image')
    p.add_argument('--remix-config', type=Path, help='rtx.conf for the Remix helper (DXVK_RTX_CONFIG_FILE)')
    p.add_argument('--look', choices=sorted(LOOKS), default='dlss5', help='Final stage: supplied DLSS 5 or public DLAA')
    p.add_argument('--output-size', choices=['640x480', '1280x960'], default='1280x960')
    p.add_argument('--sun', default=DEFAULT_SUN, help='World-space unit direction the sun travels')
    p.add_argument('--sun-radiance', help='Sun radiance 0..30 (helper default 3)')
    p.add_argument('--fill', default=DEFAULT_FILL, help="Fill 'X Y Z radiance', or 'none'")
    p.add_argument('--logs', type=Path, help='New log directory (default: workspace/play-logs/<time>)')
    p.add_argument('--allow-other-flycast', action='store_true')
    p.add_argument('--verbose-log', action='store_true',
                   help='Keep per-frame renderer diagnostics (about 450 MB per hour of play)')
    p.add_argument('--scripted-input', action='store_true',
                   help='Smoke test: replay scripts/<game>.input (automation build) instead of a controller')
    args = p.parse_args()
    if other_flycast_running() and not args.allow_other_flycast:
        sys.exit('Another flycast.exe is running (possibly another session); refusing to start.')
    logs = (args.logs or args.workspace/'play-logs'/datetime.datetime.now().strftime('%Y%m%d-%H%M%S')).resolve()
    logs.mkdir(parents=True, exist_ok=False)
    channel = 'fc067-play-' + uuid.uuid4().hex[:16]
    ws, look_ini, env, flycast, helper = build(args, channel, logs)
    shutil.copyfile(look_ini, ws/'reshade.ini')
    (logs/'play.txt').write_text('\n'.join([f'look={args.look}', f'output={args.output_size}',
                                            'flycast=' + subprocess.list2cmdline(flycast),
                                            'helper=' + subprocess.list2cmdline(helper)]) + '\n')
    print(f'[play] look={args.look} output={args.output_size} logs={logs}', flush=True)
    code = supervise(ws, env, flycast, helper, logs, repo, args.verbose_log)
    print(f'[play] Flycast exited ({code})', flush=True)


if __name__ == '__main__':
    main()
