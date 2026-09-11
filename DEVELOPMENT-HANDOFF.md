# Flycast experimental Remix + external DLSS 5 development handoff

## Current checkpoint: capture texture-query batching committed (LOG893)

Proof, moving and matched cost runs pass their exactness checks; no
capture-cost gain claimed (native-draw 14.71 ms inside the earlier spread).
Four builds and three selftests pass; committed. Correction (LOG894): the
CODEX-GOAL OIT items are already done and accepted (LOG807 to LOG845, H17
to H24, about 18.3 ms median at 1280x960). HEAD re-measure on the OIT route
recorded in LOG894. LOG895 attributes the normal route's 71 ms frame
period to per-frame D3D11 resource creation and copy traffic in the native
effects capture (about 126 owned objects per frame, 11.3 ms median capture,
39 ms driver gap). LOG896/D-237: the pool is committed; render-thread
frame period on the normal route 76.6 to 21.2 ms, native draw 18.0 to
0.74 ms, equality proof and per-frame composition exact. The route's
freshness is now feed-worker and helper bound (347 worker-busy skips in
the capture run; the no-capture run is 99.1 percent fresh at 18.2 ms).
LOG897: the native-lane floor is 11.3 ms and the emulator thread is busy
about 16 ms per frame, so the source-observation hooks (about 250k store
hooks per frame) are the remaining 60 fps item on both routes. LOG898:
the inline store fast path covered 29 percent of store hooks and gave no
whole-frame gain (rejected, reverted); the gap is the aggregate of all
observation hooks (about 5 ms clean), so 60 fps at 1280x960 needs the
observation scope narrowed, a user decision on the anchor certificate.
LOG899 attributes VRAM by phase (no leak; a 512 MB block toggles below the
owned objects; per-sample VRAM now in every performance report). LOG900/D-238: alpha ownership on the normal route exercised live (0
exclusions in this scene, composition exact) and the presentation latch
now recovers after 60 fresh ticks. Next, in order: (1) put the 60 fps
observation-scope decision to the user with the LOG897/LOG898 numbers;
(2) human visual review of the composited pilot output; returned-output
integration with alpha ownership and resource accounting (LOG859 next
item); lifecycle/budget contract review for the 120 s helper watchdog
without captures (retain failures); (2) VRAM by phase and pressure/stall
measurement rather than raw growth rejection; (3) 60 fps remains open on
both routes; do not repeat micro-optimizations without renewed evidence of
a meaningful bottleneck (LOG845).

## Previous checkpoint: capture texture-query batching (LOG892)

HEAD29b056f21 verified. New uncommitted candidate batches256 VS/PS texture
Get calls into2, retaining all slots and releasing returned references with
RAII on early failure. Existing WARP controls pass. Automation build38316 active:
normal-capture-batch-build.log. Next poll recorded session, native equality,
then same-profile capture cost. No game/helper running at build start.

## Accepted replay checkpoint (LOG891)

Parent3371b7b1b4548c0b3d9f40eacd56374ea1489ee2. Replay submits all348
vertex/constant/view/sampler bindings in7 calls, including null slots. Snapshot
ownership and per-draw constants remain unchanged. Added gated CPU scopes for
normal-color-conversion and native-effects-compose. No ordinary-mode promotion.

Evidence: focused WARP mutation/source/layout/alpha/depth controls pass. Native
proof pilot-normal-batch-proof sources2560..2562 matches native and previous
reference byte-exact at1280x960. Moving pilot-normal-batch-moving has12 exact
composition/backbuffer checks and completed-Present joins;492 mesh materials
and camera/geometry match prior profile-A run. Moving62846 exits0 with host0,
helper11 orderly=true, no forced children; host log archived.

Matched417 source-frame CPU scopes: effects-compose median2.4913ms before,
0.5815ms batched. Overall speed is NOT accepted: native drawing slower and
no-capture diagnostic84794 hit helper watchdog124. No performance-eligible
run or60fps claim. Existing helper worker budget is120s without captures,
420s with diagnostic capture; initial source wait can consume90s. Review
lifecycle/budget contract separately; retain failures, never relabel them passes.

