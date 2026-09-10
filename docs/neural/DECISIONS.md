# Neural rendering decisions

## D-218: consistent sampling windows, redundant native work off the lane, cheaper observation records, three sources in flight

LOG790. (a) All remake CPU scopes sample only while the lane is active, so
stage numbers are comparable with the whole-frame scopes; the LOG789
sub-stage numbers came from the warmup feeds. (b) Work the remake lane cannot
use is skipped only under the lane's own condition
(`FLYCAST_REMAKE_ASYNC_NEURAL=1` with a channel): native draw
correspondence, which native and DLAA lanes still compute exactly as before.
(c) The wire, the digest, the anchor's arithmetic and every observation
record are unchanged in content; what changed is copies, allocations and
calls that could not alter state (staged serialization, raw mapped input,
in-place SQ
records in pooled batches, a compact store ring, per-register live bytes as
a recompiler gate, chunked anchor embedding with the first failure in packet
order). The anchor's rejections and the identity checks remain the evidence
that observation is unchanged. (d) Three sources in flight on the live
channel is a pipelining decision, taken because the feed-return round trip
exceeded two frame periods once the host reached the emulated frame period;
it adds no latency at the mean because it removes the repeats, and it is
bounded by the same age expiry. (e) `--hook-cycles` is a separate diagnostic
opt-in because its two time-stamp reads per hook call cost several
milliseconds per frame. Nothing about acceptance, native fallback, defaults,
or external configuration changes.

## D-217: attribute the whole frame before moving work; cut hook cost, not observation

LOG788 showed the consumer was not the limit and LOG789 showed the render
thread's remake scopes were not either: the emulation thread was, because the
source-observation recompiler hooks the anchored lane depends on cost about
8 ms per frame, and the two threads serialize on the one queued render. The
decisions: (a) whole-frame and emulation-thread scopes are part of the CPU
timing diagnostic, sampled only while the lane is active, so no stage is
optimized blind again; (b) evaluation resources persist across evaluations
(depth upload, raster targets in two sets, dynamic buffers) and one D3D11on12
acquire covers the upload, raster and copies; deferring output ownership was
measured and rejected (the wait moves, LOG789); (c) the observation hooks keep
observing exactly the same events with the same records, but skip work that
cannot change state: scalar-only reset of the store record, the derived-store
origin looked up in the single store call, early returns when no origin is
live, and an inline recompiler gate on a plain mirror of the live count so
those calls are not made at all; the anchor's rejections are the evidence
that observation is unchanged. Nothing about acceptance, identity, archives
or the consumer changes. The atomic counters that briefly measured the hooks
cost more than the hooks and were replaced by thread-local counters.

## D-216: a render-thread budget makes the slow lane fall back explicitly

The remaining remake work on the render thread (about13 ms, LOG786) is
device-bound and cannot be moved to a worker without a different evaluation
architecture. The600-frame gate's rule is that a slow lane must fall back
explicitly rather than slow emulation, so the lane now accepts an optional
budget (`FLYCAST_REMAKE_FRAME_BUDGET_MS`, launcher `--frame-budget-ms`): each
emulated frame adds that much credit, the scene feed runs whenever the credit
is not negative, both the feed and the evaluation it leads to are charged their
measured cost, and credit is clamped to plus or minus two frames' worth so an
idle stretch cannot flood the lane and a one-time stall (the first consumer
submit takes about a second) cannot starve it. The scene feed skips
(`frame-budget`) on the frames in between; a returned image is never deferred,
since it is the consumer's finished work and throttling the feed already bounds
how many arrive (gating both stages starved the evaluation, LOG787);
presentation repeats the last combined output as it already did. Nothing about acceptance, identity or
archives changes; the default stays unlimited, so every existing lane behaves
as before. The trade is stated, not hidden: at a budget the combined image
updates at the duty cycle budget over cost, not every frame.

## D-215: owned outputs from a ring; the consumer is timed, not assumed

The owned copy of an evaluated output is read by at most the next composite
and display, and copies are ordered on the immediate context, so a ring of
three owned textures replaces a texture per evaluation; the VRAM growth
recorded since LOG784 was those textures awaiting deferred destruction. The
helper reports its own per-image phases (draw, present, readback and lock
waits, turnaround) on every return line so the consumer's cost is measured
in evidence rather than inferred from the host's publish rate. Neither changes
any acceptance criterion or archive.

## D-214: the return worker receives; controls are measured, not remembered

The return receive (memcpy and digest of a2.4 MB image) was the largest
CPU-only cost left in the scene feed (LOG784). The return worker now receives
from the channel itself and prepares each image; the render thread applies the
identity gate when it takes the image, one at a time. Because the worker's
receive releases return credit early, the overlay ring is four slots by channel
sequence so a publish can never overwrite the overlay of an image not yet taken.
Every gate, log line and archive keeps its meaning. Separately, the600-frame
gate's control is now a measured run with the same harness settings on the
hooks-disabled host (native and DLAA lanes, LOG785); a remembered figure is not
a control.

## D-213: returned-image preparation and packet build off the render thread

After D-211 and D-212 the render thread still carried the packet build and the
CPU half of the returned-image evaluation (LOG783). Both move to workers with
explicit, never-waiting fallbacks: the feed worker builds the packet from
texture bytes the render thread staged by draw (device reads stay on the
render thread; an unstaged texture is an explicit packet skip), and a return
worker prepares the motion stream and neural input for each accepted returned
image. The prepared stream is used only when the history it was built against
is the accepted history at evaluation time; otherwise the render thread
rebuilds it. One prepared image is pending at a time so no unevaluated image is
overwritten; a busy return worker keeps the synchronous path for that image.
Locked replay and the remix-only comparison lane keep the synchronous path
because they substitute or bypass the live pixels. Every acceptance check,
identity gate, log line and archive keeps its meaning; what changed is where
the CPU work runs and that evaluation may follow receipt by one frame.

## D-212: textures by reference on the live wire, never in archives

Re-sending every texture with every packet cost12.6 MB per source on both
sides of the channel (LOG780..782). Packet wire version5 carries a per-mesh
texture carriage word: carried, registered (bytes travel, consumer remembers
them under the texture identity for this channel session) or referenced (no
bytes). The channel's strict sequence continuity guarantees a consumer never
misses a registration; a reference it cannot resolve, or a registration beyond
the shared bound, is a failed live source, not an evicted or guessed texture.
The host records a registration only after the feed worker reports Published
and clears the set with the channel. Saved capture archives and locked replay
never use version5, so existing locked-archive lineages and their digests are
unchanged and carried-only packets remain byte-identical. Texture bytes seen by
the external consumer are unchanged, so any hash-keyed replacement it applies
keeps applying. This changes no acceptance criterion.

## D-211: scene feed off the render thread with explicit fallback

The600-frame gate requires that a slow lane fall back explicitly rather than
slow emulation. The measured combined lane spent about37 ms per frame of
render-thread CPU in the scene feed (LOG780), mostly serialization, digest and
anchoring. The feed is therefore split: device-bound stages stay on the render
thread; anchor, temporal capture, serialization and digest run on one worker
with a single pending job, and a busy worker is a labeled native fallback for
that source. The worker owns the camera anchor so support lineage stays
sequential; results are drained on the render thread in source order and every
history retirement and log line keeps its previous meaning. The channel's host
bookkeeping is locked; the long serialization is not. This changes no
acceptance threshold, no digest contract and no archive; it changes where the
work runs. Remaining render-thread cost (returned-image evaluation, packet
build) and the external consumer's return rate are separate, recorded limits.

## D-210: bounded on-screen effect for vertices far outside the viewport

Sessions d and h left frames rejected by the exact projection guard only for
vertices thousands of viewports outside the image at the near plane (LOG770,
LOG776: expected coordinates such as -15898,-23370 viewports, raw errors 5..68
pixels, relative error below float precision). There a single depth rounding
step of the float world embedding scales the projection about the center by
tens of pixels while no visible pixel moves: every edge that can affect the
image passes within one viewport diagonal of the center, so a radial error of r
pixels at distance d moves the visible crossing point by at most r*diagonal/d.
The anchor therefore decomposes the remaining error in double into radial and
tangential parts for vertices at least four diagonals out and accepts the vertex
when the bounded effect and the tangential part both satisfy the unchanged 0.01
pixel guard; everything nearer keeps the exact guard, the depth tolerance is
unchanged, and each accepted frame records the count and the raw and bounded
maxima in its omissions and the observed-camera log. This bounds the
consequence on the image at the existing guard instead of relaxing the guard;
frames whose off-screen error is tangential or whose effect exceeds the guard
still reject with the decomposition in the reason. Verified in play (LOG778:
104 frames accepted with effect at most0.0014 pixel; the remaining rejections
are tangential float quantization of near-plane vertices and stay rejected).

## D-209: returned depth beyond the declared far plane is the far plane

The helper's D3D9 projection maps view depth z to f(z-n)/((f-n)z): exactly1 at
the declared far plane and up to f/(f-n) for points beyond it, which the
ray-traced runtime does not clip. On an open stage nearly every return carried
such values one rounding step above1 and the session died at the rejected-return
bound (LOG771); a closed stage then returned slightly negative values for points
between the camera and the near plane, which the runtime does not clip either
(LOG773). The host contract stays [0,1]. The helper labels values in
(1, f/(f-n)] as beyond the far plane and returns them as1, labels negative
values as before the near plane and returns them as0, and logs the counts and
measured extremes per return (`beyond_far_clamped`, `before_near_clamped`,
`above_limit`, `max_depth`, `min_depth`, `far_limit`); values above the limit,
which no point in front of the camera projects to, are left as measured and
remain rejected. Depth inside the clip range is never altered and raw depth
artifacts stay raw. This makes the projection's own semantic explicit; it is not
a relaxed acceptance, and the returned depth semantics of the runtime remain
unverified. Verified in play on the open stage (LOG776: measured maxima one to
three float steps above1, nothing above the limit, no rejected return).

## D-208: lineage-based support, in-place view cuts and resilient sessions

Manual play (LOG766-770) showed that the first-view support snapshot rejects
ordinary visibility drift inside one arena, that five seconds without a3D
source is too short for2D screens between rounds, and that one rejected return
must not end a session. Support lineage is therefore checked against the last
accepted set with unchanged thresholds, chained back to the fixed reference; a
large single-frame basis jump with continuing support retires histories only;
several rigid bases in one frame anchor on the dominant one with the rest labeled
as moving objects, while an even split stays ambiguous; session workers tolerate
60 seconds idle and survive a bounded number of rejected returns. None of this
recovers a world camera, relaxes the exact projection/depth guards, or promotes
manual sessions to performance evidence. Open: basis selection by point lineage
on stages with two large rigid groups, float embedding precision on far off-
screen vertices, returned depth one rounding step above1, and round-end overlay
protection.

## D-207: genuine source-view cuts re-anchor in-session, never by lowering support

The source3099 boundary is a measured cut: 81 degrees and13.5 anchored units in
one frame against a smooth-motion maximum of1.62 degrees/0.741, with shared
support falling to42 percent against a74 percent floor (LOG760). The shared-
support rule, projection and depth guards stay unchanged. In a managed session a
support rejection now retires the fixed view, temporal/raster history, pending
returns and presentation carry-over in-session and keeps the channel and helper.
The next accepted source starts a labeled anchor generation whose diagnostic
origin is the retired view's camera-relative position of that source: a distinct
label that fails strict continuity for every consumer, not a world relation.
Consumers treat it as an explicit generation change: correspondence resets and
the anchored light re-fixes exactly as a fresh session would. World-consistent
continuity across same-arena cuts remains deferred to a supported camera/world
contract (M2-camera); the diagnostic embedding label is preserved. Fresh helper
generations remain for renderer restarts and epoch changes.

## D-206: integration priority and explicit user pause

The current300-frame source/combined review closes only its recorded comparison
scope. Remaining matrix expansion is deferred behind live camera/scene continuity,
not removed from final acceptance. Investigate the source3099 support boundary;
preserve reset for genuine cuts and require source evidence before continuity.
Do not weaken point-support/projection guards merely to avoid session renewal.
The subsequent user pause takes precedence over autonomous progression. The
development handoff preserves work and next steps; documentation is not resume
authorization. Backlog remains the single queue and full goal remains unfinished.

## D-205: separate returned-scene comparison stages and owned test lifetime

Capture-only Remix-only mode skips neural submit/history advance but reuses owned
native effects and late HUD composition. Returned-DLAA is distinct from native-PVR
DLAA and requires fresh hooks-disabled host evidence. Compare frozen returned
color/depth and effects before attributing output differences to processing;
identical geometry alone is insufficient. Keep unmatched startup frames and
scope limits; no still or single metric declares a visual winner.

The performance harness assigns its suspended emulator child to a noninherited
kill-on-close job before execution. A failed assignment fails launch; abrupt
harness exit must not orphan its emulator. This owns only the launched process
tree, never arbitrary processes discovered by executable name. Depth validation
retains strict acceptance while reporting the particular failed component.

## D-204: managed workers outlive diagnostic batches

Managed returned-scene workers use explicit session-worker mode with a finite
10000-frame ceiling, unchanged watchdogs and channel-close ownership. Ordinary
diagnostic commands retain their existing limits. Do not restart live rendering
just because a600-return diagnostic batch ended. Capture session tokens join to
actual helper receipts; a single-generation continuity check cannot substitute
for a cross-generation recovery check. Preserve both scopes and failed gaps.

## D-203: managed sessions allocate fresh ownership, never reclaim receipts

Explicit --managed-session enables a launcher-owned16-byte control mapping.
Validate owner liveness/version, allocate at most eight atomic generations, and
derive fresh channel names. Renderer resource retirement, epoch change, closed
helper or exact anchor-support rejection resets retained scene/history before
new publication. Unexpected helper exits abort supervision; raw superseded exits
remain in evidence. A claimed publisher closes its channel promptly; failed
duplicate opens have no close authority. Whole-run timeouts remain failures.
This is bounded lifecycle support, not general camera truth or seamless visual
recovery. Verify images/history identity across generations before promotion.

## D-202: report stopped sessions without reclaiming their channel

Live UI reports requested/open/stopped separately from external presentation.
Renderer restart may retire the current single-publisher token; a claimed-token
open must report relaunch-required rather than silently recycle receipts. The
next lifecycle integration must negotiate a fresh token and reset both owners'
history. Current helper source timeout remains failed continuation, even when
Flycast completes its native-fallback run. Developer JSON markers use the classic
locale; formatted thousands separators are not valid JSON evidence.

## D-201: separate authored light lifetime from material rebuilds

The live helper previously selected camera.forward when material resources were
rebuilt. Add an explicit --scene-light-anchor harness option that retains the
initial direction across rebuilds and rejects changed coordinate origin, epoch,
game or unsupported scope on every packet. Keep current default unchanged.
Observed arena-coordinate stability and live fixed-direction delivery support
this diagnostic control, not recovered game lighting or general camera truth.
Neither brighter output nor pixel differences promote a visual winner. Preserve
native effects/HUD and leave external consumer settings untouched.

## D-200: const wire output and integration-first continuation

Write validated packets without copying owned texture payloads. Preserve wire
versions and retain the old writer solely as a harness oracle, including actual
saved-packet comparison. Optional CPU elapsed scopes include driver waits and
must not be labeled GPU timings. The measured improvement does not establish
pipeline speed acceptance. Close this bounded optimization and prioritize the
usable camera/scene contract; exact projected pixels alone do not prove stable
world coordinates or separation of camera and object motion. Preserve diagnostic
labels and the parked strict-source failure until independent evidence warrants
a stronger contract.

## D-199: optimize observer invalidation without weakening source authority

The observer-only native control reproduces the combined path's slowdown.
Disengaging an arithmetic-origin optional and clearing its epoch/value avoids
unnecessary inactive matrix payload writes while retaining live-count decrement,
epoch invalidation and exact-value lookup rules. Two isolated candidate runs
improve total frame interval; this is not direct function profiling or proof of
combined performance. Require exact source geometry/material/native preservation
before accepting the optimization. Do not drop observation or fabricate camera
truth to meet timing. Forced process termination is a failed shutdown regardless
of completed samples; harness exit status must agree with its recorded close flag.

## D-198: use owned current upload bytes before asynchronous material readback

For explicitly enabled async-neural gameplay, retain complete decoded CPU texture
uploads and encode their DDS once. Charge raw bytes plus encoded bytes against
a process-wide64MiB bound. Retain the GPU resource identity and require matching
upload/RTT revisions and resource layout at consumption; clear on new upload or
deletion. Native CPU bytes remain owned, never borrowed after upload. Unqualified GPU palettes,
generated mipmaps and unavailable/unsupported copies retain existing bounded
readback behavior. This neither reuses stale textures nor changes scene geometry,
GPU waits or rendering defaults. Qualified GPU palettes use copied indices and
the actual palette-upload snapshot plus per-bank generation checks (LOG656);
unqualified palettes retain staging. Validate identical GPU-readback material bytes
and real-game handoff before claiming delivery improvement.

## D-197: longer image capture is an explicit diagnostic process budget

The standalone async-return helper may opt into a fixed300-second watchdog
using a final --diagnostic-capture-budget flag. Default30/120-second budgets,
source-wait bounds and production rendering policies remain unchanged. Log
the override and performance exclusion. The300-frame moving review retains
source gaps and failed HUD frames; neither capture completion nor composition
checks establish continuous gameplay, visual quality or external provenance.

## D-196: use a same-frame HUD cohort instead of repeated absolute-depth tuning

The longer combat replay falsifies sufficiency of D-195's sampled floors. A
title-only certificate identifies the known header, timer and both health-bar
sides using the existing atlas/layout/state/shape rules, and requires their
foreground-equivalent depths to agree within2percent. The observed plate depth
is.9 of foreground. Normalize a copied title-classifier record to.18 only when
all four roles agree. Do not alter real depth, geometry, motion or generic HUD
classification. Missing/incoherent cohorts retain strict prior behavior; never
reuse a previous frame's mask. This is a title-specific rendering relationship,
not recovered physical depth. Live broader pixel validation is required before
acceptance and remains separate from the overall working-pipeline gate.

## D-195: HUD safety needs nonempty classification plus pixel equality

Zero mismatches with an empty HUD mask do not demonstrate preservation. LOG644
finds visible native text passed through the scene after the title classifier
rejects captured low-depth HUD layers. LOG646 adds failing real draw fixtures;
the correction extends only the known Soulcalibur atlas floors to observed
combat depths, retaining world/texture/region/list/RTT negatives. LOG647 verifies
12 nonempty exact native-overlay compositions and completed Presents. This is
bounded title evidence, not a universal depth envelope. Future moving checks
must surface zero protection when source HUD remains visible, not average it away.

## D-194: authored scene lighting is separate from neural intensity

The existing legacy scene uploader's fixed neutral headlight at radiance3 is
now a bounded standalone-helper option, default unchanged. Keep source vertex
colors/textures, geometry, camera, effects and external settings unchanged when
testing it. The2372 moving capture is already substantially brighter upstream
of neural evaluation; this motivates a controlled scene-light experiment, not
a claim that radiance alone explains the difference. Log the authored value,
reject invalid/unsupported use before loading a runtime, and never call it
recovered game lighting or a DLSS control. No automatic default promotion.

## D-193: CPU delivery records do not depend on GPU query availability

Create ordered per-frame metadata at EndFrame regardless of query-ring capacity.
Completed Present updates that record; asynchronous query resolution updates
only its timing fields by sequence. Ring-busy/disjoint samples retain cadence,
source/reset/resource data with null GPU durations, excluded from GPU percentiles.
Never wait for queries to obtain a cadence record. LOG640 verifies617 exact raw/
report Present joins under616 ring-busy frames. GPU availability itself remains
an integration issue, not solved by retaining metadata.

## D-192: refine float representation without changing the camera contract

For a rounded embedded vertex that fails existing projection/clip/depth checks,
search bounded neighboring floats and the same original view ray inside the
already permitted depth tolerance. Recheck the unchanged0.01px and depth bounds
before accepting. Keep initially valid vertices unchanged. No recovered world
semantics, lens adjustment or relaxed rejection follows from this numerical
correction. LOG636 records a deterministic pre-fix failure and intermediate
failures; sustained live acceptance and cost remain pending.

## D-191: accepted public evaluation and external presentation are separate gates

Retain returned-scene geometry/depth only after successful stage submission, as
before. External readiness must gate output wrapping/presentation, not erase
the source history of an already successful public NGX evaluation. LOG631's OFF
control exposed repeated resets and false zero-motion inputs from that coupling.
Move readiness rejection below source-history retention; failed/busy submission
still cannot advance history, and no external-ready claim follows from public
success. This also keeps exact-input OFF controls comparable with ON history.
Existing reset/device/game boundaries remain. Validate the changed branch live
and retain the mismatched control rather than weakening provenance checks.

## D-190: returned shading consistency is an opt-in reconstruction experiment

LOG620 isolates detail loss in the public reconstruction lane as well as the
combined route. Depth and draw identity cannot certify stable returned lighting.
FLYCAST_REMAKE_COLOR_CONSISTENCY=1 therefore tests bilinear previous-source RGB
against current returned RGB after geometric checks, using the last accepted
evaluation only. A provisional per-channel 8/255 SDR threshold rejects history
with bias-current one (reason8). LOG623 rejects the first experiment which also
zeroed motion/confidence: changed shading must retain already-validated geometry
motion/confidence, unlike failed depth/identity correspondence. Alpha is ignored;
native effects/HUD are composed later and are not removed. This is neither
semantic art-direction masking nor optical flow. Threshold and visual benefit
remain unproven until matched moving comparisons; it is off by default. Missing
or malformed color inputs reject the requested raster experiment. Two bounded
color uploads add cost; synchronous fixture results cannot establish performance.

