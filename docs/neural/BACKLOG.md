# Neural rendering backlog

## Active execution authority - 2026-09-08

This is the single current queue. User authorization now covers autonomous,
bounded engineering progression through it, not repeated approval per trace.
D-114 records this authority and supersedes procedural permission stops.
Old audit stops are historical scope, not active permission gates. AGENTS.md
retains the hard boundaries; the remake plan retains architecture/acceptance.
Do not create replacement FC IDs or another task-state system.

### Short standing goal

> Follow docs/neural/BACKLOG.md autonomously until Flycast has a verified working experimental RTX Remix + externally supplied DLSS 5 gameplay pipeline. Implement, test, record evidence, and advance through the backlog without routine human approval; preserve its safety boundaries and stop only for a genuine external blocker or the documented working-pipeline acceptance.

Run with Astra low as requested. On 2026-09-08 the app reported no existing
goal, and the short objective above was successfully registered as active.
Use the goal tools to verify current state; this document is not live app
telemetry. Do not falsely complete an objective or edit app storage to replace
it. The tracker carries the objective; this backlog carries the work plan.

### Current checkpoint

- Latest pushed checkpoint: `5e68316f0634abc57474f0297f63e96f0a4aea30`; LOG627-632 record launcher synchronization and accepted-history corrections plus completed28-frame changed-guidance provenance. Read actual HEAD/status on resume; builds are incremental, not fresh exact-SHA evidence.
- FC-067: scene/material export, public-header mock, selected opaque FTRV-to-TA
  lineage and native preservation are proven only in their recorded scopes.
- Actual transform `8c03c944` in block `8c03c93a` supplies opaque vertex4.
  Preserve nonunit W. Source coordinate space, matrix semantics and broader
  coverage are unproven. Use OPAQUE-INITIAL-STORES-AUDIT.md, LOG #175.
- Bounded synthetic and textured scene GPU captures exist (LOG401-422).
  Combined returned-scene presentation is experimentally demonstrated, including
  repaired OIT exact-input external-output proof (LOG544). Complete moving
  relighting, camera and working-pipeline acceptance remain unproven. M2 strict
  source equality remains failed/parked.
- Existing standalone neural transport/provenance and runtime coverage remain
  valid evidence; do not redo them just because a new task starts.

### Ordered queue

Current card: **FC-067 / M2-scene**.
Usable camera contract: **pending**.
Statuses: `todo`, `doing`, `blocked(reason -> next action)`, `done`.
A blocked card does not block independent rows. "Done" requires linked evidence,
not a plan or successful API call. The labels below are FC-067 substeps, not
new FC IDs or implicit acceptance of old M1-M5 requirements.

| Card | Status | Dependencies | Deliverable / acceptance | Evidence |
|---|---|---|---|---|
| FC-067 / M2-camera | todo | LOG175, LOG509-511 | Complete the usable coordinate/camera contract alongside the supported live-scene experiment. Retain camera-relative labels and failed exact arithmetic; no further count-only or constant-fitting phase. | Live common-origin subset and measured calibration candidate exist; world/camera acceptance pending |
| FC-067 / M1-GPU | todo | public-header adapter already tested | Preserve verified synthetic and actual moving GPU output. Remaining runtime cleanup warning needs focused ownership work, not repeated factory/camera bring-up. | LOG404-412, LOG467-474; GPU output verified in bounded scope, cleanup warning remains open |
| FC-067 / M2-scene | doing | LOG484 owned snapshot, LOG509 live dependency subset, LOG467 legacy uploader | Connect owned live geometry/materials to the existing uploader, then advance return-image integration. Experimental camera-relative transport may be implemented now; completed scene acceptance still requires a supported camera contract, moving coverage/depth evidence and explicit omissions. Parked strict replay failure is not waived. | LOG529 ordinary-frame feed matches60 moving fighter/arena scenes and paired image files, with56 bounded retained replies. Camera-relative approximation remains explicit; delayed HUD/returned presentation and combined ordinary gameplay remain pending |
| FC-067 / M3-relighting | todo | M1-GPU, M2-scene | Actual moving Soulcalibur fighters/arena rendered through Remix with source assets, controlled lighting, stable camera/occlusion and explicit baked-light/material limitations. Preserve moving native/Remix comparison. | pending |
| FC-067 / M4-presentation | todo | M3-relighting | Return actual Remix output to Flycast with explicit ownership, synchronization, frame identity, bounded latency, native fallback and protected HUD/OSD. Source scene/guidance must describe the new image. | LOG542 sustained OIT delivery and LOG544 completed original-HUD Presents are experimental evidence; scene completeness and full acceptance remain open |
| FC-067 / M4-DLSS5 | todo | M4-presentation | Prove that returned Remix scene reaches the supplied external DLSS 5 consumer and the combined result reaches Present. Exact-input ON/OFF, active consumer tuple, native/public/Remix/combined distinction and focused Gate 10 negative controls. | LOG564 confirms28 consecutive exact-input external results after native effects/HUD and completed Present, including effect identity and marked/clean/OFF controls. Full moving quality matrix and upstream scene acceptance remain open; do not repeat this regression without a changed dependency |
| FC-045, FC-054, FC-055, FC-063, FC-064 / combined hardening | todo | M4-DLSS5 | Capture/overlay/transition/failure/cadence checks on the changed route; asynchronous performance and repeatable launch. Satisfy the working-pipeline checklist below. | pending |
| FC-067 / M5 and FC-065 / style expansion | todo | working pipeline; legal content where needed | Optional further art direction and title coverage. Not substitutes for making the combined route work, and not factory-default promotion without the existing quality gates. | pending |

### Next-card bounds: FC-067 / M2-scene

**Operational next action (LOG651):** save the long-capture/review checkpoint,
then address material delivery causing gameplay source gaps. All300 captures
2164..2470 have nonempty exact HUD/composition/backbuffer and completed Presents;
four gaps remain and prevent a continuous300 claim. LOG652 resolves six omitted
sources to material-cache-pending and2377 to no-return-credit. Improve current
texture availability and inspect backpressure without stale texture reuse,
relaxed ownership, GPU waits or emulation slowdown. Do not begin another
HUD-floor/light-tuning loop.
Native/returned/combined moving review is retained, with negative empty-HUD frames
marked as failures. Four builds and three711/0 suites pass. Default authored
light3 and external configuration unchanged. Review success does not establish
temporal quality, external provenance, full scene reconstruction or performance.

Lighting control is implemented at532da3645, but tuning is suspended until
the repaired interval survives broader moving inspection. The earlier40-frame
lighting comparison preserves its gap/unmatched records and declares no winner.
No external configuration changes or generic provenance rerun. Intentional
source effects remain protected; do not call them reconstruction defects.
GPU query availability remains deferred until an actual performance issue or
final acceptance requires it; incomplete timings must never become estimates.

Latest sustained OIT evidence:597 accepted evaluations,617 completed combined
Presents,595 distinct sources,149 longest consecutive source IDs. Camera rejects
fell281->1 with unchanged0.01px/depth/clip guards. The earlier10-frame claim was
a timing-report artifact, corrected in LOG638-640; presentation policy is unchanged.
Full300/600-frame exit criteria, residual camera rejection, moving pixel evidence,
complete timing and resource stability remain open. Do not declare acceptance.
Changed-guidance provenance remains accepted for28 exact-input frames (LOG632).
Do not repeat generic transport, mask thresholds or PNG-throughput detours.
D-190 remains parked. Failed launches and causal guesses remain in LOG.

Historical integration rationale (LOG620, completed/parked): public isolation is complete; do not expand
the settings sweep or repeat transport bring-up. Implement a bounded returned-color
reprojection consistency experiment at the existing accepted-history boundary:
protect shading/color changes that geometric depth/identity alone cannot detect,
without changing source effects or manufacturing motion. First prove rejection
and stable-surface controls, then rerun the frozen public/combined moving interval.
Measure retained detail and temporal behavior together; do not promote reset-only
or maximum rejection as the completed integration. co/cn already demonstrate that
detail loss exists with external hooks disabled. All six cm/co guidance binaries
match for all28 frames. Public temporal source/gradient MAE increases from
0.7441/0.8547 to3.4293/2.7305 while temporal delta drops2.4127 to1.7784.
Nearest-sample source reprojection on trusted pixels only marginally improves
over zero motion (1.92869 versus1.93107); this is not proof of incorrect vectors,
but geometric trust alone is insufficient evidence of stable returned shading.
Keep the complete pipeline goal, source identity and accepted-history invariants.

Previous isolation bounds (completed, LOG620): comparison/replay checkpoint a701b7155 is
pushed with all four builds and three673/0 suites passing. cn public-DLAA reset
replay verifies all30 accepted sources and28 frozen-input/final captures with
SAFE MODE confirmed. Finish co public temporal in the same hooks-disabled stage,
on ck frozen inputs and the same2250 evaluation boundary. Require SAFE MODE host
report, actual public evaluations and exact source/final checks; do not edit
consumer configuration. Isolate public reconstruction versus supplied-consumer
response, including focused changed-guidance provenance, before another quality
change. The comparer supports explicit lane labels so public DLAA is not called NR.
cl/cm prove28 exact-source2252..2279 moving comparisons and identical accepted
sequences2250..2279. Temporal RGB delta improves3.3103 ->2.0480 but source MAE
worsens12.2597 ->14.2512 and gradient MAE2.0058 ->3.5878. No visual winner or
default promotion. The moving comparison is generated/opened; stop coverage-only
tuning. ci/cj remain rejected because their returned inputs differed. The comparer
falsifies that mismatch before writing artifacts. Keep exact scene/effect/frame
checks and repeat the same frozen-source interval for the next isolation lanes.
Temporal locked replay is permitted only in bounded comparison/effect-identity
mode and requires original/current source-frame equality. Automation build/test
passes673/0. Do not bypass identity checks or compare differing upstream inputs.
Focused changed-guidance external provenance
remains separate. ch verifies58 accepted
evaluations,57 history-enabled and34..50-percent trusted pixel coverage in three
combat captures, with exact HUD/composition/Present checks. Both-surface shifted-
slope and crossing/thin-surface/previous-ID/excessive-offset fixtures pass668/0;
failed HLSL/early-return attempts remain recorded. Stop coverage-only tuning:
more trusted pixels do not prove a visual win. The previous ce/cf sparse coverage
is retained as rejected evidence. Do not redo camera anchor,
CPU matcher, reference ownership or generic transport bring-up. Complete the
serial four-build/commit checkpoint for the accumulated bounded follow-up.

The paragraphs below retain dependency and experiment history; their older
"next" instructions do not supersede the operational next action above.

Current action after LOG582: alpha-material integration now passes28 exact-input
marked/clean/OFF frames with source-qualified native exclusions and protected HUD.
Committed and pushed as52945db59; post-commit incremental four builds and three
608-test suites pass. Implement the usable camera / scene contract using existing
source observations and live uploader. First remove the live wire's identity-pose
restriction: carry supplied proper camera basis/position and fixed sequence origin
under an explicit diagnostic anchored scope, leaving legacy bytes unchanged.
Prove camera motion and projection survive serialization and malformed poses fail;
then connect a source-qualified sequence anchor, not a first-arbitrary-draw camera.
LOG586-594 wire and verify the off-by-default common-source camera anchor.
Final bz has62 publications, zero anchor rejections, maximum0.00245-pixel
projection error and three independently checked HUD/composition/Present captures.
Numerical failures and corrections remain recorded; physical world semantics,
scene completeness and broader moving acceptance remain unproven. Finish the
remaining build/commit checkpoint, then returned-scene temporal integration:
derive motion from owned geometry/camera against the last successfully evaluated
receipt, with depth/disocclusion/current-color protection. The current reset /
zero-motion/full-bias route remains fallback, not temporal quality acceptance.
The pinned public output API has no motion-vector enum; do not invent one or
inspect private binaries. Reuse existing geometry raster and correspondence code.
First temporal integration bound: explicit TEMPORAL_PREPARE developer mode
retains bounded geometry/camera/material-generation data without texture bytes,
bound to each published receipt. Validate it against the returned image and
retain previous successful evaluation plus depth, never latest emulated frame.
Failed/busy/duplicate/wrong-receipt results must not advance this reference.
This prepares correspondence; neural reset/zero-motion/full-bias remains unchanged
until the subsequent geometry-motion raster/disocclusion fixtures are proven.
LOG596-597 implement this reference ownership and verify58 actual evaluation
joins plus three protected final captures;648 automation tests pass. Finish
remaining builds/commit, then use the existing minimum-cost matcher with full
generation/topology validation to form current/previous geometry streams. Test
static/translation/deformation/reorder/ambiguity/gap controls before GPU motion
and returned-depth disocclusion. LOG599-600 implement/test the CPU stream and
measure58 actual evaluation candidates with up to26 trusted draws, preserving
three independently checked final captures. Shared motion/depth-consistency
shaders compile in664/0 automation selftests but are not yet dispatched. Current
next action is the bounded GPU raster and its perspective/depth/draw-ID negative
controls, then accepted-history resource integration and moving output. Do not
repeat CPU candidate logging or reference ownership as a new phase. LOG601 adds
the shared deferred GPU raster and passes both-surface fixtures (666/0). The
explicit TEMPORAL_RASTER experiment connects motion/mask uploads and accepted
draw-ID history; next run bounded cc gameplay, measure actual guidance coverage
and inspect moving output, retaining numerical fixture failures. No quality or
performance acceptance follows from GPU fixture success alone. LOG602 cc verifies
58 actual accepted evaluations,57 with history and zero GPU-guidance rejections,
plus three exact final captures. Next capture actual receipt-matched guidance
surfaces, measure trusted/reactive coverage and compare moving reset-only versus
temporal output with focused changed-guidance provenance. The connection now
executes; visual improvement and full working-pipeline acceptance remain open.
LOG603-605 add source-bound guidance captures and correct the host-wrapped device
owner check (failed cd retained; ce zero rejects). Actual trusted coverage is
only0.44..0.61 percent in three combat frames, despite58 accepted evaluations.
Current next action: identify current-depth versus previous-ID/depth rejection
with bounded raster-depth/reason surfaces and controlled sample-position fixtures;
correct the demonstrated mismatch before claiming useful temporal quality.
Do not loosen depth tolerances merely to increase counts.668/0 automation tests
include foreign-device and wrong-source rejection; remaining build checkpoint
and moving comparison remain open.
LOG606-607 localize the dominant failure to current-depth consistency, affecting
70..87 percent of pixels; uncovered geometry is only about2.3 percent. Global
offset fitting failed and is retained. Next implement/test depth-sample footprint
handling on analytic slopes with crossing/thin-surface negatives, then rerun
moving coverage. Do not hard-code a fitted offset or relax a global tolerance.
This transport seam alone cannot close M2-camera. Do not
restart generic effects/provenance or diagnose intentional native weapon trails
as ghosting. Preserve the explicit projected-depth approximation until evidence
supports an actual coordinate/camera contract. Returned-scene temporal guidance
remains a subsequent integration requirement, not satisfied by source guidance.

The following LOG-linked bounds retain the history of this integration slice;
completed experiments below are not instructions to rerun them.

Current action (LOG564): post-effect external-output provenance is accepted for
28 consecutive exact-input frames. Advance M2 scene/camera completeness toward
moving relighting; retain the camera-relative approximation label and use the
existing actual geometry/material uploader. No further snapshot/provenance phase
unless the next rendering change creates a specific regression. Capture throughput is
parked; LOG552's299 consecutive images do not satisfy300 and the acceptance
threshold remains unchanged. Reuse LOG542/544 delivery and external-output proof.