Validation matrix2633 passed: four serial builds (automation no-op recheck), all three enabled selftests986/0, SDK302/0 and Python26. Logs batch-final-*.log under D:/Flycast-Evidence. No game/helper/build active. Private assets untouched.

Next engineering target: measured normal native-draw capture overhead, which
remains much larger than color conversion (~0.2ms) and batched replay (~0.6ms).
Audit capture-side binding queries/copies and measure before accepting changes.
Preserve same-source native replay, moving effects/HUD, freshness and latency.
Use existing profile-A/temple lights: 12-frame matched comparison reduced world
any-channel>=250 pixels1.48345% to0.005327%, but hair/edge artifacts persist.
Visual quality, external neural provenance, full normal coverage and lifecycle
acceptance remain OPEN. Broad goal ACTIVE; eGPU evaluation follows CPU work.

Evidence roots: pilot-normal-batch-proof, pilot-normal-batch-cost (cost-summary
and matched-compose-cost.json), pilot-normal-batch-moving (independent-
composition-check.json and archived host log), pilot-normal-color-profile-a.
All are under D:/Flycast-Evidence. Historical notes below are chronology only.

## Historical CPU scheduling checkpoint (LOG841 / D-233)

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
LOG828 h17 display diagnostic terminal0/11,log/summary archived under
pilot-h17-display-cpu1280.600 medians:target+upload0.8589ms,context/Quad
1.7295ms,record0.03725ms,execute0.0343ms,total2.877ms. Candidate retains
renderer context/Quad,checks device,returns only successful finished streams,
discards failed partial recording,clears at Term,counts6 owned resources.
Per-frame targets/initial upload remain unchanged. All4 builds pass,982/0
x3,SDK302/0,Python24. Candidate diagnostic pilot-h17-reuse-cpu1280 terminal
0/11,log/summary archived:setup1.7295 to0.0005ms,total selection2.877 to
0.9844ms,frame-display1.07845ms,render14.44085ms,emu20.137199ms. Clean
performance,moving/restart checks pending; no source acceptance yet.
LOG831 built-in trial complete:4/4 outputs returned,all4 rejected for technical
integration. Two materials retain baseline. Agent+parent reviewed all images;
baked lighting,roughness semantics and detail/seam agreement fail,all1254square
instead of1024. Floor midline was amplified,not proven newly invented (retained
height has faint seam). No scaling/retries/integration. Report/images/ledger:
D:/Flycast-Evidence/package-d-builtin-trial-20260912/REVIEW.md. Generation done.
LOG833 / D-232 H17 display context/Quad reuse ACCEPTED for1280 OIT pilot.
Clean runs pilot-h17-reuse-perf1280 / -b terminal0/11,orderly,eligible;
p50 18.9847/19.2234,p95 24.1773/25.2614,p99 26.9177/28.6518ms;
fresh1074/1080 and1073/1080,max4,zero identity errors. Logs archived.
Moving first run has no HUD coverage; moving-b proves12 exact completed joins
with72537..74349 protected pixels andzero HUD/world/backbuffer mismatches.
Actual2561/current2567 PNG reviewed. Restart at main2400 recovers g2,
terminal0/11,1200 returned post-warmup Presents,one repeat,max4,no identity errors.
Post-restart objects200->200 andVRAM4,772,089,856->same; full resource audit
remains open. Clean final object difference12 versus control only partly
explained by six retained objects; do not invent remaining attribution.
All current jobs terminal,logs/summaries archived. Four serial builds982/0 x3,
SDK302/0,Python24 and backlog checks pass. Next inspect target/initial upload
(~0.824ms) for redundant full-screen overwrite; preserve raw-return fallback,
exact HUD/world/backbuffer equality and performance denominators. Full gates
and visual approval open. Source checkpoint10a7ad2e2 (fork verified) remains accepted. LOG834 H18
redundant-upload candidate REJECTED for promotion; only owned source removed,
patch preserved in pilot-h18-upload-perf1280-b/rejected-source.patch.
Four serial builds982/0 x3,SDK302/0,Python24. Diagnostic target0.8241->0.2979ms,
selection0.9844->0.44705; moving12 completed joins,72537..72769 HUD pixels,
zero HUD/world/backbuffer mismatches,actual2561/current2566 reviewed. Clean
runs18.8951/19.6139ms medians,p95 26.7405/25.9939,p99 29.6601/29.6387:
no robust benefit versus h17 18.9847/19.2234,p95 24.1773/25.2614.
Fresh1075/1080 and1076/1080,max4,no identity errors,all terminal0/11.
LOG836 H19 thread CPU sampling complete on accepted staged h17. Both runs
terminal0/11,logs/raw snapshots/summaries archived. First lost names due to
HRESULT==0 bug; own-process control proved nonzero success,repeat fixed>=0.
Corrected -b after sampler20s has25 named intervals:Flycast-emu mean90.68%,
median95.78% of one core;Flycast-rend66.97/71.02%;helper geometry-processing
threads97.40/98.47 and97.63/98.47%. Names identify roles,not call stacks or
useful versus spin work. Diagnostic observer means no clean FPS claim.
No jobs active,no source changes. Existing frame-gap includes queue/Process/
present/cleanup,not a measured driver stall. Next refresh existing bounded
--hook-cycles diagnostic on accepted h17 for remaining emulator hot paths;
LOG790 reductions are already implemented,do not repeat that setup. The helper
geometry CPU saturation may be work/spin; do not infer or change private runtime.
LOG841 / D-233 H21 grouped live-register scan plus H22 paired helper copy
ACCEPTED together for1280 OIT pilot. Standalone H21 LOG838 freshness failure
remains failed. Combined two clean medians18.4732/18.9916ms,p95 21.9197/
22.0984,p99 24.7582/25.74;both1074/1080=99.444444% fresh,max4,zero identity
errors,8raw repeats,22native,orderly0/11. Median benefit varies;tails improve
versus h17. No60fps or broad acceptance claim. Logs/summaries archived.
Four serial builds983/0 x3,SDK302/0,Python24. H21 scalar scan equivalence
and10 same-source packets/239334 vertices+indices exact;H22 eight depth checks
x1,228,800 pixels exact,12 moving completed joins,72629..78153 HUD pixels,
zero HUD/world/backbuffer errors. Actual2560/current2567 image reviewed.
No active jobs. Host staged flycast-pilot-h21-live-groups.exe plus current
build-neural-automation/neuraltest/remake-runtime-smoke.exe is tested combined
pair. Source checkpoint is commit containing LOG841. Prior candidate patches
and failed captures remain private. Next inspect emitted RAM-store/register-read
hook work for repeated lookups/temporary initialization; preserve all witness
semantics. Phase/resource/normal-renderer/full-quality/human gates stay open.
LOG843 H23 accepted: six scalar stores replace inactive144-byte read temporary.
Four serial builds,selftests985/0 x3,SDK302/0,Python24/0. Two clean runs
18.2998/18.3807ms median,21.4133/21.3866 p95,99.444/99.537% fresh,max4,
zero identity errors. Moving12 completed joins/nonempty protected HUD,zero
HUD/world/backbuffer errors;10 same-source meshes preserve239,322 vertices
plus indices exactly. Evidence pilot-h23-read-reset-{perf1280,perf1280-b,moving}.
Staged host flycast-pilot-h23-read-reset.exe plus accepted H22 helper is tested.
No active jobs. Next inspect finishSourceRead repeated writer lookup/optional
copies before choosing another bounded CPU change. Full acceptance stays open.
LOG845 H24 accepted: combined RAM observation lookup;four serial builds986/0
x3,SDK302/0,Python24/0. Clean medians18.2321/18.4037ms,p95 21.3038/21.1179,
fresh1074/1080 and1073/1080,max4,zero identity errors. Median roughly unchanged,
p95 slightly improved. Moving12 completed joins/nonempty HUD,zero composition
mismatches;10 matching sources239,334 vertices plus indices exact versusH23.
Evidence pilot-h24-read-observation-{perf1280,perf1280-b,moving};logs archived.
Staged automation host flycast-pilot-h24-read-observation.exe with unchanged
H22 helper. No active jobs. LOG846 diagnoses normal-renderer gap: only OIT produces retained fragment
snapshots;normal drawStrips blends immediately. Reactive coverage has no depth
and cannot substitute. Next inspect source-owned normal draw replay resources,
then prove retained effects over native pre-effect color equal native output
before Remix integration. Preserve blend order,depth,alpha,HUD and producer
identity;never remove the requirement or relabel OIT as normal.
LOG847 ownership audit: existing packet replay borrows live texture/palette/fog
and mutable constants;COM retention does not freeze contents. Next implement
normal translucent DrawIndexed capture with native pipeline states and owned
geometry/constants/texture/depth copies,deduplicated per source. Prove native
background equality and rejection/isolation of later resource mutation before
returned-output integration. Retire through existing overlay slots and count
owned resources. LOG848 unintegrated owned helper core/rend/neural/remake_native_resource.h
implemented. Private normal-effect-resource-test.cpp/.exe passes WARP buffer/
texture later-mutation isolation and null/deferred rejection;not hardware proof.
Exact Texture2D SRV descriptor capture added;WARP mip1/array slice1 view
and later-mutation pixel isolation passed. Non-2D views rejected. Next capture
native draw state and replay over owned native background. LOG849 adds
unintegrated remake_native_draw.h capture/replay prototype;MSVC compile only,
resource-isolation fixture still passes. Next exact offscreen draw replay plus
mutated live-resource control;review UAV/ranged constants and context restoration.
LOG850 actual basic draw replay passes WARP:120 nonzero pixels,live geometry/
constant mutation changes output,retained replay after ClearState equals all256
original pixels. Private normal-effect-draw-test.cpp/.exe. LOG851 UAV/ranged-CB guards implemented and WARP rejection controls pass.
GetConstantBuffers1 null-output crash corrected with actual owned output.
LOG852 combined WARP texture/blend/GEQ-depth replay exact after mutations;
depth negative control changes output. Private normal-effect-draw-combined-test.*.
LOG853 remake_native_context.h adds scoped D3D11.1 state swap;WARP test
restores vertex/constants/target/depth after exact combined replay. Three owned
untracked headers now. LOG854 remake_native_effects.h wrapper owns color/depth/draws,seals expected
count and rejects producer mismatch. WARP exact after color/depth mutation;
wrong epoch/ordinal/cycle rejected. Four headers owned/untracked. Next diagnostic
normal-renderer pre-translucency capture and DrawIndexed append/native comparison,
then native-game equality. No physical-device or game acceptance yet. Both headers
untracked/owned;do not lose it. No production call site,four-build commit gates
not yet run. No active jobs.
Full60fps,quality,resource and human visual gates remain open.
PBRFusion4 user-requested subagent check complete (LOG835): five depth/normal/
intensity outputs,no delit albedo/roughness/metallic. Existing PBRify maps kept.
No installation/download/GPU runs. Review with proposed HTTP graph:
D:/Flycast-Evidence/package-d-pbrfusion4-review-20260911/REVIEW.md.
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