## D-189: temporal comparison freezes upstream inputs and evaluation history start

Identical game geometry/source frames do not imply identical Remix returned
color/depth: ci/cj falsified that assumption on all30 captures. Freeze one
bounded archive and replay its color/depth through both reconstruction lanes.
Retain existing exact scene/effect checks and require original/current source
frame equality for temporal replay; do not inherit image-only frame remapping.
The capture-only comparison boundary must actually produce the same accepted
source sequence with reset at its start. Temporal preparation permits locked
input only in bounded comparison/effect-identity scope. Ordinary rendering is
unchanged when the request is absent. Capture two warm-up sources before the
first displayed comparison. Metrics use pre-native-effect neural output; final
composition and protected pixels are independently checked against original
source ownership. Frozen source equality is not identical guidance: history and
motion intentionally differ between reset-only and temporal lanes. A valid
method comparison does not itself prove external-consumer mutation or a winner.
LOG616's28-frame comparison reduces temporal change while increasing source and
gradient error; retain that mixed result, do not promote the mode by delta alone.

## D-188: depth consistency uses the triangle's bounded sampling footprint

Strict center-depth comparison rejected70..87 percent of pixels in cf despite
only about2.3 percent uncovered raster geometry. A fitted global camera offset
did not explain the error and was rejected. Compare returned depth against the
triangle's local projection-depth plane over a one-render-pixel footprint, with
all3x3 neighboring depths required to follow that plane. This is an experimental
sampling-uncertainty bound, not a claim about undocumented consumer jitter.
Do not increase the global view-depth tolerance. For previous-frame checks,
transform the depth slope through the previous-position Jacobian, account for
the sampled pixel center and require every neighboring draw ID to match the
assigned previous draw. Singular mappings, crossings, missing IDs and isolated
thin surfaces remain reactive. All derivatives precede divergent rejection;
avoid the redundant early center-ID return that failed the neighboring-ID
fixture. Keep all falsifying shader/fixture attempts in LOG609-610.
Current-footprint cg increases trusted coverage to4.4..8.7 percent; this does not
prove image quality. Acceptance of a visual improvement requires the moving
comparison and changed-guidance provenance, not more trusted pixels alone.

Retained-view device validation compares canonical identities of resource-owner
devices on both sides. A host-wrapped creation interface is not comparable by
raw pointer to a view's GetDevice result. cd failed under that incorrect check;
ce fixes it and both GPU surfaces still reject a genuinely foreign-device view.

## D-187: returned geometry drives isolated temporal guidance

Reuse minimum-cost draw assignment with exact full-generation/topology/UV/color
rechecks; ambiguous, alpha-tested/blended and changed-source draws are reactive.
Ordinal is not identity. Raster current and homogeneous previous positions from
the owned camera/geometry stream, then divide at the pixel. Require agreement
with returned current depth, previous accepted depth and previous accepted draw
identity before exporting previous-minus-current render-pixel motion. This is
explicit projected-depth experimental guidance, not recovered physical world
truth. Reject missing/outside/incorrect previous samples with zero motion and
full current-color bias. Suppress numerical noise below1/4096 pixel only.
The additional TEMPORAL_RASTER developer switch is off by default. Its deferred
pass restores caller state and owns bounded output resources; only accepted
evaluation advances draw-ID ownership alongside the existing scene/depth owner.
Failure/busy retains the previous owner, and renderer/channel reset clears both.
First-frame/missing history resets neural evaluation. No runtime configuration,
public feature parameter, jitter contract or production default is changed.
LOG601 GPU controls pass both D3D11 surfaces; live coverage, visual improvement,
changed-guidance provenance and resource/performance acceptance remain open.

## D-186: returned temporal reference belongs to successful evaluation

The explicit TEMPORAL_PREPARE mode retains geometry/camera/UV/color/topology and
full texture/palette/RTT generations with each published return receipt, without
copying texture payloads. It requires anchored live neural input and rejects
locked-image replay. Validate receipt, producer, frame and camera clips before
using a returned scene; retain its geometry and depth together only after
successful neural submission and experimental-contract acceptance. Failed,
busy, duplicate or mismatched results never advance the reference. Reset clears
it with channel/renderer ownership. Publication anchor state remains separate.
Reference compatibility is not motion trust or permission to enable history.
LOG596-597 exercise unit ownership controls and58 actual evaluation-reference
joins; reset/zero-motion/full-bias still applies until GPU guidance is proven.

## D-185: observed camera anchor is fixed, publication-owned and explicitly scoped

FLYCAST_REMAKE_CAMERA_ANCHOR=1 uses the common top-three transform of already
validated exported source vertices, unit input W and persistent input-point
support. Keep the first published source-view anchor fixed; later source matrices
produce relative camera motion. Commit proposed anchor state only with successful
publication, and clear it with renderer/token reset. It is not accepted neural
history, physical world recovery or arbitrary first-draw camera selection.
Use true inverses of observed and published bases, accounting for published float
position. Represent near/far intersections along their original ray with a bounded
rounding correction; never widen clip planes or weaken the pixel/depth guard.
LOG586-594 retain source/negative tests and failed numerical attempts; final bz
passes62 publications without anchor rejection and three original-HUD Presents.
The next integration dependency is returned-scene geometry-derived temporal
guidance. Keep reset/zero-motion/full-bias default until that contract is tested.

## D-184: live camera transport preserves a fixed diagnostic anchor

Wire version4 carries supplied position, proper orthonormal basis and sequence
origin only under diagnostic-camera-embedded-anchor-not-world-reconstruction.
Versions1..3 retain their identity-camera bytes. Never silently recenter each
frame, overwrite a supplied pose with identity, or infer a world-space claim
from successful serialization. Existing adapter validation rejects invalid bases;
deserialization remains atomic. LOG584-585 prove transport/projection controls,
not producer camera recovery. A live anchored producer must bind its fixed
reference to source continuity and reset/reject incompatible source evidence.

## D-183: promoted alpha materials have one source-qualified owner

The explicit alpha-combined experiment exports supported primary source-alpha /
inverse-source-alpha surfaces with original texture/vertex alpha and disabled
depth writes. Wire version3 carries this state; older opaque/cutout versions
remain readable. The public SDK adapter rejects this material until implemented.
Only successfully exported source draws may be excluded from native effect replay.
Their receipt owns exact producer, list ordinal and packed native parameters;
validate all before cloning replay parameters and replacing blend with ZERO/ONE.
Never mutate the native fragment stack or infer material identity from ordinal
alone. Additive weapon effects remain native. LOG576-582 record GPU controls,
live relit alpha geometry, exact HUD and changed-path external-output evidence.
This is an opt-in integration slice, not recovered camera/world or performance
acceptance. Reuse its proof unless a subsequent change affects the contract.

## D-182: protected HUD does not belong in the ray-traced scene

LOG569's exact-source omission/raster controls identify the first two live
cutout meshes as HUD, not world geometry. Reuse the existing native overlay
classifier and concatenated source ordinal to exclude protected draws before
world export, only under the same policy that restores native HUD afterward.
Do not invent a rectangle classifier or promote cutout transport to demonstrated
world completeness. Palette/alpha support remains experimental; world-cutout
acceptance requires actual world content. LOG570 verifies native HUD restoration.

## D-181: palette-bank identity belongs to the draw

Native GPU-paletted texture-cache keys intentionally exclude palette selection.
The shared texture owns index pixels, not a unique palette bank or current bank
generation. Snapshot and revalidation therefore use the draw TCW's bank and
native pal_hash generation; CPU-expanded textures retain cached palette identity.
Permit only the palette-selection difference in GPU index-resource TCW checks,
retaining address/format and all other bits. Copy index/palette resources together
without waits, cache their exact bank/generations, and expand through the existing
decoder. Missing palette state fails closed. LOG567/568 retain live failures and
test evidence; this design alone is not gameplay acceptance.

## D-180: explicit cutout state cannot silently become opaque

Carry an optional source alpha reference, retaining enabled threshold0 separately
from no alpha test. Emit view-wire version2 only for packets containing cutouts,
keeping opaque version1 archives compatible. Until each consumer implements and
proves the corresponding alpha behavior, reject these meshes rather than using
its opaque defaults. LOG565 is schema groundwork, not permission to include
unvalidated punch-through geometry or claim a recovered camera.

## D-179: reuse the accepted post-effect provenance checkpoint

LOG564 traces28 consecutive exact-input external results through source-owned
native OIT effects, original HUD and completed Present, with marked/clean/OFF
controls. This closes the regression introduced by effect composition, not the
whole remake pipeline or a quality/performance gate. Resume scene/camera and
relighting integration; require another provenance regression only when a later
change affects the demonstrated ownership/output path. Keep intentional native
trails, the hybrid/translucency scope and approximation labels explicit.

## D-178: initialize uploaded constants; retain strict effect replay equality

The first live effect identity failure was canonical word28, an unused but
uninitialized dither field. Initialize the entire shared pixel constant record
and its explicit96-byte aligned tail, rather than normalizing away mismatches.
LOG563 verifies native pixel parity and full identity equality across three
source-matched live frames. Optional synchronous effect archives/replay remain
bounded at30 attempts and require the existing scene/input checks first; missing
or altered effect evidence rejects. One completed evaluated Present is a replay
integration check, not replacement for marked/clean/OFF external-output proof.

## D-177: snapshot identity readback is explicit evidence work

Read retained GPU data only through the explicit synchronous evidence API;
ordinary capture/resolve must not call it. Preserve actual compiled layer count,
selected effect resolver variant and exact constant words. LOG561 tests extraction
against uploaded fixture truth on both D3D11 surfaces. The initial diagnostic
implementation stages the existing bounded allocation and is deliberately not a
performance optimization. Evidence-file/replay linkage is the next dependency;
no live locked-effects guard is removed by this component alone.

## D-176: effect replay identity describes reachable ordered fragments

An opaque scene receipt cannot identify the separate retained native effect
stack. Compare reachable fragment semantics and referenced polygon state under
the native stable ordering, together with source and exact resolver state.
Unreachable storage and allocation addresses are not identity. Equal-key blend
order remains significant. Malformed or truncated descriptions fail closed.
LOG560 tests the CPU description component; GPU extraction and replay enforcement
are still pending, so the existing locked-effects rejection remains in place.

## D-175: effect ownership accounting is allocation-based

Count each retained effect snapshot once across renderer aliases, and distinguish
logical copied bytes from physical GPU allocation size. Six owned data objects
exclude borrowed resolver shaders. The startup appearance of one snapshot is
expected bounded growth, not evidence for or against long-run leakage. LOG558
adds the missing snapshot counts without claiming the rest of the experimental
remake resource inventory is complete. Size alone does not establish GPU cost.

## D-174: preserve ordered native effects rather than flattening translucency

The OIT resolver uses all64 blend pairs, destination alpha/color, a secondary
accumulation buffer and saturation after each fragment. Reuse its blend kernel
for effect preservation; a single alpha layer or native-minus-opaque residual
cannot represent this contract. The production kernel is GPU-tested against
independent coefficient tables on both D3D11 surfaces. Delayed ownership,
occlusion, multipass and actual moving composition remain required before
declaring effects integrated. No current-frame effect data may be attached to
an older returned scene. LOG554 records failures and bounded kernel evidence.
LOG555 implements the first copy-based, off-by-default single-pass OIT path:
source receipt owns fragment/pointer/parameter/constants; the original resolver
is replayed over evaluated color on a state-restoring deferred command list.
Shaders are selected from the owning native renderer; copied resources and the
factory/context remain device-validated. The installed shader interceptor's
GetDevice identity must not be mistaken for copied-resource ownership. Different
real devices still fail the GPU fixture. Preserve the pre-effect neural color in
the next evidence slice; a combined image alone cannot isolate the contribution.

## D-173: captured moving HUD envelope and depleted plate

Known Soulcalibur HUD atlas paths admit depth up to .24 after the moving
capture demonstrated .230914, retaining lower/finite/list/order/region and
shape checks. Atlas801607344 is admitted only in the two measured planar
health-plate rectangles. LOG547-551 retain failed controls and the exact2192
comparison with restored HUD. This is measured profile coverage, not general
depth-independent HUD recognition. Additive HUD effects and world translucency
remain open. The implementation is fork-verified at3f3600b44.

## D-172: explicitly gated OIT return and monotonic HUD coverage

The returned-scene path is permitted in OIT only with exact developer opt-in
ASYNC_OIT=1, after native OIT resolve and guidance export. Preserve original
receipt-owned native color/mask, accepted-output ownership and existing stale
fallback. Reactive coverage uses MAX union on only mask/overlay targets so
later non-HUD fragments cannot erase prior protection. The captured Soulcalibur
name atlas alone admits the measured lower animated depth envelope; other
classification guards remain unchanged. LOG537/541 cover GPU/negative/visual
checks; LOG542 covers600-source delivery and LOG544 covers25 exact-input
external-output/display joins. None establishes full camera/scene, timing,
cleanup or title-matrix acceptance. Missing external readiness must withhold
experimental display, including in the OFF negative-control run.

## D-171: clip estimated crossing geometry after primitive assembly

Keep finite projected-depth estimates outside the supplied near/far planes
until the existing bounded triangle clipper. Per-vertex enclosure rejection
before assembly discarded whole draws, including visible arena floor. The
transport still enforces .1..2501 and vertex limits; observed-only defaults,
native PVR depth and the explicit camera-relative approximation remain unchanged.
LOG538-540 provide failing controls, repaired conversion and same-frame floor
evidence. This does not establish recovered world/camera or full-scene parity.

## D-170: pair return capacity with bounded source credits

Shared-memory version4 has two paired color/depth image slots for the existing
two outstanding source credits. Slots are selected by sequence parity and read
oldest-ready-first; all receipt, depth, duplicate, stale and close checks stay
intact. No unbounded queue or stale-age relaxation is introduced. Both sides
must use the same layout version. LOG536's600-source run publishes every return
without busy drops; this is delivery evidence, not GPU-only performance or
external pixel provenance for the entire sequence.

## D-169: clip crossing triangles without weakening the scene contract

LOG534 shows the live publication gap is the supplied enclosure validator,
not primarily transport throughput. Clip expanded camera-relative triangles
against the unchanged near/far planes before strict adapter validation. Retain
flat normals, linearly interpolate UV and packed color/alpha, preserve inside
ordering and original PVR vertices, and bound generated output. Fully outside
or nonfinite scenes still reject. This does not recover camera truth, implement
PVR tile clipping, or justify changing source depth semantics. The existing
eight-frame presentation expiry remains intact. Opt-in skip diagnostics identify
future refusal stages without changing readiness or permitting silent reentry.

## D-168: returned-scene evaluation keeps original presentation identity

The explicit async neural experiment reserves stage submissions for returned
scenes, not alternating current native PVR and delayed Remix inputs. Submit
fresh source IDs once with reset-only/full-bias, zero-motion, zero-jitter inputs
using returned projection depth; never advance native correspondence from them.
Copy accepted output to an owned texture while wrapped output is acquired, then
retain original receipt-qualified HUD ownership. Composite/display that pair
under the existing bounded monotonic policy. Count remake-evaluated separately
from raw Remix and ordinary neural Presents. This establishes neither external
mutation nor temporal-quality acceptance; the combined route still needs its
own source/output/provenance evidence. Diagnostic captures save evaluated color
separately and compare world pixels to it, not to raw Remix input.
LOG532 k/l/m now supplies the focused proof for ten consecutive retained-input
displayed frames, not continuous live delivery or temporal reconstruction.
Keep reset-only guidance and eight-frame expiry explicit; the next integration
work must address sustained delivery and OIT rather than another generic
provenance cycle. Intentional source trails are content, not automatic defects.

## D-167: distinguish returned preview from neural presentation

The opt-in ordinary returned-image preview composites receipt-owned original
HUD surfaces before display. A short native hold aligns initial source time;
displayed returned IDs never decrease. Eight-frame expiry latches fallback
until explicit reset/disable. Raw Remix, held native and public neural output
have separate presentation accounting. Running native NGX in the background
does not prove the returned image was evaluated or displayed through DLSS 5.
This is pending live validation, not combined-pipeline acceptance.

## D-166: delayed images own their original native color and overlay mask

Before publishing an ordinary scene, GPU-copy its native framebuffer and R8
overlay mask to private shader-readable textures. Attach the issued receipt
only after publication succeeds. Two pending snapshot slots mirror source
receipt ownership; one separate accepted snapshot moves with the validated
returned image. Exact receipt/frame/producer/epoch/age match is mandatory.
Expired, reset or closed sessions release snapshots, never substitute the
current frame's HUD. Return-credit preflight avoids copies when either pending
return ownership or transport slots are full. A provisional candidate exists
only within publication; bounded snapshot ownership is not yet full VRAM/leak
measurement. Copy commands use normal wrapped-input acquire/release ordering
and never CPU readback/wait. Native color SRV remains BGRA UNORM, not sRGB.
No display override or neural-history advancement is introduced by retention.

## D-165: opt-in ordinary scene feed is separate from returned presentation

FLYCAST_REMAKE_ASYNC_CHANNEL enables source stamping and DX11 scene publication
without NeuralCaptureFrames. Complete ready material sets are required; missing
textures skip the whole current scene while all required copies are prewarmed.
No capture writer or synchronous wait participates. Return credits and8-frame
expiration bound delayed source ownership; at most one validated returned pair
is retained under its original identity. It is not composited or passed to NGX
with current-frame HUD/motion. Epoch or renderer-resource change closes this
session and requires a fresh consumer token. Ordinary native/public presentation
is preserved until returned-frame and overlay ownership is implemented.

The helper's explicit --live-channel-async accepts forward frame/producer gaps
under the same game/build/epoch/origin. Existing --live-channel and diagnostic
continuity remain strict. A gap recreates uploader resources and invalidates our
correspondence; this is NOT proof of resetting the external Remix runtime's
temporal accumulation. The inspected public camera struct has no reset field.
Do not invent a parameter or label gapped output temporally correct. This is
integration evidence only; returned-scene temporal guidance remains open.

## D-164: own asynchronous texture copies by resource and generation

MaterialReadback queues one staging copy/event on the owning immediate D3D11
context. Poll uses event DONOTFLUSH and Map DO_NOT_WAIT; pending never publishes
partial pixels. The source resource/context are AddRef-owned through retirement,
with exact upload/RTT/palette identity. Mismatch/error/reset retires the ticket.
RemakeTextureCache limits ownership to128 entries and64MiB of aggregate pending
raw/ready DDS payload, expires unused entries after120 render frames and clears
on epoch/backward-frame/context changes. This is a payload bound, not a measured
VRAM or total allocator ceiling; source GPU references and transient conversion
buffers also exist. Resource references prevent address reuse from fabricating
a hit. Cache entries never substitute older generations while a copy is pending.
The caller remains responsible for authoritative generation and frame stamps.
These render-thread-only helpers do not flush, sleep or spin; bounded GPU waits
exist only in the fixture. ReadRemakeViewTexture optionally uses this cache,
retaining synchronous capture as its default. Ordinary-feed integration is next;
the cache alone is not frame delivery, performance or neural-history acceptance.

## D-163: consumed transport slots do not retire returned-image ownership

The source transport and returned-image ownership have different lifetimes.
PublishForReturn reserves the existing two-record source ledger until a valid
image is received or the caller explicitly expires ownership by source age or
epoch. Busy does not publish, advance sequence or wait. Expired replies fail
without changing the caller's owned image. Expiration is not neural evaluation,
history advancement or permission to label delayed output as the current frame.
Legacy one-way Publish remains available for source-only diagnostics. The live
capture exchange now requests return credits. Ordinary integration must poll
and expire on its own bounded frame policy and restart the channel on epoch
change; this API alone does not supply async textures, HUD ownership or pacing.

## D-162: lock returned pixels only after exact transported-scene qualification

The explicit capture-only locked-input root is not a live output provider.
It requires one unique retained packet with matching game epoch/ordinal/cycle
and full serialized scene content; only renderer counter and source build label
are normalized for comparison. Original receipt, original live-input manifest,
and converted RGBA/inverted-depth hashes must also agree. Replay-of-replay,
corrupt/truncated inputs and source differences reject without changing output.
Hashes use classic locale regardless of Flycast's user locale. Receipts identify
locked replay and original frame; accepted metadata never calls it fresh output.
--start-producer provides an ordinal threshold while exact epoch/cycle/scene
checks remain mandatory. It does not replace conservative bypass or prove that
ordinals alone identify matching content. Existing exact hash, marker, Present
and host controls remain unchanged; requested-native-fallback is now explicitly
ineligible for promotion. This diagnostic supports downstream exact-input provenance,
not ordinary async gameplay or temporal-quality acceptance.

## D-161: submit producer GPU work before a synchronous cross-process wait

The explicit capture/channel path flushes its D3D11 context after constructing
the owned scene packet and before publication/optional developer wait. This
submits pending work; it is not a completion fence, a neural acceptance signal
or Present proof. Ordinary disabled gameplay never enters this path.
LOG525's live external-consumer run passes the returned input/composition and
marker checks after this change; prior missing/empty-return failures remain.
Retained-packet replay succeeds even with no-activation and a running external
host, narrowing but not proving the underlying runtime cause. An explicit
no-activation diagnostic switch preserves this reproducible isolation control.
Retained scene replay has differing final-color hashes, so combined provenance
must lock actual returned input images, not assume packets guarantee identical
path-traced pixels. Such replay stays labeled diagnostic and cannot replace
the live/asynchronous gameplay requirements.