Implementation bounds: inspect the native sorted/OIT blend and depth paths and
retain source-owned effect data with the same receipt as returned color/depth.
Prefer native effect replay/composition with original blend/depth semantics;
do not admit translucent meshes as opaque, copy rectangular native patches,
or infer an effect layer by subtracting two gamma-encoded images. Preserve
intentional trails, alpha, additive light and occlusion; classify only additional
reconstruction persistence as ghosting. Keep HUD/OSD ordering and age8 fallback.
First falsifying fixtures must cover additive and alpha blending, opaque
occlusion, absent effects, wrong-source rejection and unsupported blend fallback.
Then inspect the same moving combat interval with source effects visible.
This is scene-completeness integration, not full camera acceptance. Do not add
another configuration UI before checking the existing activation/status controls.
LOG554 extracts the native OIT blend kernel for reuse and GPU-tests all64 mode
pairs with independent truth on both D3D11 surfaces. All four builds and574
enabled selftests pass; ordered delayed effect storage/composition, occlusion,
wrong-source controls and gameplay proof are still pending. A single flattened
alpha layer is rejected because it loses native secondary-buffer/clamp behavior.
Next implementation bound: exact developer-only FLYCAST_REMAKE_NATIVE_EFFECTS=1 retains
an immutable OIT fragment/pointer/parameter/constant snapshot for a single
autosorted640x480 source pass, using the existing two receipt slots. Each pixel
buffer is capped at512MiB; no readback/wait, unbounded queue or ownership by current
frame. Replay the original resolver over evaluated returned color on an isolated
deferred command list, then original HUD. Unsupported/missing snapshots reject
publication/evaluation; raw preview and locked-input evidence are not supported
by this first effect experiment. This costly copy-based path is functional work,
not performance acceptance; multipass and normal sorted rendering remain open.
LOG555 implements receipt-owned stack retention and post-evaluation resolve;
GPU snapshot/repeat/wrong-source/wrong-device tests pass, and startup probe at
now captures under the installed host. Earlier live capture failures are retained.
Av now completes37 source-owned effect composites, three pixel/Present checks
and exact native parity against floor-z, with clean exits. Current next check is
a moving interval with a visible source trail/particle, retaining pre-effect
neural color separately so effect contribution can be checked, then source-matched
effect/occlusion inspection and focused changed-output provenance. Do not call
the GPU fixture or successful resource capture restored gameplay effects.
LOG556 aw now verifies30 consecutive2175..2204 captures with unchanged native
pixels, exact final/HUD/Present joins and separated pre/post-effect images. The
blue weapon arc, impact light and particles at2192 are visibly restored. Commit
the independently proven opt-in OIT preservation slice; next retain native
effect contribution in focused changed-output provenance, then address remaining
scene completeness and bounded resource/latency costs. Do not restart the
300-frame PNG-throughput detour or claim all occlusion/multipass paths accepted.
Resource prerequisite before the next replay/provenance extension: expose unique
retained native-effect snapshot objects and logical copied bytes. Count shared
aliases once across current/pending/accepted/evaluated owners; do not label
logical bytes physical VRAM or omit the remaining uninstrumented temporary
resources. GPU fixtures must falsify double-counting and verify empty ownership.
This is a bounded accounting correction, not another general profiling phase.
LOG558 completes that prerequisite: unique aliases and separate allocations
pass GPU controls; live ax reports about513MiB logical copy per snapshot and
the expected first-snapshot object increase125 ->131. Physical VRAM, transient
resources and sustained timing remain unproven. Resume focused post-effect
provenance next; do not equate this startup probe with a stability pass.

Focused replay dependency: retain a source-qualified canonical description of
the visible native OIT fragment stack, including original color/depth/sequence,
referenced polygon blend/modifier parameters, resolver constants and variant.
Allocation addresses and unused fragment-buffer capacity are not semantic
identity. Preserve the native stable depth/poly ordering, including equal-key
ties; reject truncated, cyclic, out-of-range or unsupported stacks. First prove
that relocated equivalent stacks compare equal and changed color, blend, shadow,
depth/order, producer or resolver state fail. Only then connect bounded,
developer-only capture/replay verification and remove the existing locked-effects
rejection for positively matched inputs. No per-frame CPU readback in ordinary
gameplay, and no claim that an opaque scene digest identifies translucent effects.
This work unlocks the changed-output proof; it is not a new generic capture or
profiling phase. Keep the marked/clean/OFF comparison and completed-Present join.
LOG560 implements the bounded CPU canonical comparison component and exercises
relocated/unused storage equality, semantic mutations, stable equal-key ordering,
malformed pointers/cycles/depth/polygon and truncation rejection. It is not yet
wired to GPU snapshot extraction or replay. Next connect the owned GPU snapshot
to this description in explicit evidence mode, retaining exact resolver variant
and constants; then enforce equality before permitting locked effects replay.
LOG561 adds explicit synchronous identity readback from the owned GPU snapshot,
including actual compiled layer count, selected resolver variant and constants.
GPU fixture checks compare against uploaded CPU truth and preserve identity after
original-pointer mutation. This API is not called by ordinary rendering or yet
by capture/replay. Next wire bounded evidence artifacts and replay equality; keep
the guard until that linkage and negative controls pass. Staging the existing
large pixel allocation is diagnostic-only and must not enter performance runs.
Current linkage bounds: exact FLYCAST_REMAKE_EFFECT_IDENTITY=1 writes the retained
canonical words alongside preview captures and allows locked effect replay only
after byte-exact identity comparison in the already matched scene directory.
Cap identity capture and replay attempts at30 per renderer; missing, altered,
truncated or trailing-data evidence rejects. Existing input/source/age and
presentation checks remain. First run a three-frame live archive around the
known combat effect interval, then matched replay; do not claim provenance from
successful identity extraction alone. Entire run is synchronous/non-performance.
LOG562 ay archives three real stacks successfully; az rejects all three with
effect-replay-content despite matching scene inputs. Current next action is to
locate the exact differing canonical words using the added diagnostics, then
correct the cause or retain the rejection. Do not relax identity equality or
claim replay/provenance accepted from the successful archive alone.
Ba localizes all three failures to word28 (ditherDivisor.x), an uninitialized
field when dithering is off. Correct initialization of the shared constants and
explicit96-byte tail, preserving full equality rather than omitting state.
Next generate a new archive and replay from the initialized build; old ay stays
unchanged and is not compatible proof for the corrected constant bytes.
LOG563 bc now matches all three new effect identities exactly; source2173 also
completes evaluated composition/Present with exact independent pixel checks.
Initialization preserves all three native reference images. The replay linkage
is accepted in this bounded scope; next archive a longer post-startup interval
within30 attempts and run marked/clean/OFF exact-input external-output proof on
the changed post-effect path. Do not repeat generic transport or reopen capture
throughput. Full camera/scene/performance acceptance remains open.
LOG564 completes that focused matrix:28 consecutive originals2176..2203 have
exact scene/color/depth/motion/mask and effect identity, marked/clean returned
hash equality, external-versus-disabled differences, and clean original-HUD/
post-effect/backbuffer/completed-Present joins. Reuse this result. Next inspect
the current scene omissions and usable camera acceptance requirements, select
a concrete geometry/camera correction with moving source comparison, and advance
M2/M3 rather than reopening proven transport. The native effect layer remains
hybrid preservation, not ray-traced translucent materials or recovered world space.
Next bounded scene correction: the view exporter currently omits punch-through
draws (list1), and the D3D9 uploader has no alpha-test state. Carry an explicit
source alpha threshold through the owned scene/wire contract, preserving null
(opaque) versus threshold0 (enabled cutout). Keep old opaque archives readable
and reject cutouts in consumers until their alpha path is implemented/tested.
Then connect source-owned threshold and GPU alpha behavior, proving threshold,
texture/vertex-alpha, wrong-opaque-silhouette and moving native comparisons before
enabling the optional cutout scene lane. No translucent-as-opaque shortcut, no
camera/world acceptance claim and no silent threshold default.
LOG565 implements that explicit packet field and version2 round-trip contract;
opaque-only version1 remains unchanged. Both unimplemented consumers reject
cutouts, including threshold0. Next connect source-owned alpha state and GPU
alpha-test behavior; do not add more packet-only work instead of this hookup.
Current cutout implementation bounds: exact FLYCAST_REMAKE_PUNCH_THROUGH=1
adds supported list1 geometry with source-owned alpha reference captured beside
the uploaded native constants. Require default alpha clamp, ShadInstr3 and no
fog-replaced alpha; retain other unsupported-state omissions. IDs include list
identity; texture reads match the actual opaque/PT list and generation. A
Flycast-owned D3D9 pixel shader rounds alpha before discard and writes opaque
alpha after passing; public SDK material adapter remains unsupported. Native
D3D9 fixtures cover reference0/1/64/128/255, texture/vertex alpha, depth through
holes, wrong rounding and wrong-opaque controls. Now run the supplied runtime
with three diagnostic frames before claiming cutout integration accepted.
LOG566 native GPU alpha/depth controls pass; live bh exports two cutout meshes
with alpha reference255 but rejects their A8 palette-index textures at the
existing DDS boundary. No scene reaches Remix; host exits0/helper first-source
timeout exits1. Next extend the existing asynchronous, generation-qualified
texture cache to retain source index and palette resources plus exact bank/base,
then resolve colors/alpha using the existing DecodeMaterialTexel semantics.
Never treat A8 indices as alpha, bypass generation checks, drop those draws or
claim native-GPU tests prove the supplied runtime. Keep the current slice WIP
until this dependency and actual runtime behavior are validated.
LOG567 implements paired palette staging/decoding; automation589/0 and the
WARP palette fixture pass. Live bi advances past DDS rejection but fails
view-texture-binding, with no delivered scene. Native GPU-paletted cache keys
intentionally omit PalSelect; a shared index texture's cached TCW/palette_hash
cannot identify each draw's active palette bank. Next qualify the live cutout
binding using native index-resource identity plus draw-owned bank and current
authoritative palette generation. Preserve exact source-state checks; do not
merely remove the equality guard or use the shared texture's stale palette hash.
LOG568 corrects that binding. Live bj now delivers two cutout meshes through
Remix return, neural evaluation, native effects/HUD and completed Present.
Three independently checked captures2179..2181 preserve protected HUD and
backbuffer RGB exactly; both processes exit0. This is integration progress,
not cutout visual acceptance: image2180 contains conspicuous dark geometry.
LOG569 exact packets match for all3 bj/bk frames; dark geometry persists with
cutouts omitted. More importantly, isolated60-frame raster on/off runs using the
same source2180 packet identify the admitted meshes as HUD stage/time and round
timer:3426 changed RGB pixels, bounds28,22..357,75, zero below y100. Do not call
these missing world geometry or feed protected HUD into ray tracing simply
because alpha testing works. Next reuse neuralInstrumentation.IsOverlayOrdinal
classification at scene export, carrying exact list/ordinal mapping, to exclude
protected HUD meshes before uploader submission while retaining native overlay
composition. Then continue actual world/camera completeness; the dark world
shapes are not caused by these cutouts. Keep cutout lane off by default and
unaccepted for world coverage until a genuine world-cutout case is demonstrated.
LOG570 implements classifier-based source exclusion. Live bl excludes8 HUD draws,
including both cutouts, with3 independently verified original-HUD/backbuffer/
completed-Present captures and both exits0. Finish the build/commit checkpoint,
then investigate the persistent dark world shapes using retained source geometry
and controlled draw attribution; do not reopen cutout/HUD or generic provenance.
LOG571 narrows that artifact: identical source2178 rendered settled and in a
short consecutive2176..2178 replay lacks live fence-like dark geometry. Live
has large source gaps and unproven runtime scene retirement; short replay has
a resource refresh without the artifact. Next reproduce a discontinuity using
retained actual packets and test documented scene/object lifetime handling.
Preserve geometry/camera truth and do not hide stale geometry with exclusions.
CORRECTION LOG572: direct stage inspection disproves that attribution for the
observed shapes. Returned Remix and pre-effects neural images lack them; native
effect composition introduces them. Do not pursue speculative runtime resets.
Next retain actual native opaque resolver input, replay the same retained stack
over it and the neural background, and verify native-reference parity plus
per-pixel blend/depth provenance. Determine whether this is incorrect replay or
native destination-dependent shading exposed by relighting before changing it.
LOG573 completes that comparison:3 actual native-background replays exactly
match native RGB, zero mismatches/max delta/MAE. Do not suppress valid source
effects or implement speculative resets. Finish the bounded diagnostic's
ownership/build checkpoint, then resume scene/camera/material integration;
relit translucent world materials need deliberate treatment, while native
weapon trails/HUD remain protected. No additional generic effects-provenance loop.
LOG574 GPU ownership checks pass on both APIs. Finish the serial build checkpoint,
then inspect actual translucent-list world draw blend/depth/material identities
for a bounded material-integration slice. List membership alone is insufficient;
keep particles/trails native and prevent dual ownership of any promoted surface.
LOG575 inspects28 existing identities: only source-alpha/additive and ordinary
source-alpha blending occur; broad world candidate has partial alpha and cannot
be made opaque. Draw ordinal changes over the interval. Next carry actual
source-qualified translucent draw/texture plus explicit alpha blend contract
into a bounded separate material experiment using public draw-alpha support;
do not substitute refractive glass, infer identity from ordinal, or disable
native fragments before the corresponding new material is verified. Retained
census avoids another generic capture phase; source geometry linkage is next.
LOG576 implements a separate raw alpha-material preview and source/texture/wire
linkage; live bo delivers6 meshes and visibly relights the railings, both exits0.
Combined neural/native-effects use is fail-closed to prevent duplicate surfaces.
Next prove GPU source-alpha/texture-alpha/depth-write behavior and preserve source
draw identity into an explicit native-effect exclusion mask for those meshes
only. Then re-enable native particles/trails and combined evaluation with focused
changed-path validation. Raw preview is not full pipeline acceptance.
LOG577 executes GPU alpha/depth-write positives and negatives successfully and
tests a source-qualified exact-parameter exclusion planner (605 selftests).
Next wire source-owned packed native parameters and receipt-local replay
overrides, leaving the original native stack untouched. Combined guard remains
until actual GPU ownership/exclusion controls and integration pass.
LOG578 wires receipt-local GPU overrides and explicit alpha-combined lane;
both GPU APIs pass original/excluded/original-preservation controls. Live bp
returns relit alpha surfaces with remaining native weapon effects and neural
evaluation. Three independent HUD/backbuffer/completed-Present checks pass,
both processes exit0. Next focused selection-aware external-output provenance,
remaining build matrix and moving material review before commit/acceptance.
Do not repeat generic transport or claim this3-frame result closes full pipeline.
LOG579 adds selection sidecar fail-closed replay and608 passing selftests.
Archive bq has30 verified sidecars with a3-frame gap; retain exact matched
subsets, longest contiguous19. Next run marked/clean/OFF against bq, requiring
matching scene/guidance/native stack AND alpha exclusions, then finish builds
and commit. Do not redo archive for a prettier frame count or claim cadence.
LOG580 marked br times out; retry bs with existing longer helper bound exits0/0
and saves28 marked frames. Next clean restored bt, disabled bu, then exact-input
selection-aware audit using bs/bt/bu against bq. Do not repeat successful bs.

The following sequence records historical bounds/results, not competing current
assignments. Former action: compose and present delayed returned scenes using their owned
original HUD/overlay/native surfaces and explicitly distinct source/current IDs.
LOG531 working-tree live b now reaches22 successful raw-Remix Presents and five
held-native Presents; eight-frame stale timeout latches fallback without later
reentry. All four incremental builds and544/544 enabled tests pass. Next capture
the actual Flycast composite for source/HUD pixel validation, then connect the
returned image to neural evaluation. Raw preview is not combined DLSS5 proof.
LOG531 pixel c completes the bounded display check: three actual backbuffers
match receipt-owned native HUD and returned world pixels exactly, each joined
to successful Present; source/current pairs1861/1863,1862/1864,1864/1865. Visual
review shows fighters/temple/HUD with unresolved approximate scene omissions.
Next: finish focused capture/fallback regression and commit this display slice,
then feed returned-scene color/depth through neural evaluation with original
overlay ownership and separate accepted source/current identities. Do not redo
generic transport or claim camera/relighting acceptance from the preview.
Pixel-check bounds: a separate explicit developer output directory permits at
most three synchronous preview captures per renderer instance. Preserve raw
returned color, original native/HUD mask, composite and actual pre-OSD backbuffer
with source/current/receipt IDs. Require exact640x480, do not overwrite an
existing frame directory, and label the entire run ineligible for performance.

