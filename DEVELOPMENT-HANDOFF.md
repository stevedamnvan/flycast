# Flycast experimental Remix + external DLSS 5 development handoff

## Current state (2026-09-10, D-224)

H item5 implementation is uncommitted after5c2fc4e2a. Helper now extracts
and clamps R depth in one traversal. Memory-only returns try R32F, falling
back to RGBA32F on creation/copy/readback/lock failure; raw exports remain
RGBA32F. Use launcher --depth-rgba32f to force the reference path for checks.
RemakeReturnTasks owns one reusable CPU worker; opted-in ReturnImage copies
and hashes color/depth on two threads, joins before publication, and retains
all existing validation. No D3D calls move threads. New fused tests compare
bits/counters to the old pass for padded R32F/RGBA32F, exceptional values and
invalid planes; existing channel tests exercise worker reuse, closure and
invalid depth. New header core/rend/neural/remake_return_tasks.h is owned.
Four builds pass after same-frame diagnostic (h5-build-d.log),915/0 x3
(h5-selftest-c.log), SDK280/0. Python24 now passes including explicit depth
control propagation. Corrected diagnostic pilot-extent1280-h5-helper-b
finished0, host0/helper11 orderly. After120 published returns (1073 samples):
depth_convert3.6393ms, return1.3453, depth_lock0.5926 versus item4 baseline
4.8745/1.8524/1.6741. Scope returned-evaluate5.452ms. Logs/summaries archived.
Moving pilot-h5-r32-moving and pilot-h5-rgba-moving each have12 captures,
12 completed-Present joins and zero HUD mismatch. Reviewed R32 source2573.
IMPORTANT: the latter was NOT RGBA32F: launcher scrubs inherited FLYCAST
controls, so both used R32F. Packet/native inputs also differ; their A/B
comparison is rejected. First pilot-h5-depth-format-exact ran no checks for
the same reason. Neither establishes format equivalence. All runs terminal.
Launcher now has --depth-rgba32f and --verify-depth-format, tested explicit
propagation and requiring CPU timing for verification. Helper verifies eight
same-completed-frame pairs from source2560, bit comparison across all pixels,
and fails on mismatch. This diagnostic is additionally CPU-timing gated.
Corrected pilot-h5-depth-format-exact-b finished0 (process61436): eight
checks actually executed, each1228800 values bit-identical, hresult0.
Host log archived; no game/helper running. Item5 verified, ready to commit
with LOG812. Next item6 performance-eligible640/1280 with original exposure A,
no capture/CPU timing/format verification. No60fps claim. Keep first failed
measurement and rejected comparisons in evidence; never treat them as proof.

H item4 verified (LOG811): shared owned color/depth snapshots, detach before
writes, independent copies after writable aliases. Four final serial builds
pass,906/0 x3, SDK272/0, Python23. Matched diagnostic history acceptance
0.7702 to0.0144ms; returned evaluation6.7586 to5.3897. Moving12 captures,
12 completed-Present joins, zero HUD mismatches; reviewed source2573.
Both runs finished, logs archived; no game/helper running. No60fps claim.
Next CODEX-GOAL item5: helper fused depth extraction/clamp, try R32F with
RGBA32F fallback, split color/depth return across two threads. Preserve
source/format/depth validation and publish only after both buffers complete.
Keep original exposure A for timing;0.30 remains the opt-in visual candidate.

H item3 verified (LOG810): depth texture ping-pong with immutable content
identity. Final four builds,901/0 x3, SDK272/0, Python23; exact sampled GPU
fixture on both APIs. Matched diagnostic raster upload1.009 to0.557ms,
returned-evaluate7.325 to6.759. Moving run12 captures/12 Present joins,
zero HUD mismatches; reviewed frame2573. No60fps claim. Both runs finished,
logs archived. Next CODEX-GOAL item4: remove/move render-thread buffer copies
without weakening alias/accepted-history ownership. Keep exposure A for
performance runs and the separate0.30 candidate for visual review.

H item2 verified (LOG808): four builds,893/0 x3, SDK272/0, Python23.
Matched diagnostic pilot-extent1280-h2-depth: returned-evaluate median
10.82 to7.33ms; raster validation3.08 to0.26ms and cached history/input
checks near zero. No identity/acceptance checks removed. No60fps claim.
Next CODEX-GOAL item3: ping-pong depth textures with previous-source identity.
Exposure comparison complete (LOG809): pilot-h2-exposure-current/low have12
matched captures, identical native inputs/source digests, zero protected HUD
mismatches and12 completed-Present joins. Reviewed lower profile0.30 as a
less washed-out candidate; human visual approval remains open. Profile lives
in D:/Flycast-Evidence/pilot-curated/profiles/exposure-probe-h2-low.conf.
No live external configuration changed. Both runs finished; no game/helper
left running. Resume item3 depth-texture ping-pong. Retain original exposure
A for performance comparisons so the denominator remains unchanged.