## D-160: absolute capture scheduling aligns controls without disabling bypass

Public/native capture skip counts eligible capture callbacks. Experimental
menu/2D bypass excludes some callbacks, while evidence markers use renderer
frame IDs. Therefore the same skip count does not specify the same source
interval across modes. An explicit --start-frame config selects a positive
renderer-ID threshold, separate from default skip counting. The writer observes
the next instrumentation ID before capture-start reset and GPU timing; Capture
also checks actual metadata ID. Unchanged configuration cannot rearm reset.
This does not force ineligible frames through generative processing and does
not weaken exact input/provenance comparison. No production pacing changes.

## D-159: connect returned inputs without borrowing native temporal truth

The capture-only test exchanges before TrySubmit, uploads matched returned color
and inverted normalized projection depth to the existing ring, and uses existing
D3D11On12 acquire/release ownership. A temporary initialized texture copies depth;
UpdateSubresource is not used on a depth-stencil resource. Ordinary gameplay and
missing/unsupported returns retain native input. Native motion, draw identity and
prior depth do not automatically describe returned color. This first connection
test therefore resets every frame, exports zero motion/confidence/identity and
full bias-current-color, clears correspondence diagnostics, and invalidates native
history after accepted evaluation. It is not a replacement for geometry-derived
returned-surface temporal guidance or an ordinary quality mode. LOG523 confirms
actual public evaluation and protected final composition on nativeD3D11 and
D3D11On12. External neural provenance and normal pacing remain separate. Do not
interpret public bias-current-color as external semantic art-direction masking.

## D-158: paired returned guidance owns the same publication boundary as color

The optional projection-depth return is published only after both public
readbacks succeed. Source receipt, frame, producer and projection clips must
agree; fixed extent, finite normalized range and separate corruption digests
are checked before receiving ownership. Invalid depth must never silently
degrade a requested pair into color-only output. Explicit color-only diagnostic
callers remain supported and do not acquire neural-guidance authority.
LOG522 verifies exact paired bytes on three live combat frames. This is a
dependency for submission, not proof of compatible motion, depth conversion,
asynchronous pacing or combined neural presentation.

## D-157: capture returned-scene depth before attaching native guidance to new color

The public CopyRenderingOutput DEPTH enum alone does not specify numerical
semantics. A new bounded legacy memory-depth option captures typedRGBA32F from
the same completed Remix frame as final color, without binary inspection or
configuration changes. It preserves raw channels and source frame/sequence.
The inspector compares both a linear-view-Z hypothesis and the exact supplied
D3D9 projection, using emitted opaque draw overlap; it never fits corrections
or declares guidance accepted merely because normalized errors are small.

Actual depth-a values lie near1 and strongly reject linear view distance.
They are consistent with the supplied normal-direction projection depth;
reversed normalized depth is a failing analytic control. Compressed far depth
can conceal large view-space differences. Clear/no-geometry, edge/disocclusion,
motion and missing-surface semantics still need the changed-route contract.
Next return matched color/depth together and validate the explicit projection
conversion/polarity before neural submission; do not relabel native guidance
or close Gate16/17/combined provenance on this readback alone.

## D-156: estimated view-space coverage is an explicit alternate experiment

FLYCAST_REMAKE_ESTIMATE_UNTRACED=1 opts into mixed observed/projected-depth
camera-relative positions. Untraced finite positive-depth PVR vertices are
embedded using the existing measured focal lengths and .95 reciprocal-depth
scale, inside the existing supplied diagnostic enclosure. At least three
current observed anchors are required. Estimated vertices carry no transform
serial/W claim, and the serialized scope explicitly names the approximation.
The count records converted candidate vertices, including candidates not used
by emitted opaque meshes; it is not a coverage percentage. Original attributes,
default observed-only behavior, bounds and unsupported draw exclusions remain.

This route is not recovered world geometry, camera truth or completion of M2.
It is the already permitted labeled view-space experiment used to test full
visible combat integration instead of repeatedly tracing one missing instruction.
Actual est-a expands19-21 observed opaque draws to40 and reveals both fighters
and the arena floor. It does not recover culled geometry, physical normals,
transparent effects, game lights or world-space temporal identity. No automatic
selection/presentation follows; moving quality and combined-route gates remain.

## D-155: source exchange precedes composition, archival does not own delivery

Extracted source exchange from capture-file writing. On the explicit native
capture/channel route, displayFramebuffer prepares an owned source snapshot,
current textures and frame-qualified return before the composite starts. Late
capture archives that existing result without submitting a duplicate. Cache
identity includes frame, epoch, ordinal and cycle; configuration resets clear
ownership. The receipt explicitly records prepared_before_composite. Native,
accepted-history and output-presentation ownership remain unchanged.

This is still a bounded synchronous developer lane, not production pacing.
RTT/direct framebuffer/OIT and nonnative consumer routes do not enter this new
preparation hook. The partial scene cannot authorize full-frame replacement;
complete moving scene coverage remains required alongside asynchronous feeding.

## D-154: zero-alpha discard is blend-scoped, not universal transparency policy

For classified overlay draws replayed through normal SrcAlpha/InvSrcAlpha
reactive coverage, mode2 discards samples whose final shaded alpha is zero.
This preserves coverage already beneath a noncontributing sample. Saturate
the mode to a binary output mask. Opaque, punch-through, additive and unknown
blend paths keep existing semantics; zero alpha alone cannot prove no color
contribution for arbitrary blend modes. This changes guidance only, not native
color. Partial alpha remains conservatively protected, not layer-separated.

Actual alpha-a captures remove969/1071/1052 mask pixels but retain timer/header
background rectangles. Do not claim all rectangles came from translucent
zero-alpha samples or widen punch-through discard speculatively. Keep this
visible defect open alongside pre-Present ownership/full-scene work; do not
make another unbounded mask-polishing phase a prerequisite for all integration.

## D-153: captured Soulcalibur HUD identity is separate from temporal history

The existing T1401N profile now has narrow captured-atlas rules for header,
timer, health fill, names and counters. Require known texture/state, exact
640x480 regions, bounded planar/layered depth, appropriate list and late order
where applicable. Changing palette/glyph topology must not remove known HUD
protection just because accepted-history stability is below three. Generic and
unknown-title rules remain unchanged. Unknown atlas reuse/layouts are not proven
by this three-frame title capture; no whole-title or cross-title acceptance.

Actual composition reveals native-background rectangles in transparent HUD
quads: existing coverage emits a draw mask even where alpha contributes nothing.
This is a separate mask-rasterization limitation, not permission to hide it or
declare complete overlay preservation. Correct alpha coverage before returned
presentation; do not erase transparent game effects indiscriminately.

## D-152: reuse the production overlay shader before returned presentation

The bounded return-composite experiment uploads the exact owned640x480 BGRA
image into a separate D3D11 target and runs the existing neural overlay shader
with current native color/mask. A deferred command list executed with state
restoration isolates immediate pipeline state and never binds the presented
target for writes. Source/guidance frame, producer, dimensions and native route
must agree. Explicit FLYCAST_REMAKE_COMPOSITE_TEST=1 is developer-only, off by
default; no output/history acceptance or performance claim follows.

Protected-mask correctness is not complete HUD classification. Actual first
frames protect only1004/1659/1004 pixels, visually mostly HUD outlines. Preserve
that shortfall; do not call a pixel-exact masked composite full HUD protection.
The next integration must retain full HUD and full scene coverage before native
replacement, and relocate source/return handling before normal composition.

## D-151: returned diagnostic color does not authorize presentation

Channel version2 adds one fixed640x480 BGRA reverse slot, matched to the exact
source receipt/frame/producer and checked for pixel corruption. Two retained
source identities bound staleness; duplicates and malformed sources are rejected.
Owned returned pixels do not advance neural history or replace native gameplay.
FNV is corruption detection, not authentication. Completed returns may drain
after orderly consumer close. Ordinary polling is nonblocking; busy means skip.
FLYCAST_REMAKE_RETURN_TEST_WAIT=1 is a separate developer-only capture proof
control, off by default, bounded to10 seconds per published frame. Restore its
temporary environment after testing; exclude it from all performance evidence.
The initial three-frame producer finished before consumer warmup, so that run
failed round-trip acceptance. Production asynchronous polling and protected
composition remain required, not replaced by the diagnostic wait.

## D-150: bounded live memory delivery is not neural acceptance or presentation

An explicit local channel token connects one Flycast publisher to one Remix
consumer. The consumer creates a new, session-local Windows mapping; existing
names and duplicate publishers are rejected. Two72MiB payload slots use exclusive
write/ready/read ownership. A full ring returns Busy without waiting for the
consumer; failed serialization frees its slot without advancing the publication
sequence. Consumer shutdown yields publisher fallback. FNV64 receipts detect
accidental corruption and aid comparison; they are not authentication or GPU
presentation proof. Slot sequences are transport bookkeeping, never accepted
neural frame history.

The first hooked producer is still the synchronous developer capture boundary,
with existing source observation plus FLYCAST_REMAKE_CHANNEL explicitly set.
No ordinary gameplay behavior, external configuration, private neural API or
Flycast presentation is activated. Optional archival is independent of live
publication; the live-channel consumer does not read saved scene packets.
The bounded runtime test warms the first received packet and consumes two more;
it is not an FPS/latency or continuous ordinary-gameplay acceptance run.

## D-149: share the scene contract and connect live-derived packets explicitly

The existing scene types/validation move from neuraltest into
core/rend/neural/remake_scene.h/.cpp. The test header retains compatibility
names, but Flycast producers no longer depend on a harness-owned model.
The opt-in source capture now converts fully witnessed opaque draws directly
into a camera-relative triangle scene, retaining original PVR attributes,
source draw/generation identity and transform W. Flat normals are derived,
not original normals. LOG511's measured title-specific lens and unit-scale
assumption remain diagnostic; the0.01-pixel relation tolerance is NOT a new
strict-parity gate or permission to call this recovered world geometry.

Current textures are read at the owned render-thread boundary with generation
checks and encoded into bounded packet-owned DDS bytes. A8/palette decoding in
this new direct route is unsupported rather than guessed. All of this remains
synchronous developer capture, off outside explicit source/capture opt-in.
The bounded FC-view wire format preserves producer/frame identity, geometry,
source color, texture bytes and omission labels for the diagnostic handoff.
Reading saved packets is not continuous live integration or presentation proof.

Only the live-artifact compatibility experiment permits resource recreation
when topology/draw count/texture content changes. Failures poison that uploader;
the harness must not Present a failed draw. This is not a trusted temporal
history or production failure policy. Camera-relative +Z packets use a supplied
camera-forward diagnostic light; the old reflected-anchor fixed light produced
black output and remains a retained negative result. No game lighting is inferred.

## D-148: prioritize the live uploader connection, preserve diagnostic limits

The user's integration correction moves active implementation to M2-scene.
M2-camera acceptance remains pending, but independent live transport must not
wait on further tiny-residual fitting. Bounded camera-relative experiments keep
their labels and cannot claim recovered world space. Existing GPU evidence is
reused; unresolved runtime cleanup stays open rather than becoming an obsolete
unavailable-runtime stop. Preserve authored trails under D-143.

The compatibility uploader may consume packet-owned texture bytes in the same
reviewed RGBA8 DDS layout as its existing file path. Exactly one texture source,
validated mip layout and aggregate packet bounds are required. Owned texture
payloads have a separate 64 MiB aggregate limit; existing geometry/metadata limits
remain 8 MiB. The earlier attempt charging all DDS payloads to the geometry
budget failed before runtime load and remains recorded. No raw cache
pointer crosses ownership. The separate public path-only material adapter must
reject memory textures rather than silently render them untextured. File-backed
capture conversion is a test bridge, not a live producer or asynchronous proof.

## D-147: common-origin vertex/draw coverage is a dependency subset, not a camera

Validated register handoff across compiled blocks allows observed transform
dependencies to reach captured XYZ. Coverage requires all three components to
carry the same executed transform serial and payload; complete draws require
every referenced vertex to satisfy that condition. Duplicated source vertex
records do not receive coverage. This is a selection boundary for the next
supported live scene experiment, not permission to reinterpret original input
coordinates as world space or claim evaluated projection reconstruction.
Retain complete derivation evidence and establish camera/coordinate semantics
with falsifying controls before activating reconstructed geometry. Preserve
the zero-coverage and timed-out earlier attempts; diagnostic observers stay
off by default and excluded from performance measurements.

## D-146: observed source dataflow is diagnostic until transform correspondence closes

The opt-in x64 source observer now carries actual SQ stores, same-block direct
register RAM-read links, bounded RAM-producer candidates, and a separate rolling
FTRV input/matrix/output ring. DirectSourceRead rejects intervening overlapping
destinations and interpreter boundaries. RAM candidates normalize aliases,
reject value/address mismatches and expire on collisions, partial writes,
reset/fallback and the inspected bulk-copy paths. This does not claim coverage
of every possible RAM mutation or camera/world semantics. An FTRV record may
not be attached to geometry just because it is recent or numerically similar.
Require explicit executed dataflow through subsequent arithmetic and stores,
with matched-record ownership beyond ring eviction. Keep this observer off by
default and excluded from performance evidence. LOG501's exact-pixel comparison
remains failed/parked; byte-exact decoded packets prove only that narrower scope.

## D-145: retain child-qualified live submission joins, not inferred transforms

The opt-in x64 non-MMU observer records executed SQ invocation identity and
exact TA copy bytes. Queue publication stamps each child batch with the root
producer identity. Polygon decoding joins by child-local offset and exact
packet bytes, including PCW; typed vertex payload starts four bytes later.
The render context retains a bounded owned collection, and the capture snapshot
copies it only when submitted XYZ still equals the decoded vertex. Context
retirement cannot invalidate the copied records. Disk packet versions remain
unchanged and do not implicitly carry this optional live provenance.
SQ invocation serial is not RAM content generation, PREF PC is not the upstream
transform writer, and a projected vertex join does not establish camera/world
truth. Those remain separate obligations before Remix scene activation.
Per user reprioritization, LOG491 native pixel discrepancy stays failed/parked
without blocking diagnostic integration work. No production parity waiver.

## D-144: owned projected snapshots at the bounded capture boundary

SnapshotPvrScenePacket shares range validation with the existing writer, owns
geometry/pass metadata and texture-generation values, and clears raw texture
pointers. QualityCaptureWriter publishes one frame-qualified render-thread-only
view after its capture success path, invalidating it on the next capture or
configuration change. This is not an asynchronous resource lease or live Remix
provider. LOG481-483 exercise actual Soulcalibur capture, returned frame identity,
current texture bindings and deliberately incorrect upload/RTT metadata.
Unknown camera, missing texture pixels and other omissions remain explicit.
Do not forward this projected packet as recovered world geometry. The next
provider must attach witnessed live transform/camera lineage, not replay the
offline three-frame composition or invent projection truth.

## D-143: preserve game-authored temporal effects

User correction: do not classify apparent weapon trails, afterimages or blended
effects as reconstruction defects merely because they persist during motion.
Compare the exact matched native moving sequence and source pass coverage.
Moving-versus-settled residual is a diagnostic difference, not isolated unwanted
trail energy. Attribute a defect only to additional persistence/displacement
introduced by the pipeline, with frame identity established. Current opaque-only
diagnostic packets omit effects and cannot establish full-frame effect fidelity.
Never remove native effects to improve a temporal metric.

## D-142: bounded actual-packet legacy transport experiment

Extend D-141 only inside the standalone harness. Preserve per-frame source
positions, normals, vertex colors, UV and exposed TSP sampling state. Reuse
texture resources only when exact DDS bytes and recorded generations agree;
different capture directory names are not texture changes. Reject unsupported
sampling/shading and changed texture content instead of reusing stale inputs.
LOG467 establishes three actual-scene output captures, not full PVR shading,
camera truth, temporal correctness, performance or combined presentation.
The synchronous byte checks are diagnostic and must not become a live-frame
implementation. Production integration remains gated by the active backlog.

## D-141: standard D3D9 factory for legacy draw-capture experiments

LOG463-464 retain failed black Remix captures while backbuffer output proves
ordinary D3D9 geometry was drawn. Pinned public rtx_remix_api.cpp factory calls
CreateD3D9 with WithDrawCallConversion=false. Standard Direct3DCreate9Ex in
d3d9_main.cpp uses the defaulttrue. Use the standard exported factory only for
the isolated legacy fixture, then register the device through the public API.
Existing public-mesh fixtures keep their original factory. No runtime binary
patch or external configuration write is involved.

LOG464-465 visibly exercise changing vertex colors/deformation in Remix output
and a gray frozen-color control. This is not motion-quality or game acceptance.
Prefer investigating direct original-attribute updates over fixed per-frame
triangle DDS rebaking: LOG461 measured its fidelity/data-size tradeoff. Keep the
sampler/tile probes as diagnostic evidence, not a competing production renderer.

## D-140: retained triangles are a labeled geometry-history ablation

LOG457-458 split validated triangle lists into at most256 triangles per instance
and carry one affine edge/normal transform per triangle using public bone data.
No game skeleton is recovered. Initial material, UV and vertex colors remain
explicitly frozen in this diagnostic; this is not source-attribute preservation.
Rebuilt-frozen and settled-frozen controls share that limitation and permit a
geometry-history comparison without confounding changing colors. The measured
regional error reduction is narrow evidence, not temporal or gameplay acceptance.
Reject incompatible frame/topology before drawing; after a draw failure discard
the scene. Production integration must advance history only on accepted output,
preserve changing source attributes and satisfy the full backlog checks.
Next work must leave the frozen-only ablation rather than tuning it as a product.

## D-139: explicit source-color modulation, not inferred baked-light removal

LOG452 proves that HardcodedVertex color transport alone is insufficient:
the synthetic direct-color reference renders gray with default instance state.
Pinned public surface_shared.h defines Texture1,VertexColor0=2,Modulate3 and
SelectArg1=1; these are not D3DTOP numeric constants. LOG453 supplies BlendEXT
explicitly and restores the gradient, with no-blend as the falsifying control.
Preserve any bone extension when adding blend state. Setting the public
isVertexColorBakedLighting field false makes this an explicit modulation
experiment; it does not extract physical albedo or remove baked game lighting.

LOG455 exposes --capture-source-color only for artifact captures, retaining
the same explicit reverse light as the existing composed-scene control.
Normal production/default harness behavior is unchanged. Actual source colors
are visible but isolated-geometry history still trails; do not promote color
transport, still-image metrics or a three-frame capture to gameplay acceptance.

## D-138: explicit diagnostic material replacement and affine transport

LOG443-446 support float32 per-triangle affine position/normal transport on
CPU and an unsaturated synthetic GPU fixture, including wrong-normal control.
This is not a recovered skeleton or real-game temporal pass. LOG447 proves
constant material appearance changes on retained geometry through public
DestroyMaterial then CreateMaterial with the same handle/hash. Repeated
registration alone is explicitly ignored by pinned public runtime source.

Validate inputs before mutation, clear successfully destroyed ownership slots,
track even handles returned with failure, and discard/disable the scene after
a partial replacement. LOG448 tests this ownership in mocks. Dynamic per-vertex
colors and UVs are not solved by constant material replacement; any texture
representation must preserve their variation and be independently compared.
Keep all new controls synthetic-only/off the production path until that work
and actual moving fighter evidence are complete.

## D-137: retained public skinning is transport, not recovered animation

LOG438-439 prove a bounded synthetic deformation through retained public
MeshInfoSkinning and InstanceInfoBoneTransformsEXT, with a wrong-sign control.
Keep this fixture synthetic-only and retain its arrays through consumption.
It does not establish game skeletons, arbitrary normal correctness or temporal
acceptance. The pinned interface limits each instance to256 bones.

LOG440-441 show actual source slots persist but base colors and some UVs change.
HardcodedVertex carries those immutable values; the inspected public instance
extensions expose blend state/tFactor, not arbitrary per-vertex color or UV
updates. Do not substitute a uniform tint for varying vertex updates, or freeze
first-frame attributes without explicitly labeling a lighting ablation.
Separate base color imported by the adapter from currently unimported PVR
offset color in diagnostics. A future source-lighting removal experiment must
be compared explicitly with source-color output, not called faithful transport.
Production integration and any live update route remain open.

## D-136: preserve camera-embedded and mixed-source provenance

LOG427-434 reuse witnessed preprojection geometry and explicitly embed its
camera-relative coordinates into the diagnostic reference camera. This is not
recovered world/object transforms. Preserve source attributes, proper camera
basis, projection checks and the winding change required by prior Y reflection.
The distinct diagnostic-camera-embedded-anchor label survives preparation/loading.

Cross-revision reuse requires full source-scene content equality excluding only
revision, equal material metadata and independently verified selected texture
bytes. Keep both revisions and the canonical content digest. Mixed compositions
retain each group's coordinate label, original revision, owned draw list and
equivalence/embedding record. The top-level revision identifies the reference,
not a claim that all input captures came from that revision. Loader checks do
not themselves rerun source arithmetic or prove image correctness.

The40-mesh static capture is diagnostic scene coverage only. It does not close
camera/world semantics, moving gameplay, physical materials, cleanup or the
combined external DLSS5 presentation gate.

## D-135: bounded scene joins and explicit diagnostic lighting

Same-frame batch joins require matching game/SHA/origin/coordinate semantics,
unique source draws and identical metadata for shared texture assets. Verify
every input DDS hash before create-only publication. Retain both independently
derived cameras; a diagnostic absolute roundoff allowance of1e-12 is recorded
for their binary64 differences, not a relaxation of strict PVR reprojection.
Keep all omissions and unaccepted camera/complete-scene status.