Next integration bounds (base fc415a22f): explicit
`FLYCAST_REMAKE_ASYNC_NEURAL=1` reserves neural evaluation for fresh returned
scenes; do not alternate native and returned inputs in one temporal history.
Validate original overlay/producer/receipt and eight-frame age, clear native
draw correspondence, use zero jitter/motion/confidence and reset/full bias.
Own an accepted evaluated-color snapshot plus its original overlay. The working
display hookup now composites that snapshot, records remake-evaluated separately
and optionally captures evaluated color plus actual backbuffer. First test public
DLAA on the unchanged OFF stage, then the supplied external route with focused
provenance controls; public success is not external proof. No external configuration changes. Falsify
duplicate evaluation, native-history advancement and current-HUD substitution.
LOG532 public eval-d now accepts25 unique returned evaluations and completes19
evaluated Presents. Three actual backbuffers exactly match evaluated world and
original HUD, with independent raw-substitution negatives. All four incremental
builds and545/545 enabled tests pass. Next: supplied external route plus focused
output/provenance controls on this changed display path; no public-as-external
claim. Working slice remains uncommitted while that integration is in progress.
LOG532 supplied-ON eval-e runs and captures evaluated output with protected HUD,
but external provenance is pending. Same-frame public/external raw Remix inputs
differ (runtime history), so their output comparison is rejected. Next reuse
source-qualified locked returned-input replay for exact ON/clean/OFF controls;
the evaluated display now notifies the existing original-ID sentinel verifier.
Do not treat active tuple/Feature18 log plus a screenshot as proof.
Replay implementation bounds: only explicit preview capture retains a shared
owned source packet with the existing two-pending/accepted/evaluated overlay
slots (packet wire limit72MiB). At most three new capture directories archive
source packet, exact returned BGRA/depth and checked receipt/input hashes.
ASYNC_LOCKED_INPUT_ROOT reuses the existing producer/scene/hash verifier before
substituting pixels, logs retained replay distinctly, and keeps current receipt
ownership only after matching the whole scene. This is developer-only replay,
not fresh consumer delivery or performance evidence. Wrong receipt, changed
scene/input and overwrite must fail. No external config changes.
LOG532 archive-f produces three source/input archives verified by the existing
locked reader. Next use that root for marked/restored/OFF comparisons via the
new strictly gated --remake-evidence launcher option; do not recapture another
unmatched ordinary-run comparison. Archive-f itself has no sentinel proof.
LOG532 g/h/i now confirms ONE original1867 retained-input external result reaches
the changed evaluated display path under exact ON/clean/OFF inputs, sentinel,
PNG/backbuffer and HUD checks. Earlier original1864/1866 outputs equal OFF and
remain explicitly unconfirmed (startup interval). Next widen the bounded archive
to obtain a post-startup moving sequence; do not call one frame the working
pipeline. External configuration and previously completed gates stay unchanged.
Next bounds: keep three captures by default; explicit developer-only
FLYCAST_REMAKE_PREVIEW_CAPTURE_FRAMES allows1..30, with invalid input disabling
capture. Request12 for the next live archive and matched controls, retaining
startup records rather than counting them as external. This changes only the
bounded diagnostic window, not eight-frame fallback or ordinary rendering.
LOG532 archive-j captured and verified all12 requested source/input archives
(1861,1862,1864..1873). Use that root for the next matched marked/restored/OFF
controls. Sixty moving source pairs were checked,56 retained; do not equate
transport coverage with sustained combined presentation. The full300/600-frame
working-pipeline criteria below are still open and must not be reduced.
LOG532 marked-k has completed:12 evaluated retained inputs, ten distinct
displayed-source captures, successful marker readback on the later frames.
Restored-l and unchanged hook-disabled-m now complete the focused comparison:
ten consecutive original frames1864..1873 have exact-input external alteration,
marked successful Presents and clean backbuffer/original-HUD pixel equality.
The two unchanged startup frames are excluded. Commit the independently proven
integration slice, then remove sustained-delivery bottlenecks and add OIT support.
Next bounded implementation (cae204ca9): add an explicit async helper return-only
mode that retains paired color/depth readback and receipt publication but writes
no BMP/depth artifacts. The current helper writes about6MiB per returned source
and performs480 depth file writes before publishing. Keep the existing capture
mode unchanged, source-age8 and bounded run unchanged. Falsify no-file behavior
and require actual live paired returns/evaluations before claiming the new mode
works. CPU/GPU readback remains a known cost, not zero-copy performance proof.
Return-o now proves60 disk-free paired returns,56 unique accepted evaluations,
but only16 unique displayed sources (23 Presents). Source1877 to1896 publication
gap expires the age8 display latch. Next isolate why publication pauses across
that interval (scene/texture readiness or channel credit), preserving fallback
and without another generic external-output proof phase.
Add opt-in ASYNC_DIAGNOSTICS skip reasons for credit, snapshot, scene, texture,
packet, guidance, overlay and publication. Run the same bounded live interval
once to identify the missing-stage cause; no acceptance or timing-policy change.
Gap-p identifies packet clip-unsupported for every frame1878..1895. Next implement
bounded near/far triangle clipping with interpolated UV/color before the existing
adapter validator, preserving current enclosure and native source geometry.
Test unchanged inside, crossing near/far and wholly outside controls, then require
live publication through the previously rejected interval. Do not weaken the
validator or expand the supplied clips to manufacture acceptance.
Clip-q now publishes and displays all18 formerly rejected sources1878..1895,
with zero clipping rejection and54 distinct displayed evaluated sources overall.
All four builds/556 enabled tests pass. Commit the clipping/skip-diagnostic slice,
then extend bounded no-file runtime coverage beyond60 sources to locate remaining
sustained-delivery limits. Keep full300/600-frame acceptance and OIT work open.
Next bounded test: permit at most660 helper Presents (60 warmup plus600 source
frames) only for async return-only mode, with120-second total runtime watchdog
after initial source arrival. Existing capture modes retain120/30-second bounds.
Run without image capture or locked replay; inspect gaps/expiry rather than
claiming this source-observation-heavy run satisfies final performance gates.
Long-r completes600 paired sources,589 accepted evaluations and587 distinct
displayed sources (605 Presents), max age5 and longest consecutive run222.
Next inspect/fix return mailbox contention: two outstanding source credits share
one image slot, and nine returns were busy-dropped. Preserve bounded ownership,
receipt matching and monotonic delivery; then advance OIT rather than retuning
visual settings. Full acceptance remains open.
Next bounded implementation: version the shared layout to4 and use two paired
image slots indexed by source sequence, matching the existing two source credits.
Receive oldest ready first; retain receipt, depth, stale, duplicate and close
checks. Both Flycast and helper must be rebuilt/staged together. Require FIFO
and independent-slot tests plus the same600-source live run; no capacity-only
claim of final performance or unbounded queue growth.
Slots-s publishes all600 paired returns with zero busy drops;597 unique accepted
evaluations and595 distinct displayed sources include543 consecutive source IDs.
All four builds and enabled556/556 tests pass. Complete host-close/log recording
and commit this slice, then enable/test the changed path in OIT with original
overlay ownership. Do not spend another phase proving unchanged normal transport.
OIT bounded implementation: explicit ASYNC_OIT=1 permits the inherited returned
scene path only after OIT's drawStrips resolve and renderNeuralExports. Preserve
the actual resolved fbTex and original-frame R8 overlay mask; public/native
paths and RTT/direct-framebuffer bypass remain unchanged. First require three
actual OIT composite/backbuffer pixel checks with original HUD, then sustained
delivery. This does not implement missing translucent world geometry or claim
full OIT acceptance merely because the opt-in runs.
Oit-t is CORRECTIONS_REQUIRED despite three mask-based composite equality checks:
visual review finds holes in the HUD mask and missing arena floor, absent in
original native. Inspect OIT replay geometry/state and mask generation; do not
commit or start long OIT testing until actual HUD coverage is corrected. Preserve
this falsifying capture rather than reporting mask equality as overlay success.
Normal-u exact scene replay confirms OIT-specific mask holes, separate from
shared returned-scene floor limitations. Next add MAX accumulation only for
reactive coverage/mask targets so unclassified translucent draws cannot erase
earlier protection; require overwrite negative and failed-frame comparison.
Do not conflate continuous health bars with complete HUD/title acceptance.
Coverage-v restores906 formerly erased pixels on exact native/returned inputs,
with no lost coverage and exact composite/backbuffer equality. Health bars now
pass visual review; names remain incompletely classified. Focused blend GPU
regression passes on D3D11 and D3D11On12. Prioritize shared floor/scene export
before additional name-classification work: test estimated vertices crossing
the supplied clip planes through conversion and downstream clipping. Preserve
the failed floor captures; a synthetic repair is not real-game floor acceptance.
LOG539 fresh OIT output now visibly restores wooden floor beneath the fighters;
three actual composites/backbuffers pass, with completed evaluated presentation.
All four builds and559 enabled tests pass. LOG540 now verifies source1860 with
byte-identical native input/mask and repaired floor reaching completed Present.
The export fix is committed and fork-verified at6a7588b20. Next bounded OIT
integration repair: captured name-atlas depths span .138055..203861, while the
existing common HUD cutoff starts at .15. Extend only the known name atlas's
lower bound to .138, retaining list/order/alignment/region/span checks. Require
failing-old/passing-new tests and a fresh composite; no generic HUD-band rule.
LOG541 hud-aa now verifies both names restored in three actual backbuffers with
completed evaluated Presents and independent composition equality. All four
builds/565 enabled tests pass. Next sustained OIT delivery with synchronous
captures disabled, then focused combined-route provenance; do not restart
generic transport or expand HUD diagnostics without a specific new failure.
LOG542 sustained OIT ab completes600 matching paired returns, zero busy drops,
597 accepted evaluations and595 distinct displayed sources (544 consecutive,
max age5), clean host/helper exits, no synchronous preview capture. Next use
the repaired OIT retained inputs for focused external-output ON/clean/OFF
proof before committing the combined OIT integration slice. The40-object
helper cleanup warning, full camera/scene and final timing acceptance stay open.
LOG544 closes focused repaired-OIT external-output proof for25 consecutive
originals1856..1880: matched inputs, ON/OFF output differences, marked/clean
hash identity and actual clean original-HUD/backbuffer/completed-Present joins.
OFF correctly stays native when external readiness fails. Commit the verified
OIT/coverage/name integration slice, preserving ac's partial watchdog failure;
then advance300-frame moving evidence and resource/timing deficiencies rather
than rerunning this provenance comparison.
Next bounded moving-evidence implementation: exact developer opt-in
FLYCAST_REMAKE_MOVING_CAPTURE=1 permits up to360 captures; ordinary cap30 and
default3 remain unchanged. Keep two outstanding sources, source-age8, no
overwrite and explicit synchronous/performance-ineligible metadata. First
request330 fresh OIT captures with the existing660-Present/120-second helper
bound; measure actual source continuity rather than declaring300 from count.
Retain source packets for matched comparison lanes; this is not timing evidence.
LOG547 moving-af retains330 captures but only286 consecutive sources, helper
watchdog expiry, and a real HUD disappearance at original2192/current2194.
Fix this longer-window overlay failure before repeating the long capture;
do not extend the earlier three-frame HUD acceptance to the whole sequence.
LOG548 raises the captured-atlas upper depth bound to .24 after all five HUD
atlases reached .230914. Old-code positives fail; corrected automation571 tests
pass including world/unknown-atlas/.25 negatives. Next recapture original2192
with retained af inputs, then complete remaining builds and long-run evidence.
LOG550 ah verifies exact original2192 inputs and restores15233 protected pixels
through completed Present. Names/timer/fills return, but depleted health-bar
outline remains visually incomplete; complete HUD coverage stays open. Long
capture mode now suppresses redundant per-draw logs (pending rebuild) while
retaining all pixel/identity evidence. Do not repeat generic provenance.
LOG551 plate-ai restores the depleted plate on exact source2192, adding3182
protected pixels without losing coverage, with matched backbuffer/Present.
All four builds and574 enabled tests pass. Commit the captured depth/plate
repair independently, then resume long moving evidence with reduced diagnostic
logging. Additive HUD effects and world translucency remain explicit omissions.
LOG552 aj improves moving capture to349 images/299 consecutive sources; all
pixel/Present checks pass and no HUD mask is empty, but helper total watchdog
expires. This is not300. Capture throughput is parked under LOG553;
do not round the threshold or stitch an extra frame from another run.
Complete
scene/camera, name coverage and sustained OIT acceptance remain open.
Do not label this bounded repair full OIT acceptance.
Do not divert into aesthetic trail tuning or general profiling before sustained
delivery and OIT integration; preserve intentional source effects.

