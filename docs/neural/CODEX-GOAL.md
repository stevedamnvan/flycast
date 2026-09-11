# Codex goal: substep H, 1280x960 pipeline cost (feat/neural-rendering)

Read first: `AGENTS.md`, `DEVELOPMENT-HANDOFF.md`, the H row of
`docs/neural/BACKLOG.md`, and LOG805/LOG806 in `docs/neural/LOG.md`.

## Status (2026-09-11, LOG894)

Items 1 to 6 below are done and accepted on the OIT route (LOG807 to
LOG845): 1280x960 median about 18.3 ms, 99.4 percent fresh, max latency 4.
Remaining work is listed in the handoff: normal-renderer returned-output
integration, helper lifecycle budget review, VRAM by phase, and the open
60 fps goal. Keep this file's rules; treat the ordered list as history.

## Goal

Reduce the per-frame CPU work on the returned image so the combined
1280x960 presentation approaches 60 fps, measured by performance-eligible
runs, without changing what the pipeline accepts or rejects.

## Ordered work (one item per commit, smallest first)

1. Add finer `RemakeCpuScope` stages inside `evaluate-raster`,
   `return-worker` and `evaluate-history-accept` (validate, upload, copy,
   draw). Diagnostic only, gated by `FLYCAST_REMAKE_CPU_TIMING=1`.
2. Replace the repeated per-pixel depth validation passes
   (`remake_live_channel.cpp`, `remake_neural_input.h`,
   `remake_temporal_scene.h`, `remake_motion_raster.h`) with one
   vectorizable range check per image. Same accept/reject result for every
   input, including NaN and infinities. Add a unit test that proves it.
3. Ping-pong the motion raster's depth textures so the previous depth is
   not re-uploaded each frame.
4. Move the colour/depth buffer copies off the render thread into the
   return worker, or share them by reference, keeping ownership rules.
5. Helper (`neuraltest/remake_runtime_smoke.cpp`): fuse depth extract and
   far-plane clamp; try an R32F depth target with fallback to RGBA32F;
   split colour and depth return work across two threads.
6. Re-measure with performance-eligible runs at 640x480 and 1280x960 and
   record present p50/p95 and helper period in the LOG.

## Rules that apply to every item

- Build all four configurations serially; run `neuraltest.exe selftest`
  (883/0), `remake-sdk-contract.exe` (272/0) and the python suites (23 OK)
  before any source commit. Never build while gameplay or the helper runs.
- Do not weaken certificates or lower acceptance to gain speed. Do not
  claim 60 fps or "production ready" from CPU-timing or capture runs.
- Do not touch external configurations, third-party binaries or game
  media. Do not reset, clean, stash, rebase or discard user changes.
  Commit only owned source and docs; push only to `fork` and verify SHA.
- Record each measurement as a LOG entry with the run name under
  `D:\Flycast-Evidence\`, and update the BACKLOG H row and the handoff.

## Done when

A performance-eligible 1280x960 run shows the improvement with the same
denominator as LOG805 (perf-a launch), the LOG/BACKLOG/handoff are
updated, and all selftests still pass.