CODEX-GOAL item1 verified (LOG807): finer diagnostic scopes and matched
pilot-extent1280-h1-scopes run complete; four builds,883/0 x3, SDK272/0,
Python23 and backlog6 pass. Validation dominates raster/history CPU time.
Next H item2: preserve every accept/reject result while removing redundant
depth scans. User reports overexposed delivered composite; perform a bounded
same-source exposure comparison under D/E, preserving native HUD/effects.
No speed improvement or visual acceptance claimed. Item2-6 remain open.

Resume 2026-09-11 (LOG806): host-side attribution done with matched
640/1280 CPU-timing runs (`pilot-extent640-cpu-a`, `pilot-extent1280-cpu-a`).
The render thread's returned-image work (validation passes, buffer copies,
raster depth uploads) triples with pixels and throttles the emulator; the
helper's conversion and return add the rest. Next steps are ordered in the
BACKLOG H row and LOG806 (instrument, single validation pass, depth
ping-pong, copies off the render thread, helper extract/return, re-measure).
Rules unchanged: four builds serial, 883/0 x3, contract 272/0, python 23/0
before any source commit; CPU-timing runs are diagnostic only; 60 fps is
not claimed until a performance-eligible run shows it.

Resume 2026-09-11 (LOG805): F moving review at 1280x960 (40 captures, HUD
identical to native, no pops) technical ACCEPTED, look NOT_REVIEWABLE; H
performance-eligible runs at 1280x960 give present p50 26.5 ms (about 38
fps); DLSS modes do not help; the growth over 640x480 is the helper's CPU
per-pixel work (depth convert, return, prepare). Next: parallelize those in
the helper, re-measure with the same denominator; E/D and look decisions
open. The composited image (Remix scene plus native effects, native alpha
surfaces and HUD) is the delivered target image; the Remix-only capture is
a diagnostic intermediate.

Resume 2026-09-10 (LOG804): user said "resume". The paused v6 run was void
(baseline-configuration executable). Three extent fixes (OIT effects gate,
motion raster, neural-input diagnostic) gave the first live 1280x960
combined presentation with captures (`pilot-extent1280-v10-raster`). Four
builds, 883/0 x3, contract 272/0, python 23/0; source and docs committed.
Next: F moving combat/HUD and temporal review at 1280x960, then H cost
attribution (present p50 29.4 ms at 1280 against 20.0 at 640); E key/fill
tuning and D masks remain open; look decisions remain the user's.

PAUSED by user request ("puse here"). No builds, launches or implementation
until explicit resume. The v6 depth diagnostic was launched, then its owned
process tree was forcibly stopped on request; it is incomplete evidence.
Resume from pilot-extent1280-v6-depth and its launch log under
D:\Flycast-Evidence. Baseline diagnostic build finished successfully;
prepared executable is flycast-pilot-extent-v6.exe. Preserve all dirty work.

Active continuation: user withdrew stopping and requested a polished playable
remaster goal. Goal registered in this task; BACKLOG.md remains authority.
PBRify D and temple E are documented in PILOT-TEMPLE-LIGHTING.md, source
commit46fc20eb0. F1280x960 integration is in progress in the working tree:
extent/channel/temporal/effects/motion/preview plus transient window handling.
Read LOG803 for failed live attempts. The framebuffer guard now passes;
v4 fails observed-anchor matching at both1280x960 and640x480. Inspect
pilot-extent1280-v5-diagnose:2090 has no XYZ links, but3300 has862 valid
lens matches and still rejects. Observed/predicted depth counters are added
for the next run; baseline build was started. This is not yet a
resolution-specific diagnosis. Do not weaken certificates to obtain output.
Do not call F accepted from CPU transport tests or883/0 selftests. Continue
the actual live capture, then combat/HUD and temporal quality checks. Preserve
all preexisting dirty documentation and private evidence; no reset/clean.