Implementation bounds: developer-only `FLYCAST_REMAKE_ASYNC_PRESENT=1`, normal
DX11 at 640x480, capture mode disabled. First align source time with a bounded
native hold, then display monotonically advancing returned frames with their
original HUD. Eight-frame expiration latches fallback until reset; raw Remix
must not count as displayed DLSS 5. Falsify backward/future IDs, stale returns,
and timeout reentry; build before actual moving presentation validation.
LOG530 retains receipt-keyed original surfaces; actual30-source On12 run accepts
27 paired replies with matching original overlays at age2. No display override
exists yet. LOG529 ordinary-frame feed
matches60 sender/receiver scenes,60 image pairs,59 published replies and56
retained pairs at2..3 frames old. One return is explicitly busy-dropped and three
published replies are not retained. This is not returned presentation or combined
ordinary gameplay acceptance. LOG526 marked/clean/hook-disabled
comparison confirms all three exact-input external outputs with active tuple
off/1/1/203/0/0/enabled. Marked and clean inputs/composition pass all three.
This is source-qualified retained-input replay, not fresh continuous output,
temporal quality or performance acceptance. No additional general provenance
phase; preserve these controls for a focused changed-route regression.
The next implementation slice must retain a bounded source-frame ownership
ledger (returned color/depth plus matching overlay/native fallback), poll without
waiting for the consumer, and reject expired or different-epoch replies. Do not
stamp delayed images with the current frame or pair them with current-frame HUD
or native motion. Keep the reset-only/full-bias limitation explicit until real
returned-scene temporal guidance is implemented. Measure delivery/skips/latency
on moving gameplay; do not call synchronous capture performance evidence.
LOG530 working slice: GPU-copy native color and R8 overlay mask before each
publication; attach the issued receipt only on success. Keep two pending source
snapshots and one accepted snapshot. A returned pair requires exact receipt,
frame/producer and age match before its original overlays move into accepted
ownership. No display override yet. Test source mutation after snapshot, wrong
receipt/epoch/age and actual ordinary On12 handoff; do not use a current HUD as
the fallback for a missing older snapshot. Return-credit preflight avoids new
GPU copies while the bounded ledger/transport is busy.
LOG527 implements explicit return-credit publication and source-age/epoch
expiration in the existing channel, with516/516 automation tests after a
512pass/4fail RED control. A consumed source slot is
not retired until its returned image is received or its source is explicitly
expired; a full return ledger skips without waiting. Falsifying case: consume
two source packets without returning their images, then attempt a third publish.
It now does not overwrite the first source receipt. Legacy one-way transport is preserved.
This is an asynchronous ownership prerequisite, not ordinary-feed acceptance;
next replace synchronous material Map with retained, generation-qualified
staging copies and nonblocking completion before wiring the ordinary frame hook.
LOG528 material slice implements an owned D3D11 readback ticket with exact
texture resource/generation/context identity, fixed byte bounds, retained staging
and event query. Begin queues only; Poll uses DONOTFLUSH/DO_NOT_WAIT and returns
pending instead of waiting. Reject generation/resource/context changes without
publishing partial pixels. Compare all supported formats/mips to the existing
texture fixture, with busy/budget/reset/mismatch paths. Bounded cache now exists;
WARP fixture reports28 exact mip/DDS comparisons and92 async controls. Deliberate
generation-invalidation bypass fails; restored implementation passes. LOG529
wires the opt-in ordinary-frame feed using this cache and return-credit channel,
prewarms required materials before publishing a complete current scene and
handles skipped frames explicitly in the helper without false consecutive
history. The strict existing consecutive diagnostic path remains unchanged.
Do not add another isolated texture fixture phase instead of this hookup.
LOG529 working bounds: explicit FLYCAST_REMAKE_ASYNC_CHANNEL ordinary DX11
hook streams complete current scenes using the cache, without capture gating or
GPU/readback waits. Unsupported/2D/RTT/direct-FB stay on existing presentation.
Poll paired returns under original identity; expire at8 renderer frames, retain
at most one valid pair, but do not present it without matching HUD ownership.
New helper --live-channel-async accepts forward source gaps explicitly while
old --live-channel remains strict. Gap handling resets our uploader resources;
the supplied public camera interface exposes no proven runtime history reset,
so do not claim a Remix temporal reset or temporal quality. Test actual ordinary
Soulcalibur replay with capture disabled and compare sender/receiver receipts.
LOG529 async-c does so over60 moving sources; current live card now advances to
original-frame HUD/overlay ownership and delayed result presentation. Preserve
separate renderer/current versus source/returned IDs in stage, cadence and
capture metadata. Do not stamp an older image as current or reuse native PVR
motion/depth for the changed Remix scene. Start with explicit reset/full-bias
guidance and label temporal limitations, then implement returned-scene guidance.
The installed runtime is available; the former unavailable-runtime stop is
obsolete. LOG512 removes draw-time texture-file dependence: memory/file raster
outputs match exactly on three moving frames and actual Remix output is viewed.
LOG513 now converts that witnessed subset inside running Flycast and packages
its current textures using the same shared scene contract as the uploader.
Actual20/21/19-draw endpoints render through a bounded saved-packet handoff;
the first fixed-light output was black, and camera-forward diagnostic lighting
reveals the partial temple. Fighter/full-arena coverage is NOT present.
LOG514 now delivers packets directly from running Flycast to running Remix using
two bounded shared-memory slots, with no saved-scene reads in the consumer.
Three source frames have matching sender/receiver receipts and rendered outputs.
This still uses the synchronous developer capture boundary, not real-time pacing.
LOG515 return-image transport verified: a version2 reverse memory slot carries640x480
BGRA final color with the exact source receipt/frame/producer, corruption check,
bounded ownership and stale/duplicate rejection. Flycast polls and retains owned
pixels at the developer capture boundary; no presentation or history acceptance.
Three return-d frames have valid source receipts and byte-identical returned
pixels against consumer output. Invalid source/size, full slot, duplicate and
orderly-close controls pass. The source hook currently runs after composition;
LOG516 verified bounded implementation: reuse the existing DX11 overlay shader in an
isolated deferred command list on the returned640x480 texture. Explicit
FLYCAST_REMAKE_COMPOSITE_TEST=1 enables diagnostic capture only. Require same
source/guidance frame and producer. Three composite-b frames have zero protected
and unprotected mismatches and same-frame native-target preservation. Actual
mask coverage1004/1659/1004 pixels is HUD outlines, not full HUD acceptance.
LOG517 captured-atlas classification is now verified on the three moving frames:
coverage21771/22451/21765 and zero composite mismatches/native-target changes.
LOG518 blend-scoped zero-alpha discard now removes969/1071/1052 unnecessary
mask pixels with exact remaining composite pixels and unchanged native targets.
Timer/header rectangles remain; full layer-separated HUD acceptance is pending.
Do not widen punch-through/additive discard without evidence or stall every
integration step on the remaining rectangles. Preserve captured-atlas bounds
and generic/unknown-title behavior; wrong texture, region,
depth, RTT and title controls already pass. Capture moving comparisons and
report alpha-boundary/background preservation, not just masked-pixel equality.
LOG519 moves exchange before composition on the bounded native capture/channel
route. Three actual receipts are ready before composite, with exact protected
composition and native-target preservation. Archival does not own delivery or
resubmit the packet. Async pacing and returned Present remain unproven.
LOG520 expands visible opaque coverage from19-21 to40 meshes: both fighters
and foreground arena now appear in the returned image. The bounded opt-in
FLYCAST_REMAKE_ESTIMATE_UNTRACED=1 alternative keeps observed
vertices, derive missing camera-relative positions from current PVR projected
XY/reciprocal depth using the already measured lens/scale, require current
observed anchors, and explicitly label all estimated positions/provenance.
This is an approximation experiment, not recovered world/camera acceptance.
Default observed conversion, geometry/texture bounds and omissions stay intact.
Three moving endpoints pass exact return/composition/native-target checks;
physical/world, missing translucent effects and longer temporal coverage remain
unproven. Next integrate returned scene color at the neural submission and
presentation boundary. LOG521 captures public typed Remix depth alongside color
on all three frames. Linear-view-distance interpretation fails; supplied normal
projection-depth interpretation is strongly supported but full guidance remains
unproven. Next transport matched returned color/depth together, explicitly
validate projection conversion/polarity and align motion/masks before neural
submission. Preserve clear/omitted-surface uncertainty and native fallback;
Current bounded implementation: channel version3 optional matched projection
depth, same source receipt and clips, fixed size/range/digest checks, atomic
publication only after both public color/depth readbacks succeed. Archive and
compare received depth bytes; truncated/NaN/range/wrong-clip cases must reject
without publishing color alone. Color-only diagnostics remain separately valid.
LOG522 now verifies those rejection controls and exact received color/depth on
three live combat frames. Offscreen composition remains exact and native targets
unchanged. Finish affected regression, then implement projection conversion and
the actual pre-evaluation returned-input selection; do not repeat paired transport.
Current bounded implementation: move capture exchange before TrySubmit and add
FLYCAST_REMAKE_INPUT_TEST=1 for reset-only same-resolution640x480 capture.
Convert returned BGRA to RGBA and normal projection depth to1-d, not PVR log.
Zero motion/confidence/identity and full current-color bias explicitly avoid
claiming native guidance for changed surfaces. Require exact producer/frame,
paired depth, zero jitter and supported normal renderer; reject to native.
No temporal-quality acceptance: motion/coverage alignment and async feed remain
required. Verify captured GPU inputs, accepted evaluation and output separately.
LOG523 verifies exact returned GPU inputs, accepted publicDLAA evaluation and
final protected/world composition on three frames each of nativeD3D11 and
D3D11On12. This is reset-only, not temporal-quality acceptance. Next run the
supplied external consumer through this changed route with focused provenance;
then replace reset-only guidance with verified returned-surface correspondence
and move the feed off synchronous capture. Do not repeat public connection proof.
First supplied-consumer attempt nr-on-a failed to deliver Remix packets: capture
skip counts eligible captures, whereas marker IDs include menu-bypassed renderer
frames. Native-input markers are not combined proof. Add explicit bounded
--start-frame renderer-ID capture scheduling, preserving default skip semantics,
then rerun1782-1784. No bypass disabling or exact-input relaxation.
LOG524 rerun delivered packets but fails combined acceptance: first two replies
late, third wholly empty, helper shutdown watchdog124. Also renderer1782 maps
to game producer1829 versus baseline1781. Next align actual producer identity
and isolate returned-output/dual-runtime failure. Reject empty paired output;
do not treat the existing native-input markers as combined provenance.
Immediate discriminator: render the retained failing packet alone and with the
external host active, preserving identical scene bytes and no config edits.
The swapchain warning also exists in a successful run; do not chase it alone.
Retained-packet comparison is explicitly diagnostic, not live-source acceptance.
Retained nr-on-b scene renders nonempty alone and with a running external host;
the first concurrency attempt missed overlap and is retained as invalid.
The successful overlapping helper used SW_SHOW, unlike live SW_SHOWNOACTIVATE,
so foreground activation may have paused the host. Next use explicit
FLYCAST_REMAKE_NOACTIVATE_TEST=1 with the identical retained packets to match
the live window policy before attributing failure to GPU submission timing.
No-activation retained replay also succeeds while the external host remains
live. Next focused experiment flushes pending producer GPU work after packet
construction and before the developer wait. It is not a completion fence or
performance proof; retain failure if the cold live handoff still returns empty.
LOG525 flush-a now returns all three nonempty pairs, applies them to external
evaluation and passes exact input/HUD/world composition plus1024/1024 markers;
both processes exit0. Full combined provenance remains unconfirmed. Next lock
actual returned input images for clean/marked/policy-off comparison while
requiring exact game-producer provenance. Packet-only replay changes color
hashes and cannot satisfy this gate. Then real temporal guidance/async feed.
Current implementation: explicit FLYCAST_REMAKE_LOCKED_INPUT_ROOT diagnostic
reuses retained returned pixels only after exact producer epoch/ordinal/cycle,
full serialized scene equality (excluding renderer counter/build label), source
receipt and input-pixel hash checks. Label replay, never fresh live output;
missing/malformed/ambiguous/different inputs fall back. Add --start-producer to
schedule matching game ordinals across startup-dependent renderer counters.
Verify loader negative controls and actual ON/clean/OFF captures without changing
the supplied consumer configurations or relaxing existing confirmation checks.
do not repeat old Gate10 transport or native polarity fixtures.
Continue integration at the
presentation boundary, with distinct experimental provenance and fail-closed
native fallback. Do not label it public DLAA or complete recovered geometry.
Keep protected overlays late and require changed-route Gate10 before combined
DLSS5 acceptance. Retain native fallback and explicit diagnostic labels;
move the supported feed off capture into an asynchronous/budgeted ordinary-frame
path. Do not replace the native frame with the incomplete temple or make another
offline viewer/count-only tracing phase. Expand missing fighter/arena correspondence alongside
that integration, never substitute the partial temple for the full objective.
Opt-in resource refresh now handles changed draw counts/topology/generations;
it does not establish temporal identity or asynchronous retirement safety.
Native fallback remains authoritative
until complete-scene returned presentation and overlay composition are proven.

Completed scoped checks for the material slice: malformed/truncated texture,
ambiguous file-plus-memory source, aggregate byte overflow, producer mutation,
and file-versus-memory raster equality. Next source-provider checks must reject
missing correspondence, stale producer identity and unsupported camera domains
(CPU controls now exist); continuous-delivery and returned-image controls remain;
runtime success alone is not image evidence. No new camera tracing phase,
performance claim, external configuration edit or full-goal acceptance.

Historical progression below retains superseded next actions as evidence only:

