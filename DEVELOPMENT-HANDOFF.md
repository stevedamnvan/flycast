# Flycast experimental Remix + external DLSS 5 development handoff

## Current state (2026-09-11, LOG824-825 / D-231)

CPU scheduling checkpoint ACCEPTED for the tested1280x960 OIT pilot only.
The accepted baseline combines off-thread smoothing, one bounded FIFO pending feed,
reused smoothing scratch, redundant-sort removal and explicitly owned retained
anchor workers. Earlier h8-h12 rejected experiments remain in LOG815-817.
The broad app goal is ACTIVE; neither full CPU optimization nor60fps achieved.

Final four serial builds pass;982/0 selftests in all3 enabled configurations,
SDK302/0,Python24,backlog contract pass. Production executor output matches
scoped-thread packet bytes; smoothing matches frozen original arithmetic.
Initial TLS-worker shutdown failure is fixed by feed-worker-owned executor.
Final performance pilot-h12-workers-perf1280-b/c:1200 samples,2100 warmup,
original exposure A,no captures/scopes; both eligible,orderly exits0/11.
Present p50/p95/p99=20.261/24.845/28.1081 and20.7264/25.6128/28.5602ms.
Existing120-frame exclusion:1070/1080=99.074074% and1075/1080=99.537037%
fresh Remix over all remaining presents; max latency4 both,zero identity errors.
Raw repeats13/8,native23/24 remain reported; denominator has not changed.
Original pre-offload h7 p50/p95=22.246/25.8046ms. About48-49fps now,not60.
Diagnostic anchor median4.25475 to3.4157ms; total feed15.9912 to15.31ms.
Moving pilot-h12-workers-moving-b:12 captures,12 exact completed-Present joins,
zero protected HUD mismatches; source2574/current2578 composited PNG reviewed.
This focused check does not replace300-frame quality/600-frame full acceptance
or normal-renderer coverage. Human visual approval and neural contribution
remain open. Existing exposure0.30 candidate stays opt-in, timing uses A.

All runs/builds terminal and host logs archived; no game/helper/build active.
Source checkpoint is the commit containing LOG818; verify HEAD/fork live.
Private evidence lives under D:/Flycast-Evidence and is not staged.

h13 exact-input smoothing cache REJECTED and its owned source changes removed.
Automation1006/0 but only600/24106 mesh hits (2.49%) in600-frame diagnostic;
packet/smoothing6.26935ms versus h12 5.87695, feed15.4397 versus15.31.
No performance-eligible run warranted. Run terminal0/11, host log archived;
patch/header retained privately under pilot-h13-cache-cpu1280. No active jobs.
LOG821 h14 exact-key weld grouping ACCEPTED. Four builds982/0 x3,SDK302/0,
Python24. Packet+smoothing5.87695 to3.2435ms; total feed15.31 to12.37185.
Clean h14-group-perf1280 / -b medians19.8419/19.6728ms,p95 23.8781/23.3666;
fresh1071/1080 and1073/1080 after existing120 exclusion,max4,zero identity
errors,orderly0/11. Moving12 completed joins,0 HUD mismatches;10 matched
sources against h12 have239322 vertex records+indices byte-identical. Source
2573/current2577 reviewed. All runs/builds terminal,host logs archived.
LOG822 h15 returned-input conversion rejected: local input-build CPU fell
3.01725 to2.0701ms, but clean gameplay medians20.0565/21.8636/20.0642ms
versus fresh accepted h14 control19.1361ms. Four builds1012/0 x3,SDK302/0,
Python24 and moving12 joins/0 HUD mismatches passed. Owned candidate source
removed; patch/header retained under pilot-h15-input-perf1280-c. All jobs
terminal,logs archived. Accepted source3264e2b1b remains the baseline.
LOG827 h16 retained-effect-context candidate rejected for promotion. Local
composition1.1255 to0.45365ms, but clean candidate20.50/20.36ms versus fresh
h14 control20.60ms is not robust whole-frame gain; p95 candidate28.79/25.98
versus control25.04ms. Control/candidate b both1076/1080 fresh,zero identity
errors,max latency3/4. All runs terminal0/11,logs/summaries archived.
Moving12 completed joins/0HUD mismatches; brightness difference from older
h14 capture remains unattributed,not visually accepted. Four corrected builds
982/0 x3,SDK302/0,Python24 passed. Owned source removed; patch preserved under
pilot-h16-replay-perf1280-b. Accepted source3264e2b1b remains baseline.
Next H: reassess existing whole-frame CPU/worker/GPU coverage and critical
path before another micro-optimization. No build/game/helper active. Do not
repeat h15/h16 or mistake diagnostic savings for delivered FPS. Full60fps,
normal-renderer,resource lifecycle and visual acceptance remain open.
Package D authorized subagent completed offline accounting additions in the
existing external pbrify_run.py. Parent independently reran12 tests:pass.
Production dispatch always disabled. Backup/diff/test evidence in
package-d-preflight-20260911/BUDGET-CONTROLS.md. Inventory membership/source
revalidation and actual no-retry bounded transport remain unfinished. No real
budget approved; no paid requests. Original functions AST unchanged by agent
check. Full-set preflight:26 materials,104 retained maps,67 proposed outputs;
USD10.43 provisional,USD20 proposed only. H engineering remains first.
User priority: spend memory on useful retained work; judge pressure, evictions,
stalls, bounded lifetime and cleanup rather than minimum allocation. Current
VRAM growth1,724,174,336 bytes is reproducible, not a proven leak; phase/plateau
attribution and40-object teardown warning remain separate open work.