Latest resume (LOG801): HEAD485b7fe07293ad48eba64c3a2850b4a170670d32.
PBRify setup, driver, 26 generated materials and104 ingested maps are already
complete (LOG800); do not restart installation. Toolkit MCP confirms the
reimagined layer is strongest. Corrected height stays on the floor only.
Reviewed existing outputs and ran a same-source radiance3/1 comparison with
exposure A at1280x960 using the existing helper. Light1 preserves bright
detail but darkens the floor; glossy costume material persists. Comparison:
`D:\Flycast-Evidence\pilot-curated\resume-light-comparison.png` and JSON.
This is a lighting candidate, not final visual or moving acceptance. Continue
pilot D/E: mixed-atlas roughness/metal masks and warm-key/cool-fill lighting;
then F's opt-in live resolution work. Keep native alpha and welded normals.
All older sections below retain their historical scope; read live backlog.

Verified HEAD on entry `81654643daeaf0b4333978ee4a99da649dd40a6b` (LOG794);
recheck HEAD/status on resume. The active work is the opt-in "Soulcalibur
Faithful RTX" pilot recorded as substeps A-H under the existing cards in
`docs/neural/BACKLOG.md` (D-224). The OIT fresh-output criterion is met in two
runs; full speed, VRAM attribution, normal-renderer coverage, world/camera
truth and visual acceptance remain open. Toolkit 1.5.2.0 is installed and
scripted; the earlier "Toolkit unavailable" and helper 20-image-per-second
ceiling statements are superseded. The sections below are the 2026-09-09
checkpoint kept for its controls; the "next implementor task" there is done
(LOG759-794) and is not a current instruction.

## Pause and authority (historical)

User requested a pause, then this documentation handoff, then resumed on
2026-09-09 with "proceed from here". The sections below record the resumed
result; the standing goal remains unfinished. Follow `AGENTS.md` and
`docs/neural/BACKLOG.md`. The earlier tracker "blocked" state meant the user
pause only, not a technical dependency.
The backlog is the sole execution queue; this handoff is a checkpoint, not a
replacement roadmap. Requested routing is GPT-6 Astra low/light, no subagents;
that is user intent, not confirmation of the active model configuration.

## Exact checkout and unfinished changes

Branch: `feat/neural-rendering`.
Verified HEAD: `995308035f8c320da6c305fc5b06b44f1931703b`.
Recheck HEAD/status on resume. Never reset, stash, clean or overwrite newer work.
The following implementation changes are still uncommitted:

- `core/rend/dx11/dx11_renderer.cpp`: bounded capture start/end, first-source
  presentation boundary, and locked archive preparation before helper request.
- `core/rend/neural/remake_input_replay.{h,cpp}`: bounded archive identity index,
  invalidation and selected-packet revalidation, plus explicit preparation API.
- `core/rend/neural/remake_oit_effects.h` and `remake_presentation.h`: explicit
  300-frame diagnostic bound (legacy default30), interval and boundary policy.
- `neuraltest/main.cpp`, `remake_launch.py`, `test_remake_launch.py`,
  `remake_scene_tests.cpp`, `unit_tests.cpp`: launcher wiring and controls.
- `docs/neural/REMAKE-LAUNCH.md`, BACKLOG and LOG: accompanying working notes.

Preserve untracked user items `metrics.txt`, `remake-runtime-smoke.dxvk-cache`,
and `rtx-remix/`. This handoff adds documentation only. It does not commit or
push the above changes. Review and commit independently proven slices after
resumption; use explicit staging, never include private artifacts/config/media.
Push only the user's `fork` remote, not upstream `origin`, and verify remote SHA.

## Verified checkpoint and evidence locations

External evidence directory names below are under the existing second-drive
`Flycast-Evidence` root; do not move raw evidence into Git.

- `fc067-extended-source-b`:300 consecutive source frames2400..2699.
- `fc067-extended-combined-b`:300 matching combined captures.
- `fc067-extended-remix-combined-review-b/comparison.json`:300 frames, no gaps,
  neither side has unmatched sources, zero HUD mismatch. Comparator checks
  scene/native/HUD/returned color/depth/effect identity and completed presentation.
  Its stated scope excludes identical temporal histories/full NGX inputs, fresh
  external provenance, performance and a quality winner. Moving GIF and midpoint
  PNG exist; do not claim full moving visual approval from their existence.
- `fc067-four-lane-review-b`: earlier short28-frame native/public-DLAA/Remix/
  combined join. The full300-frame public/native matrix is still incomplete.