Earlier next action: apply the bounded retained-mesh transport investigation to
actual source topology, preserving position and normal correctness without
claiming a recovered game skeleton. LOG440 confirms40 matching topology slots
but15 draws change color/UV across L/M. Resolve supported dynamic-attribute
transport before silently freezing source colors or texture coordinates.
LOG443 establishes a CPU-only per-triangle affine candidate (8337 triangles,
63 bounded bone batches, normals carried); next prove nonrigid GPU skinning
and supported dynamic-material transport. No game skeleton or temporal pass.
LOG444-446 render retained/reference affine synthetic GPU images. Unsaturated
correct-normal reference is closer than wrong-normal control (interior MAE0.54
versus3.21), scoped support only, not byte equality or real-fighter acceptance.
LOG447 proves synthetic retained-mesh constant material destroy/recreate visibly
updates; duplicate registration correctly leaves old color. LOG448-449 close
scoped failure-ownership/mock regression (SDK157,Python316,3x331 selftests,four
builds). Next dynamic texture/attribute representation plus actual retained
fighter geometry. LOG450 proves a first texture-backed replacement on retained
synthetic geometry versus duplicate-register control; SDK159 plus chart test
pass. Full regression remains pending. Next within-triangle source-attribute
representation and bounded texture lifetime; constant tint is insufficient.
LOG452 falsifies default texture/vertex gradient equivalence: vertex reference
renders gray. LOG453 restores gradient via explicit public BlendEXT; texture
comparison PSNR55.23 vs23.12 without blend. Next actual scene color policy and
bounded dynamic-attribute representation; retain no-blend negative control.
LOG455 exercises explicit source color on actual40-mesh H/L/M sequence with
three readbacks; moving-fighter trails remain. Next retain real triangle geometry
while representing changing source attributes; color contract is no longer
blocked on proving visible vertex modulation. Do not repeat static color proof.
LOG457 runs actual retained triangle H/L/M with explicit frozen-attribute
ablation; floor/leg trails visibly reduced, upper-body artifacts remain. Next
expanded topology/normal/failure controls, quantified trail comparison and
source-attribute-preserving update. Frozen attributes are not the final route.
LOG458 matched rebuilt/settled controls support a modest error reduction in
floor/fighter regions, not general temporal acceptance. SDK169 includes bounded
bones/topology/duplicate-frame/failure checks. Next changing source attributes
on retained geometry; stop extending frozen-only visual experiments.
LOG460 supplies tested mip-zero source-attribute sampling;4490/8337 actual
triangle centroids change L/M. Next bounded per-triangle representation with
measured interpolation/filtering error, then moving material updates. Mip-zero
sampling does not prove full PVR shading or original minification fidelity.
LOG461 shows fixed tiles trade detail loss against46MB/generation for only
three fighter draws. Prioritize a bounded D3D9 dynamic-buffer compatibility
experiment for direct vertex attributes before implementing per-frame DDS
baking. This is isolated runtime investigation, not a new production backend.
LOG463 dynamic D3D9 fixture draws/readbacks succeed but images are black,
including explicit-camera rerun. Next diagnose legacy capture/injection ownership
and separate raster output from Remix output using public interfaces; do not
call HRESULT success a graphics pass or repeat the same black-image run unchanged.
LOG464 resolves black output: API factory disables legacy draw conversion;
standard Direct3DCreate9Ex factory enables it. Actual dynamic fixture now appears
in Remix final output. Next changing-attribute controls and bounded game-packet
submission through that isolated legacy path; no production migration yet.
LOG465 frozen-color control stays gray versus dynamic colors with identical
deformation. Proceed to bounded actual packet upload/camera/texture conversion;
do not repeat the factory or synthetic color proof. Exact fidelity remains open.
LOG467 now renders actual40-mesh H/L/M through standard D3D9 dynamic buffers,
updating positions/normals/colors/UV and sampling state. All63 Presents and
three Remix-output readbacks succeed; first/last images visibly contain both
fighters and temple with changing pose. Next quantify moving fidelity against
matched controls, add uploader rejection/lifetime tests and complete four-build
regression. No live Flycast integration, temporal pass or combined DLSS5 claim.
LOG468 completes four incremental builds,SDK179,Python331 and3x341 selftests.
Resource/sampler rejection controls are CPU-tested, not COM failure injection.
The validated63-frame rerun also exits0 with three readbacks; middle image
viewed with both fighters/temple visible. Next matched temporal/attribute
controls and resource-lifetime handling; keep the existing40-object warning
explicit. No need to repeat unchanged build/factory proofs before those tests.
LOG470 settled-M and frozen-attribute controls complete but output variability
confounds attribution: unchanged runs differ almost as much as frozen/current.
Next deterministic scene backbuffer attribute comparison (explicitly not Remix
output proof), followed by session/repeat-controlled raytraced evidence. Do not
promote whole-image difference alone as dynamic-attribute correctness.
LOG471 raster current/repeat is byte-exact across three frames; frozen control
changes7400 pixels at1784 but none at1783. Next verify explicit source-frame
alignment at the raster readback boundary (pre-Present or analytic marker)
before treating post-Present backbuffer labels as current-frame truth. This
is diagnostic capture correctness, not a reason to reprove all old gates.
Apply D-143 to all temporal judgments: preserve native authored effects and
compare matched moving native evidence before labeling residuals as unwanted
ghosting. Settled-reference differences alone do not establish such defects.
LOG472 resolves raster label mismatch: old post-Present1784 exactly equals
pre-Present1783. Raster-only capture now precedes Present. Current/repeat exact;
frozen attrs change7400/9133 pixels at1783/1784. Next complete changed-capture
regression and checkpoint; then raytraced attribute/frame contract and actual
integration. Do not extrapolate raster lag to public Remix-output readback.
LOG473 completes incremental checkpoint regression. Next verify raytraced
attribute/frame correspondence with controlled evidence and then connect the
supported live scene route; preserve D-143 and do not repeat raster-only proof.
LOG474 red/green vertex markers visibly reach the corresponding raytraced
frames, establishing bounded current attribute response but not exact color
semantics or temporal fidelity. Next quantify response/interpretation and
advance the supported live scene connection; markers are diagnostic only.
LOG475 live-source audit makes the next dependency explicit: current40-mesh
input is offline composition of transform-tape/ledger evidence for1782-1784.
Do not feed those recorded packets as purported live gameplay. Next build a
bounded in-memory PVR snapshot seam retaining frame/game/pass/texture-generation
identity, then connect witnessed live transform/camera lineage for the supported
title/domain. Unknown camera or missing lineage must remain native fallback.
This is a dependency of M2-scene, not permission to claim recovered world space
or replace the requested live pipeline with an offline replay viewer.
LOG476 adds shared-validation owned SnapshotPvrScenePacket, with automation
selftest345/345 including mutation/atomic-rejection controls. Next expand
texture-generation/pass/bound tests, hook the snapshot behind an explicit
developer option at the owned live renderer seam, then connect supported
transform/camera lineage. No raw texture pointers may cross the ownership seam.
LOG477 adds bound/nonfinite/pass/frame/sorted-order tests; automation350/350.
Next dedicated texture-generation fixture and bounded live capture hook; no
texture-lifetime or live-hook pass is implied by existing no-texture fixtures.
LOG478 wires snapshot publication into existing opt-in bounded capture only,
with frame-qualified render-thread accessor and invalidation. Automation build
and350 selftests pass; actual game hook/accessor lifecycle and texture-generation
fixtures remain next, followed by broader regression and live camera lineage.
LOG479 fixes publication before final completion-file failure and adds unavailable/
failed-capture accessor checks; automation352/352. Actual successful GPU capture,
forced final-write failure, and texture-cache generation ownership remain unrun.
LOG480 actual3-frame game capture completes but carries stale configure-time
version stamp. Reconfigure/rebuild then repeat with correct source provenance
before checkpoint acceptance; do not relabel old capture metadata. Working-tree
changes must remain explicitly distinguished from the base commit.
LOG481 restamped working-tree native replay capture succeeds for1782-1784,
correct base8c5cd655e, clean close and real textured packets. Snapshot capture
path exercised, but direct accessor/generation-lifetime tests remain pending.
Native image retains bright weapon effect/HUD/shadows; preserve D-143.
LOG482 actual post-capture accessor and texture metadata match live cache on
all3 frames, wrong-frame requests rejected, log retained. Next falsifying
generation/pointer controls and full snapshot checkpoint; positive logging is
not yet an automated fail-closed gate or asynchronous lifetime validation.
LOG483 actual3-frame upload/RTT corruption controls rejected; exact3-row log
assertion passes with positive bindings/frame checks. Next complete snapshot
checkpoint regression and live transform/camera provider; palette-specific and
async resource retirement remain explicitly unproven, not reasons to repeat
the unchanged upload/RTT controls.
LOG484 completes snapshot checkpoint: four incremental builds,Python331,
SDK179,3x352 selftests. Advance witnessed live transform/camera provider;
snapshot ownership scope does not satisfy recovered geometry or M4 presentation.
LOG485 locates prior live observations in retained temporary x64/SQ/TA patches,
not tracked runtime code. Their1781..1783 gates cannot serve live gameplay.
Next extract bounded per-frame producer/source-copy handoff from those proven
points, with reset/overflow/incomplete-generation rejection; do not wholesale
apply the historical tracing patch or invent camera identity from PVR depth.
LOG486 starts bounded SourceObservationBatch transport with frame/epoch lookup
and failure controls. Not a live provider yet. Next wire actual TA-context copy
observations with child/context identity and reset lifetime; no fixed frame lists.
LOG487 corrects queue timing: collect by context generation, attach actual
producer identity only on queue publication, reject future-cycle records.
Continue context-local observer hookup; do not guess next frame stamps.
LOG488 attaches optional observation ownership to each TA_context and resets it
on reuse. No records emitted yet. Next x64/SQ observer must supply actual writer/
generation; ta_thd_data32_i alone lacks that upstream provenance.
LOG489 adds opt-in x64 non-MMU SQ invocation scope (actual PC/address, serial
not RAM generation). Next scope reentrancy/overflow and actual native parity,
then TA-context join; no upstream transform or TA record authority yet.
LOG490 deeper nesting/exhaustion/unwind tests pass366; actual opt-in invocation
confirmed,3 captures complete. Native parity against prior off run is2/3 exact,
1783 differs: next matched same-build off/on/repeat and packet comparison.
Do not call full parity passed. The user subsequently prioritized live
integration; retain this discrepancy without blocking diagnostic TA attribution.
LOG491 same-binary off/on/repeat: all source packet values exact; off/repeat
pixels exact3/3, on differs8 pixels by1 at1783. Reproducible zero-tolerance
parity failure remains parked, not waived or blamed on run noise. Both follow-up
material captures complete3 frames/clean close. Next connect actual scoped SQ
copies to child-local TA records and decoded vertices, then the live transform/
camera provider and Remix return path. Further pixel-isolation work is deferred
unless this discrepancy prevents trustworthy integration evidence.
LOG493 connects live SQ copies to actual decoded polygon vertices. Corrected
replay records14806 copies and14764 exact-byte vertex joins per late gameplay
producer, with3 captures/clean close and367 selftests. This establishes the
submission/consumer seam, not upstream transforms. Next retain the join in the
owned snapshot and connect actual source RAM/transform observations; camera
semantics and combined presentation remain pending. Complete four-build
regression before committing this observer slice. LOG494 now retains14764 live
source joins in each of3 actual owned captures and passes370 selftests, including
source retirement/wrong-vertex controls. Next connect executed SQ stores to
their RAM-read/transform lineage through the existing x64 memory-operation seam;
PREF submission PC alone is not the upstream writer. Serial build regression
completes in LOG495: four builds,3x370 selftests,SDK179 andPython331 pass.
Do not repeat the now-proven snapshot join as a substitute for
advancing the transform provider.
LOG496 now observes executed SQ writes and retains all12 XYZ-byte writer PCs
for14764 vertices in each of3 gameplay captures; automation372 tests pass.
Next invalidate writer metadata on reset/interpreter fallback, then connect
the observed SQ source operands to RAM reads/transform lineage. No more
submission-only scaffolding: source dataflow is now the missing connection.
Full configuration regression is pending for this store-hook change.
LOG497 reset/interpreter invalidation passes374 tests and preserves full observed
XYZ-store coverage in3 live captures. Nine captured store PCs correspond to the
three historical read groups. Next implement executed read-to-store register
lineage, not a pc-minus6/value-only guess and not fixed capture-frame lists.
Serial remaining configuration builds are running; retain their terminal results.
LOG498 confirms that prior matrix passed and implements same-block direct
register read-to-store linkage. All14764 captured polygon vertices now carry
complete XYZ RAM-read addresses in3 actual frames; automation376 tests pass.
Next test compile-analysis overlapping/intervening-register-write rejection,
then connect the RAM-producing writes/transform operations. Do not confuse
observed reads of projected coordinates with recovered upstream transform
truth. Full regression remains pending for the new read-link hook.
LOG499 proves focused reaching-definition rejection controls and adds observed
RAM-producing writes. Three captures retain14692/14655/14724 complete producer
links; automation384 tests pass. Next connect the producer instructions to
executed transform input/result records and address missing DMA/HLE/lifetime
invalidation before treating these candidates as authoritative upstream truth.
Keep native fallback for missing domains. Full configuration regression pending.
LOG500 now records actual live FTRV input/matrix/output using original arithmetic
and adds bulk-memory invalidation. Three captures complete and384 selftests
pass. Next connect these executed transforms through producer arithmetic to RAM
writes, retaining matched records beyond the rolling ring. Latest-PC proximity
or equal floats are not correspondence proof. Do not repeat invocation counts
as a substitute for this dataflow connection; full regression remains pending.
LOG501 completes four-build regression,3x384 selftests,Python331 andSDK179.
Decoded packets remain exact3/3 against the pre-transform observer; native
pixels differ1/9/0 by max1, failed/parked. The source-observer checkpoint is
diagnostic only; next remains transform-to-RAM arithmetic correspondence and
matched-record ownership, not another downstream invocation-count experiment.
LOG502 adds actual direct FTRV-component-to-RAM-store correspondence, accepting
only exact reaching definition, executed serial and output bits. Three captures
show thousands of such stores per late frame; automation388 tests pass. Next
propagate owned matched transform data through RAM reads and the TA snapshot;
then cover the intervening projection arithmetic. Store counts are not vertex
coverage, and serials alone do not survive ring eviction. Full regression pending.
LOG503 completes owned direct-transform handoff and390 selftests, but actual
captured XYZ has ZERO directly linked components in all3 frames. Direct-only
correspondence is insufficient. Pivot now to executed projection/arithmetic
and intermediate RAM-copy dataflow; retain the zero result and stop repeating
direct-store count captures. No recovered geometry or camera acceptance.
LOG504 adds direct RAM-copy ownership forwarding and392 tests, but actual
captured XYZ still has zero transform components. Direct copy omission is not
the sufficient explanation. Next instrument/evaluate intervening scalar SHIL
arithmetic operands/results and explicit transform dependencies; do not rerun
the unchanged direct-only route or declare nearby/equal-value matches valid.
LOG505 records actual scalar operands/results in FTRV-containing blocks;
395 selftests pass and3 captures complete with20550637 observations,0 exact
result rejections. Next propagate explicit transform dependencies through these
verified operations and into RAM stores, rejecting overwritten/mixed origins.
The rolling operation ring alone is not geometry lineage or frame coverage.
LOG506 propagates same-block arithmetic origins with398 tests but actual XYZ
coverage remains ZERO. Initial whole-tag-clear run timed out; epoch version
completes3 captures. Next locate cross-block/unsupported-operation loss on the
actual producer path and extend only with explicit overwrite/invalidation
coverage. Do not repeat the unchanged same-block scope or call it accepted
transform-to-vertex correspondence. Full configuration regression pending.
LOG507 locates the actual loss: block entry8c03c94c clears4 live tags after
the known FTRV path;8c03c95c/8c03c984 also clear4. Next replace blanket block
clearing with validated entry-register handoff and cover destination writes
through traversed blocks, including fallback and bank changes. Boundary-only
report cap initially filled with startup overwrites; filtered run completed
and retained the specific gameplay evidence. Do not repeat the same-block trial.
LOG508 adds validated cross-block register handoff and overwrite/bank/fallback
protection;400 selftests pass. Three actual captures now retain7533/7581/7498
transform-dependent XYZ components (NOT complete vertices), first nonzero
coverage. Next measure complete XYZ/common-origin and draw coverage, retain
full evaluated derivations, and validate supported geometry/camera semantics.
Do not promote component counts to recovered scene proof. Full regression pending.
LOG509 identifies2508/2525/2497 complete common-origin vertices and20/21/19
fully covered draws across3 captures (4/3/5 partial). Four builds,3x402 tests,
Python331 andSDK179 pass. Next retain/export covered-draw transform/derivation
evidence, validate live coordinate/camera relation with wrong-relation controls,
and connect the supported subset to Remix. Coverage is not reconstruction
acceptance; missing geometry stays native fallback, and count-only experiments
must not displace the actual scene connection.
LOG510 exports owned bit-exact source witnesses and runs moving relation analysis.
First-frame fit validates later-frame common-origin XYZ close to X/Z+320,
Y/Z+240,0.95/Z; wrong-row control is grossly wrong.404 selftests and3 captures
pass. Next prove exact projection arithmetic and matrix/intrinsic/extrinsic
semantics for this candidate, then connect supported live geometry; empirical
fit alone is not recovered-camera or render acceptance. Full regression pending.
LOG511 saved-witness matrix analysis gives a stable calibration candidate:
FOV45.9903307484 and aspect1.22666655659 under explicit orthogonal/uniform-scale
basis assumptions. Simplified float32 projection FAILS exact reconstruction
(maxXY0.00390625); nonunit/zero W remains preserved. Next use the measured
calibration in a labeled supported scene experiment only after checking its
coordinate interpretation and retaining executed derivations; do not force the
failed simplified formula into an exact-proof gate or keep refitting constants.
LOG438-439 prove synthetic deformation and
wrong-sign separation with retained public bone transport; four incremental
builds,3x331 selftests,144 SDK checks and307 Python tests pass. Real fighter
history remains unproven. LOG437 finds new
internal geometry hashes on every CreateMesh; stable external IDs alone are not
history proof. Do not invent private update/previous-position parameters.
LOG436 renders H/L/M in one
session but shows moving-fighter ghosting; isolated resource namespaces remain
diagnostic, not trusted history. Preserve failed temporal output. Complete
frame-selector/sequence-light checkpoint; LOG437 records completed regressions.
LOG434 completes focused
composition tests/full incremental regression for checkpoint. LOG433 renders both fighters
and temple together (40meshes), retaining four source groups, and passes20 actual
loader rejection controls. Static diagnostic only; no combined-DLSS5 acceptance.
LOG430 renders first embedded draw277 (partial fighter) with original revision
and coordinate labels preserved. Full regression checkpoint remains required.
Seven large draws were revalidated in LOG427 (5722triangles/frame). LOG428 embedding and LOG429 exact
H scene/material content equivalence retain distinct source revisions.
Do not relabel camera-relative coordinates as recovered world transforms.
Continue moving joined endpoints afterward. LOG426 joins first/second H batches (33meshes/22assets),
renders the retained geometry under explicit diagnostic lighting and passes
four incremental builds plus all recorded focused regressions.
LOG425 reverse-light control reveals full submitted second batch without
deletions; default unchanged, not recovered game lighting. Run full regression
for later changes to subset/preparation/lighting tools.
LOG424 single-sided and SKY-category controls both remain black and were
reverted; do not repeat or adopt them as fixes.
LOG423 individually restores four suspects;
2522 is the primary darkening trigger and full batch minus only2522 restores
visible temple. Do not permanently drop it. Distinguish shadowing, surface
occlusion and material conversion before joining full batches.
The subset is explicitly incomplete, not accepted scene coverage. Runtime
cleanup and typed normal readback remain open; do not repeat completed waits.

Historical progression (superseded next actions, retained for evidence):
validate the LOG401 opaque alpha-test correction with focused
regressions and analytic overlap/camera/light controls. LOG404 measures final
silhouetteIoU0.9953 against analytic camera, reversed-expectation control0.2963;
normal visualizationIoU0.8323 remains discrepant, not pixel-aligned truth.
LOG405 actual reversed-camera finalIoU0.9955 (wrong-camera expectation0.2964)
and zero-light0 bright pixels establish bounded camera/light response.
Investigate guidance sampling/overlap and add capture unit coverage next.
LOG408 supersedes normal alignment suspicion: public normal source is packed
R32_UINT; float-blit normal images/IoU are invalid evidence. Capture now rejects
that option pending typed integer readback. Do not tune camera against them.
LOG407 raw depth now has0 nonfinite pixels and analytic coverageIoU0.99376;
max error0.02506 remains unexplained, so isolate edges/interiors and add negative
depth-order controls. Raw normal sidecar support exists but has not been rerun.
LOG408-410 supersede that older next action: interior max0.00004521; reversed
actual submission order produces byte-identical raw depth. Packed normal
capture is unsupported. Direct PresentEx does not remove40-object teardown
warning; temporary bypass reverted. Completion/lifetime remains next.
LOG411 EVENT completion succeeds but same40 live common objects remain at exit;
public counter is runtime CommonDeviceObject instances, not caller mesh count.
Do not add longer waits as a proposed fix; ownership/lifetime remains unresolved.
LOG412 empty-scene control still reports37 objects without any scene handles
(rendered runs40). Do not repeat wait/Present-bypass experiments. Preserve this
open runtime ownership failure; next checkpoint controls, then expand actual
scene coverage under the still-pending camera contract without claiming M1 done.
LOG414 rechecks actual first-L frame1783 lineage (3682vertices/2152triangles,
five retained strict failures). Next prepare L/M with per-frame verified assets
and fixed H origin, then test actual runtime endpoints; not inferred motion.
LOG415-416 complete separate L/M prepared/runtime endpoints (not sequence).
Next bounded one-session H-to-L-to-M transition, preserving source IDs,
explicit omissions, and resource lifetime; preparation guard positive still pending.
LOG417-418 now capture each of1782/1783/1784 in one warmed runtime session.
Swapped endpoints reject before runtime; origin/SHA/skipped-frame controls and
full regression remain next. Isolated resource namespaces do not prove temporal
identity; do not promote this3-frame diagnostic to continuous gameplay.
First actual final-color
capture now shows synthetic triangles after matching public alphaTestType7;
cleanup warnings and full scene acceptance remain open. Historical LOG400 task:
diagnose black public final-color readback using controlled
depth/normal output and scene lighting checks on Remix1.5.2. LOG400 adds WIP
owned-device capture:60 Present/readback successes but visually near-black output;
COPY swap effect alone does not fix it. Keep both failed images. Resolve
standalone public ownership/cleanup as well. LOG399
records3/120 Present successes after moving window destruction before Shutdown,
but runtime teardown reports37/40 undisposed objects. Keep the correction WIP
until relevant controls/regressions and image evidence; no cleanup acceptance.
LOG398 supersedes the unavailable-runtime stop below.
Retain the first failed run; do not infer Present success from swapchain logs.
No runtime internals or live consumer configuration edits are authorized.

