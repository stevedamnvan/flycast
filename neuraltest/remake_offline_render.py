"""Offline same-packet Remix re-render (diagnostic; no emulator needed).

Re-renders one saved remake-view.bin through the helper with a chosen Remix
config and light, and writes <out-root>/<name>/render.png. Used to try lighting
and config changes in seconds (LOG1185). Not performance or acceptance evidence.

Example:
  python neuraltest/remake_offline_render.py sun C:/Flycast-Evidence/<run>/captures/frame-6364-present-6368 ^
      --out-root C:/Flycast-Evidence/lighting-fix-b --conf "rtx.vertexColorStrength = 0.0"

Add --env DXVK_RTX_CAPTURE_ENABLE_ON_FRAME=60 --env DXVK_DISABLE_ASSET_REPLACEMENT=1
--env FLYCAST_REMAKE_HELPER_LINGER_MS=18000 to also write a Remix USD capture
(textures named by runtime hash) under flycast/rtx-remix/captures.
"""
import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
BASE_CONF = ['rtx.upscalerType = 0', 'rtx.resolutionScale = 1.0',
             'rtx.texturemanager.samplerFeedbackEnable = False',
             'rtx.texturemanager.neverDowngradeTextures = False']
DEFAULT_SUN = '-0.635885233 -0.730868090 0.247955248'
DEFAULT_FILL = '-0.487994879 -0.284207851 0.825279772 0.3'


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('name', help='New subfolder name for this render')
    ap.add_argument('frame_dir', type=Path, help='Capture folder containing remake-view.bin')
    ap.add_argument('--out-root', type=Path, required=True, help='Evidence folder (created if missing)')
    ap.add_argument('--conf', action='append', default=[], help="Extra rtx.conf line, e.g. 'rtx.x = 1'")
    ap.add_argument('--conf-file', type=Path, help='Use this rtx.conf instead of the base lines')
    ap.add_argument('--direction', default=DEFAULT_SUN, help='Sun travel direction (unit XYZ)')
    ap.add_argument('--radiance', help='Sun radiance 0..30 (helper default 3)')
    ap.add_argument('--fill', default=DEFAULT_FILL, help="'X Y Z radiance' or 'none'")
    ap.add_argument('--size', default='640x480', choices=['640x480', '1280x960'])
    ap.add_argument('--env', action='append', default=[], help='Extra KEY=VALUE for the helper')
    ap.add_argument('--helper', type=Path, default=REPO/'build-neural-automation/neuraltest/remake-runtime-smoke.exe')
    ap.add_argument('--runtime', type=Path,
                    default=REPO.parent/'external-runtimes/remix-1.5.2/runtime/.trex/d3d9.dll')
    a = ap.parse_args()
    packet = (a.frame_dir/'remake-view.bin').resolve(strict=True)
    out = (a.out_root/a.name).resolve()
    out.mkdir(parents=True, exist_ok=False)
    conf = (a.conf_file.read_text().splitlines() if a.conf_file else BASE_CONF) + a.conf
    (out/'rtx.conf').write_text('\n'.join(conf) + '\n')
    cmd = [str(a.helper.resolve(strict=True)), '--runtime', str(a.runtime.resolve(strict=True)), '--frames', '120',
           '--live-artifact', str(packet), '--assets', str(out), '--clips', '0.1', '2501',
           '--capture-d3d9-scene-memory', str(out/'render.bmp')]
    if a.radiance:
        cmd += ['--scene-light-radiance', a.radiance]
    cmd += ['--scene-light-direction', a.direction]
    if a.fill != 'none':
        cmd += ['--scene-fill', a.fill]
    cmd += ['--scene-light-anchor']
    env = {k: v for k, v in os.environ.items() if not k.startswith(('FLYCAST_REMAKE_', 'DXVK_'))}
    env.update(FLYCAST_REMAKE_HELPER_STARTUP_WAIT_MS='1000', FLYCAST_REMAKE_HELPER_LINGER_MS='1000',
               FLYCAST_REMAKE_HELPER_RENDER_SIZE=a.size, FLYCAST_REMAKE_OUTPUT_SIZE=a.size,
               DXVK_RTX_CONFIG_FILE=str(out/'rtx.conf'), DXVK_DISABLE_ASSET_REPLACEMENT='0')
    for kv in a.env:
        key, value = kv.split('=', 1)
        env[key] = value
    (out/'command.json').write_text(json.dumps({'cmd': cmd, 'conf': conf, 'packet': str(packet),
                                                'env': {k: env[k] for k in env if k.startswith(('FLYCAST_', 'DXVK_'))}}, indent=1))
    # cwd is the repo so the helper loads rtx-remix/mods (the Remix project mod).
    with (out/'helper.log').open('w') as log:
        rc = subprocess.run(cmd, cwd=REPO, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=240).returncode
    ok = rc == 0 and (out/'render.bmp').exists()
    if ok:
        from PIL import Image
        Image.open(out/'render.bmp').convert('RGB').save(out/'render.png')
    print(f'{a.name}: exit={rc} render={"ok" if ok else "MISSING"} -> {out}')
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