- Existing build logs `replay-prewarm-test.log` in automation/baseline/no-NGX
  record772 passed/0 failed. Feature-off `replay-prewarm-build.log` ends in link.
  These are incremental working-tree results, NOT fresh exact-SHA builds.

Retain falsifying attempts: extended-source-a lost the first two frames;
extended-combined-a captured only three before cold archive scanning caused
helper receive timeout and a subsequent strict replay mismatch. Boundary and
pre-session index preparation are the subsequent fixes. The earlier intermittent
returned-DLAA depth rejection was not reproduced and is not declared fixed.
Detailed history is LOG743-757; do not replace it with an all-green summary.

## Completed 2026-09-09 task (historical): live scene/camera continuity

Prioritize integration over further comparison infrastructure or settings sweeps.
Inspect `core/rend/neural/remake_camera_anchor.h` and its caller in
`core/rend/dx11/dx11_renderer.cpp`, plus the existing unit fixtures.

Concrete observed boundary: source frame3099 / producer3098 repeatedly rejects
with `anchor-source-support-changed`, retires the session/history, then requests
a fresh session. Example: `fc067-extended-combined-b/previous-flycast.log` around
that rejection. This log is a preserved prior run, not the current combined-b
run's live log. Do not confuse their identities.

The current anchor compares source-input point support to the first accepted
set, requiring at least16 shared points and half the smaller set. It also checks
source domain, a common rigid basis, producer ordering and projection. It keeps
the explicit `diagnostic-camera-embedded-anchor-not-world-reconstruction` label.
Resumed result (LOG759-763, D-207): frame3099 is a measured genuine source-view
cut (81 degrees /13.5 units in one frame; smooth-motion maximum1.62 degrees /
0.741;424 of1000 points shared with the last accepted set against a0.739 floor).
The reset is retained but performed in-session: labeled anchor generation with a
distinct diagnostic origin, histories and presentation retired, helper
correspondence and anchored light reset explicitly. Demonstrated in
`D:\Flycast-Evidence\fc067-anchor-boundary-e`: one helper generation, five
native frames per cut instead of about120. Runs a (wrong build configuration)
and d (helper continuity rejection) are retained failures. Staged executables
must come from `build-neural-automation` (TEST_AUTOMATION input replay).
The original step list is kept below for its controls; steps1-3 are done in the
recorded scope, step4 is recorded in LOG.

1. Inspect existing source packets/observations around that boundary. Determine
   whether source basis/arena identity continues or genuinely changes. If those
   observations were not retained, capture only the missing bounded boundary
   evidence after resumption; do not restart a broad tracing campaign.
2. If continuity is supported, implement source-qualified continuity without
   merely lowering support thresholds or accumulating unbounded point history.
   If it is a real cut, retain reset and address the measured fresh-session gap
   instead. Do not manufacture a camera interpretation to remove a rejection.
3. Test visibility changes, wrong basis, changed scene, producer discontinuity,
   rejected publication and accepted-history ownership. Preserve exact projection
   and depth guards. Demonstrate the relevant boundary in moving gameplay.
4. Build four configurations serially; run all three enabled selftests and
   focused fixtures. Never build while gameplay/helper runs. Record failures,
   review actual output, update the active card and commit proven slices.

Do not reprove generic Gate10 transport or old helper-lifetime gates without a
changed dependency. Do not replace camera work with repeated vertex counts,
constant fitting, light tuning, PNG tooling or hardware-blocked investigations.

## Boundaries and remaining acceptance

Native fallback and experimental-off defaults remain mandatory. Public DLAA
remains separate. Do not inspect/patch/acquire third-party binaries or game media,
invent NVIDIA keys, or automatically change external configurations. Existing
explicit user Apply/test-sweep authority is narrow, not blanket permission.
Preserve RTT/direct-framebuffer bypass, original game effects, protected HUD,
late OSD/ImGui and history advancement only after accepted evaluation.
Intentional Soulcalibur weapon trails are source effects, not neural defects.
Archive index assumes an owned frozen diagnostic directory; selected contents
are revalidated, but metadata caching is not hostile-filesystem authentication.

Full success still requires the backlog's supported camera/world contract,
ordinary interactive combined gameplay, synchronized300-frame four-lane moving
evidence, normal/OIT600-frame noncapture cadence, timing/resource/latency data,
emulation-cycle/audio checks, transition/failure coverage and applicable visual
gates/title coverage. Diagnostic anchoring and synchronous replay do not satisfy
these. Keep the full goal intact; no production-ready or highest-fidelity claim.