Historical dependency stop (superseded by LOG398): obtain the explicit path to a compatible user-supplied x64
Remix runtime exposing the reviewed public API and its installed dependencies,
then run bounded bring-up under REMAKE-RUNTIME-BRINGUP.md. LOG397 closes exact
snapshot checkpoint. No ready independent card remains; keep the full objective
unfinished and do not expand mock-only infrastructure to substitute for runtime
evidence. No proprietary runtime download or external configuration change is
authorized. LOG395 wires explicit artifact/assets/clips into diagnostic
runtime submission without synthetic camera motion/aspect substitution. Real
DLL startup/Present remains unavailable pending compatible supplied runtime.
LOG393 passes actual positive plus13 malformed controls.
LOG392 loads actual
H17meshes/6456vertices with calibrated aspect and retained source SHA/omissions;
far100 rejects. Loader remains WIP, no runtime activation.
Reuse vendored MIT json.hpp; cap input32MiB,
128meshes/65536vertices/262144indices and existing packet bytes before allocating
converted arrays. Reject unknown schema, null diagnostic clips, malformed indices,
paths and changed assets. Preserve calibrated aspect rather than window substitution.
Keep synthetic CLI behavior separate; no automatic runtime load or config changes.
LOG390 closes exact diagnostic-entry checkpoint.
LOG389 implements D-133 with sampled coordinates/clip declaration and retained
omissions across redraw; no ordinary readiness bypass. LOG386 tests actual
H containment for caller range0.1..104; source game clips remain null.
LOG385 falsifies Synthetic far100
on17 H vertices; native depth clipping is disabled. Do not equate a diagnostic
enclosure with recovered game clips or camera acceptance. LOG384 closes
scene preparation checkpoint; API field presence alone cannot resolve clipping.
LOG382 assembles actual H17meshes/6456split vertices with derived normals and
verified materials; null clips and renderable=false remain. Preserve
unknown clips and coordinate/coverage exclusions through serialization; see
CAMERA-COORDINATE-CONTRACT.md. LOG381 closes exact checkpoint. LOG379 joins actual H geometry
and14 published textures; strict reprojection and renderable flags stay false.
LOG375 publishes14 H assets outside Git; LOG377 closes publisher checkpoint.
LOG374 closes exact checkpoint verification
and actual H mesh-to-texture selection join (17 draws,14 assets).
LOG373 connects verified captured mip bytes to in-memory DDS hashes/generations
and positive mock API transport; no runtime texture result. LOG371 closes exact
flat-normal checkpoint verification. LOG370 validates
the actual H sample and strip-break/expansion controls. Source normals stay unknown;
derived normals are not camera, shading or real-runtime acceptance.
The source-color experiment is not physical albedo or full native PVR shading.
LOG367 closes exact adapter checkpoint verification. Independent M1-GPU still
requires a compatible supplied runtime; the reviewed cache has header/licenses
only. Do not repeat that inventory absent a dependency change, and do not add
mock checks as a substitute for actual rendering. LOG366 header/missing-file controls
pass4builds/3x312/103 SDK/284 inspectors. LOG365 positive file/lifetime, malformed
payload and aggregate path budget pass4builds/3x312 selftests/96 SDK checks.
Captured game texture readiness stays blocked. LOG363/D-129 validates28 base
decodes and pinned AUTO format preservation; no GPU load claim. LOG362 adds bounded
explicit-format DDS serialization; public loader accepts DDS and queues copied paths.
LOG361 verifies
failure handles and per-surface association (SDK88/88). LOG360 implements
per-mesh untextured parameters with unknown-material rejection. LOG359/D-128
prove pinned public BGRA layout; actual runtime/material behavior remains pending.
c6d50ec73 checkpoint verified/pushed. Then
then implement an explicitly supported adapter material representation and resolve
missing normal semantics. LOG355 connects actual source attributes/textures/fog in
explicit-mip CPU samples; do not expand this into a substitute renderer. All51 sampled
draw states bind; synthetic equation inputs are not actual pixel/GPU proof.
LOG353 proves snapshot CPU
source bytes unchanged across3 captured frames; not GPU readback or image parity.
LOG352 retains8 one-level pixel differences at1783 as open regression evidence.
Do not repeat broad toggles or waive mismatch; continue independent material work.
Four builds and3x306
selftests pass; focused3frame capture closes cleanly with constants present.
Implementation remains uncommitted WIP; no exact native parity claim.
LOG348 proves old packets omit fog globals; selected equation rejects missing
state and synthetic goldens pass. Do not infer old-frame constants from new data.
LOG347
finds all17draws request offset,vertex fog,linear repeat and ignored texture alpha;
global overrides are not yet applied. LOG346 preserves original
UV/color/offset fields;462 vertices per frame require out-of-unit UV handling.
LOG345 converts14assets/30mips per
frame; selected shading mode3 multiplies vertex and texture RGBA, not texture alone.
Keep unknown normals explicit. LOG344 binds17draws to14assets across H/L/M with identity/generation
controls; this is not physical albedo or a renderable game packet. Scope and exclusions are in
CAMERA-COORDINATE-CONTRACT.md (D-126). Do not repeat numerical micro-tests.
LOG343 fixed common origin
plus actual C++ double intermediates passes H/L/M at unchanged0.001 pixels.
Float API inputs/output retained. This CPU reference result is not GPU proof;
strict captured-rounding failures and unknown physical-world semantics remain.
Do not independently recenter each frame or change source identity.
LOG341 isolates camera-origin quantization as the
largest individual error. Preserve original failed evidence. Actual full-mesh float diagnostic fails the
unchanged0.001-pixel bound at16/3682 vertices, maximum0.0020168246 (LOG340).
Do not waive or widen it. The recovered camera golden and public-ABI fixture pass (LOG339);
this does not establish a complete game scene or GPU rendering. Keep measured
effective lens aspect and explicitly unknown game near/far. Paired source/view
reflection and winding reversal implemented; H3682/2152
mathematical projection delta6.36646e-12, strict captured reprojection still fails.
Orientation support now
implemented and tested; do not flip only camera Y and create a reflected basis.
Preserve default synthetic axes and actual source geometry; no runtime proof yet.
Anchored first-batch mesh roundtrip passes, strict reprojection still fails.
Three sample camera poses derived without changing matrix
words; maximum normalized orthogonality error1.18046e-7. Do not promote runtime
or static-arena semantics. All1622 selected divided vertices retain exact source words
across H/L/M; stale first-frame matrix exceeds0.001 at all3244 later-frame samples.
Do not infer physical units or unique absolute world coordinates. Distinguish a
chosen coordinate basis from proven
static world/arena semantics; inspect scene/material context before naming it.
Normalized relative motion passes existing rigidity tolerance; firstframe1622
vertices across9 accepted draws bind to these variants. Both
source sets have homogeneous rank4 and identical relative XYZ transforms across
H/L/M; shared top-three-row matrices mean this is not independent camera evidence.
Retained H/L/M has288 common
source multisets,8 ambiguous; largest1852 and255 distinct points have one matrix
each per frame with changing words. Candidate continuity, not static arena/world
identity. Check shared camera-style delta and geometric rank before promotion.
Use existing H/L/M and
I/J/K3 ledgers before any new capture. Record ambiguity if model/view split remains
unidentifiable; no replacement of the world-camera requirement with view extrusion.
Hook-free checkpoint dff556917 passed exact4 builds,3x298 selftests,260 inspectors,
focused contracts and fresh native capture; pushed/remote SHA verified.
No more
producer captures needed for this sampled matrix. First batch H/L/M proves3682/2152
per sampled frame; all incident strict-outlier triangles reject common clip plane.
World-camera/cross-frame object identity and real Remix GPU remain unproven.
Second batch now has
accepted separate captures for all3 producer frames; not cross-frame identity proof.
K interrupted with no frames; K2 timed out at180000ms despite complete3frame
artifacts and valid920/463 frame1784 lineage. Retain K2 as diagnostic evidence,
not clean capture pass. Longer diagnostic timeout is not a performance concession.
Second-J proves920/463 expression coverage at producer1782/frame1783 plus3frame
BGP construction/binding. Reuse known producer paths, not new discovery.
Enable existing BGP construction/queue witness in same bounded runs. Do not
reinterpret BGP screen-plane depth as world geometry. Both batch mesh builders
ran with strict residual failures retained; later-frame ancestry still missing.
Second packet batch920 vertices/463 triangles has exact first-frame expressions,
429 rigid calibration witnesses; no selected gaps. Background4 vertices remains
separate. Native parity frame1783 differs8 pixels by1 channel; other2 exact, all3
scene JSON exact. Preserve strict reprojection failures despite outlier triangles
outside rectangle. Do not extrapolate first-frame ancestry to later frames.
Selected matrix calibration has1708 agreeing rigid witnesses and115 explicitly
general-affine contributions; doubled calibration rejects. No world-camera claim.
Selected maximum0.0013122621 exceeds
0.001 at one of3682 vertices; no shape rejections. Compare ordered binary32 versus
mathematical projection without changing coordinates or tolerance to force a pass.
All-record maximum0.0016823 exceeds0.001;
retain failure,do not widen tolerance. Predivision Z distinct from0.95 output-depth
scale;18 direct records excluded from projection shape,255 tests pass.
Historical derivation of calibrated camera-relative geometry from accepted H
expressions, then extend second remaining batch. H covers all3682 vertices/2152
triangles in17 draws of firstframe with exact expressions and same-capture lineage;
all3 color/scene pairs exact versus G,253 tests pass(LOG315). No later-frame ancestry
or whole-scene/world-camera extrapolation.
Historical inclusion of standard gather/copy ownership stream for remaining
1692 selected vertices using existing cc7c/cc7e/cc80 hooks (LOG313). Integrated G
exact-expression join covers1990 vertices/1096 triangles in firstframe; partial
draw70/1114 coverage explicit. Camera-relative3D still pending.
Historical join of exact expression IDs to selected scene and derivation of bounded
calibrated camera-relative geometry.4327 records reconstruct bit-exactly with ordered
binary32 expression evaluation;1951 incomplete remain explicit,253 tests pass(LOG312).
No absolute world-camera decomposition claimed.
Historical reconstruction of ordered accumulated transform expressions for
selected outputs; contribution sets alone do not prove expression equality (LOG311).
All1990 selected alternate consumers have supported sets including368 CALC; gap,
overwrite and2-contribution propagation tests pass. Other1692 consumers separate.
Historical falsification of composed ancestry sets and validation of selected CALC
consumer coverage (LOG310). Diagnostic4327 complete sets include2274 CALC with1..5
matrix contributions;1951 histories incomplete. Not accepted until negative controls
and selected-chain checks pass. Preserve all contributions and UNKNOWN across gaps.
Historical composition of dispatch/register matrix origins with10059 verified
executed memory edges in ancestry-G; preserve multiple accumulation contributions
(LOG309). Zero tracked loads lack store identity;42340 out-of-domain loads explicit.
G scene acceptance passes, native image parity remains nonexact for first2frames.
Historical propagation of initial/accumulation provenance through actual RAM
loads/stores to CALC in ancestry-G and validate full scene/tape/native comparisons.
G completes25280 blocks,zero full-session rejections,tape present;4225 initial and
1718 accumulation FTRVs pass arithmetic (LOG308). Memory ancestry still pending.
Historical capture of known SUPPLY/INITIAL/accumulation execution operands
for same-frame ancestry. CALC-F6828 input words match observed preceding writes;
5855 untracked inputs remain explicit,6 prefetches read prior CALC (LOG306).
No new producer search needed; preserve source-domain bounds.
Historical linkage of CALC input reads to known INITIAL/SUPPLY/accumulation
executions in same capture (LOG305). All1990 selected alternate consumers now bind
to completed records:1622 divided plus368 CALC. CALC original-transform ancestry
remains open; do not equate completed-record ownership with recovered camera.
Historical binding of4225 verified CALC records to combined-F consumers and
verify input-read predecessor lineage (LOG304). Arithmetic/event checks pass;
all3 scene JSONs exact versus E, but first2 native images differ at9/8 pixels(max1).
No exact-image claim. CALC input ancestry remains pending.
Historical verification of combined CALC-F arithmetic/event coverage with explicit
combined bounds. F completes15112 blocks,zero full-session rejection,tape present
(LOG303); counts are transport evidence only. Reuse known CALC checker and verify
native/tape parity before binding remaining368 consumers.
Historical reuse of known CALC lifecycle/arithmetic for remaining368
consumers; accepted-E inventory proves all have c9ce/c9cc/c9ca XYZ writers (LOG301).
No new producer discovery needed. Bind same-frame executions and preserve marker
invalidation; prior-frame arithmetic alone is insufficient.
Historical inventory of actual latest writers for remaining368 consumers in
draw70(123),1114(141),2246(104) using retained ownership-E evidence. Integrated
projection/scene acceptance supports886 triangles across9 complete draws in first
frame only (LOG300). No whole-scene/world-camera or later-frame extrapolation.
Historical integration of exact Z-projection into selected scene acceptance:
quantify supported triangles and investigate remaining368 consumers. All2035 divided
records reconstruct bit-exactly;2035 wrong-W and2035 wrong-offset controls fail
(LOG299). Coordinate/world-camera meaning remains separate.
Historical proof of explicit Z-divided3x4 projection with captured screen
offset/depth scaling and selected-output reprojection (LOG298). Raw matrix variants
have identical first3rows but different fourth rows; keep exact words. Mathematical
3x4 calibration matches614.714431/565.537219; not yet scene reconstruction.
Historical extraction of source points/matrices for645 accepted selected
executions in preserved-counter E, test projection/rigidity, and investigate368
remaining alternate selected consumers (LOG297). E passes full-session/tape checks;
all3 native color and scene JSON pairs byte-identical to counter-off D.1622 selected
consumers across9 draws have matrix/store/copy lineage; coordinate meaning pending.
Rejected C is retained. Historical investigation:
fresh continuity capture with counter preserving RAX/RFLAGS. Counter-off D uses
same binary and restores valid tape/expected topology,zero rejections (LOG296).
Counter implicated, exact mechanism not yet established; patched counter pending test.
one accepted continuity-plus-tape capture (LOG295). C has no tape and full-session
observer rejection/semantic_failed=1 despite arithmetic-only ledger checks passing.
Do not combine accepted B tape with C dispatch evidence. Prior C continuity counts
remain diagnostics, not selected geometry acceptance.
Historical steps toward binding assembled records to selected-source/lifetime connections:
using proven matrix-record identities; determine relevance of1 unlinked record.
Ownership-C continuity links2055/2056 records, verifies71274 register comparisons,
and clears origins across172 dispatch segments (LOG294). Coordinate meaning pending.
using ownership-C dispatch serials plus register continuity (LOG293). C completes
cleanly but differs from B; do not claim exact-input parity.2038 divided records,
2539 owned copies/998 executions; hidden-block gaps explicitly identified.
by proving matrix-to-store-block execution continuity. Timeline join now binds2529
copies to995 distinct completed XYZ executions with intervening-write invalidation
(LOG291). This is record ownership, not yet original-transform lineage.
from exact producer invocation through last-writer timeline. Ownership-B matches1990
selected tape records by ordinal/generation/TA offset/source/XYZ; full3frame scene
binding passes3682 vertices/2152 triangles per frame (LOG290). Tape lacks copy IDs;
do not describe composite-key join as direct ID proof. Other paths remain explicit.
through actual copy/tape acceptance. Guarded ownership-B passes3377 fresh source-read
to-copy bindings with27 outside-domain copies explicit (LOG289); no observer rejection.
using fresh ownership-B after closing interpreter/HLE guard gap and adding exact
copy identity (LOG288). Ownership-A must not be promoted across this gap.
at copy/TA identity and audit non-x64 write coverage. Ownership-A completes cleanly;
all10470 source reads match preceding observed writes, zero missing writers,
7818 components latest-written by new XYZ paths (LOG287). Byte ownership is not
yet complete matrix-to-copy lineage; no legacy rejection relaxed.
with explicit intervening-write invalidation. Values-B now assembles2035 divided
and18 direct records over360 addresses, preserving execution identities;238 tests
pass (LOG285). Assembly is not consumer ownership or cross-block lineage.
including marker invalidation and non-storing transforms (LOG284). Complete event
coverage,8872 read addresses and9489 store addresses/SSA values now verify.
Divided path has2200 transforms but2035 XYZ store triplets;165 must remain
explicitly non-storing, not inferred geometry. Markers can temporarily alias X.
for FTRV paths a9b0/a9ea. Values-B confirms live FP mode and independent reference
matches2218 FTRV,2200 divisions,6105 multiplies and4070 additions (LOG283).
This still does not prove source loads, cross-block continuation or camera meaning.
Independent rational dot reference matches all2218 captured FTRVs under assumed
toward-zero mode;231 tests pass (LOG282). This is not full producer arithmetic
or source lineage. Values-B subsequently verifies live FP mode (LOG283).
Dynamic A captures6654 balanced blocks,9489 exact RAM stores and2200/18 FTRV
executions respectively at a9ea/a9b0 without observer rejection (LOG281).
This is trace-envelope evidence, not independent arithmetic acceptance.
Previous discovery:
(LOG279).14 descriptors/122 operations identify direct and divided/screen-offset
paths; a9f2 is an integer metadata write, not automatically a position. Distinguish
record layout and buffer aliasing before accepting overwrites. Mapping-only A
keeps3682-vertex/2152-triangle source map valid; first-once snapshots are not a
continuous execution trace. Dense C reaches2274 lifetimes within4096/frame, then rejects
unobserved-record-writer at slot0/address8ce6e460; this is not capacity failure.
Do not classify it as existing accumulation or relax ownership. Dense-ID tests
accept4096/reject4097, legacy contracts remain unchanged;228 tests and legacy
integrated regression pass. Retain A/B capacity failures and C producer failure.
Background live wrong-depth rejects with all3 native color/scene
pairs byte-identical;225 tests pass. Sparse incarnation reconstruction adapter
passes prior four-draw reconstruction (2093 triangles/frame, LOG271). Next merge
proven versioned lifetime instrumentation with alternate read/store copy support;
remaining transform A has run and failed its lifetime capacity, not accepted.
Background B independently reconstructs all four final XYZ vertices/frame from
decoded inputs with actual queue context/generation binding. This is geometry
construction, not absolute VRAM/UV/color/material provenance or world camera.
224 tests pass. Extended-map wrong-pointer rejects/no
tape. Second packet batch verifies920 vertices/463 triangles/frame,217 source
addresses and135 changing values; four background vertices explicitly omitted
and still required.221 tests pass. First17-draw map B verifies3682 vertices/2152
triangles/frame,656 source addresses with363 changing values; original transforms
remain pending. FillBGP reads VRAM/registers and constructs background corners,
so never manufacture SQ provenance for it. Independent verifier and three
live address/value/generation controls reject while all10212 packet words remain
unchanged.220 tests pass. Absolute source-address proof beyond trusted executed
callbacks and original-transform lineage remain unproven. Read A covers10212 alternate SQ
copies with nonzero contiguous XYZ triplets,385 distinct bases, full writer-B
packet equality and no overflow. The read sites are ab56/ab58/ab5a and
ccc2/ccc4/ccc6. Writer B covers all8730
missing-offset matches;9 executed descriptors/204 operations identify unique
matching-version reads for all writer variants. Runtime RAM addresses and
original transforms remain pending. Actual SQ-flush sites are
ab78/ab8e/cce6/ccf2 in two alternate producer families (LOG251).
B proves same-call SQ addresses for all8730 missing-offset matches and preserves
all15396 alternate packet words against A. Earlier ab88/ccec context snapshots
were not actual flush instructions. Source transforms remain unproven. Corrected inventories
C complete:1990 missing associations/frame in batch1,920 in batch2, plus four
absent packet samples/frame in batch2. Distributions repeat across all3frames;
no partial tape is accepted. Types3/4 identify packed-color textured formats,
not producer truth. Trace alternate producers without manufacturing provenance.
Sparse selector/topology preflight pass;216 tests green.34 draws partitioned into17/17:
3682 vertices/2152 triangles and924 vertices/465 triangles. Preserve4096 vertices,
existing copy/byte limits, explicit ownership and all non-contiguous index gaps;
bounded draw-selection capacity is34 with ownership/gap/order negative tests.
Four-draw slice is exact tested/pushed755b90f8e; hooks removed, live negative rejects.
Strict all-rigid calibration remains failed; explicit affine mode retains93
nonrigid contributions while10035 rigid witnesses constrain calibration, with
all2093 selected triangles/frame reconstructed below0.000064542 pixels.214 tests
pass. Combined separate draw scopes cover5722/8339 opaque triangles;2617 remain,
and unified scene/world-camera/Remix GPU are unproven. Default compression3 and
all evidence/rigid-check limits are unchanged. Temporary hooks are retained ignored.
Wrong-pointer map control passed. Map A verifies3531 vertices/
2093 triangles,1005 stable source addresses and exact final TA offset375744 in
all three frames (LOG232).201 addresses have different selected values within a
frame: bind actual incarnations, never address-only identity. Stay within4096
selected vertices and existing copy/byte budgets; do not reuse prior target header.
The prior two-draw incarnation/reconstruction slice is exact-build/native tested
and pushed as36538ae12. Live wrong-version E rejects; hooks removed.210 inspector
tests pass; all6732 lifetimes/26928 seams verify,
10176 consumers bind3366 selected versions and2038 triangles/frame reconstruct
with maximum0.000070083-pixel error. Wrong-scale and synthetic wrong-version
controls reject. Together with separate draw277 evidence,3629 distinct opaque
triangles are covered;4710 remain outside those scopes. This is not a unified
scene or recovered world camera. Real Remix GPU remains pending.
Full921 A now proves all2763 address instances/8235 consumers and reconstructs
all1591 draw277 triangles/frame.6748 whole-scene opaque triangles remain omitted;
world camera and Remix GPU are unproven.204 inspector tests pass; live wrong-edge
rejects, but one raw-depth artifact differs, so exact-input preservation is not
claimed. Next batch draws1503 and1887 together:2038 triangles/3392 vertices,
indices7272..10663 in the actual vertex array. Derive their actual copy/source
maps before extending geometry capture; the old5069-copy prefix does not prove
these later draws. Preserve4096 selected vertices/frame, explicit bounded copy
ownership and the same arithmetic/calibration tolerances. Do not repeat draw277,
walk small ordinals or expand transport-only work without new source coverage.