LOG425-426 demonstrate that the full submitted batch becomes visible under an
explicit reversed distant light. This is caller art direction, not recovered
game lighting. Keep the default unchanged and log the direction. Do not delete
draw2522 or retain the unsuccessful culling/SKY overrides as a visibility fix.


## D-134: real runtime capture is diagnostic, not pipeline acceptance

LOG398-402 use the explicitly authorized official Remix1.5.2 package outside
Git. This narrow download authorization does not permit arbitrary proprietary
binary acquisition, inspection, modification or redistribution. Public source
and pinned remix.h defaults were inspected. Opaque alphaTestType must match the
public constructor's7; zero hid actual geometry despite successful API calls.
Preserve the failed images as negative evidence and assert the material state.

The optional standalone capture route owns a D3D9 device and registers it with
the public SDK; it does not mix that initialization with Startup. Final color
and signed-normal visualization use public output copy and synchronous CPU
readback, excluded from performance evidence. Prepared snapshots retain their
camera/source identity and omissions; a static temple image cannot establish
moving camera recovery or a combined Remix/DLSS5 gameplay pipeline.

Destroy the window while the runtime remains alive, before Shutdown/unload:
the opposite order failed in actual tests. Shutdown's public implementation
drains registered COM references; do not dereference registered device pointers
after it. Runtime-reported undisposed objects remain an unresolved acceptance
failure even when the process exits0. No clean-lifetime claim is authorized.

## D-133: separate sampled diagnostic submission from accepted scene submission

ReadyForAdapter currently requires world-space and no omissions. The prepared
sample has neither proven physical world-space nor complete coverage. Do not
copy it into a Packet with Space::World and an empty omissions list to get a
green return. Add a distinct bounded diagnostic entry point with explicit
sampled-anchor coordinates and declared clip provenance. Keep its exclusions
with the retained scene across Submit and Redraw; successful API callbacks
must still report diagnostic-only, not camera/world/gameplay acceptance.
Share structural/material/normal/index safety checks without weakening the
ordinary ReadyForAdapter contract. This is preparation for an isolated real
runtime experiment, not a replacement for the standing moving-gameplay goal.

## D-132: diagnostic clipping is not recovered game clipping

Normal DX11 creates all three cull rasterizer states with DepthClipEnable=false
(DX11Renderer initialization); its pixel shader writes logarithmic SV_DEPTH.
Thus a search for conventional near/far names is not a recovery method for
the upstream game's clipping decisions. Existing game/TA clipping and unseen
geometry remain distinct questions.

Actual reconstructed H view depths span0.8441100120544434 through
102.98729705810547. The synthetic harness farPlane100 would exclude17 of3682
vertices. Never copy Synthetic camera defaults into the prepared game artifact.
For a diagnostic submission, a separately supplied finite positive clip interval
must be checked against every submitted vertex and reported as caller-supplied,
not recovered game values. The source artifact retains null game clips. A
successful enclosing-range check cannot close camera, visibility, complete
scene, GPU or moving gameplay gates. Do not silently discard out-of-range faces.

## D-131: explicit captured source-color binding

A material may name the captured TextureIdentity only in the explicit
sourceColorExperiment with a valid DDS path. Mesh and material identities must
agree on ID, upload generation, palette generation and RTT generation; both
must be known. Missing, stale or unowned associations fail before API calls.
This is caller-contract consistency, not proof of file contents or physical
albedo. The importer must retain verified capture-to-asset hashes/generations
through DDS conversion and file lifetime before any game submission. Existing
camera, world-space, transform, normal and omission checks remain intact.
This opt-in contract does not reproduce PVR offset color, fog, sampler state
or baked illumination and must not be represented as native shading parity.

## D-130: missing Dreamcast normals versus generated face normals

The selected type-3 TA packet has xyz, UV, BaseCol and OffsCol only
(ta_structs.h::TA_Vertex3). AppendPolyVertex3 copies those attributes and
does not recover normals. Vertex::nx/ny/nz are explicitly Naomi2 fields;
elan.cpp::getNormal/setNormal decode that separate path's signed normal
components. They are not Soulcalibur normal evidence. The scene exporter
correctly records unknown-for-dreamcast; preserve this source provenance.

For the explicit remake experiment, derive flat face normals from validated
reconstructed triangle positions after coordinate reflection/winding handling.
Label them geometry-derived, not game-authored. Split vertices per triangle
to preserve hard faces and all original UV/color attributes; do not average
across seams or invent smoothing groups. Reject nonfinite/degenerate faces
with counted omissions, enforce expansion budgets before allocation, and
never change the original captured vertices. Unknown coordinate domains are
still unknown: a cross product cannot establish world-space truth, camera
acceptance, physical units, or complete scene coverage. Check winding reversal,
proper rotation and degenerate controls before submitting actual supported
samples. This is the next bounded implementation, not a rendering pass claim.

## D-129: explicit DDS format and retained file lifetime

At reviewed Remix revision e876135b37295dc203ccdb7b20a8089629588201,
rtx_asset_data_manager.cpp accepts DDS, and rtx_remix_api.cpp copies source
paths into queued PreloadSource values. Files must remain available through
asynchronous use. In rtx_texture.cpp, ManagedTexture::imageCreateInfo preserves
assetInfo.format unless FORCE_BC_SRGB is selected; API preload uses AUTO.
Use explicit RGBA8 UNORM for the native-source sampling experiment, no silent
sRGB decode or gamma rewrite. This is not a physical-albedo or final-output
color-management claim. Independent Pillow decode validates base payloads,
not Remix GPU loading or the full captured mip chain's runtime selection.

## D-128: public vertex colors preserve native BGRA bytes