LOG830 review while trial runs: CaptureRemakePreview compares each output
pixel against native where mask>=128 and evaluated-world elsewhere; it also
compares RGB against pre-OSD backbuffer. H17 moving acceptance requires all
three mismatch counts zero,not only HUD. This isolates composition correctness
from upstream neural color variation. All generation calls now terminal; final disposition in LOG831.

LOG859 / D-236 current checkpoint: normal native-effects diagnostic accepted,
three1280x960 frames pixel-exact,images reviewed;four serial builds pass,
986/0 x3,SDK302/0,Python25/0. Run-a unexercised and helper-before-first-source
failures retained. No active jobs. Source commit contains LOG859.
Next normal returned-output integration: carry source-owned normal snapshot
through existing overlay slots,alpha ownership and resource accounting;avoid
per-draw duplicate immutable resources before timing. Diagnostic copies are
not performance evidence;normal gameplay/full lifecycle acceptance stays open.

LOG860 current uncommitted change: NativeEffectSnapshot Compose caller color,
strict source/layout guards,owned RT/SRV output;WARP exact/input-immutability/
alternate-background/extent controls pass. Header only;not live integrated.
Next alpha selection mapping,overlay source-slot ownership and resource counts.
No active jobs;required build matrix not rerun for this change yet.

LOG861 alpha mapping prototype added,existing source selection validator reused.
WARP selected color exclusion and wrong-parameter rejection pass. Depth-writing
exclusion fixture still needed;current fixture disables depth writes. Native
capture call sites need parameter table and exact ordinals before live selection.
No active jobs;uncommitted changes in native draw/effects headers.