Accepted bounded evidence:
- LOG180..184 / TRANSFORM-COVERAGE-AUDIT.md:15 selected samples including actual
  accumulation where present; older draw26 slot3 remains unsupported.
- LOG192 / FULL-DRAW-TRANSFORM-AUDIT.md: draw1's142 vertices and92 triangles
  have original-transform/RAM/gather lineage and calibrated reprojection.
  It omits8247 opaque triangles/frame; physical scale/world camera unknown.
- LOG194..197 / COMPACT-CONSUMER-TAPE.md: draw277's2745 vertices,3321 indices,
  576 restarts and1591 triangles have a complete compact copy/decoder/scene
  map across3frames. Tape has8235 records/1317616 bytes and references921
  XYZ addresses/frame. No differing values at reused addresses were observed.
  Address reuse is not original-transform identity or authoritative generation.
- C++ emitter13 checks, cross-language bytes, old-domain same-capture exact
  verbose/tape equality, large-domain scene-binding tests and live wrong-pointer
  controls pass. Both wrong-pointer captures created no tape.188 Python
  inspector tests pass. Temporary core hooks are removed; retained patches
  remain ignored evidence. Four restored builds, three298-test selftests,
  SDK63-check mock and compact13-check cross-language test pass. The committed
  exact-SHA build/test/native capture checkpoint is verified in LOG199.

Failures retained, not retried away: large A/B encountered a never-copied
completed gather after5069 actual copies. C identified a later unsupported
copy beyond the selected domain. D deliberately limits acquisition to the
observed5069-copy prefix, and all8235 selected vertices independently find
their actual copies. E wrong-pointer rejects. Later geometry is omitted,
not accepted. No paired native/image-preservation or performance claim.

Next original-transform collection must:
1. Derive the921-address/consumer map from the accepted tape, preserving
   frame/producer/generation/packet identity. Never assume a contiguous range.
2. Reuse known executed producer seams only where actual descriptors and
   observed source pointers agree. Preserve nonunit W, all matrix/input bits,
   accumulation, X-load continuity, actual writes and every consumer's final
   bytes. Unknown producers/writers reject or remain explicitly unsupported.
3. Replace repeated text with a bounded compact transform ledger. Keep3frames,
   at most4096 records/frame,32 descriptors with128 operations/32 live words,
   64 writes/record and8MiB emitted ledger. Record the exact layout and memory
   bounds before running; source consumer tape is not arithmetic evidence.
   LOG200 provides a tested lossless zlib envelope using the existing dependency:
   8MiB compressed/128MiB formatted-input cap and bounded streaming buffers.
   The actual921-address target table is derived and retained ignored. Next
   producer callbacks are now integrated in temporary hooks (LOG201); no further format-only phase. Read
   COMPACT-TRANSFORM-LEDGER.md before running the new ledger.
4. Independently verify arithmetic and original-transform-to-consumer binding,
   then reconstruct the supported1591-triangle domain with shared-calibration
   controls. Quantify omissions and never silently close parked strict replay.
5. Preserve the prior no-progress/pivot rule and advance the highest-coverage
   supported family. Top10 draws contain6570 of8339 triangles; do not spend
   another phase walking small draw ordinals one by one.

Independent M1-GPU: standalone runtime bring-up builds;63/63 public-header
mock tests include120-frame immutable-resource reuse and failure invalidation.
The configured cache contains only header/licenses. Real runtime startup,
GPU output/readback/completion and combined presentation remain unproven.
Use REMAKE-RUNTIME-BRINGUP.md when a legal compatible runtime is available.
Do not expand mocks further merely to avoid that dependency; no proprietary
binary fetch or configuration workaround is authorized.

### Autonomous progression and no-progress rule

Each card records its hypothesis, owned scope, dependency, success/failure
artifact, fixed bounds and next transition before execution. Review, test and
advance without routine human signoff. Do not stop after each isolated store
or turn a self-written task card into a claim that an unmet gate passed.

After two consecutive tranches produce no new causal fact, supported scene
coverage, working capability, or diagnosed/fixed regression: stop that tactic,
record why, and select a different concrete hypothesis or ready dependency.
An informative negative is progress only if it eliminates a named hypothesis
and changes the next action. Do not count larger logs, repeated audits or
unchanged captures as progress. Do not silently expand sample/caller bounds.

Missing additional games, monitor movement, spontaneous TDR or physical runtime
removal are scoped coverage gaps, not reasons to delay the available prototype.
Missing legal runtime/build dependencies are checked once and revisited only
when the environment changes; finish synthetic/mock/interface work meanwhile.
If every useful card genuinely needs unavailable resources or new external
authority, report one precise blocker and required action. "Without human
intervention" does not authorize prohibited binaries, media acquisition,
external configuration writes, spending or unrelated system changes.

### Working-pipeline acceptance (standing-goal exit)

All of the following are required together; this is an experimental supported-
scope gameplay prototype, not universal Dreamcast/Naomi or production readiness.

- A reproducible opt-in launch/build route, with exact source/API/runtime
  provenance and explicit supported renderer/game/domain restrictions.
- Actual original Soulcalibur moving fighters and arena processed by RTX Remix
  on the GPU, then by the supplied DLSS 5 consumer, with the combined pixels
  presented by Flycast. Publish synchronized native, public DLAA, Remix-only
  and combined moving evidence for at least 300 consecutive gameplay frames.
  A synthetic-only scene, two unrelated demos, depth extrusion or module/log
  detection cannot satisfy this requirement.
- Scene/guidance/content-rectangle contracts validated for the combined route;
  exact frame/history/provenance and deliberately wrong-input/output controls
  fail. Consumer settings must be positively reported; no undocumented keys.
- Zero protected HUD/text mismatch, correct late OSD/ImGui, explicit safe
  treatment of transparency and native bypass for unsupported 2D/FMVs/RTT/
  direct-framebuffer cases. Unsupported world geometry cannot silently vanish.
- Separate non-synchronous 600-frame normal and OIT runs on the selected
  D3D11On12 route at a recorded resolution/quality lane. After at most120
  declared warm-up frames, at least99 percent of steady eligible frames must
  use actual combined output; no missing/incorrect frame identity, drops,
  unbounded waits, stale fallback output or growing owned-resource count.
  Report latency and P50/P95/P99 per-pass/frame timing and VRAM, not pass alone.
  Do not lower these criteria after seeing results to force a success.
- Emulation/audio timing ownership unchanged; compare emulated-cycle advance
  per wall time to paired native control (within1 percent over the same bounded
  run). A slow lane must fall back explicitly, not slow emulation. Fallback-only
  success does not meet the steady combined-output requirement.
- Focused mode-off/on, resize, renderer restart, game reload, save-state load,
  injected create/evaluate failure, ring-busy/device-loss recovery and shutdown
  checks on affected code, preserving prior coverage instead of claiming a
  full fresh hardware matrix. Native/no-NGX/feature-off builds remain green.
- Evidence index, configuration instructions, known limitations, tested source
  checkpoint committed/pushed to the fork, and clean worktree. The old strict
  replay residual and unavailable-title/hardware gates stay visible, not passed.
  A user-selected transformative style is allowed; factory Faithful stays
  public DLAA Auto unless its separate quality acceptance is met.

### Validation and checkpoint discipline

Use existing build/capture/selftest procedures in REMAKE-FEASIBILITY-PLAN.md
and DIAGNOSTICS.md. Build all four configurations serially and run all enabled
selftests plus relevant fixtures after code commits. Follow the original
per-commit matrix; document-only changes additionally need live-queue/link/
safety consistency checks, not a fabricated gameplay result. Record actual
counts. Keep the LOG's exact-checkpoint addenda distinct from fresh tests.

Update the current card in place. Keep detailed evidence in LOG/audits and
private raw artifacts outside Git. Commit each independently proven capability
or falsifying result, not an indistinguishable pile of unproven quality edits.
Do not add more historical "current task" headers to this active section.

## Existing FC registry and evidence

The table below retains existing IDs and historical evidence. The active queue
above determines priority; old "pending" phrases in evidence are dated claims,
not requests to repeat closed gates. Verify the relevant latest audit when a
card actually touches that behavior.