Reviewed public source at existing pinned Remix revision
e876135b37295dc203ccdb7b20a8089629588201:
[rtx_remix_api.cpp](https://github.com/NVIDIAGameWorks/dxvk-remix/blob/e876135b37295dc203ccdb7b20a8089629588201/src/dxvk/rtx_render/rtx_remix_api.cpp)
assigns HardcodedVertex.color to color0Buffer with VK_FORMAT_B8G8R8A8_UNORM.
Thus Flycast raw native BGRA bytes pack little-endian without a channel swap.
Keep independent red/blue/alpha goldens. This source contract does not prove
that a supplied runtime matches the revision or that its final material uses
vertex color as desired; that remains GPU/material validation. No binary read.

## D-127: source material evidence is not physical albedo or native parity

LOG344-355 connects same-frame draw generations, verified raw textures, original
UV/color/offset attributes and uploaded fog globals. Preserve BGRA input layout,
effective clamp semantics and explicit-mip sampling labels. Missing fog state
must reject; later captures cannot retroactively supply old-frame constants.
CPU snapshot nonmutation is distinct from GPU verification and native-image
parity. The retained eight-pixel one-level discrepancy remains open; do not
accept repeated lucky frames or loosen it. This bounded material data path may
advance independently, but does not make an incomplete scene renderable.
Normals remain unknown, source textures are not physical albedo, and source
fog/offset lighting cannot silently be passed off as a complete PBR material.

## D-126: preserve a fixed sequence basis and separate CPU camera proof

CAMERA-COORDINATE-CONTRACT.md consolidates LOG331-343. Reflect source and view
together; retain proper axes, effective lens aspect and winding. One shared
origin may reduce float quantization without changing relative motion; a
per-frame origin must not manufacture stationary-camera evidence. CPU double
intermediates preserve the float ABI but are not proof of runtime GPU math.
Unknown game clipping, normals/materials, scene completeness and physical-world
semantics remain explicit. The strict captured-rounding failures remain failed.
Do not repeat numerical micro-tests once their bounded contract is proven;
advance to binding actual scene/material records and resolving missing domains.

## D-125: expression lineage and screen rejection do not relax camera acceptance

LOG315-327 binds ordered binary32 expressions through observed writes, reads,
copies, tape and selected scene geometry in separate sampled-frame captures.
An unordered set of contributing matrices is insufficient: intermediate rounding
and accumulation order must reproduce the actual stored words. Unknown writers,
dispatch gaps and record reuse must invalidate unsupported ancestry.

Predivision Z remains distinct from the divided path's scaled output depth.
Calibration is checked against independent rigid matrix witnesses, not inferred
from a reprojection whose normalization cancels algebraically. General-affine
contributors remain explicit under D-124. A zero captured fourth row is preserved;
only the mathematical top-three-row analysis uses a labeled homogeneous row.

The evidence mesh preserves every supported vertex and all strict0.001-pixel
failures. Common-plane rejection establishes only that the incident triangle
does not intersect the specified content rectangle in that sample; it is not a
waiver of the all-vertex gate, proof of GPU clipping, or a world-space camera.
Separate successful frame captures do not prove cross-frame object identity.
These harness artifacts are not production-enabled or Remix-renderable. Complete
world/camera semantics and real runtime presentation remain separate requirements.

## D-124: calibration witnesses are distinct from general-affine contributions

LOG238/239 demonstrates93 executed nonorthogonal matrix contributions in the
four-draw capture. The all-rigid gate remains failed and unchanged. An explicit
affine-contribution experiment uses all10035 matrices satisfying the existing
rigid predicate as calibration witnesses, checks their unchanged shared-scale
tolerance, and retains every one of the93 other affine contributions in exact
arithmetic and vertex reconstruction. These contributions cannot calibrate a
camera by row norms. They are not dropped, repaired or declared rigid; malformed
and degenerate matrices still reject. Controlled shear fixtures distinguish this
case and wrong calibration must still fail the rigid witnesses. This establishes
only a bounded calibrated camera-relative representation; it does not identify
object-deformation semantics, physical scale, a unique world camera or Remix GPU
behavior. The explicit --affine-contributions flag keeps the changed assumption
visible and preserves strict default failure as evidence.

## D-123: RAM addresses are not transform lifetimes

LOG222..226 falsified single-record-per-address ownership over the later draw
interval. Use generation/address/incarnation identity; preserve all completed
versions and bind consumers through their actual gather/copy IDs, not the latest
contents at an address. Fully projected but unread versions are unused evidence,
not geometry. Partial seams/XYZ reads, duplicate seams and out-of-budget reuse
remain failures. The temporary observer bounds four incarnations/address and
closes at the independently observed final selected TA packet, not an arbitrary
starting frame offset. Independent lifecycle verification precedes arithmetic.
Version-separated slots only index evidence; they do not invent game identities,
world coordinates or persistent production objects. LOG227 verifies the current
two-draw scope with unchanged arithmetic/calibration tolerances; Remix GPU and
complete scene reconstruction remain pending.

## D-122: indexed first records and serialized evidence transport

The first-record path is not the loop-body path. LOG211..215 proves the c932
indexed SUPPLY variant and c998 direct-X PRED variant with separate exact
instruction programs. Preserve the signed16-bit address index and separately
loaded fourth float even though they share source bytes. Bind both to actual
stores and consumers; do not assume r4 is the destination or manufacture an
external X-load event for the direct-X path. Synthetic and live negative
controls must preserve the distinctions.

Capture callbacks can arrive from emulator and renderer threads. Serialize
append/finish on the diagnostic compression stream and verify bounded
concurrent roundtrips independently. A semantic-success log does not excuse a
bad checksum; discovery D stays rejected. Destruction requires joined callers.
Diagnostic synchronization is not production performance evidence.

The integrated full921 capture accepts all1591 triangles of draw277 only.
It does not close world-camera, whole-scene, real Remix GPU or combined-output
acceptance. The raw-depth discrepancy in its live wrong-edge control remains
visible; do not turn matching scene JSON/PNGs into an all-input equality claim.

## D-121: bounded transform ledger and supported-triangle acceptance

Reuse the existing executed-instruction event vocabulary in a lossless bounded
zlib stream. Decode independently and reject semantic errors separately from
compression integrity. Tie each selected consumer to its actual copy ID, not
its compact sequence number. Observation ends after the last retained actual
copy; a later write cannot alter those copied bytes. Frame/generation and all
selected decoder associations remain mandatory. Retain the failed overlong
lease as evidence, not a reason to accept untracked writes within the prefix.

LOG201/202 establishes906 supported and15 unsupported addresses/frame in the
draw277 domain. Indexed arithmetic and shared-calibration validation supports
1529 triangles/frame, not all1591 or all8339 opaque triangles. Never synthesize
the missing initial producers or infer a physical world camera from a scaled
orthogonal matrix. Next batch the unsupported producer family; do not repeat
the already accepted records. The real Remix runtime/GPU dependency remains
separate from this offline calibrated reconstruction.

## D-120: compact observed consumers and explicit selected-domain limits

Use fixed-width little-endian records with actual64-bit copy/decoder pointers,
before/after packet bytes and producer association. Require exact completion;
partial, corrupt or wrong-pointer data is not a valid tape. CRC is corruption
detection, not authenticity. Live same-capture verbose/binary equivalence and
large-domain scene binding establish transport only, not original transforms.

A gather with no completed TA copy creates no decoder provenance. Do not
confuse a superseded pending gather with an overwritten copied record. The
draw277 map deliberately uses an observed5069-copy prefix and independently
requires all8235 selected associations. Later unsupported paths stay omitted;
this bound does not excuse a missing selected vertex. Preserve failed A/B/C
attempts. Move to original-transform records rather than repeat this map.

## D-119: shared-record leases and bounded calibrated geometry

One transformed RAM record can supply several indexed PVR vertices. Retain
writer ownership through every actually mapped consumer, not merely the first
XYZ triple. Use actual address/generation/consumer associations, compact reused
descriptors and exact successor-entry clocks. An address range is not identity.

FULL-DRAW-TRANSFORM-AUDIT.md supports one complete92-triangle opaque draw;
8247 opaque triangles/frame remain omitted. Calibrated camera-relative geometry
is a derived representation with verified reprojection, not recovered physical
scale/world camera or a working Remix scene. Next prioritize remaining-domain
coverage by triangle contribution rather than repeated single-point proofs.
Preserve failed budget and timing-mismatched controls as explicit limitations.

## D-118: preserve accumulated transforms and distinguish supported domains

LOG180..184 falsify a single-FTRV replacement model for selected draw26 records.
Use observed FTRV contributions plus existing XYZ additions, preserving all
four input words and actual byte-writer generations. Fifteen samples now have
bounded arithmetic/load/store/gather lineage; slot3 and coordinate semantics
remain unsupported. TRANSFORM-COVERAGE-AUDIT.md defines this exact scope.
Do not invent bone, camera or whole-scene meaning from this pattern.

Standalone Remix bring-up reuses immutable resources across camera-only redraws.
Public API success and CPU lifetime tests do not prove GPU output or completion.
The supplied-runtime dependency remains explicit; no fetch/config workaround.

## D-117: bind gathered positions before attributing their original transform

Collection pass B reuses scalar SSA validation for18 actual gathered XYZ
triples, bound through physical SQ bytes, context generation, actual copy
destination and producer-stamped decoding. Address equality across frames
does not establish source identity or a camera. Next extend the known original
transform/record seams for these six records; no broader caller census or
production camera activation follows from this map. Completion belongs to the
actual last queued producer, not a mutable next-frame hint. C/D/E exact graphics
preservation is scoped to the recorded controls; latest-checkpoint GPU-repeat
differences remain visible. See CAMERA-GATHER-AUDIT.md and LOG #179.

## D-116: bounded packet ownership and GPU-repeat preservation stay separate

M2-camera collection uses six explicit decoded vertices in three actual
producer frames, verified against root-owned TA offsets and final opaque
index membership. Do not infer upstream geometry from float/cycle similarity.
The resulting packet map bounds the next actual gather/record observations;
it is not camera calibration or proof that these pixels are visible.
Positive A and disabled C have exact native/guidance preservation. Negative B
correctly rejects its expected epoch and preserves complete scene packets,
but its small color/raw-depth differences prevent a paired GPU-preservation
claim. Retain that failure without lowering equality, claiming a cause, or
reopening the parked replay investigation. Temporary probe code is removed;
production camera activation remains unavailable until broader proof exists.

## D-115: share algebra code, not unproven projection-domain assumptions

The linked opaque sample and earlier translucent sample can use the same
scaled-axis factorizer, but their observed projection programs differ. The
opaque path uses a rounded reciprocal followed by separate multiply/add and
unit reciprocal-depth scaling. The earlier translucent fixture uses fused
screen arithmetic and an additional 1.04 depth multiplier. Bind each to its
actual producer/record/TA witness; do not silently transfer depth calibration.
Preserve actual nonunit W. An equivalent model/view basis is not an error,
and an approximately orthogonal matrix alone does not prove camera intrinsics.
The next M2-camera step remains bounded actual sample coverage, not another
single-point factorization or production camera activation.

## D-114: backlog-driven autonomous delivery, not per-trace human approvals

The user's 2026-09-08 request changes execution authority: use a short standing
goal and follow the live backlog until a working experimental RTX Remix plus
externally supplied DLSS 5 gameplay pipeline is verified. Routine engineering
implementation, bounded hypothesis refinement, self-review, checkpointing and
progression are authorized without another human approval per block/card.
This supersedes procedural permission stops in D-111 through D-113 and earlier
task cards. Their observations, negative evidence and technical limits remain.

BACKLOG.md owns one current card, dependency-aware queue, no-progress rule and
working-pipeline acceptance. AGENTS.md is the concise operating contract;
the remake plan owns architecture, and LOG/audits own evidence. Remove stale
current-task launch stacks instead of prepending another competing authority.
Old versions remain recoverable in Git; do not remove failed evidence.

Two no-progress tranches trigger a changed hypothesis or another ready
dependency, not infinite capture/caller expansion. Independent synthetic real
Remix GPU work need not wait for a complete game camera. A dependency gap must
not be hidden by relabeling mocks/approximations as the requested runtime.
Actual combined game output, overlay safety, provenance and asynchronous
cadence define success; full production/title gates remain separate claims.

No blanket external configuration writes, proprietary binary inspection or
downloads, media acquisition, spending, unsafe system changes or false gate
promotion is authorized. Escalate only genuine external blockers after safe
independent work is exhausted. Do not use a blocked goal tracker as a reason
to refuse authorized repository work or falsify completion to replace it.

## D-113: a linked opaque FTRV is not yet a decomposed camera

The approved single predecessor now proves actual source-vector loads, live
XF matrix words, four transformed outputs and the executed edge into the
selected opaque record stores. This advances beyond an uncorrelated transform.
Preserve the observed nonunit W exactly; never replace it with1 or infer
coordinate-space semantics from a near-one value. Verify the mathematical dot
result independently, distinguish that check from a full binary64 emulator,
and keep actual dynamic results/record generation in the causal chain.
World camera, model/view/projection decomposition and broader draw coverage
remain unproven. Stop this completed trace; any further source/coverage
investigation needs a finite new scope, not another automatic predecessor.

## D-112: execute the user-approved single predecessor without reopening prior gates

On2026-09-08 the user approved one preceding block supplying FR0-FR2 to the
selected8c03c94c invocation. D-111's automatic-expansion stop remains valid,
but no longer blocks that specific experiment. Reuse the existing bounded
observer chain, falsify the actual edge/operand binding and checkpoint once.
Do not repeat the completed requirements audit, expand recursively, or reduce
M2's requirements to one vertex. Requested routing is gpt-6-astra at low; a
documented preference is not proof that the running model changed.

## D-111: stop at the bounded block when coordinates remain live-ins

An initial RAM store block may simply copy precomputed floating-point registers.
Exact store operands and subsequent reciprocal/viewport arithmetic do not prove
the calculation or coordinate system that preceded those registers. Report
that limitation, retain false coordinate-calculation/camera flags, and stop
automatic predecessor expansion. Return to the actual M2 requirements instead
of authorizing another trace through a new self-written task card. Further
investigation requires a concrete newly approved route and stopping condition.

## D-110: reciprocal record depth is not yet camera-space depth

Actual1/Zrecord arithmetic and a checked successor edge establish the selected
factor's origin without establishing the earlier record's coordinate system.
Bind denominator loads to the before-overwrite byte generation, and falsify
that binding with a mutation that the final-record verifier alone still accepts.
A matching successor factor is not enough if an intervening block/reset breaks
the edge. Stop predecessor expansion once the factor is found; next only the
already observed initial record-producing stores under OPAQUE-FACTOR-EDGE-AUDIT.md.

## D-109: prove final viewport arithmetic without inventing its factor

Observed X=a*q+320,Y=b*q+240,Z=q is a final coordinate calculation, not proof of
the factor's reciprocal-depth origin, a matrix or world camera. Retain actual
host rounding mode and every separate binary32 result; default nearest rounding
or fused arithmetic is not an acceptable substitute. Buffer only bounded data
until actual store destinations select the invocation. Falsifying controls
must mutate the intended observer, not a shadowing earlier log line. Next only
one actual predecessor edge supplying q, with fixed bounds and no recursive
caller expansion. See OPAQUE-COORDINATE-CALC-AUDIT.md.

## D-108: retain overwritten record writes and name observer coverage

A memory record can contain intermediate and final coordinates in the same
frame. Replay actual per-byte last writers through the actual consumer loads;
do not attribute the earlier words or shared effect calibration to final opaque
geometry. Unknown interpreter/HLE write paths must reject, not disappear from
the claim. Inspected SQ/DMA paths are not actual positive execution coverage.
Keep MMU/cache/debugger/direct-host limitations explicit rather than claiming a
universal observer. Six writes establish this record's observed lineage, not
their floating-point calculation or a world camera. Next only the selected
final-store block witness under OPAQUE-RAM-WRITERS-AUDIT.md's fixed bounds.

## D-107: an indexed projected-position gather is not a projection

The observed opaque consumer loads XYZ from an indexed16-byte-stride RAM array
and copies them to SQ. Prove its integer addressing, actual returned loads and
SSA store dependencies without attributing a camera or transform to that block.
Equal scheduler timestamps do not select a loop iteration; use actual target
context, packet offset and destination. Shared array bases do not confer an
effect record's calibration on an opaque record. Next only the bounded writer
generation of the observed12-byte record; see OPAQUE-XYZ-OPERANDS-AUDIT.md.

## D-106: physical SQ last writers survive a flush, not a reset

SQ physical storage is aliased across E0-E3 addresses and serves both RAM and
TA transfers. A PREF copies its contents without clearing them. Track observed
byte writers across flushes and invalidate on reset; never treat a narrow TA
address filter or a prior packet snapshot as complete provenance. An empty
start and a cap hit remain failed evidence. Equal scheduler cycles cannot order
stores; the executed callback sequence and exact post-store bytes do so here.
The accepted XYZ store instructions still do not identify their calculation or
world camera. Next operand-source work is bounded in OPAQUE-SQ-WRITERS-AUDIT.md.

## D-105: a copied SQ packet is not its filling-store producer

Accept R1 only for the actual observed type3 packet: source bytes at TAWriteSQ,
post-copy destination, same-context decoder pointer and final vertex agree.
A one-shot packet lease cleared on lifecycle invalidation prevents recycling
from rearming this diagnostic; it is not a general renderer-generation feature.
The live wrong-generation query must reject even when bytes match. R2 observes
actual last writers to the physical aliased SQ slot, not current RAM guessed
after submission, static instruction membership or a presumed camera routine.
See OPAQUE-REVERSE-PLAN.md. No expansion to all variants or whole-scene semantics.

## D-104: separate TA packet header, vertex member and original transfer

The selected normal type3 parser consumes a32-byte packet whose TA_Vertex3
member starts four bytes after PCW. Record both actual pointers/offsets, not
sizeof a generic vertex cast. Reject unsupported split/64-byte variants rather
than silently spanning another packet. Accepted-context identity and a direct
decoder-buffer association do not identify the earlier transfer or CPU producer.
R1 must add that causal copy witness before R2. Harness reset tokens must be
nonreused and monotonic; remaps require the expected generation explicitly.
See OPAQUE-REVERSE-PLAN.md; native production hooks remain removed at checkpoint.

## D-103: stop the uncorrelated scalar candidate at its observed return

The third known transform candidate has verified RAM stores, returned reads,
component-reversed copies and one bounded four-block consumer invocation. It
still has no causally observed PVR output. Preserve this negative feasibility
result without claiming recovery impossible or promoting arithmetic to camera
semantics. Remove temporary hooks, retain failed builds and runtime controls,
and audit M2 gaps before any further scope expansion. See OPAQUE-SOURCE-AUDIT.md.

## D-102: capture identity belongs to the accepted producer context

Carry capture-only scheduler cycle, submission ordinal and process-local reset
epoch on the accepted PVR context, not a global last-frame value. Keep neural
history/frame IDs and serialized game state unchanged. Decode Clear preserves
the stamp; context recycling, reset/load and synthetic replay invalidate it.
Unavailable sources remain unavailable. Epoch equality in one capture does not
authenticate an older trace or establish cross-process identity. The clock's
Owns helper checks epoch/range only, not arbitrary stamp authenticity.

Require a parsed, bounded completion marker for requested capture save/load;
locale-dependent numeric output is invalid evidence, not a recoverable pass.
FRAME-IDENTITY-AUDIT.md records actual C-run validation and retained failures.
This slice does not explain the first shifted capture or recover world camera.

## D-101: explain the fish sprite without promoting effect calibration

The second executed combat transform reaches an actual decoded translucent quad
with verified source fish texture. Keep source draw0 and merged material draw4
distinct. Its separate depth scale is evidence about this producer, not the main
scene camera. Next inspect existing opaque coverage/source seams before choosing
another bounded producer. See COMBAT-PACKET-AUDIT.md; no census or guessed camera.

## D-100: follow the new produced RAM generation, not numeric vertex similarity

A known second transform site now has a bounded executed combat block path,
exact projection arithmetic and matching pinned RAM output. Its depth multiplier
differs from the petal sample; preserve actual semantics without asserting a
common physical scale. Next track that specific generation's actual reads/copies
until TA or overwrite/reset/cap. Block-end observation is not per-access tracing,
an opaque draw association, or a camera. See COMBAT-PATH-AUDIT.md.

## D-099: sampled effect calibration is not whole-scene camera evidence

The linked transforms belong to translucent three-vertex primitives using a
verified petal-like source sprite. A matched frame packet and material binding
prove that association, not visible opaque fighter/arena coverage. Preserve
the source-draw versus merged material-state distinction. The screen labels
frame1302 Emperor's Garden; historical Hoko Temple naming locates the replay,
not title evidence. Next seek bounded opaque combat provenance or explicit
unsupported results. Do not restart 1-LSB replay diagnosis or construct a world
camera from these effect samples. See SCENE-ASSOCIATION-AUDIT.md.

## D-098: preserve sorted source geometry separately from GPU commands

Scene v2 records each draw's vertex/index range domain and actual sorted commands.
Do not relax bounds to accept source vertex ranges as index ranges. Empty draws
retain unused offsets; sorted triangle lists cannot contain restart tokens or
overlapping nonempty command ranges. Preserve v1 omitted-order semantics and
compare v2 order with retained state before developer replay. The linked samples
are translucent primitives sharing texture state; their calibration is not
evidence for opaque world coverage. See SORTED-SCENE-AUDIT.md.

## D-097: shared sample calibration needs real scene-generation coverage

Two distinct executed transforms reach different decoded vertices in the same
replay epoch/context and bulk transfer with compatible normalized projection
scales. Accept that pair only. Original fixture controls stay strict; additional
samples may use explicit ordered event profiles and need not each discriminate
nearest versus toward-zero when their actual results agree. All exact arithmetic
and lineage checks remain. Next bind the witnessed epoch to its actual scene
snapshot and establish sampled primitive/material/visibility coverage; don't
attach it to later frames or upgrade two instances to whole-scene calibration.

## D-096: distinguish camera calibration from model/view gauge

The linked composite's exact-dot/projection checks pass and wrong layout,
scale and uncompensated decomposition fail. A compensated rigid model/view
change leaves projection invariant: a unique world basis is not identifiable
from that sample and is not required merely to evaluate a labeled view-space
scene. Require shared per-context projection/depth calibration across additional
distinct, causally linked geometry before using inferred scales as a camera
contract. Do not average unlike transforms or fabricate global world semantics.

## D-095: decoded-vertex lineage does not separate model/view/projection

One exact source transform now reaches the actual TA copy and xyz conversion
of a decoded vertex, with a failing wrong-context expectation. Accept that
lineage only. A scaled/orthogonal-looking composite matrix does not uniquely
identify the camera, world basis, model transforms or physical scale. Next
test algebraic/reprojection semantics and expose equivalent decompositions;
additional linked samples must distinguish an explicit hypothesis, not repeat
presence checks. No arbitrary FOV/world normal or runtime-integration promotion.

## D-094: TA input identity is necessary but not camera or frame identity

The captured source transform reaches one exact 32-byte RAM packet through
verified arithmetic, register-linked queue stores and an actual area-3 SQ
flush. Its actual pointer is contained in a polygon TAWrite input range, with
unchanged bytes and a failing wrong-offset expectation. This is accepted at
TA input only. Next bind the packet to its actual context/decoded vertex and
assess the source transform's projection/model/view semantics. Do not infer
presentation, physical depth, world coordinates, lifetime safety or a complete
camera from that input identity. Diagnostic overwrite coverage is explicitly
limited; do not promote temporary observations to production tracking.

## D-093: verified local projection arithmetic is not a recovered camera

The observed reciprocal, depth scale and fused x/y offsets are accepted only
for the captured operation/store chain. Use exact recorded rounding and fused
semantics; a nearest-rounding assumption fails the reciprocal. Diagnostic shadow
mutations must not alter guest execution. The new derived generation starts
only after its ordered writes verify. Next require bounded actual submission
lineage with a falsifying control; do not infer TA/frame ownership from similar
values or native image equality. Source matrix role, world scale, missing
geometry and physical depth remain unresolved even after local math verifies.

## D-092: an overwritten coordinate span needs a new proven generation

The observed read/copy chain ends at the first overlapping write. Do not keep
the original transform's identity on modified bytes. Derived coordinates need
actual operand/result/store evidence and a falsifying control before becoming a
new tracked generation. CPU memory observations do not establish DMA/SQ/TA
coverage, camera semantics, physical depth or frame ownership. Native images
remaining identical prove diagnostic preservation only.

## D-091: RAM-store provenance is not TA or camera provenance

The single executed FTRV-to-RAM witness is scoped to its recorded instruction,
cycle, source contract and four actual post-store readbacks. A diagnostic-only
wrong expectation must fail without changing guest output. Select the final
launch of append-only logs, not whichever records happen to match. Following
the known 16-byte span may establish subsequent consumers, but stop on overwrite,
reset or event cap; do not invent TA association or recover camera by labeling.

## D-090: executed transforms need submission lineage before camera claims

FTRV input/matrix/output is a possible executable witness, not a recovered
camera. Require actual value lineage through arithmetic/stores to a specific
TA packet/context; neither numerical proximity nor the last matrix at SQ/DMA
submission is sufficient. Interpreter-only evidence remains separately labeled.
The next bounded witness must return uncorrelated/unsupported if association
cannot be established, without expanding into arbitrary memory scanning or a
general decompiler. See CAMERA-SOURCE-AUDIT.md. No production contract changes.

## D-089: material sidecars preserve native source data, not inferred PBR

The optional material capture is separate from the PVR scene packet. Preserve
native formats, every actual mip, palette indices and the captured palette,
per-draw slot bindings/generations and replacement/sampling metadata. Serialize
no resource pointers. Deduplicate descriptor plus full bytes, not VRAM address
or a hash alone. Hash tight rows in a fixed locale. Reject unsupported resource
layouts, unresolved textured draws and budget violations without substituting
the final scene. Run the inspector before accepting an exported package.

Keep allocation/readback inside explicit bounded developer capture. The tested
native on/off preservation is not performance evidence and does not close the
parked replay residual. Capture-seam generation matching does not establish
historical per-draw resources for titles that mutate them within a frame.
Source texture artwork may contain painted lighting; no physical albedo,
roughness, metalness, normal or recovered camera is inferred. See
MATERIAL-CAPTURE-AUDIT.md; the approved camera provenance track is next.

## D-088: export source textures before further relighting

The user approved camera/material scoping after the rejected preview. The source
audit in CAMERA-MATERIAL-PLAN.md establishes separable DX11 texture resources,
vertex/offset color, UV and ShadInstr. Preserve them separately; do not use final
scene pixels as replacement material again. Source textures may contain painted
lighting and are not automatically physical albedo. Palettes, mipmaps, format,
content generation and custom replacement status belong to the capture contract.

Implement a bounded developer-only material sidecar first, then review it. Keep
the Dreamcast camera unknown until an actual pre-projection source witness and
moving controls prove otherwise. This newly approved scope supersedes the prior
request for scope approval, not the ban on precision loops, private binaries,
automatic external configuration or unsupported rendering claims.

## D-087: hypothetical relighting remains offline and is not promoted

The FC-067 preview reuses captured indexed geometry and source-color identity
but explicitly assumes projection and reciprocal depth, derives face normals,
and modulates baked SDR appearance. No material separation, shadow rays,
offscreen scene, actual runtime, or neural output is implied. Conservative
uncertain-depth coverage and a declared world crop limit the experiment;
unchanged crop-exterior pixels do not prove a general HUD classifier.

REMAKE-PREVIEW-AUDIT.md records the 30-frame result and failed attempts. Visible
faceting and over-brightening reject its promotion. Deliver the comparison and
stop this approximation branch. Any genuine camera/material extraction or
runtime integration needs its own bounded scope, not another tuning loop or
implicit closure of M2's parked strict source-equality failure.

## D-086: park precision diagnosis and require a visible feasibility decision

The user's 2026-09-07 "Are we spinning wheels?" / "Proceed" correction ends
the trace-53 investigation as the immediate priority. `REMAKE-DISPOSITION.md`
is authoritative over older M1/M2 next-task text. Keep the 13-pixel source
equality failure explicit, preserve failed attempts and remove temporary probes;
do not lower the gate, invent a root cause, or declare M2 fully accepted.

Current projected PVR packets do not satisfy the current world-only Remix
adapter. Known projection, actual scene material/surface data and an isolated
runtime/completion/readback path are missing. This limited no-go is not a
negative GPU test and does not disprove future Remix use. Another renderer
library cannot itself recover missing game camera data.

Next permit one offline, explicitly camera-relative relighting approximation
from the existing moving interval, with assumptions, conservative coverage,
light-off identity and falsifying controls. It must deliver a moving comparison
or an input-specific no-go, not another generic harness. No production renderer
replacement, live external configuration, proprietary dependency acquisition,
or combined neural presentation is authorized by this experiment. Preserve
Faithful public Auto and user-selected Uncanny separately.

## D-001: required surface and scope

Use the existing DX11 and DX11/OIT PVR renderers. Public NGX D3D11 is the first
backend. D3D11On12 supplies a D3D12 device/queue without a new PVR renderer.
RTT never enters the neural stage; direct-framebuffer frames reset history and
bypass evaluation. Evaluation cadence follows emulated display-bound frames.

## D-002: atomic input package

Renderer instrumentation produces one renderer-neutral `NeuralFrame` after the
display-bound scene and before OSD/ImGui. Translucency is represented through
confidence/bias rather than exported depth. Modifier volumes do not contribute
to neural depth.

## D-003: C++17 view adaptation

The pinned project requires C++17 (`CMakeLists.txt:62`). `std::span` is C++20.
The frame contract therefore uses a trivial pointer/count `ArrayView` with the
same non-owning lifetime semantics rather than increasing Flycast's global
language requirement.

## D-004: local baseline build deviations

The canonical Windows CI lane is MSVC x64 + Ninja Release with `USE_DISCORD=ON`.
This host lacks the CI-installed DirectX June 2010 SDK, so the local DX11
baseline uses `USE_DX9=OFF`. Upstream `ENABLE_CTEST=ON` also leaves
`BUILD_TESTING` cached independently and its Windows `HttpTest.cpp` requires
libcurl even though the Windows application uses WinHTTP. Existing tests will
therefore be exercised using the repository's Linux CI configuration; the
Windows application baseline is built with both test switches off.

## D-005: DLSS 5 boundary

Flycast implements only the public NGX DLAA/SR contract and never names private
feature IDs or inspects/configures third-party modules. Experimental DLSS 5 is
route-neutral: a compatible direct D3D11 consumer, D3D11-to-private-D3D12
bridge, or conditional D3D11On12/native-D3D12 route may consume that public
contract. Public NGX success is not DLSS 5 proof; FC-066 and Gate 10 require
positive returned-output consumption and presentation evidence.

## D-006: pinned community reference

Behavioral reference is DLSS5-Feeder v0.10.0-beta.2 at
`b60a8ffe4073dd65f8dbf804e47886607919b6b6`. Later tags exist and are
deliberately not followed. The reference is not a dependency and is not
vendored.

## D-007: Phase 1 fixture-driver boundary

The initial ROM-free harness uses a real headless D3D11 device and GPU
rasterization through a test-only driver. Flycast currently builds its renderer
inside the application executable rather than as a linkable core library. The
test driver permits early validation of artifact I/O, deterministic hashing,
threshold failures, and genuine-resolution sampling, but it is not accepted as
production DX11/DX11-OIT evidence. `rend_context`, the real TA parser, analytic
depth/motion truth, and production renderer linkage remain required before the
Phase 1 gate can close.

## D-008: production instrumentation seam is metadata-only initially

The guarded DX11 and DX11/OIT seam snapshots real `rend_context` OP/PT/TR
metadata and submits one Geometry package before native display composition.
Its initial color and depth references used existing renderer resources only to
establish ownership and cadence: DX11 color was BGRA8, the base depth resource
had no shader-resource view, and OIT exposed its typeless depth view. Those
references were not accepted as the required RGBA8/R32 neural inputs. No
presentation selection depends on stage output until all dedicated exports and
their gates exist.

## D-009: OP/PT depth is replayed into an isolated R32 target

The first production depth export replays only each render pass's opaque and
punch-through lists into a three-slot `R32_TYPELESS` resource ring using a
`D32_FLOAT` target and `R32_FLOAT` shader view. Existing shaders preserve both
legacy log-depth and native-depth behavior, including punch-through discard,
without adding a disabled-path shader permutation. Translucent and modifier
volume lists are never issued to this target. This costs extra OP/PT draws and
must meet FC-064 timing before acceptance; the choice can be replaced by an
equivalent cheaper export if measured over budget.

## D-010: instrumentation history advances on stage acceptance

The two fixed draw buffers are explicit current-work and last-successful
reference buffers. `CaptureGeometry` never rotates them. The renderer calls
`MarkEvaluated(frameId)` only after `NeuralStage::TrySubmit` returns
`Submitted`; busy, holding, unsupported, and failed frames overwrite only the
work buffer. A Geometry/FramebufferDirect source transition increments
`historyGeneration`, and direct frames carry no draw views and always request a
reset. RTT branches do not call the stage.

## D-011: incomplete motion is always biased to current color

The production DX11 export ring now owns the complete required texture format
set. Its export-only pixel-shader permutation reuses the existing texture,
alpha-test, clipping, and depth behavior for opaque and punch-through replay.
Unmatched or invalid previous-position geometry clears motion to zero and sets
bias-current-color to one with zero confidence. Exact accepted topology may now
emit geometry-derived motion under D-033. Draw ID zero remains background and
opaque/punch-through draws use global snapshot ordinal plus one. Translucent
draw-ID/mask coverage remains pending and cannot close FC-025 or FC-033.

## D-012: public NGX calls terminate at SEH leaf wrappers

The D3D11 backend uses SDK v310.7.0's Project-ID initialization, capability
parameters, `NGX_D3D11_CREATE_DLSS_EXT`, and
`NGX_D3D11_EVALUATE_DLSS_EXT`. Every external init/query/create/evaluate/
release/destroy/shutdown call is made by a POD-only SEH leaf; C++ ownership and
fallback policy remain outside those leaves. Output uses a fixed three-slot
RGBA8 UAV/SRV ring with `D3D11_QUERY_EVENT` readiness checks and
`D3D11_ASYNC_GETDATA_DONOTFLUSH`, so a busy slot skips instead of waiting.
Create uses `DepthInverted` for the proven greater-is-near PVR depth and adds
`MVLowRes` only for standard SR. HDR, jittered-MV, auto-exposure,
alpha-upscale, sharpening, and output subrect flags are off. The stable custom
Project ID is Flycast-specific but still requires maintainer/NVIDIA review
before distribution.

## D-013: external NGX feature discovery is explicit

NGX initialization uses the specification's stable custom project ID
`7d5f2a1c-3b8e-4c6a-9f0d-2e4b6c8a1d3f` and the generated `GIT_VERSION` string.
Packaged builds rely on NGX's default application-directory lookup. Local
development may set `FLYCAST_NGX_FEATURE_PATH`; its single directory is passed
through `NVSDK_NGX_FeatureCommonInfo::PathListInfo`. This keeps the separately
licensed feature DLL outside the repository while making the harness invocation
deterministic. The first attempted live run exposed that `PATH` alone does not
satisfy NGX feature discovery.

## D-014: SR inputs must match public NGX optimal settings

For each standard SR mode, the D3D11 backend calls the public
`NGX_DLSS_GET_OPTIMAL_SETTINGS` callback after capability discovery. Feature
creation is rejected unless `NeuralFrame` render dimensions equal the returned
optimal dimensions for the configured output. The harness exposes explicit
output dimensions; this prevents a successful 1:1 Super Sampling call from
being mislabeled as an upscaling result. Dynamic-resolution min/max bounds are
recorded internally but are not accepted by this first fixed-resolution path.

## D-015: D3D12 NGX work is isolated in discardable ring slots

The public D3D12 backend owns three command allocators, command lists, output
textures, and per-slot fence values. A slot whose prior fence is incomplete
returns `Busy`. NGX create/evaluate executes inside POD-only SEH leaves; an
exception or failed external call prevents list submission and replaces the
unsubmitted allocator/list. Successful work transitions output from UAV to
shader-read and executes on the caller's direct queue. Harness readback waits
are separate and do not enter the backend or emulator frame path. Current
shutdown drains submitted work before releasing NGX and is not accepted yet as
the deferred-retirement solution for runtime resize/toggle.

## D-016: the harness proves the same-device D3D11On12 surface separately

Before each D3D12 neural run, the harness creates D3D11On12 on the exact D3D12
device and direct queue, wraps a D3D12 render target, acquires it, clears it
through D3D11, releases it to D3D12, flushes, and observes queue completion.
The synthetic neural inputs are then uploaded as native D3D12 resources. This
proves basic wrapped-resource/same-queue viability, not yet that Flycast's PVR
renderer or swapchain runs on that surface; FC-045 remains open for that reason.

## D-017: presentation retains only an accepted D3D11 neural output

`DX11Renderer` keeps an owning reference to the stage output only after
`TrySubmit` returns `Submitted`. The normal framebuffer view remains the source
for every disabled, busy, unsupported, holding, failed, or D3D12-without-wrapper
frame. The selected view enters the existing content-rect quad blit, so OSD and
later UI composition retain their native order. Mode/surface changes, resource
release, and the beginning of each new submission clear the retained view.
Global renderer reset and save-state deserialization explicitly increment the
instrumentation history generation through a renderer-neutral callback.

## D-018: experimental consumer acceptance is route-neutral

The experimental mode does not auto-enable `NeuralD3D12Surface`. Native D3D11
is a valid candidate transport and is labeled `d3d11-external-unclassified`
until evidence distinguishes direct consumption from a bridge. D3D11On12 is a
separately selected conditional route. Route selection follows measured
interception, returned-output provenance, latency, correctness, stability, and
maintenance cost rather than API label.

## D-019: compatibility rebuilds follow readiness transitions

The consumer compatibility recreation is armed only by a missing-to-present
component transition. It waits a configurable count of successful public-NGX
evaluations (default 300), consumes one idempotent release request, and bounds
release/recreate attempts (default two). Telemetry records attempts, failures,
successful rebuilds, and the initiating or retry reason. The value 300 is a
configuration default, not an architectural frame invariant.

## D-020: readiness and proof are separate states

At this decision point, status and harness reports distinguished selected route, missing
components, components present, public contract evaluated, and compatibility
rebuild activity. Later Gate 10 work must add output-produced, output-consumed,
and visually-confirmed evidence as separate fields rather than infer them from
the current ladder. Loaded modules and a successful public evaluation cannot
advance the claim past `contract-evaluated`; no current Flycast route is
recorded as passing Gate 10 in this decision. D-026 records the later controlled
Gate 10 pass. D3D11On12 process classification is measured at runtime rather
than inferred from swapchain creation.

## D-021: the available Feeder source is not a Flycast bridge runtime

Feeder v0.10.0-beta.2 demonstrates a useful D3D11-to-private-D3D12 transport,
but its D3D11 path obtains color, depth, and motion from the ReShade effect
runtime and constructs its own neural contract. It therefore cannot satisfy
FC-048's requirement to mirror Flycast's exact public NGX resources, scalars,
and frame identity. It remains a design reference. The bridge route is blocked
until a compatible contract-preserving runtime is supplied or a bounded local
bridge build is separately authorized.

## D-022: the supplied add-on selects the conditional On12 route

In a corrected native-D3D11 Soulcalibur run, Flycast evaluated its public NGX
contract while the supplied RenoDX add-on installed only D3D12 NGX hooks and
reported no intercepted evaluation or feature 18 work. This blocks only the
exact supplied direct-D3D11 arrangement; it does not generalize to all D3D11
consumers. Because the same add-on positively consumes D3D12 NGX, this measured
failure activates FC-045's conditional D3D11On12 route.

## D-023: production On12 owns every flip-model backbuffer

The queue-created two-buffer swapchain is exposed to the DX11 renderer through
one wrapped D3D11 resource and RTV per D3D12 backbuffer. Acquire and release use
`IDXGISwapChain3::GetCurrentBackBufferIndex`; the previous single-buffer draft
lost the device on its first Present and is retained only as failed evidence.
Neural input rings are D3D12 resources wrapped for D3D11 export, released to
non-pixel-shader-resource state before public NGX evaluation, and accepted D3D12
outputs are wrapped back for the existing final content blit.

## D-024: public-output presentation telemetry is not visual confirmation

The production renderer tags the accepted D3D12 public output with the Flycast
frame ID, records when that view is selected by the final content blit, and
records a successful Present. These events prove application-side ownership and
cadence for the public output resource. Even when the external log records a
feature-18 evaluation at the same contract boundary, Flycast labels the resource
an external-mutation-unconfirmed candidate until a sentinel, pixel comparison,
and controlled add-on A/B prove that the presented pixels are neural output.
If the experimental route has not reached `contract-evaluated`, the public NGX
call may still run for detection/rebuild telemetry, but its output is not selected
and Flycast keeps the native framebuffer as the presentation source.
D-026 records the later paired-input and sentinel evidence that satisfies this
decision's visual-confirmation requirement for the selected On12 route.

## D-025: sentinel evidence is explicit, synchronous, and non-production

`rend.NeuralDlss5EvidenceCapture` is off by default and is accepted only for the
experimental DLSS 5 mode. When enabled, the diagnostic forces the bias-current-
color mask to zero, reads back one or a bounded series of exact color, depth,
motion, mask, and output tuples, writes a 32x32 magenta/cyan marker into the
returned public-output resource, reads that resource back again, and checks the
final D3D11On12 swapchain backbuffer before Present.
The diagnostic deliberately waits for GPU completion and logs the wait duration;
it is prohibited from performance claims and from the normal emulator path.
`rend.NeuralDlss5EvidenceStartDelayMs` may delay arming by at most 30 seconds so a
consumer toggle can be attempted before the first contract evaluation. A marker
seen in the backbuffer proves Flycast's public-output ownership and presentation
path, not that an external feature modified the pre-marker pixels.
`rend.NeuralDlss5EvidenceCaptureFrames` defaults to one and is bounded to 480. A
larger value exists only to find exact source-hash matches across controlled
external-consumer ON/OFF runs; every captured frame retains the diagnostic GPU
wait, and the same bounded count applies to pre-Present swapchain verification.
All such measurements are excluded from production timing or performance claims.
`rend.NeuralDlss5EvidenceStartFrame` defaults to zero and delays both the marker
and synchronous readback until the requested emulated frame ID. This developer-only
control permits an input-replayed gameplay frame to be proven without stalling and
marking every preceding frame; it does not alter evaluation or accepted history.
The legacy sentinel continues to force a zero bias mask by default.
`rend.NeuralDlss5EvidencePreserveMask` is a developer-only, default-off control
used by the capture verifier to replay the exact production mask and
disocclusion contract. It changes no normal rendering path.

## D-026: the supplied D3D11On12 route passes Gate 10

The selected experimental route is Flycast D3D11On12 public NGX plus the exact
user-supplied ReShade/RenoDX consumer and signed DLSSNR runtime recorded in LOG
#53 and #59. In paired 120-frame Soulcalibur runs, external-consumer ON and
explicit `EnableHooks=0` policy-OFF had identical frame IDs and identical color,
depth, motion, and mask hashes for all 120 contracts. Returned-output hashes
differed on 118 frames while the external log recorded feature-18 creation and
evaluation only in the ON run.

Frame 9 is the compact three-way proof: native color `0E9F202CA588F23F`,
policy-OFF public DLAA `80C161B4A9783CA2`, and consumer-ON returned output
`F2D37E657D2077C0`. The ON output was marked, selected by Flycast's accepted-
output blit, found at all 1024 expected marker pixels in the frame-9 swapchain
backbuffer, and completed Present tagged as frame 9. Route latency is therefore
zero Flycast display frames; the synchronous diagnostic wait is not production
GPU latency. This causal matched-contract proof establishes external output
identity without inspecting private feature implementation or binaries.

FC-056 and FC-066 are complete for this named route. This does not make the
whole neural-rendering project or the On12 surface production-ready: FC-045's
transition matrix, Dreamcast temporal-quality work, Gate 8, failure injection,
and production performance remain open.

## D-027: quality work follows renderer truth and conservative protection

The post-Gate-10 critical path is the public guidance contract, structural PVR
correspondence, geometry-derived motion, disocclusion/reactive protection,
exact content-rectangle resolution, overlay post-composition, moving title
quality, and production hardening. D3D11On12 remains the selected proven route,
not an assertion that other contract-preserving routes are impossible.

Depth polarity, motion sign/scale, SDR exposure, and content-rectangle behavior
must be established by deterministic GPU fixtures and falsifying controls before
they change production inputs. The renderer's logarithmic depth is preserved by
default. Ambiguous geometry, particles, changed texture generations, invalid
reprojection, and newly revealed pixels prefer zero motion plus current-color
bias. Public `BiasCurrentColorMask` is a temporal-safety input only; no private
semantic meaning is inferred. The full plan and Gates 11-18 are recorded in
`QUALITY-PLAN.md`.

## D-028: PVR neural depth is inverted and retains its logarithmic encoding

The deterministic Gate 11 fixture compiles the production DX11 pixel shader in
native-color and neural-export permutations. With the production zero clear and
greater/equal depth comparison, exact R32 samples are `0` for no geometry,
`0.166836038` for the farther opaque surface, and `0.263784289` for both the
near opaque and near punch-through surfaces. Color ordering agrees, reversing
submission order is byte-identical, and a deliberately conventional less/equal
control leaves the farther surface visible over the nearer one. Native D3D11
and D3D11On12 color/depth artifacts are exact.

Both public D3D11 and D3D12 NGX feature paths therefore declare
`NVSDK_NGX_DLSS_Feature_Flags_DepthInverted`; `MVLowRes` remains additive and
SR-only. Correct/inverted and deliberately normal feature declarations each
created and evaluated 8/8 frames on the RTX 5090. Their static-chart outputs
were byte-identical, so API acceptance/output equality is not used as polarity
evidence. The falsifying production-shader ordering control is the authority.
The logarithmic PVR representation is preserved.

## D-029: motion is previous minus current in render-pixel units

The Gate 12 GPU fixture carries separate unjittered current/previous positions
and applies jitter only to current raster position. Rasterized RG16F truth is
`[0,0]` for static geometry, `[-4,0]` when current geometry moves +4 render
pixels in X, `[0,+3]` when it moves -3 render pixels in Y, approximately
`[-6,+2]` for the camera-style case, and barycentrically interpolated
per-vertex displacement for deformation. Jitter without object motion remains
exactly zero.

Reprojection selects the convention empirically: correct motion has zero color
MAE, while reversed and doubled controls have MAE `47.8868056` and
`37.318971`. Public DLAA on the same previous/current pair reaches `34.529586`
dB against current color with correct motion, versus `23.734309` reversed and
`25.660892` doubled. D3D11 and D3D12 final hashes are exact for all three
cases. Production therefore retains `InMVScaleX/Y = 1`, unjittered motion plus
separate render-pixel jitter, no `MVJittered`, and `MVLowRes` only for actual
low-resolution SR inputs. This proves the contract, not yet PVR history wiring.

## D-030: SDR color is unflagged linear-contract data with unit exposure

The Q1 chart compiles and runs Flycast's production DX11 presentation quad over
`R8G8B8A8_UNORM`. Grayscale, RGB/CMY, near-black/near-white, alpha, and
checkerboard content round-trips byte-exactly with a white multiplier and no
blend; RGB is unchanged across the alpha ramp. This rules out an RGB/BGR swap,
an extra presentation gamma transform, and alpha-dependent RGB darkening in
the scene handoff.

With the separately supplied public NGX feature path made explicit, public
DLAA submitted 8/8 frames on native D3D11 and D3D11On12. Final outputs were
byte-identical across APIs. All 214,320 RGB/alpha samples taken from the
constant interiors of the ramp/patch/step/alpha regions were exact; changes
were confined to reconstruction around spatial transitions. The SDR contract
therefore keeps `InPreExposure = 1`, `InExposureScale = 1`, and no `IsHDR`
feature flag. This does not assert an HDR path.

`ComputeContentRect` returns the exact required target examples and remains
centered over an odd-dimension sweep with no one-pixel accounting mismatch.
The neural input remains scene content rather than a composed letterboxed
backbuffer; production Match Content Rectangle raster sizing is still FC-053.

## D-031: draw identity excludes pose and content revisions gate trust

PVR draw records now keep structural topology, normalized index order, UVs,
texture/second-volume identity, render list/pass/state, and strip shape apart
from centroid, bounds, depth range, matrix indices, and draw ordinal. Absolute
vertex positions no longer participate in structural identity, so a translated
object remains the same structure. Full diagnostic signatures still include
pose and revisions so captures detect actual frame changes.

The existing texture-cache `Updates` counter is carried as decoded-content
generation, the palette content hash is carried separately, and direct DX11
render-to-texture replacement increments a dedicated monotonic RTT generation.
A matching TCW/VRAM address with any changed generation is not trusted.
Second-volume generations are folded independently from immutable TCW identity.

For exact structural buckets of at most eight draws, correspondence uses a
deterministic Hungarian minimum-cost assignment over centroid displacement,
bounds/scale change, depth-range change, and ordinal proximity. Each accepted
match records its assigned and next candidate cost for later confidence work.
Larger repeated buckets are classified ambiguous at zero confidence. This is
the conservative particle policy; it does not manufacture a confident match.

## D-032: previous positions are accepted-history data with explicit validity

The instrumentation owns two bounded CPU geometry snapshots aligned with its
draw-history buffers. Capture writes only the current candidate; the buffers
swap only in `MarkEvaluated` after the neural stage accepts that exact frame.
Skipped and failed evaluations therefore cannot replace the position reference.
Both vertex and index history are capped at 1,048,576 elements; allocation or
cap failure marks the package truncated and resets temporal history.

For exact topology, current and accepted indices are paired at the same strip
position and the accepted XYZ is written at the current vertex index. This
retains per-vertex deformation and repeated/degenerate strip positions. If two
draws map one current vertex to different accepted positions, that vertex is
invalidated. Reindexed geometry with the same vertex cardinality may use local
vertex ordinals only when a least-squares similarity fit has RMS residual at or
below 0.25 render pixel and scale in `[0.5, 2.0]`; otherwise it is rejected.
Resets, truncated frames, and out-of-range indices emit validity zero. The
original Naomi 2 zero-validity boundary is superseded only for the exact-
topology, accepted-matrix case proven by D-073; applying current matrices to
prior model-space positions remains forbidden.

## D-033: production motion is a validity-gated second vertex stream

The normal DX11 guidance replay, including the base replay used after the OIT
scene resolve, binds accepted XYZ plus validity in input slot 1. Its neural-only
vertex permutation carries current and previous unjittered screen positions as
separate `noperspective` interpolants. The pixel permutation writes
previous-minus-current in render pixels. This deliberately does not derive
current position from `SV_POSITION`, so later current-frame raster jitter cannot
leak into the motion vector.

Match confidence below 0.5, any invalid vertex in a draw, or motion above 128
render pixels produces zero motion, zero confidence, and public
`BiasCurrentColorMask=1`. The production-shader fixture proves `[-4,+3]`, draw
ID 7, mask 0, and confidence 255 for trusted motion on native D3D11 and
D3D11On12; invalid and excessive controls are fully protected. D-073 extends
the same contract to exact-topology Naomi 2 using retained accepted matrices.

If the second stream cannot be allocated or uploaded, the atomic export reports
failure and the frame is not submitted to the neural stage, so it cannot advance
accepted history with cleared or stale guidance.

## D-034: confidence is accepted-history evidence, not a fixed tier constant

Both exact-identity and topology-compatible repeated buckets use deterministic
minimum-cost one-to-one assignment and retain the selected and next-best costs.
Resource-generation disagreement cannot enter either assignment. Oversized
buckets and explicitly reactive draws remain zero-confidence. A reindexed draw
must additionally pass the bounded similarity-fit residual and scale gates.

Confidence is aged against the last successfully accepted neural frame, not the
last emulated frame: one and two skipped evaluations attenuate trust, while
three skips reject it. Less than 35 percent matched current draw area is a scene
cut; all current matches and previous positions are invalidated and history is
reset. Production magnitude and per-vertex validity gates remain the last line
of defense and write zero motion/confidence plus full current-color bias. The
Gate 13 controls cover reordered repeated objects, texture/palette/RTT revision,
oversized and reactive particle cases, rigid reindex, non-rigid residual,
shared-vertex conflict, skipped history, and scene cut. Pixel-level accepted
depth/draw-ID disocclusion is separate Gate 14 work.

## D-035: disocclusion compares against only accepted GPU guidance

Each production draw exports both its current R16 identity and the matched
accepted-frame identity. After OP/PT guidance rasterization, a full-screen pass
reprojects by the proven current-to-previous render-pixel vector and samples the
depth and draw-ID textures belonging to the last successfully submitted frame.
The resolved public bias mask is one when reprojection is outside the raster,
either depth is clear, accepted identity disagrees, or encoded logarithmic depth
differs by more than `max(0.0015, 0.01 * max(current, previous))`. It preserves
the renderer-authentic logarithmic depth rather than substituting a linear form.

The three-slot renderer ring never chooses the retained accepted-guidance slot
for a new export after a busy or failed evaluation. Accepted ownership advances
only beside `MarkEvaluated` after `SubmitStatus::Submitted`; resize, mode/surface
change, and resource release discard it. The first frame copies the rasterized
base mask byte-for-byte. D3D11On12 acquires and releases accepted wrapped depth
and identity resources separately from the current export set.

The Gate 14 production-shader fixture proves static, depth-tolerant, and
camera-pan continuation remains trusted while out-of-bounds motion, depth
disagreement, crossing identities, newly visible pixels, revealed background,
and scene-cut pixels are protected. Native D3D11 and D3D11On12 masks are exact.
Removing the pass misses 192 protected pixels and leaves synthetic trail energy
12,288 versus zero with the pass. This closes disocclusion behavior, not yet
translucency or overlay classification.

## D-036: translucent pixels are reactive coverage, never opaque neural depth

The normal DX11 path replays the translucent list after opaque/punch-through
guidance into only `BiasCurrentColorMask`. It binds no depth target, forces
zero confidence and full current-color bias, and therefore cannot turn smoke,
particles, glass, or framebuffer feedback into authoritative geometry. The
existing texture/alpha/clip shader permutations determine raster coverage;
modifier volumes are not replayed and remain lighting/shadow operations.

The DX11 OIT final resolve additionally writes an R8 reactive target from the
actually visible A-buffer stack. OIT UAVs occupy u2/u3 so scene color and
reactive coverage can coexist at RT0/RT1. The final coverage is merged into the
same conservative base mask with a discard-on-zero pass. Earlier multipass and
non-auto-sorted translucent geometry remain protected by the normal list
replay. No translucent fragment writes neural depth or trusted motion.

The exact production OIT resolve is compiled and executed by the Gate 15A GPU
fixture. Empty/modifier-only, single-layer, and multi-layer controls run on
native D3D11 and D3D11On12 and must be byte-identical. The synchronous Gate 10
evidence mode still copies its explicitly cleared base mask and does not allow
the later disocclusion pass to change that diagnostic contract.

The first integration bound the merge destination at RT1 even though the
full-screen merge shader writes `SV_Target0`. The subsequent Gate 15B slice
corrected that slot and extended the fixture to begin with an independent base
mask, merge disjoint OIT coverage, and require their exact union. This negative
finding is retained in LOG #71 and the regression now executes on both surface
types.

## D-037: protect only stable HUD evidence and composite the original bytes late

Automatic overlay classification is deliberately narrower than ordinary
reactive classification. A candidate must be non-opaque, screen-aligned,
constant-depth, edge-anchored, late, bounded in area, stationary for three
accepted frames, and use a texture repeated within the frame. Structural
identity and exact bounds carry stability across accepted history. Moving,
interior, opaque, perspective, unique-texture, RTT, Naomi 2, and degenerate
controls remain world geometry. Ambiguity therefore keeps scene content in the
world rather than deleting it.

The production neural export writes the accepted classification to a separate
R8 target. After the neural scene has been placed in the exact content
rectangle, a production full-screen shader discards unclassified pixels and
restores classified pixels from the original PVR scene. Flycast OSD and ImGui
remain later. A per-game `NeuralOverlayPolicy` supports automatic, protect-full-
PVR-frame, and disable-post-composite policies; diagnostics always name the
game ID and active policy. Flycast does not edit an external consumer.

Framebuffer-direct content already takes native fallback. Generative mode also
uses a conservative predominantly-2D detector: at least 90 percent of eligible
draws must be screen-aligned and planar, with meaningful frame coverage. Three
consecutive candidates enter the bypass and three consecutive 3D frames leave
it, preventing one-frame native/neural alternation. Entering the bypass resets
history; bypassed frames are not submitted and cannot advance accepted history.

The exact production composite fixture protects 33 pixels byte-for-byte and
leaves every unclassified pixel equal to the neural input. Omitting the
composite creates 33 protected-pixel mismatches. Native D3D11 and D3D11On12
artifacts are byte-identical. This closes the mechanism and conservative default
classifier, while title-level visual acceptance remains Gate 17.

## D-038: target-native modes own the exact content raster; SR retains manual input

`NeuralMatchOutputResolution` is enabled by default but takes effect only for
public DLAA, hook-compatible DLAA, and the external-consumer experiment on the
normal or OIT DirectX 11 renderer. For a screen frame it replaces the discrete
height scale with the exact aspect-correct content dimensions after final output
size and rotation are known. It never changes RTT sizing, framebuffer-direct
fallback, non-DX11 rendering, or the stored manual 0.5x through 9x choice.

Public SR and passthrough deliberately retain the manual raster so a lower input
can be supplied. On the 2560x1440 production fullscreen control, Soulcalibur's
4:3 target-native input/output was exactly 1920x1440 with content origin (320,0),
while the 2x SR lane remained 1280x960 into 1920x1440. This separation prevents
the match option from silently turning Quality SR into an invalid same-size
contract.

The public preset selector exposes only the documented Auto, J, and K hints and
sets the corresponding public DLAA/Quality/Balanced/Performance/Ultra-
Performance parameter keys before feature creation. Sharpness remains zero.
Flycast explicitly labels this as a public DLAA/SR control; it does not claim to
select or configure the external Neural Rendering model.

On the 240-frame textured-checker-edge fixture, Auto and K were pixel-identical
on both D3D11 and D3D12, while J differed from Auto in 399 pixels in the final
frame. Auto therefore remains the default: K has not demonstrated a benefit
over Auto across the required title matrix. This is preset-contract evidence,
not full Gate 16 quality acceptance.

## D-039: quality capture is explicit, bounded, and provenance-conservative

The production quality writer exists only when `NeuralCaptureDirectory` is
non-empty and `NeuralCaptureFrames` is positive. It synchronously stages the
actual DX11 production resources and is therefore developer-only, off by
default, capped at 240 frames, and categorically excluded from performance
measurement. It runs after the late PVR overlay composite and before Flycast
OSD/ImGui. D3D11On12 wrapped inputs are explicitly acquired and released.

Native PVR color and actual contract source color are separate artifacts. This
separation exposed a production defect in which the copy pass inherited a stale
blend state and wrote black source color; the copy now binds opaque blending.
Public output is written only for non-passthrough public-NGX results, and an
external Neural Rendering artifact is written only when the supplied contract
reports an evaluated frame. Readiness, module detection, and passthrough cannot
be mislabeled as DLAA or external output.

`neuraltest capture` owns a transient launch configuration, preserves media
outside the repository, never records its path, validates the destination, and
requests clean window closure after the completion marker. Profile and style
selection affect Flycast-owned policy and recorded recommendations only; the
launcher never writes third-party configuration.

## D-040: Faithful Dreamcast Remaster is the default policy, not a completed claim

Flycast exposes Faithful Dreamcast Remaster, Enhanced Materials, Photoreal
Experimental, and Uncanny Cinematic labels plus explicit style families.
Faithful is the factory/automatic default,
retains the conservative temporal mask and character protection, and recommends
zero tone plus the lowest useful user-controlled structure setting. Photoreal
and Uncanny are marked non-faithful and are never automatic. Uncanny remains a
persistent user-selectable candidate default and recommends the exact proven
Cinematic/Structure-200/Tone-75/max-coverage experiment. An explicit sprite-heavy/2D
style requests generative bypass; the independent production scene classifier
still owns automatic menu/2D bypass.

These descriptors, UI recommendations, capture metadata, and explicit 2D
bypass are implemented. They do not yet constitute title tuning or Gate 17:
broader Enhanced Materials trust, character/face title rules, external setting
comparisons, and representative moving sequences remain evidence-gated.

## D-041: comparison indexes expose provenance and never choose a winner

The capture index discovers only production packages that contain actual source
and final-composite images. It emits relative paths, lazy-loads only artifacts
that exist, and displays the manifest's actual renderer/API, evaluation status,
external-contract state, and failure reason beside every frame. Earlier failed
or incomplete packages may therefore remain visible without being presented as
accepted results. Its JSON contract fixes `winner_declared` to false; moving
sequences and numeric review remain required for Gate 17.

## D-042: performance telemetry is asynchronous and queue-scoped

Performance mode is a separate explicit path from quality capture and the Gate
10 sentinel. It creates a bounded 12-slot timestamp/disjoint-query ring and
polls completed data only with `D3D11_ASYNC_GETDATA_DONOTFLUSH`. No texture is
mapped, no context is flushed, and no query wait can alter emulator/audio
cadence. Synchronous capture disables the tracker. Native performance sets
NeuralMode off so its baseline does not pay guidance-export cost.

Markers bracket base PVR work, guidance export, stage evaluation, and the final
scene/overlay presentation blit. The full first-to-last timestamp is labeled a
frame GPU span because it can include idle gaps; it is not summed work. P50,
P95, P99, raw samples, Present-call intervals, stage counters, query pressure,
and post-warmup local-VRAM growth are retained.

Native D3D11 timestamps can bracket public NGX work issued on the same context.
D3D11On12 evaluation is submitted on a D3D12 queue, so the D3D11 report marks
that component unavailable and writes `null`. A future D3D12 query heap is
required before that queue's evaluation time can be claimed. Native-off and
zero-accepted-submission runs also write `null` with distinct not-applicable or
not-observed scopes; timestamp-marker overhead is not mislabeled evaluation.

## D-043: failure injection is developer-only, bounded, and call-boundary local

Hidden, default-zero Flycast options can suppress a bounded number of public
feature-create or evaluate calls, return an output-ring/delayed-fence busy
status, or return a synthetic device-removed status. The capture/performance
launchers are the only supported front end. They record the selected fault,
count, and number of accepted evaluations before arming; the UI exposes none of
these options and normal execution has no active branch.

The controls do not patch NGX, remove a real device, invent an NGX result, or
write third-party configuration. They stop immediately before the named public
call or queue action and return Flycast's existing backend status. Recoverable
create/evaluate/busy faults enter the existing bounded hold after three events,
advance neither accepted output nor history during failure, and resume with a
reset after real host-present notification. Device-removed status latches the
stage on native fallback until renderer/stage recreation; it must not silently
retry a reportedly removed device. Actual DXGI device removal remains a
separate required test.

## D-044: presentation identity is measured at the actual Present boundary

An accepted public evaluation and a displayed neural result are different
events. Each asynchronous performance-query slot therefore retains the current
emulated source frame ID, the accepted evaluation frame ID, the selected output
frame ID, and whether the host actually presented that slot. Native D3D11 now
retains its output identity just as D3D11On12 already did. The final Present
notification supplies that slot's CPU interval and presentation fact; false or
suppressed presents cannot be promoted from a queued draw.

Reports deterministically derive accepted-but-not-presented output, source and
output repeats/gaps, native/neural alternation, identity mismatch, and
frame-latency counters after sorting resolved GPU samples by submission
sequence. These are diagnostics, not an instruction to reuse stale output:
rejected frames remain native and an accepted experimental candidate withheld
for a missing external contract is explicitly counted rather than mislabeled
as displayed Neural Rendering.

## D-045: window transitions are process-scoped and evidence-gated

The production performance launcher may apply one explicit Win32 transition
sequence to the Flycast process it created: delayed resize, minimize, restore,
and exact resize-back. It never searches by title, manipulates another process,
or reports a transition as complete from an API request alone. The launch
report retains the delay and each observed action result, and the run fails if
the performance interval completes before the whole sequence.

Transition-time native fallback is permitted when a public feature or its
resources are temporarily unavailable, but it is not hidden: frame-identity
telemetry must count native/neural transitions and accepted-output disposition.
No stale output, identity mismatch, repeat, missing Present, or unbounded wait
is permitted. This bounded sequence does not substitute for the remaining
fullscreen, monitor-move, renderer-restart, or actual device-loss matrix.

## D-046: renderer-restart evidence must finish after a fresh sampler lifetime

The production launcher may request one hidden, default-off in-process
renderer/API-context teardown and recreation at an exact main-frame threshold.
The process-lifetime trigger prevents a restart loop. Flycast emits a completion
marker only after the replacement renderer initializes, and the launcher checks
the marker's exact frame and renderer instead of accepting file existence.

Renderer destruction also destroys the asynchronous performance tracker. A
passing run therefore requires a new warmup and the full requested measured
interval from the replacement renderer. Native fallback remains available
through the transition, and no synchronous evidence mode is enabled. This test
proves same-renderer context restart; it does not stand in for a renderer/API
switch, game load/unload, real device removal, or fullscreen coverage.

## D-047: renderer-variant switching is bidirectional and destination-measured

The developer transition may switch between normal DX11 and DX11 OIT in either
direction while preserving the selected native-D3D11 or D3D11On12 surface. It
is mutually exclusive with same-renderer reinitialization. The process-lifetime
trigger changes Flycast's real renderer selection; the ordinary main loop owns
teardown and replacement initialization.

The marker must contain the exact main frame, source renderer, destination
renderer, and sampler restart. Acceptance then requires the destination
renderer to identify itself in a fresh completed performance report. This is
stronger than merely observing a settings change, but it is not evidence for a
D3D11/D3D11On12 surface change or another graphics API.

## D-048: neural-surface switching is separate from renderer switching

Native D3D11 and D3D11On12 are two neural/public-NGX surface implementations
under the same DX11 renderer selection. The developer transition flips only
that surface option, forces the ordinary renderer/API-context teardown and
recreation path, and retains the normal or OIT renderer variant. It is mutually
exclusive with the reinit and renderer-variant controls so every run has one
unambiguous transition.

The exact source/destination surface marker is necessary but insufficient. The
replacement performance tracker must also identify the destination API and
finish a fresh bounded interval. This proves in-process movement between the
two selected surfaces; it does not prove a switch to another graphics API.

## D-049: fullscreen evidence uses Flycast's real F11/SDL path

Fullscreen validation sends unmodified F11 key messages to the launched
Flycast window so the normal SDL event handler calls
`SDL_SetWindowFullscreen(SDL_WINDOW_FULLSCREEN_DESKTOP)`. The harness must
observe the process-owned window become exactly monitor-sized, send F11 again,
observe windowed state, and verify the exact original rectangle is restored.
Requests alone do not pass.

SDL/Windows propagation is allowed one second between action and observation.
This covers Flycast's supported borderless desktop fullscreen path; it must not
be relabeled exclusive fullscreen. All cadence, identity, fallback, reset, and
VRAM measurements continue through the transition.

## D-050: game lifecycle evidence spans a real same-media unload/reload

The developer lifecycle control retains the current media identity only in
process memory, invokes `Emulator::unloadGame`, observes that content state was
cleared, then invokes `Emulator::loadGame` and `start` for the same media. It is
process-lifetime, hidden, default-off, and mutually exclusive with renderer or
surface transition controls. Its marker does not expose the media path.

Unlike renderer recreation, the performance tracker is intentionally not
restarted. It spans the lifecycle boundary so reset-driven source-ID gaps,
stale output, false continuity, and native/neural alternation can be measured.
Source discontinuity is expected and recorded; output identity mismatch,
accepted-output loss, stale repeat, or silent alternation is not permitted.

## D-051: save-state lifecycle evidence uses a real cross-platform memory image

The existing GUI `-2` quick-state slot is backed by `fmemopen` and is therefore
not an in-memory path on Windows. The neural developer control instead uses the
same `dc_serialize`/`Emulator::loadstate` production boundary with a bounded
process-owned byte vector. The write pass may be smaller than the dry sizing
pass because TA contexts reserve their maximum size during dry-run; the vector
is reduced to the completed write length and is never written to storage.

At an exact main-frame threshold the emulator stops, serializes, and resumes.
After a bounded frame delay it stops, deserializes through the normal
`Event::LoadState` notification, and resumes. The marker records the exact
save/load frames and nonzero byte count without media paths. Performance
sampling spans the load so the required history reset, source discontinuity,
fallback choice, output identity, and latency remain observable. A counted
native frame at that boundary is permitted only as explicit fresh-frame
fallback; stale or falsely continuous neural output is not.

## D-052: pause/resume evidence uses Flycast's actual GUI state machine

The developer pause control invokes `gui_togglePause` at an exact main frame,
requires the observed state to be `GuiState::Pause`, allows the main/UI loop to
continue for a bounded number of frames, invokes the same control again, and
requires `GuiState::Closed`. It does not call emulator stop/start directly and
therefore exercises the same input, audio, emulation-thread, and GUI ownership
used by an interactive pause.

Performance sampling is not restarted. No rendered samples are manufactured
while the game is paused; after resume, the existing tracker must complete its
requested measured interval. A source-frame gap is expected. Any native frame
must be counted as explicit fresh fallback, while missing Presents, accepted-
output loss, identity mismatch, stale repeat, or frame latency fails the run.

## D-053: resource accounting is scoped to Flycast-owned neural GPU objects

Performance telemetry counts the live GPU resources, views, queries, command
allocators, command lists, and fences created and owned by Flycast's neural
export and public-NGX backends. Borrowed device/context/queue pointers, aliasing
presentation references, NGX handles/parameters, and opaque driver or external-
consumer allocations are excluded. The report names this scope rather than
presenting the count as process-wide GPU allocation truth.

Every resolved performance sample carries renderer and backend counts. The
report publishes initial, minimum, maximum, final, growth, and final component
counts. A complete launcher run requires those fields. This complements DXGI
local-VRAM usage: stable object count can reject an object leak even when driver
memory accounting fluctuates, but it cannot prove the lifetime of objects owned
inside NGX or a supplied external consumer.

## D-054: active runtime-unavailable injection retires and latches

The default-off `runtime-unavailable` failure control becomes eligible only
after its exact accepted-evaluation threshold and may fire exactly once. Before
retirement, each backend nonblockingly verifies that all submitted output-ring
work is complete. A busy ring returns `Busy` and defers the injection; it never
forces a flush or waits for GPU completion.

Once eligible, the backend releases the public feature and parameter session,
shuts down its NGX API instance, releases its owned output/query/command
objects, clears the stage output, and returns a distinct terminal backend
status. The stage then latches native fallback until recreation. This proves
Flycast's controlled response to an active runtime becoming unavailable; it is
not represented as physical deletion or unloading of a third-party DLL.

## D-055: capture JSON is locale-independent and indexes validate dimensions

Production capture metrics, manifests, and completion markers use the classic
locale so integer dimensions and floating metrics cannot acquire host-specific
grouping separators. The comparison index accepts a package only when
`render_size` and `output_size` are strict two-element unsigned arrays and
`content_rect` is a strict four-element unsigned array, in addition to the
required source/final images.

Malformed packages remain on disk and are counted as rejected in the HTML and
JSON index. They are not silently rewritten or linked into accepted evidence.
This preserves failed attempts while preventing syntactically valid but
semantically wrong arrays such as a locale-grouped 5120 becoming two values.

## D-056: external contract evaluation is not output confirmation

Quality metadata carries separate `external_contract_evaluated` and
`external_output_confirmed` fields. Consumer components or Feature 18 activity
may set only the first. `neural-rendering-output.png` and
`neural_rendering_output_present=true` require the second, which remains false
until a capture-specific mutation and presentation proof is implemented.

Public output is still retained as `public-dlaa-output.png`. This prevents an
unconfirmed candidate from being relabeled while preserving the data needed
for a later paired proof.

## D-057: external quality capture confirmation is a fail-closed three-run proof

An unmarked quality capture is not confirmed in-process from module state or
Feature 18 logging. Its schema-3 manifest records the exact raw color, depth,
motion, bias-mask, and returned-output FNV-64 values. A separate bounded
verifier requires a same-build ON sentinel replay with all five values equal,
1024/1024 marker pixels observed in the swapchain, and completed Present on
that evidence frame. It also requires a same-build, explicit-policy-OFF replay
with the four input values equal and the returned output different.

The ON sentinel and OFF policy records may corroborate the result but cannot
individually promote it. Validation is completed for every candidate before
any write, and a mismatch leaves the clean capture unlabeled. Successful
promotion copies the already captured unmarked candidate, attaches the exact
ON/OFF frame and hash record, and remains categorically ineligible for
performance claims. The verifier reads logs only; it does not inspect or write
the supplied component binaries or configuration.

## D-058: D3D12 evaluation timing retires through the existing output ring

D3D11 timestamp queries cannot measure work executed on Flycast's dedicated
D3D12 neural queue. When production performance telemetry is explicitly
enabled, the D3D12 backend therefore owns one optional timestamp query heap and
one readback buffer with two queries per output slot. The queries bracket only
the public NGX evaluation recording. Results are mapped only when the slot's
existing submission fence is already complete; the timing path never waits,
flushes, or enables synchronous evidence capture.

Each retired duration retains its originating emulated frame ID. The report
filters aggregate percentiles to accepted frame IDs in its bounded measurement
window. Because retirement is delayed, D3D12 per-frame timing fields remain
null rather than being attached to the wrong current frame. The aggregate is
labeled `d3d12-backend-asynchronous-timestamps`; on an intercepted route it is
the inclusive command-list evaluation span and is not an isolated external-
consumer cost. The bounded synchronous quality-capture path may request the
same exact-frame timing, but remains labeled capture-only and ineligible for
performance claims. Ordinary rendering creates no query resources and performs
no timing readback.

## D-059: restored evidence presentation enables exact-input temporal review

The synchronous developer evidence path has two explicit presentation modes.
The default `marker` mode preserves Gate 10 exactly: the 32 by 32 sentinel is
presented and read back from the D3D11 swapchain. The opt-in `restored` mode
snapshots the evaluated D3D12 output, performs both unmarked and marked
readbacks, then restores the unmarked output before D3D11On12 release and final
sampling. Its log record reports `marker_presentation=restored`; a zero-marker
final candidate is not accepted without the same five-hash ON proof, marked
swapchain readback, completed Present, and exact-input policy-OFF control used
by D-057. This path remains synchronous, developer-only, and performance-
ineligible.

For exact-input quality comparison only, an experimental policy-OFF capture may
retain the accepted public D3D12 output through the existing D3D11On12 wrapper
when quality capture, synchronous evidence, and restored presentation are all
active. That retained view is written only as `public-dlaa-output.png`; it is
never assigned to the experimental presentation view. Final composition stays
native, and its byte identity is required by the capture. Ordinary policy-OFF,
performance, and presentation paths do not create or retain this reference.

## D-060: temporal comparisons require unique byte-verified input correspondence

Nominal emulated frame IDs are not cross-process state identity. The bounded
capture-comparison command pairs only unique four-input contract hashes, with
equal sequence lengths, consecutive chronology in both runs, and fixed
build/game/API/renderer/target geometry. It verifies the saved raw/decoded
source, depth, motion, mask, and output against their manifest hashes and
compares each paired input buffer byte-for-byte. Ambiguity is rejected rather
than resolved with draw order or arbitrary first-match selection.

An external artifact must already carry the complete mutation/presentation
confirmation record. A public reference must not have an evaluated external
contract, and its output hash must match the external proof's policy-OFF hash
when paired to external output. The comparison is a read-only consumer of
existing evidence, not a new transport proof or a new source of confirmation.

RGB fidelity and raw temporal change remain separate components, with alpha
accounted separately and exact-image PSNR represented without invalid JSON
infinity. Lower raw temporal change may reflect blur, altered motion, or scene
cuts, so it cannot alone justify a stability claim. Reports never select a
winner and are written only after every candidate passes validation.

## D-061: guidance follows actual sorted submissions; empty HUD masks are not proof

Triangle sorting leaves translucent PolyParam.first/count in vertex-strip
space while emitting actual index ranges in SortedTriangle. Guidance must use
the submitted triangle ranges and triangle-list topology, not reinterpret the
original vertex offsets as indices. Preserve original list ordinal slots as
empty placeholders for sorted passes and append the actual sorted submissions;
the export shader receives that exact appended ordinal. Multiple sorted spans
sharing render state remain separate records. Sorted translucent motion stays
zero/untrusted and coverage never writes authoritative depth. OIT and per-strip
indexed-strip submissions retain their existing paths.

Capture-only draw diagnostics and nonempty protected-pixel counts are required
before claiming that the real-game HUD participates in the exact compositor.
The compositor's selected-pixel byte equality does not establish complete HUD
classification. LOG #104 reopens FC-055 and real-title Gates 15A/15B while
retaining their narrower synthetic evidence. Keep classifier widening and title
rules separate from this indexing correction; unchanged original scene/depth
buffers are a regression control, not a substitute for moving-title validation.

## D-062: overlay proof is topology-first; exact title profiles stay bounded

Screen-aligned classification is primitive evidence, not a loose draw bounding
box. Indexed strips and sorted triangle lists must decompose into complete pairs
of nondegenerate triangles whose six submitted vertices prove exactly four
axis-aligned corners. Primitive restart and degenerate connectors may separate
valid pairs; an incomplete, skewed, or mixed pair invalidates the batch. Empty
sorted placeholders do not count as texture use or geometry.

Overlay continuity is separate from motion structural identity. Generic single
quads retain UV and topology in their continuity signature. A topology-proven
batch may change atlas UVs or rebuild indices while its PVR-native bounds,
resource generations, state, and accepted depth remain stable. Large or
ambiguous occurrence buckets stay untrusted.

The exact Dreamcast game ID `T1401N` selects the visible diagnostic profile
`soulcalibur-t1401n-hud-v1`. It admits only late, bounded, stable translucent
draws wholly inside the top fifth of the 640x480 PVR screen. The two coincident
depth layers used for each name plate form a maximum-four occurrence bucket and
are paired one-to-one in sorted depth order; changed counts or depth disagreement
reset protection. This Flycast profile never writes or changes external model
settings. A named-title capture proves only that title and scene, not the
representative Gate 17 matrix.

## D-063: external settings require consumer-reported capture provenance

Flycast's profile text is a recommendation, not evidence of the settings an
external Neural Rendering consumer actually used. External confirmation now
parses the consumer's complete `DLSS5 active settings` tuple from the ON host
log. The tuple records upscaling, intensity, global tone, diffuse-white nits,
preset, style, and enabled state exactly as reported. Confirmation rejects a
missing tuple, a disabled consumer, or any tuple change within the host log.
Repeated identical reports are permitted.

The stable tuple is copied into every promoted manifest and the confirmation
report as `external_settings_proof`, with its source explicitly labeled
`consumer ON host log`. Comparison validates the typed proof, requires it to
remain identical within a lane, and exposes it separately from
`external_settings_recommendation`. Old confirmed captures remain readable but
continue to report `actual_external_settings_verified=false` when they lack
this proof. Settings may differ between A/B lanes because that is the purpose
of a controlled settings comparison.

This is evidence capture only. Flycast does not map undocumented selector
numbers to model names, infer semantic-mask behavior, or write any RenoDX,
ReShade, RHI, or consumer configuration. A consumer log line, like module or
Feature 18 activity, does not replace the existing exact-input mutation,
sentinel-Present, and policy-OFF presentation proof.

## D-064: live UI reads snapshots; guidance debug views are presentation-only

The settings UI must not read renderer-owned `NeuralStage`, texture, or history
state directly. The DX11 render thread publishes a self-contained copy through
a mutex-protected Flycast-owned status boundary. The snapshot contains the
active mode and API, submit result and reason, raster dimensions, frame IDs,
stage counters and asynchronous evaluation timing, route/readiness, overlay
state, bypass state, and developer-view state. Renderer termination resets the
snapshot so a new settings page cannot display stale device data.

The developer selector visualizes the current exported source color,
logarithmic PVR depth, render-pixel motion, resolved bias mask, correspondence
confidence, draw identity, or overlay classification. It runs only when the
selected frame has fresh guidance; a conservative bypass or failed export
cannot display a stale ring slot. It replaces only the PVR content blit, keeps
Flycast OSD and ImGui later in presentation, and never changes the guidance
buffers, evaluation, accepted-history reference, or external configuration.

A debug frame is explicitly accounted as native presentation. Even when a
neural output is ready, Flycast does not queue or log that output as presented
while a guidance view is on screen. This prevents a developer visualization
from becoming false Gate 10 or cadence evidence. Debug mode remains off by
default and is not a performance-measurement lane.

## D-065: Gate 8 uses an exact pre/post production capture boundary

Flycast OSD and ImGui exclusion is a pixel contract, not an inference from call
sites. The optional `NeuralLateOverlayProof` capture mode retains the complete
backbuffer immediately after neural scene selection and protected Dreamcast
overlay composition. After `gui_display_osd` submits its ImGui draw data, the
same frame and content rectangle are read back again. The package records the
pre-overlay composite, presented image, exact difference image, changed-pixel
count, and maximum channel delta.

The proof passes only when a real late Flycast overlay changes pixels in the
content rectangle. The bounded launcher uses Flycast's FPS OSD as that visible
control and rejects a capture with no late delta. Normal DX11 and DX11 OIT use
their own render loops and both must invoke the post-OSD boundary. This mode is
synchronous, developer-only, off by default, and never valid performance data.
It changes no neural input, accepted history, external consumer configuration,
or ordinary presentation.

## D-066: runtime mode round trips keep one continuous evidence interval

The hidden `NeuralModeRoundtripAfter` control changes only Flycast's live neural
mode: it stores the requested nonzero mode, sets mode zero at an exact main
frame, and restores the stored mode after a bounded frame duration. It does not
restart the renderer or the asynchronous performance sampler. The public stage,
guidance resources, and accepted correspondence history retire through the same
production `syncNeuralMode` path used by an interactive setting change.

Each performance sample therefore records its active mode and whether an
accepted evaluation carried `resetHistory`. Acceptance requires two mode
transitions, an all-native off interval with zero accepted evaluations, the
first accepted post-off frame explicitly reset, and no missing Present,
accepted-output loss, identity mismatch, repeated output, or frame latency. A
single requested-mode native Present is allowed only as the already documented
nonblocking busy fallback; it is counted explicitly and may not become stale
neural output. The control is developer-only, default-off, mutually exclusive
with other lifecycle controls, and changes no external-consumer configuration.

## D-067: native parity is captured at the PVR scene boundary

Gate 1 compares the production `fbTex` BGRA8 bytes immediately after the full
normal or OIT PVR resolve and before neural submission, protected overlays,
Flycast OSD, ImGui, aspect bars, or presentation. The same small synchronous
readback is compiled into neural-capable and compile-time feature-off builds;
it is hidden, default-off, bounded, and explicitly excluded from performance
measurements.

The enabled build must be in mode zero and report no instrumentation, draw
records, previous-position history, neural input layout, guidance export
resources or replay, or backend objects. Neural shader input-layout creation is
therefore lazy and is released again when mode zero is selected. The launcher
retains the exact input replay, requires raw byte equality for every paired
frame, and requires a materially different mismatched-frame control.

The feature-off build cannot instantiate D3D11On12 because that route is
compiled with the experimental neural surface. Cross-surface Gate 1 therefore
compares the enabled/mode-off production PVR path on an explicitly verified
D3D11On12 device against the compile-time feature-off native-D3D11 baseline.
The actual surface of each process is recorded; it is never inferred from a
requested option.

## D-068: Gate 2 compares production high-resolution pixels to exact nearest

Genuine PVR scaling is proven at the same pre-neural, pre-overlay production
scene boundary used by Gate 1. The deterministic launcher captures one source
frame at 1x, 4x, and 8x with the same executable and replay, and requires exact
integer dimensions, source-frame identity, Git identity, renderer, active API
surface, and mode-zero instrumentation state before comparing pixels.

The rejected hypothesis is an exact nearest-neighbor enlargement of the 1x
BGRA8 frame. Acceptance requires material differences over the complete high-
resolution raster, material differences inside source-edge blocks, and many
edge blocks with more than one high-resolution subpixel value. This prevents
texture-wide differences alone from masquerading as geometry sampling. The
all-zero nearest control is evaluated by the same predicate and must fail.
The command is synchronous, developer-only, disabled by default, and excluded
from performance claims.

## D-069: legacy gate aggregation preserves the Naomi 2 boundary

The original Gates 4-7 remain useful acceptance labels, but they do not replace
quality Gates 11-18. Current evidence closes static zero motion, wrong-history
rejection, and shared-stage passthrough identity. Normal Dreamcast Gate 5
translation, camera, deformation, topology, edge-clip, and particle controls
are also green.

Naomi 2 is not promoted from its conservative fallback by the CPU matrix test
alone. The production export still lacks accepted prior model-view/projection
matrices and therefore emits zero-validity motion for those draws. Legacy Gate
5 and FC-061 remain open until the accepted-history matrices feed the actual
Naomi 2 neural vertex shader within the 1e-3 threshold. This is a stated missing
capability, not a reason to manufacture motion from current matrices.

## D-070: focus lifecycle evidence requires observed foreground ownership

A focus transition is accepted only when the launched Flycast window first
owns foreground focus, a real visible process-owned top-level control window
then owns it while Flycast does not, and the same visible, non-minimized
Flycast window finally owns it again. API requests without the corresponding
`GetForegroundWindow` observations fail the launch. The control window is
destroyed on success, timeout, and cleanup.

The asynchronous production sampler remains active across this sequence. Its
report must still reject missing Presents, accepted-but-unpresented output,
frame-identity mismatch, stale/repeated output, native/neural alternation,
latency, or Flycast-owned resource growth. A source-frame gap paired with the
focus lifecycle reset is retained rather than hidden. This proves the Windows
focus-loss/restore boundary relevant to Alt+Tab; it does not claim that the
harness synthesized the keyboard gesture, moved monitors, or changed desktop
sessions.

## D-071: SEH is tested with an application-defined software exception

The developer-only `seh-exception` control enters the same production NGX
evaluate leaf as a real call, but raises application-defined Windows exception
`0xE0424E47` immediately before calling the runtime. This avoids altering or
faulting a third-party binary and avoids colliding with Flycast's global access-
violation handler. The local `__try`/`__except` filter must record the exact
code and return through the ordinary recoverable-failure state machine.

Three consecutive injections must trigger the bounded hold, explicit native
fallback, and reset recovery already used for runtime failures. A passing
performance interval requires both native and recovered neural Presents with
no accepted-output loss, stale output, identity mismatch, latency, crash, or
Flycast-owned object growth. This validates Flycast's exception containment;
it is not evidence that a supplied NGX runtime itself raised an exception.

## D-072: actual D3D12 removal recovery uses an isolated evidence control

The Gate 18 device-loss control removes Flycast's live D3D11On12 backing device
through `ID3D12Device5::RemoveDevice` at an exact main-frame boundary. A pass
requires both the returned device reason `DXGI_ERROR_DEVICE_REMOVED` and a newly
initialized renderer that exposes D3D11On12 again. The old performance tracker
is retired with the removed renderer; a completely new warmup and measured
interval must then satisfy the ordinary accepted/output/cadence/resource
acceptance rules. No sample from before removal is used as post-recovery proof.

This control is hidden, opt-in, mutually exclusive with other developer
transitions, and valid only on the selected D3D11On12 neural surface. It is
stronger than the synthetic removed-status injection because it invalidates the
actual process device and its resources. It is still bounded controlled-removal
evidence, not a claim that a spontaneous driver reset or TDR was observed.

## D-073: Naomi 2 motion requires both accepted matrices and exact topology

Each captured Naomi 2 draw copies its model-view and projection matrices into
the same double-buffered history generation as its draw and vertex/index
snapshot. The matrix buffer swaps only when `MarkEvaluated` accepts that exact
frame. During neural replay, the current object-space vertex uses current
matrices while the second vertex stream uses the accepted object-space XYZ and
accepted matrices; both paths share the current unjittered NDC transform before
producing render-pixel previous-minus-current motion.

Matrix presence, exact strip/index topology, draw confidence, and every vertex
validity bit are all required. Reindexed Naomi 2 is deliberately rejected even
when ordinary Dreamcast geometry could use a similarity fit, because the fit
would not independently prove object-space correspondence under per-draw
matrix history. The production HLSL fixture must return analytic `[-4,+3]` on
native D3D11 and D3D11On12 and its missing-history control must fail closed.

## D-074: Faithful settings must preserve identity before adding enhancement

External Neural Rendering settings are accepted only when the consumer reports
the active tuple and the existing marker/policy-OFF proof binds it to exact
color, depth, motion, and mask inputs. A lower raw frame-to-frame delta alone is
not a win. Trail energy, silhouette displacement, thin-line continuity, color,
saturation, black level, source identity, and moving visual review are Faithful
constraints; material uplift is considered only inside them.

For the deterministic Soulcalibur interval, intensity 0.125 with Natural style
is the least damaging nonzero tuple tested, but it still loses public DLAA Auto
on every listed constraint and shows no clear material benefit. Global tone 0
does not change this SDR output, and external preset 2 is byte-identical to
preset 0 with upscaling disabled. Intensity 0 is not an external winner: it
fails provenance because no policy-OFF output difference exists. Public DLAA
Auto remains the Faithful default until a nonzero external candidate wins after
production raster jitter and broader legal-title evidence.

## D-075: external control UX remains separate from Flycast's neural contract

The supplied consumer already honors `NRLocalStructure`: an exact-input
Soulcalibur A/B that changed only `NRLocalStructure` from absent to `2.0`
changed all 30 returned frames (mean RGB MAE `1.401243`, maximum channel delta
36). The host log did not report that field, so it is labeled requested plus
isolated-output-proven rather than consumer-reported. This is sufficient to
prove the control is functional, but not to weaken fail-closed provenance.

Do not insert DLSS5-Feeder into Flycast's selected D3D11On12 route. It produces
a separate image-derived contract and would compete with the emulator-native
depth, motion, mask, history, and presentation contract. A source-controlled
companion in the Feeder checkout may edit only an explicitly selected text
configuration after explicit `--apply`, create an exact backup, and print the
requested tuple. It must never load or implement Neural Rendering, claim the
consumer's active state without log/capture evidence, or become a Flycast
runtime dependency.

## D-076: production jitter uses a separate scene replay and never owns fallback

Normal DX11 may apply bounded Halton jitter only to the separately replayed
neural scene. Current and accepted previous positions remain unjittered for
motion, and the jitter is reported independently to NGX. The ordinary PVR
framebuffer is never jittered or replaced in place. Protected overlays disable
the replay for that frame, and DX11 OIT remains zero-jitter with explicit reason
`oit-scene-replay-pending` until it gains an equivalent proven replay.

Acceptance requires content-bearing production frames, not a uniform startup
frame. Exact native-control comparison must prove the PVR framebuffer is
byte-identical while the neural source differs. An injected failure on an
actively jittered frame must emit no public output and present that same native
frame byte-for-byte. These conditions hold independently of the selected
Faithful, Enhanced, Photoreal, or Uncanny quality-profile metadata.

## D-077: raster jitter phase follows accepted neural history

Production Halton phase is indexed by the number of successfully accepted
neural evaluations in the current history generation, not by Flycast's
absolute emulated-frame ID. `MarkEvaluated` alone advances the phase, while
every neural discontinuity resets it to zero with the rest of accepted
history. A skipped, rejected, or externally delayed startup frame therefore
cannot silently put two otherwise exact replay lanes on different jitter
phases.

This is required for both temporal correctness and exact-input visual
comparison. The consumer-enabled and policy-off controls must receive the same
color, depth, motion, mask, and jitter for the same accepted-history sample;
host startup timing is not part of the image contract.

## D-078: Uncanny may be user-selected without becoming the Faithful default

The exact-commit post-jitter Soulcalibur comparison accepts Uncanny Cinematic
as a valid, persistent, deliberately transformative profile. Maximum-coverage
and automatic-HUD-protection lanes both pass 30/30 exact-input external
provenance. The HUD-safe lane protects an average 15,313 pixels per frame with
zero mismatches, repeats, or drops.

This is not evidence for replacing Faithful Dreamcast Remaster. Against public
DLAA Auto, the maximum-coverage Uncanny lane raises mean source RGB MAE from
3.383 to 10.500, trail energy from 6.025 to 13.254, silhouette displacement
from 0.817 to 1.192, saturation drift from 3.893 to 12.646, and black-level
drift from 2.771 to 8.796 while reducing thin-line continuity from 77.871 to
64.874. Its lower raw frame-to-frame MAE is not sufficient to override those
identity and artifact failures. Faithful therefore remains public DLAA Auto;
Uncanny remains an explicit user preference pending broader-title review.

## D-079: the presentation sentinel has an explicit, strict corner

The synchronous developer-only Gate 10 sentinel remains a 32x32 pattern
copied into the returned D3D12 neural output before Flycast presentation. Its
default origin remains top-left. A bounded bottom-right control is permitted
when protected post-neural HUD pixels legitimately occupy the default corner,
as observed in the Soulcalibur OIT lane.

Both the D3D12 copy and D3D11 swapchain verifier derive the origin from the
same output-relative contract, logs identify the corner, and acceptance still
requires 1024/1024 pixels plus same-frame Present. Drawing a replacement marker
after overlay composition is forbidden because that would no longer prove the
returned neural resource reached presentation.

## D-080: OIT neural replay owns isolated persistent state

Production OIT raster jitter is permitted only through a separate neural scene
replay. That replay owns dedicated opaque and multipass color textures, its own
pixel-list buffer and pointer texture, and an R8 reactive target written by the
same jittered final resolve. It is initialized from the pre-native opaque base
for retained frames or the real clear color for cleared frames. It never uses
the fully resolved native framebuffer as a substitute for persistent OIT state.

The replay swaps these resources into the existing OIT submission machinery
under scoped restoration, clears the replay pointer texture before each replay,
and unbinds the u2/u3 OIT UAV slots before guidance output. Native persistent
color, pointer, and pixel-list state therefore remain untouched. RTT and direct
framebuffer paths retain native bypass, while protected overlays and
predominantly-2D frames still suppress jitter conservatively.

Acceptance requires both a production OIT shader coverage fixture and
content-bearing multi-frame parity. The fixture must move coverage by the exact
render-pixel jitter on native D3D11 and D3D11On12. The gameplay control must
leave `native-pvr-color.png` byte-identical to an unjittered run while changing
only the neural scene input. A failed intermediate implementation cleared the
native rather than replay pointer texture and was rejected before commit.

## D-081: the optional neural status panel is a late Flycast OSD

The compact neural status panel is a persistent `rend.NeuralStatusOverlay`
option exposed in Video settings and disabled by default. It consumes only the
mutex-protected `LiveStatus` copy plus Flycast's existing FPS clock. It never
queries or edits an external consumer and never reads renderer-owned state from
the UI thread.

The panel is drawn in the lower-right foreground during `gui_display_osd`,
after native/neural scene selection and protected game-overlay composition. It
shows mode, abbreviated profile, public preset, D3D11/D3D11On12 route,
submit/bypass/HUD state, render/output dimensions, active raster jitter,
FPS/frame interval, and accepted/busy/fallback counters. Drop count remains
`n/a` until a production live cadence counter is published. In DLSS 5 mode,
public-contract status is explicitly separated from `External unverified` so
the UI cannot be mistaken for Gate 10 output provenance.

Acceptance compares deterministic overlay-on/off captures. All source,
guidance, public output, protected-game composite, and native PVR artifacts must
remain byte-identical; only the post-OSD presentation artifact may differ.

## D-083: explicit user Apply may invoke the external settings companion

The user authorized an integrated intensity panel with an explicit Apply action.
This supersedes the earlier no-configuration-write boundary only for that user
action. Sliders, profile selection, mode changes, startup, and rendering never
write external settings. Apply launches the user-selected installed
`flycast-nr-control.exe` with its documented controls and selected existing
configuration. The companion creates a backup and verifies readback. Flycast
reports requested settings and restart required, not consumer-active proof.
It does not change hook policy or consumer activation implicitly.

The UI owns a bounded background invocation, hidden window, validated argument
values, and explicit executable launch without a shell. Files and third-party
components remain outside the repository; saved local paths are user options.
Public DLAA/SR and all render paths remain independent of this panel.

## D-084: reuse-first remake feasibility precedes bespoke renderer/assets

The user authorized FC-067 as the next bounded implementation on 2026-09-07.
Use public Remix SDK interfaces as the first candidate and PCSX2's experimental
integration as a reference, with RTGL1 as an alternative only after a reasoned
decision. No public-project README is accepted as Flycast runtime evidence.
The primary unknown is a coherent scene/camera contract, not path-tracer code.
Start with the M1 harness in REMAKE-FEASIBILITY-PLAN.md, then separately prove
moving-game reconstruction. Do not infer world-space geometry or normals from
screen-space PVR data or Naomi 2-only fields. Keep proprietary neural runtime
boundaries, external settings, native rendering, and existing Gate 10 untouched.

Public open-source source/header inspection and isolated builds are within the
authorized implementation; dependency/license review precedes reuse, and no
proprietary binary auto-downloads or vendor files are staged. A standalone
synthetic path-traced result is not proof of scene completeness, in-emulator
presentation, or combined DLSS 5 compatibility. Original Gates 11-18 remain
separate. Transformative appearance may depart from source art, but not from
stable motion, readable protected HUD, correct timing, or honest provenance.
Implementation routing is GPT-6 Astra/low (Astra light), as explicitly
corrected by the user; do not substitute Sol/high.

## D-085: M1 scene data and public API evidence are separate from rendering

M2 causal follow-up: compare native-only repeated runs before attributing
cross-run differences to replay. Use the existing early native readback to
exclude late framebuffer mutation, but do not promote synchronization to a
production fix. Exact early/late pixels and exact decoded/retained-buffer pixels
do not waive a failing original-source gate. See LOG #140 for both falsifiers.

Temporary per-draw probes must be removed after diagnosis; retain their patch
and evidence locally, not their overhead in normal rendering. Matching bound
objects and resource bytes do not by themselves prove driver arithmetic truth.
LOG #141 separates the observed D24 variation from the selected color error;
require a controlled repeated-shader fixture before choosing a precision fix.

The generic repeat-raster fixture uses runtime compiler settings and retains
every color/depth output plus shader source. A synthetic same-input pass is a
narrow exclusion only; actual failing draw inputs and pre-target surfaces must
be isolated before selecting any arithmetic/precision change (LOG #142).

M2 GPU diagnostic: retain pre-draw framebuffer content and the original shader
contract; use new decoded buffers without repeating the flat-shading color
adjustment. Resolve texture state only against the retained same frame, never
serialized pointers. Include an original-GPU-buffer replay control to separate
decoding from repeated native drawing. Keep original-source exactness separate
from decoded/control equality and do not silently accept a one-step tolerance.
The isolated test retains supplemental native state and is not standalone scene
reconstruction; it is restricted to explicit native-D3D11 normal-renderer captures.

M2 decode policy: parsing a saved packet never activates a renderer. Bound file
allocation and JSON construction as well as decoded arrays, reject duplicate
keys and narrowing conversions, and preserve the caller's previous output on
failure. Keep texture handles unresolved and omissions mandatory. Unknown
camera/world provenance cannot be upgraded by the loader; Naomi 2 remains
rejected until its omitted transform domain is explicitly supported.

M2 snapshot policy: preserve exact PVR buffer bits, including nonfinite unused
vertices, while separately counting invalid references. Snapshotting observed
data is not validation for 3D reconstruction. Keep the actual raster transform
separate from unknown world-camera provenance. Export only under the bounded,
explicit developer capture option; never serialize live resource pointers.

Follow-up: finish topology conversion for the entire packet before the first
external API resource creation. A later all-degenerate strip must not cause
partial resource activity. The adapter remains single-use on this failure;
no retry or implicit fallback presentation is introduced.

The M1 packet lives entirely in neuraltest, with hard aggregate bounds and
explicit unknown projection, normal, transform, texture, and omission metadata.
The first adapter rejects all but a deliberately synthetic untextured subset;
it never substitutes guessed normals/camera parameters for captured game truth.
Its reviewed Remix 0.6.4 header is external, hash-pinned, and opt-in. Tests inject
the public function table and prove argument mapping/failure cleanup only.
There is no runtime loader, renderer integration, or Present call in M1.
No API-call test, CPU reprojection, or default gray material is GPU relighting
or game reconstruction evidence. The caller must own completion and lifetime
before the adapter can ever be used with a real GPU runtime. M2 starts only
after review of this bounded contract; the separate GPU block stays visible.

## D-082: bounded comparisons reset accepted history at capture start

The synchronous developer-only quality capture resets neural instrumentation
history exactly once, immediately before the first retained frame after its
configured warm-up skip. The existing discontinuity contract increments the
history generation, requests a temporal reset, and returns the accepted-
evaluation jitter index to zero before geometry and jitter are captured.

This reset is owned by `QualityCaptureWriter`, is armed only by a non-empty
capture directory plus a positive frame limit, and does not run during ordinary
gameplay or asynchronous performance measurement. Repeating `Configure` with
the same capture identity cannot rearm it. Changing the capture identity does.
Candidate, marker, and policy-off lanes must all use this boundary; weakening
exact-input comparison or returning production jitter to absolute frame IDs is
forbidden.