User hardware: RTX5090 plus Thunderbolt4 RTX5060Ti. Evaluate dual GPU only
after CPU work. Direct display hookup/live5060Ti enumeration remain unverified.
MGPU Bridge upstream architecture was read; no binaries downloaded or private
neural code copied. D3D12 add-on compatibility with D3D11On12 remains unverified.
https://github.com/maohgad-web/Neural-coprocessor

Performance follow-up LOG814: batched R32F conversion reduces helper cost
3.6393 to0.79905ms;937/0 x3, SDK302/0, Python24, four builds pass. Eight
full-image same-frame scalar comparisons bit-identical; moving12 captures
and completed Presents,0 HUD mismatches, source2574 reviewed. Performance
run pilot-h7-perf1280 is22.246/25.8046ms p50/p95 versus22.6025/25.0958:
no robust overall speed-up;60fps remains unresolved. All runs terminal,
logs archived. Next investigate view-scene conversion/smoothing on render
thread (4.6593ms of6.9239 scene-feed). Motion rebuild is only19 samples,
not a steady bottleneck. Preserve exact certificates/output and original
exposure denominator. See LOG814 for web sources and rejected extrapolation.

CODEX-GOAL six-item CPU-cost optimization is complete (LOG813).
Items1-5 committed through45f6ffba3; final performance evidence is
pilot-h6-perf1280-a and pilot-h6-perf640-a. Original exposure A,1200 samples,
2100 warmup, no captures/CPU scopes/verification, both performance-eligible.
1280 present p50/p95=22.6025/25.0958ms versus26.4818/29.6755 baseline;
640=19.6594/23.4325. Helper period medians22.13185/19.69855ms. About44.2fps
at1280,50.9fps at640:60fps is NOT achieved.915/0 x3, SDK280/0, Python24;
four builds passed. All runs finished, host logs archived, no active helper.
Item5 same-frame format proof: eight pairs,1228800 bit-identical depth values
each (pilot-h5-depth-format-exact-b). Rejected inherited-control runs and
first accumulating readback-timer evidence remain documented in LOG812.
Broader backlog remains open:60fps, resource growth/teardown ownership,
normal renderer, full neural provenance/quality and human visual approval.
1280 VRAM growth627601408 bytes, objects149 to188; both extents max latency4.
The existing lower-exposure0.30 candidate remains opt-in; performance evidence
uses original A. Follow BACKLOG for further work; do not mistake completion
of this bounded CPU goal for a polished/full accepted remaster.

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

Package D correction: built-in imagegen available to parent/subagent without
API key; official docs name gpt-image-2,not verified Sunburst. No exposed model
selector. Proxy cost/retry blockers do not describe built-in availability.
Preflight and BACKLOG corrected; no generation or route change authorized.