| ID | Phase | Item | Acceptance | Status | Evidence |
|---|---:|---|---|---|---|
| FC-000 | 0 | Pinned branch and baseline | Build, tests, hashes, timing, present notes | doing | LOG #1-#4; hashes/timing pending harness |
| FC-001 | 0 | Verify source facts | Exact paths and symbols recorded | done | 00-render-path.md |
| FC-002 | 0 | Document render path | TA through Present before code changes | done | 00-render-path.md |
| FC-003 | 0 | Build options and skeleton | OFF, instrumentation, NGX configs build | done | LOG #5-#9 |
| FC-004 | 0 | Planning documents | Required docs and full backlog exist | done | docs/neural |
| FC-005 | 0 | Licensing note | No binaries; maintainer review stated | done | THIRD_PARTY_NOTICES.md |
| FC-009 | 0 | Phase gate | Three builds and backlog complete | done | LOG #5-#9 |
| FC-010 | 1 | Synthetic fixtures | All named scenes plus TA-stream fixture | doing | LOG #12; test-only geometry exists, `rend_context`/TA parser and analytic truth pending |
| FC-011 | 1 | Render/determinism | Five-run hashes and deltas | done | LOG #11-#13,#111; all 14 fixtures are exact across five runs under both renderer labels, and deterministic production normal/OIT captures plus material wrong-frame controls are green |
| FC-012 | 1 | Scaling/depth commands | Genuine 4x/8x samples; depth ground truth | done | LOG #11-#12,#61,#112; production PVR scaling and production-shader depth truth are both captured with failing controls |
| FC-013 | 1 | Motion command | Reports error, trust, tiers, reactive pixels | doing | LOG #14; explicit no-data result, matcher pending |
| FC-014 | 1 | Neural passthrough | Shared stage and artifact package | done | LOG #13,#29,#73,#113; passthrough color identity is exact, the shared stage carries the complete GPU export set, and production native-lane packages do not fabricate public/external output |
| FC-015 | 1 | Harness NGX | D3D11/D3D12 live or precise unsupported | done | LOG #14,#34-#46; public D3D11 and D3D12 create/evaluate matrices are live on RTX while WARP/no-NGX paths return precise unsupported status |
| FC-016 | 1 | Compare/capture | Threshold exit and legal capture path | done | LOG #13-#14,#73; compare thresholds and the bounded legal-media production capture launcher return explicit success/failure codes |
| FC-019 | 1 | Phase gate | Commands run for both renderers | todo | |
| FC-020 | 2 | Public structures | Renderer-neutral atomic frame contract | doing | LOG #20,#29; C++17 neutral contract and complete DX11 GPU texture set attached atomically; D3D12 ownership pending |
| FC-021 | 2 | Runtime probes | Disabled path has zero probe work | done | LOG #20,#111; mode-off production markers prove zero instrumentation, draw records, previous-position history, neural input-layout/export/backend allocation, and guidance replay on normal/OIT D3D11 and D3D11On12 |
| FC-022 | 2 | Determinism / Gate 1 | Disabled equals baseline | done | LOG #111; deterministic Soulcalibur replay is bit-exact for 5/5 1440x1080 production PVR frames across normal/OIT native D3D11 and enabled-build D3D11On12 versus compile-time feature-off; 28 synthetic renderer/fixture five-run cases are deterministic and material wrong-frame controls fail |
| FC-023 | 2 | Genuine scaling / Gate 2 | 4x/8x edge samples differ from nearest | done | LOG #112; exact Soulcalibur frame 30 production captures prove material full-frame and edge-only differences plus subpixel-diverse edge blocks at 4x/8x on normal/OIT native D3D11 and D3D11On12; the zero-difference nearest control fails |
| FC-024 | 2 | Depth / Gate 3 | OP+PT only, correct ordering | done | LOG #25,#29,#61; production-shader Gate 11 proves zero clear, greater-is-near ordering, OP/PT agreement, reverse-order stability, exact D3D11/On12 parity, and a failing wrong-polarity control; both public NGX APIs create/evaluate with the corrected flag |
| FC-025 | 2 | Draw records and IDs | Fixed history; R16_UINT ID | doing | LOG #20,#29,#64,#69; fixed 8192-entry history separates topology/UV/resource identity, content generations, and pose; current and expected-accepted OP/PT R16_UINT IDs now gate pixel reprojection; translucent coverage remains pending |
| FC-026 | 2 | History generation | All structural reset sources increment | doing | LOG #20,#26,#49,#80-#82,#85-#86; enable/mode/resize/overflow/framebuffer-source plus global render reset/save-state deserialize are wired; context, renderer, surface, same-media reload, and real in-memory save/load transitions retain explicit resets and discontinuities without false output continuity; cross-title runtime evidence remains pending |
| FC-027 | 2 | Atomic package | Once per display frame; never RTT evaluate | doing | LOG #20,#26,#29; complete DX11 TextureRef set is attached to one Geometry package; runtime cadence/RTT evidence pending |
| FC-029 | 2 | Phase gate | Gates 1-3 and deterministic snapshots | todo | |
| FC-030 | 3 | Previous selection | Last successfully evaluated frame | done | LOG #17,#26; renderer commits reference history only after `Submitted`, skip-reference unit control green |
| FC-031 | 3 | Draw matching | Three tiers, strips, rigid fit, reactive/N2 | doing | LOG #17,#27,#64,#66,#68,#117; exact and structural buckets use deterministic minimum-cost assignment; exact strips retain per-vertex history, reindexed ordinary geometry uses a bounded similarity fit, and exact-topology Naomi 2 retains accepted matrices; reindexed Naomi 2 remains conservatively rejected |
| FC-032 | 3 | Motion rasterization | RG16F render-pixel current-to-previous | done | LOG #27,#29,#62,#66-#67,#94,#117,#122; accepted previous positions are bound as a second DX11 vertex stream and the production PVR shaders rasterize exact `[-4,+3]` truth on native D3D11/D3D11On12; Naomi 2 does the same with accepted model/projection history and a failing missing-history control; the primitive-restart validator defect found by deterministic Soulcalibur gameplay is corrected, yielding 82.81% average trusted pixels over the 30-frame sample; active production jitter remains separate from motion and preserves the unjittered native framebuffer. |
| FC-033 | 3 | Confidence/mask | R8 confidence and required bias rules | done | LOG #27,#29,#64,#67-#69; evidence-based confidence, validity/magnitude rejection, and accepted-depth/draw-ID reprojection all feed the final public current-color bias mask; wrong-disocclusion control has measurable trail energy |
| FC-034 | 3 | Reset rules | All cadence and scene-cut resets | doing | LOG #17,#26,#68-#69,#80-#82,#85-#86; accepted-frame age/skips and unmatched-area scene cuts conservatively invalidate motion, increment history generation, and produce full current-color protection; renderer/context/surface, same-media reload, and real in-memory save/load call-site evidence is green; broader-title scene transitions remain pending |
| FC-035 | 3 | Jitter | All VS variants; Halton; unjittered motion | done | LOG #17,#62,#67,#118,#122-#123,#127; production standard-PVR, OIT, and Naomi 2 shader fixtures prove exact +1-pixel coverage movement with zero jitter-only motion on native D3D11 and D3D11On12. Deterministic Soulcalibur frames 1804-1806 exercise nonzero Halton jitter in both normal and isolated OIT scene replays while every native PVR frame remains byte-identical to its unjittered control. Injected failure returns native output with no stale public artifact; protected overlays remain conservatively zero-jitter. DLSS5 experimental uses the same accepted-history-indexed phase as public DLAA. |
| FC-036 | 3 | Gates 4-6 | Fixture thresholds pass | done | LOG #27,#62,#67-#69,#113,#117,#122,#127; legacy Gates 4-6 and quality Gate 12 are green: static motion is exactly zero, analytic translation/deformation and sign/scale negatives pass, current/previous positions remain unjittered, and production Halton jitter affects only the isolated normal/OIT neural raster without perturbing native output. |
| FC-039 | 3 | Phase gate | CPU/HLSL evidence and unit tests | todo | |
| FC-040 | 4 | Stage API | Nonblocking submit/status/output contract | done | LOG #18,#31,#34; 720 live D3D11 submissions use shared stage and return output without production waits |
| FC-041 | 4 | Resource rings | Three deep; fixed; deferred retirement | doing | LOG #25,#29,#31; fixed D3D11 input and NGX output/query rings implemented; deferred retirement pending |
| FC-042 | 4 | NGX D3D11 lifecycle | RAII, SEH leaves, readable capability | done | LOG #31,#33-#35; exact project identity/version, external feature path, live create/evaluate/cleanup and readable unsupported paths verified |
| FC-043 | 4 | Recovery/timing | State machine, removal, async timings | doing | LOG #18,#31,#98,#116; sliding-window hold, nonblocking ring readiness, exception/device-removal mapping, and controlled actual D3D12 removal/reconstruction are green; native D3D11 and asynchronously retired D3D12 queue timings are measured without waits, while spontaneous driver/TDR evidence remains unavailable |
| FC-044 | 4 | D3D11 DLAA/SR | 240-frame public NGX matrix | doing | LOG #34,#37-#40,#61,#63,#72,#94-#95,#101,#103,#106-#107,#118,#120,#124,#130; deterministic Soulcalibur combat covers target-native Auto/J/K, exact 427x320-to-640x480 Quality SR, and a 5120x3840 8x-native reference. A fail-closed settings sweep proves intensity 0.5, 0.25, and 0.125 plus tone/style/preset variants: lower intensity and Natural style reduce damage, global tone 0 is output-inert on this SDR interval, and external preset 2 is byte-identical to preset 0 with upscaling off. The exact capture-reset OIT rerun confirms that 0.125/Natural slightly lowers raw temporal MAE but still loses public Auto on source, trail, thin-line, color, saturation, and black constraints. Public Auto therefore remains the Faithful baseline. Uncanny Cinematic/Structure-200 is valid as an explicit transformative profile but remains non-faithful. |
| FC-045 | 4 | Conditional D3D11On12 surface | Same renderer, dedicated D3D12 lists, selected only by measured route need | doing | LOG #42-#44,#52-#53,#78,#80-#87,#114,#116; production queue/device, wrapped input/output rings, flip-model backbuffer ring, active-render resize/minimize/restore, borderless-fullscreen roundtrip, observed foreground focus loss/restore, same-renderer API-context recreation, bidirectional normal/OIT renderer switching, bidirectional D3D11/D3D11On12 surface switching, same-media unload/reload, in-memory save/load, pause/resume, and controlled actual D3D12 removal/reconstruction are green for Soulcalibur; monitor-move, cross-title load, other-API switching, and spontaneous driver/TDR coverage remain open |
| FC-046 | 4 | Cross-API parity | Identical inputs within 1 LSB | done | LOG #45; all 12 final D3D11/D3D12 DLAA/hook/SR fixture pairs are byte-identical |
| FC-047 | 4 | Hook-compatible DLAA | Zero jitter standard D3D12 NGX shape | done | LOG #44-#45,#53,#59; three 240-frame zero-jitter standard D3D12 evaluations and the production Soulcalibur interception route are green |
| FC-048 | 4 | D3D11 bridge transport | Genuine D3D11 contract mirrored through a private D3D12 consumer and returned with frame identity | blocked(no compatible contract-preserving bridge runtime supplied -> obtain a compatible package or authorize a bounded local bridge build) | LOG #51; Feeder v0.10.0-beta.2 constructs its own ReShade image-derived contract and is transport reference only, while standalone NIGos is absent |
| FC-049 | 4 | No-RTX behavior | WARP/no-NGX green on both APIs | done | LOG #46; SDK and no-SDK WARP GPU texture allocation plus D3D11On12 surface creation return clean explicit unsupported status on both APIs |
| FC-050 | 5 | Settings/UI | Modes, reason, metrics, debug view | done | LOG #21,#108,#121,#128; the thread-safe live snapshot now feeds both the settings diagnostics and an off-by-default, lower-right in-game status OSD. It reports mode/profile/preset, D3D11 or D3D11On12, submit/bypass/HUD state, raster/output size, jitter, FPS/frame interval, and accepted/busy/fallback counters while labeling unavailable drop data honestly. DLSS 5 mode identifies public-contract status and keeps external output explicitly unverified. The seven-view guidance selector and Uncanny profile remain separately available. |
| FC-051 | 5 | Presentation | Evaluate before OSD/ImGui, once/frame | done | LOG #49,#77,#109; accepted output is selected once per emulated frame with native fallback, while exact pre/post production captures on normal/OIT D3D11 and D3D11On12 prove Flycast's ImGui-rendered OSD appears only after the saved neural/game-overlay composite |
| FC-052 | 5 | Reset/cadence wiring | Actual emulator call sites connected | doing | LOG #49,#80-#82,#85-#87; production render/reset/save-state and source-transition call sites are connected; context recreation, renderer/surface switches, same-media unload/reload, real in-memory save/load, and pause/resume are identity-safe across normal/OIT D3D11 and D3D11On12; cross-title load remains pending |
| FC-053 | 5 | Internal resolution | Set/restore and resize rules | done | LOG #63,#72; production Match Neural Output uses exact post-aspect content dimensions for target-native DX11 lanes, excludes bars, follows fullscreen size, leaves RTT/direct-framebuffer paths untouched, and preserves manual/SR sizing |
| FC-054 | 5 | Capture CLI | Rate-limited artifact package | doing | LOG #73-#74,#90-#95,#99-#103,#106-#107,#118,#124-#126,#128-#130; bounded production D3D11/D3D11On12 normal/OIT capture writes the full guidance/output package and can now select the neural-status late-OSD proof independently from the legacy FPS proof. Exact on/off Soulcalibur pairs keep nine pre-OSD artifacts byte-identical on normal and OIT while the late presented artifact alone changes. A one-shot capture-start discontinuity normalizes accepted-history jitter across candidate/marker/policy-off lanes after warm-up; exact-SHA conservative, maximum-coverage Uncanny, and HUD-safe Uncanny OIT lanes now each confirm 30/30. The full legal-title matrix remains. |
| FC-055 | 5 | Optional layer classes | Only after FC-044 green | doing | LOG #70-#71,#104-#105,#125-#127; actual sorted submissions retain reactive-only motion; topology-proven strip/triangle-list batches, PVR-native coordinate classification, and exact `T1401N` profile continuity protect Soulcalibur text, bars, timer, counters, and names across moving normal/OIT frames with zero protected-pixel mismatches. OIT neural replay now owns a separate A-buffer, opaque/multipass history, and aligned resolve-time reactive target, leaving native PVR output byte-exact on both D3D11 surfaces. Broader-title/uncertain-overlay Gate 15A/15B coverage remains open. |
| FC-056 | 5 | Experimental DLSS 5 consumer mode | Route-neutral public contract, readiness ladder, native fallback, no private implementation | done | LOG #50-#59; the selected D3D11On12 route passes Gate 10 with full-contract ON/OFF hashes, per-frame sentinel presentation, zero display-frame latency, and native fallback; direct D3D11 and bridge remain unselected candidate routes |
| FC-059 | 5 | Phase gate | Runtime toggles and Gate 8 | done | LOG #49,#109-#110; production pixel Gate 8 is green with a failing no-OSD control, and a continuous exact-frame neural-off/on round trip proves native fallback, zero off-mode evaluation, fresh reset re-entry, and clean identity on normal/OIT D3D11 and D3D11On12 |
| FC-060 | 6 | Unit tests | Fifteen specified behaviors pass | doing | LOG #50,#61-#64,#66-#73,#76-#77,#105-#106,#108,#121-#124,#126-#129; 168 checks pass in automation, NGX, and no-NGX builds, including exact production OIT raster-jitter coverage, one-shot capture-start reset ownership, and the prior profile, history, motion, disocclusion, overlay, provenance, marker, and live-status contracts; all seven developer visualization shaders compile from production source |
| FC-061 | 6 | Harness acceptance | Gates 1-8 and cross-API/debug checks | doing | LOG #11-#14,#27,#45,#61-#63,#67-#71,#108-#113,#117; legacy Gates 1-8 are green with current falsifying controls, including accepted-history Naomi 2 HLSL motion; a final debug-layer-clean aggregation remains blocked by unavailable host debug-layer components |
| FC-062 | 6 | Public NGX acceptance | Live matrix or exact hardware blocks | doing | LOG #34,#38-#40,#44-#45; public RTX harness matrix green on both APIs; production captures/cadence remain pending |
| FC-063 | 6 | Failure acceptance | No crash/stall/poison/leak | doing | LOG #35,#75-#76,#78,#80-#89,#110,#114-#116; missing runtime, WARP/non-NVIDIA, and no-NGX return clean unsupported; injected failures, production-leaf SEH containment, controlled active-runtime-unavailable, and controlled actual D3D12 removal/reconstruction retain explicit safe behavior; bounded holds recover, terminal failures latch, and active window/focus/context/renderer/surface/game/save-state/pause/mode lifecycles close cleanly on normal/OIT paths; spontaneous driver/TDR, physical loaded-runtime removal, and broader transitions remain |
| FC-064 | 6 | Performance | Invariants and measured targets | doing | LOG #75,#77-#89,#96-#98,#102,#110,#114,#116; asynchronous no-flush production telemetry reports per-pass GPU P50/P95/P99, exact source/accepted/displayed frame identity, present-call intervals, repeat/drop/gap/alternation/latency counters, live mode, accepted-reset state, stage/fallback counts, query-ring pressure, post-warmup VRAM, and scoped Flycast-owned neural GPU-object counts; transition and four 10000-sample normal/OIT soaks cover both surfaces; continuous 600-sample off/on and foreground-focus intervals on all four paths and fresh post-device-removal normal/OIT intervals have zero missing/drop/identity/repeat/latency faults and zero owned-object growth; external ON/OFF spans remain separate rather than an isolated external-cost claim, and broader-title/spontaneous-device evidence remains |
| FC-065 | 6 | Manual game matrix | Legal available images; gaps stated | doing | LOG #73,#90,#94-#95,#100-#103,#106-#107,#118-#121,#124,#126,#130; Soulcalibur has a pixel-repeatable Hoko Temple combat sequence across native, target-native DLAA Auto/J/K, accepted Quality SR, 8x native, conservative external tuples, and explicitly non-faithful Photoreal/Cinematic captures. The exact capture-reset OIT rerun again finds no Faithful external winner. Cinematic/Structure-200/Tone-75/max coverage and automatic-HUD lanes complete 30/30 exact contracts on both normal DX11 and OIT; the current OIT HUD-safe lane protects an average 15,803 pixels with zero mismatch/repeat/drop while remaining visibly and numerically non-faithful. Uncanny Cinematic is therefore a valid user-selected candidate default, not the factory default; other legal titles remain unavailable. |
| FC-066 | 6 | Mandatory DLSS 5 provenance test | User-supplied real-emulator route passes all Gate 10 items | done | LOG #53-#59; all 120 full input contracts matched across ON/policy-OFF, 118 returned outputs differed, frame 9 distinguished native/public-DLAA/external hashes and carried 1024/1024 sentinel pixels through successful same-frame Present, and negative controls retained native fallback |
| FC-067 | R | Reuse-first remake feasibility | Proven scene contract, real Remix GPU/gameplay and separately proven combined external DLSS 5 presentation | doing | D-114 and active queue above. LOG #175 links an opaque FTRV to actual TA output; camera domains/coverage, real Remix GPU, moving relighting and combined presentation remain open. No routine human approval between bounded cards. Strict old replay residual remains failed/parked. Does not replace FC-048 or FC-056/066. |
| FC-069 | 6 | Definition of done | All non-contingent requirements green | todo | |

## Retained UI and title follow-ups (not the current assignment)

FC-050 explicit intensity panel extension (LOG #131): DLSS 5 mode now exposes
pending 0-200 percent Overall/Structure/global/local Tone, style, mask, and UI
correction controls with Use Uncanny values and an explicit Apply action. Apply
invokes the installed settings companion, shows its backup/result, and requires
a restart. It does not establish consumer-active provenance. The helper launch
and exact backup/preserved-keys tests use a disposable configuration; manual
settings-panel click/layout coverage remains pending.

LOG #127 closes the focused OIT replay/jitter implementation and LOG #128 adds
the late neural-status OSD with exact normal/OIT layer isolation. A manual
settings-click and resize smoke remains useful UI coverage but does not block
the image-contract work.

LOG #129 rejects two recapture attempts whose pre-capture external-host startup
accepted different evaluation counts and therefore selected different Halton
phases. The bounded capture writer now issues one discontinuity before the
first retained frame so every comparison lane begins at accepted-history phase
zero. This reset is synchronous-capture-only and must not be enabled for
performance measurements.

LOG #130 completes the affected Gate 16/17 OIT recapture. Public Auto,
conservative external, maximum-coverage Uncanny, and automatic-HUD Uncanny now
have reset-aligned moving comparisons with exact provenance. Preserve those
artifacts and do not rerun them absent a focused regression.

When another user-supplied legal title becomes available, fill one missing
style family with native, public Auto, automatic-HUD Uncanny, and separately
labeled maximum-coverage evidence. Until then, a manual Video-setting toggle
and resize smoke for the optional neural-status OSD remains a bounded local
UI check; it does not block the accepted image contract.

## Subsequent assignment — Gate 17 style-family expansion

LOG #124 completes the exact-commit post-jitter Soulcalibur rerun. Preserve its
three accepted external lanes, moving comparisons, rejected controls, and
restored config hash.

1. Keep public DLAA Auto as Faithful for Soulcalibur and retain Uncanny as a
   persistent, user-selected, explicitly non-faithful profile.
2. When additional user-supplied legal titles become available, capture one
   deterministic moving interval per missing style family before considering
   any factory-default change.
3. Run automatic HUD protection for every candidate-default lane and require
   zero protected-pixel mismatch; label disabled-overlay runs maximum-coverage
   experiments only.
4. Keep accepted-history-indexed jitter and exact candidate/marker/policy-off
   provenance. Never reuse the rejected absolute-frame phase design.
5. Preserve completed OIT scene-replay jitter and reset-aligned evidence from
   LOG #127-#130; rerun only if a new change creates a focused regression.

## FC-056 / FC-066 route sub-items

- Direct D3D11: blocked for the exact supplied add-on; LOG #51 proves Flycast's D3D11 contract evaluates while the add-on arms D3D12 NGX hooks only. Retest only with a compatible direct-D3D11 consumer.
- D3D11 bridge: blocked pending a compatible contract-preserving runtime or an authorized bounded build; Feeder's image-derived contract is not Flycast provenance.
- Conditional D3D11On12/native D3D12: selected experimental consumer route; LOG #52-#59 prove stable queue-owned presentation, public-contract interception, repeated feature-18 evaluation, exact full-contract ON/OFF pairing, three-way pixel-hash differentiation, per-frame public-output sentinels, and same-frame Present. This completes Gate 10 for the named supplied-component route. FC-045's resize/fullscreen/device-loss/OIT/OSD/ImGui and production-performance matrix remains open and prevents a general production-readiness claim.

## Quality-phase gate mapping

The quality rebaseline does not replace existing FC identifiers. Detailed
acceptance is in `QUALITY-PLAN.md`.

- Gate 11: FC-024, FC-044, FC-060, FC-061 — depth polarity, clear semantics,
  OP/PT agreement, API parity, and NGX flag A/B.
- Gate 12: FC-032, FC-035, FC-036, FC-044 — analytic render-pixel motion and
  explicit sign/scale/jitter negative controls.
- Gate 13: FC-025, FC-031, FC-033, FC-036 — structural matching, generations,
  ambiguity, and confidence.
- Gate 14: FC-033, FC-034, FC-036 — reprojection/disocclusion protection.
- Gate 15A/15B: FC-025, FC-050, FC-055, FC-065 — transparency, OIT, modifier
  volumes, and byte-identical overlay composition.
- Gate 16: FC-044, FC-053, FC-065 — exact target resolution and preset lanes.
- Gate 17: FC-054, FC-065 — capture CLI, metrics, and moving title matrix.
- Gate 18: FC-045, FC-063, FC-064 — transitions, failures, performance, and
  long-run stability.
