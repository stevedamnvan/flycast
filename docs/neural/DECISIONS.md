# Neural rendering decisions

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
Resets, truncated frames, and out-of-range indices emit validity zero. Naomi 2
is also validity zero until previous matrix state is carried and proven;
applying current matrices to prior model-space positions would create false
camera/object motion.

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
D3D11On12; invalid and excessive controls are fully protected. Naomi 2 stays
invalid until prior model/projection matrices are retained and proven.

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

Flycast exposes Faithful Dreamcast Remaster, Enhanced Materials, and Photoreal
Experimental labels plus explicit style families. Faithful is the default,
retains the conservative temporal mask and character protection, and recommends
zero tone plus the lowest useful user-controlled structure setting. Photoreal
is marked non-faithful and is never automatic. An explicit sprite-heavy/2D
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