LOG862 depth-writing exclusion WARP passed120 depth pixels with unchanged color.
Renderer exact list/sorted ordinals and parameter table now wired. Automation
build42160 active;poll same handle,normal-alpha-map-build.log. No game active.
Next source-slot integration and resource accounting;all changes uncommitted.

LOG863 alpha-map build42160 passed. Existing overlay has normalEffects ownership;
retained resource accounting deduplicates snapshot pointers. New automation
build55722 active,normal-slot-ownership-build.log. No live publication/composition
yet. Next carry source capture through slots and route returned composition.

LOG864 ownership build55722 passed. Composition now dispatches through overlay
with exactly-one/source matching;OIT-only locked replay rejects normal safely.
Routing automation build71514 active,normal-compose-routing-build.log. Next
normal capture retention/publication and preview provenance;no normal output yet.

LOG865 routing build71514 passed. --normal-effects diagnostic-only prototype
now retains source normal snapshot and feeds existing slots. Automation18276
active,normal-live-prototype-build.log. Before launch check compile and replace
OIT-specific provenance log label on normal branch;format checks stay strict.
No live normal output demonstrated yet. Source uncommitted.

LOG866 normal-live-a active15800;poll same handle,archive stage flycast.log at terminal. Builds18276/99352 and Python25 passed. First live normal composition diagnostic,12 captures from2560,not performance evidence. Source uncommitted;no acceptance yet.

LOG867 normal-live-a FAILED: helper bounded receive timeout before first source;
host still progressing atsource1476/108s. Closed own host gracefully after helper
terminal;session15800 terminal1,log archived. No active jobs. Next measured
capture-cost attribution and within-source immutable-resource deduplication,
not timeout inflation. Per-draw constants must remain versioned. No normal live
output proved;current source uncommitted.

LOG868 precise capture CPU/object counters added;automation46174 active,
normal-capture-cost-build.log. Poll then stage/run bounded attribution. Previous
run has no usable frame-pvr-draw timings;do not invent cost. No gameplay active.

LOG869 found duplicate source2 capture in neural color replay. Added original-
native-pass RAII guard;replay cannot capture/clear snapshots. Source275 measured
23.661ms/41draws/494objects,not full GPU time. Host closed,log archived;session18135
awaiting helper35592 exit. No build until terminal;then rebuild/reprove corrected
source capture. Prior native-proof pass ownership now requires revalidation.

LOG870 no active jobs. Original-native-pass guard build75636 passed;corrected
proof60933 completes3 exact1280 native/replay sources2560..2562,images checked
and viewed. Log archived in pilot-normal-native-pass-proof. Next within-source
stable-resource deduplication with changing constants preserved;retest ownership.
Live normal composition still unproven,all current changes uncommitted.

LOG871 within-pass geometry cache candidate;WARP geometry sharing/distinct constants and replay controls pass. Automation73193 active,normal-geometry-cache-build.log. Next native equality/capture cost remeasure. No gameplay active;source uncommitted.

LOG872 historical: geometry build/proof passed3 exact game sources2560..2562,including
previous native reference. Log archived. Cost run98758 active,
pilot-normal-geometry-cost;poll same handle,measure source/object/time and
archive stage log. Source uncommitted;no live normal composition claim.
