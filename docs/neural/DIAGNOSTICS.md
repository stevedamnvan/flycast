# Neural diagnostics

The Video settings page exposes a thread-safe **Live neural status** block for
the DirectX 11 renderer. It reports the active mode/API, latest submit result
and readable reason, input/output raster, source and actually presented neural
frame IDs, accepted/busy/fallback/reset and failure counters, the latest
asynchronously retired evaluation timing when one exists, Flycast-owned backend
resource count, overlay/bypass state, and DLSS 5 route/readiness. Closing or
recreating the renderer clears the snapshot rather than leaving stale values.

The developer debug selector is off by default and can replace the PVR content
with source color, logarithmic PVR depth, motion, resolved bias mask,
confidence, draw ID, or overlay classification. Motion encodes X and Y around
neutral gray with magnitude in blue; draw IDs use deterministic false color;
the scalar surfaces use grayscale. Only fresh guidance for the current source
frame is eligible. OSD and ImGui are still drawn later. Evaluation/history are
unchanged, but the visible debug frame is intentionally counted as native and
cannot satisfy neural-output presentation evidence. Do not use this view for
performance measurements or as a substitute for the raw capture package.

The production capture command is:

`neuraltest capture --game PATH --frames N --skip M --out DIR [--flycast EXE]
[--lane native|dlaa|sr-quality|dlss5] [--api d3d11|d3d11on12]
[--renderer dx11|dx11-oit] [--preset auto|j|k]
[--profile faithful|enhanced|photoreal] [--style FAMILY]
[--render-height N] [--feature-path DIR] [--input-replay yes|no]
[--evidence-frames 0..480] [--evidence-start-frame N]
[--evidence-mask zero|production]
[--evidence-presentation marker|restored] [--timeout-ms N]`

It launches Flycast with transient command-line configuration, limits a run to
1 through 240 frames, waits for a completion marker, requests a normal window
close, and reports whether forced termination was required. Missing media,
invalid arguments, a completed destination, early emulator exit, and timeout
return nonzero. It does not copy media or record the media path.

Each production `frame-NNNNNN` directory contains:

- `manifest.json`: Flycast SHA, game ID, frame/history/reset identity, render and
  output dimensions, exact content rectangle, correspondence rejection/area
  counts, API, mode, preset, profile,
  external recommendation, evaluation/provenance status, and the explicit
  synchronous/performance-ineligible label.
- `native-pvr-color.png` and `source-color.png`: original PVR scene and actual
  public-contract RGBA input. These are separate artifacts so an export-copy
  failure cannot hide behind a valid native image.
- `depth.f32`, `depth.png`, `motion.rg16f`, `motion.png`, `bias-mask.png`,
  `confidence.png`, `draw-id.r16u`, `draw-id.png`, and
  `overlay-classification.png`.
- `public-dlaa-output.png` only when a public NGX result exists. Native
  passthrough is never labeled as DLAA.
- `neural-rendering-output.png` only when the supplied external contract was
  independently confirmed for that capture; module/readiness or contract
  evaluation alone cannot create it.
- `final-composited.png`, optional `native-versus-output-difference.png`, and
  an optional `temporal-flicker.png` beginning with the second captured frame.
- `metrics.json`: temporal variance, motion reprojection error, reactive-region
  trail energy, silhouette/line measures, color/saturation/black drift, HUD
  mismatch, repeat/drop counts, invalid guidance coverage, and trusted/reactive
  percentages plus capture-only GPU spans where available. An exact D3D12
  evaluation timestamp may remain `null` when its delayed result cannot retire
  for the captured frame within the bounded developer poll.

Raw game data and user paths are excluded. Normal emulator execution performs
no capture readback. This synchronous developer-only path is disabled unless an
explicit destination and positive frame limit are supplied, and every package
is marked ineligible for performance measurements.

`--input-replay yes` is a developer-only reproducibility control and requires a
Flycast build configured with `TEST_AUTOMATION=ON`. The launcher resolves
`scripts/<media-stem>.input` beside the selected executable, fails if it is
missing, and retains the exact sequence as `input-replay.input`. The launch
record includes its locale-stable FNV-64 and byte count without recording the
source path. Normal builds and the default `--input-replay no` path are
unchanged. The Flycast log reports both successful file open and first event
application; a mere requested flag is not proof that replay occurred.

Schema-3 manifests include exact FNV-64 hashes of the raw color, depth,
motion, bias-mask, and returned-output contract resources. Hash text is emitted
with the classic locale. `--evidence-frames` remains synchronous and is valid
only for the experimental D3D11On12 lane. Its default `--evidence-mask zero`
preserves the Gate 10 sentinel contract; `production` retains the real resolved
mask and disocclusion path for an exact quality-capture replay.

`--evidence-presentation marker` is the default and presents Gate 10's sentinel
unchanged. `restored` snapshots the evaluated output, performs the unmarked and
marked D3D12 readbacks, then restores the unmarked output before final sampling.
The candidate's swapchain readback contains zero marker pixels; a separate
ON-marker run must prove 1024/1024 marker pixels and completed Present. Both modes are
synchronous, developer-only, and excluded from performance measurements.

On an experimental policy-OFF run, restored synchronous capture may retain the
accepted public result solely as `public-dlaa-output.png` for an exact-input
comparison. It never becomes the presentation view: `final-composited.png`
must remain byte-identical to the native PVR source. This exception is inactive
outside an explicit bounded quality capture.

An unmarked external candidate is promoted only by:

`neuraltest confirm-external-capture --capture DIR --on-log FILE
--on-host-log FILE --off-log FILE --off-host-log FILE --git-sha SHA`

The command first validates every frame without writing. Each candidate must
match a same-build ON evidence record in all five hashes; that ON frame must
have a distinct marked hash, 1024/1024 marker pixels in the swapchain, and a
completed same-frame Present. It must also match an explicit-host-policy-OFF
record in color/depth/motion/mask while the returned hash differs. ON consumer
activity and the OFF `EnableHooks=0` safe-mode record are required controls,
but neither is treated as presentation proof by itself. Only then is the clean
unmarked image copied to `neural-rendering-output.png`, the manifest marked
confirmed, and `external-confirmation.json` written. A mismatch leaves every
candidate unpromoted. All sentinel timings remain excluded from performance.

The ON host log must also contain one stable, enabled
`DLSS5 active settings` tuple. Missing settings, a disabled report, or any
change in upscaling/intensity/global-tone/diffuse-white/preset/style during the
log rejects confirmation. The exact consumer-reported tuple is stored as
`external_settings_proof`; it is distinct from Flycast's user-facing
recommendation and never causes a third-party configuration write.

`neuraltest capture-index --root DIR [--out HTML]` recursively discovers only
production packages containing both source and final images plus strict two-
element render/output size arrays and a four-element content rectangle. It writes a lazy-
loading HTML contact sheet and a machine-readable JSON manifest using relative
paths. Every card shows game/profile, actual renderer/API, acceptance, external-
evaluation state, and submit status; available native/guidance/public/external/
final/difference/flicker artifacts are linked in place. The index explicitly
sets `winner_declared=false` and does not convert still images into a title-
quality decision. Malformed packages are retained, excluded, and counted in
`rejected_package_count`. An empty valid root writes an empty diagnostic index
and exits 3.

`neuraltest compare-captures --a DIR --b DIR --out JSON
[--a-output external|public] [--b-output external|public]` defaults to an
external-versus-public comparison. It requires equally sized, consecutive
sequences of 2 through 240 target-native schema-3 captures and an existing
output parent directory. It refuses to overwrite an existing report.

Frames are paired by all four contract hashes, never by nominal frame number.
Duplicate identities, gaps, reversed chronology, mixed build/game/API/renderer
identity, invalid dimensions, intercepted candidates labeled as public output,
and incomplete external confirmation all reject the comparison before writing.
Profiles, presets, overlay policies, and external-setting recommendations must
also stay constant within each lane. When confirmation supplied a typed
`external_settings_proof`, that proof must also remain constant within its lane
and the report sets `actual_external_settings_verified=true` with the exact
tuple. Older captures without proof remain explicitly unverified rather than
being inferred from Flycast recommendations.
The command decodes every source/output/mask PNG, checks dimensions before
allocation, reloads depth/motion bytes, verifies all five raw FNV64 values,
and then compares all four input buffers byte-for-byte. An external/public pair
also requires the public returned hash to equal its external proof's OFF hash.
This validates already confirmed artifacts; it does not replace Gate 10 proof.

The JSON report preserves each pair's independent frame IDs and relative
manifest links, RGB MAE/MSE/PSNR against both its peer and source, alpha mismatch
counts, scene-cut labels, and consecutive raw temporal RGB MAE. Null PSNR plus
`rgb_exact=true` means infinite PSNR. Raw temporal MAE includes object motion
and scene cuts: it is not motion-compensated flicker or proof of stability.
No resampling is implicit, no automatic winner is declared, and captures remain
performance-ineligible. Existing per-frame manifests retain the profile,
external-setting recommendations, guidance metrics, and exact evidence record.

All production capture JSON streams use the classic locale. A high-resolution
manifest must encode 5120 by 3840 as `[5120, 3840]`, independent of the Windows
user locale.

Production performance measurement is separate:

`neuraltest performance --game PATH --frames N --warmup N --out DIR
[--lane native|dlaa|sr-quality|dlss5] [--api d3d11|d3d11on12]
[--renderer dx11|dx11-oit] [--preset auto|j|k] [--render-height N]
[--feature-path DIR] [--input-replay yes|no]
[--inject none|create|evaluate|ring-busy|device-removed|runtime-unavailable]
[--inject-count N] [--inject-after N]
[--transition none|resize-minimize-restore|fullscreen-roundtrip] [--transition-delay-ms N]
[--renderer-reinit-after N]
[--renderer-switch-after N]
[--surface-switch-after N]
[--game-reload-after N]
[--savestate-roundtrip-after N] [--savestate-load-delay N]
[--pause-roundtrip-after N] [--pause-duration N]
[--timeout-ms N]`

This command forces synchronous capture and Gate 10 evidence readback off. A
12-slot D3D11 timestamp/disjoint-query ring is polled only with
`D3D11_ASYNC_GETDATA_DONOTFLUSH`; it never waits, flushes, maps a texture, or
changes emulation cadence. `performance.json` contains every resolved sample,
P50/P95/P99 base-PVR, guidance, stage-evaluation, overlay/presentation-blit, GPU
timestamp-span, and Present-call interval values, stage busy/fallback/failure
counts, ring pressure, plus local-VRAM initial/final/growth after warmup. Schema
2 also records the source, accepted-evaluation, and displayed-output frame ID
for every timed sample. Its cadence summary counts missing presents, accepted
outputs not presented, source gaps/repeats, neural-output repeats,
native/neural transitions, frame-identity mismatches, and mean/maximum
accepted-output latency in emulated frames. Native D3D11 and D3D11On12 use the
same accounting; a public evaluation that is deliberately withheld because an
experimental external contract is not ready is counted as accepted but not
presented, never as neural presentation.

Developer automation builds may add `--input-replay yes`. The launcher requires
`scripts/<media-stem>.input` beside the selected executable, passes the replay
through the transient `record:` namespace, and retains its FNV64 and byte count
in the performance output. Production builds and the default `no` path are
unchanged.

The same report records `resource_objects` for Flycast-owned neural GPU
resources, views, queries, command allocators, command lists, and fences. It
contains initial/minimum/maximum/final/growth plus final renderer/backend
components, and the launcher rejects a completed run if this accounting is
absent. Borrowed device/context/queue pointers, NGX handles and opaque internal
allocations, aliasing presentation references, and external-consumer objects
are intentionally excluded; the emitted scope string makes that boundary
machine-readable.

On D3D11On12, schema-3 reports may expose
`stage_evaluate_scope=d3d12-backend-asynchronous-timestamps`. Those samples
come from an optional two-timestamp-per-output-slot D3D12 query ring and are
mapped only after the slot's existing fence has completed. They are filtered
to accepted frame IDs in the measured interval and summarized as aggregate
P50/P95/P99. `samples_ms[].evaluate` intentionally stays null because the
retired result belongs to an earlier frame. With an external interceptor the
span is inclusive of work recorded through the public evaluation contract;
it is not an exclusive external-cost measurement. The resources and readback
path do not exist unless performance telemetry or bounded developer capture is
explicitly active.

Each quality package's `metrics.json` records capture-only GPU timestamp spans
for base PVR, guidance, evaluation, overlay/presentation blit, and the overall
frame where available. D3D11On12 evaluation is populated only when the retired
D3D12 query carries the exact captured emulated-frame ID; otherwise it remains
null with an unavailable scope. The manifest marks timing presence and still
states `capture_stalls_gpu=true` and `eligible_for_performance_metrics=false`.

The optional window transition is a bounded Windows-only production check. It
finds only the launched Flycast process's unowned visible window, waits the
requested 0..60000 ms, then resizes outward, minimizes, restores, and returns
to the exact original window rectangle. Each OS action must be positively
observed before the launch report can pass. The performance sampler continues
through the sequence, so fallback, reset, frame-identity, cadence, and VRAM
effects are retained. This covers window resize and minimize/restore; it does
not claim borderless/exclusive-fullscreen, cross-monitor movement, alt-tab, or
renderer restart.

`fullscreen-roundtrip` posts the same unmodified F11 input Flycast handles in
normal use. Acceptance separately requires the key request, monitor-sized SDL
desktop-fullscreen state, exit request, observed return to windowed state, and
exact restoration of the original window rectangle. Fullscreen observations
use a one-second interval because SDL/Windows state propagation is asynchronous.
Flycast exposes `SDL_WINDOW_FULLSCREEN_DESKTOP` here, so this proves borderless
desktop fullscreen, not an unsupported exclusive-fullscreen mode.

`--renderer-reinit-after N` is a separate hidden, default-off developer check
for a real in-process renderer/API-context teardown and recreation after main
frame `N` (1..10000). Flycast writes `renderer-reinit-complete.json` only after
the replacement renderer initializes; the launcher verifies its exact frame,
renderer, completion, and sampler-restart fields. The old asynchronous tracker
is destroyed with the renderer, and the final `performance.json` contains a
fresh warmup plus the requested samples from the replacement renderer. Thus a
passing launch proves both the reinitialization marker and a complete bounded
post-restart rendering interval; it does not claim an API switch, game reload,
fullscreen transition, or device-loss recovery.

`--renderer-switch-after N` is mutually exclusive with renderer reinit and
switches normal DX11 to DX11 OIT, or DX11 OIT to normal DX11, at the exact main
frame. The launched process is not replaced. The completion marker records and
the launcher verifies the exact source and destination renderer. As with
reinit, the final performance report must identify the destination renderer
and complete a fresh warmup and measured interval. This proves both directions
within the DX11 API; it does not claim a D3D11-to-D3D11On12 surface switch.

`--surface-switch-after N` is mutually exclusive with both renderer transitions
and flips the neural/public-NGX surface between native D3D11 and D3D11On12 at
the exact main frame while retaining normal DX11 or DX11 OIT. Its marker
records the source and destination surface; acceptance requires a new report
whose `api` identifies that destination plus a fresh warmup and complete sample
interval. This is an in-process surface/context switch, not a switch to Vulkan,
OpenGL, or DirectX 9.

`--game-reload-after N` is mutually exclusive with the renderer/surface
developer transitions. At the exact main frame, Flycast saves the current media
identity in memory, calls the real emulator unload path, positively observes
cleared content state, reloads the same media through `Emulator::loadGame`, and
starts it again. The marker contains no media path; it records only that unload
was observed and the game ID and media path returned unchanged. Performance
telemetry deliberately spans the boundary so history resets and discontinuous
source IDs remain visible. This proves same-media unload/reload, not switching
to a second title.

`--savestate-roundtrip-after N` serializes the production emulator state to a
bounded process-owned byte vector and reloads it after
`--savestate-load-delay N` main frames. `--pause-roundtrip-after N` invokes the
actual `gui_togglePause` path and resumes after `--pause-duration N` main
frames. Both controls are mutually exclusive with the other developer
transitions, write path-free exact-frame markers, and keep performance sampling
across the lifecycle boundary.

Native performance means NeuralMode off, unlike native artifact capture which
uses the passthrough stage to retain guidance images. Native D3D11 can bracket
public NGX work on its D3D11 context. D3D11On12 public NGX executes on the D3D12
queue, so its D3D11 timestamp report deliberately writes stage evaluation as
`null` with scope `unavailable-d3d11on12-cross-queue`; it does not relabel a
cross-queue command gap as NGX GPU time. Native-off and zero-accepted-submission
runs likewise write `null` with explicit scopes rather than reporting timestamp
marker overhead as evaluation. The complete report is bounded evidence
for one interval, not a long-run leak or full-title stability claim.

`capture` accepts the same `--inject`, `--inject-count`, and `--inject-after`
controls. `--inject-after N` arms only after N accepted evaluations, allowing a
capture to prove that a rejected frame does not reuse the previously accepted
output. For create/evaluate/busy controls, three consecutive injected results
exercise the bounded hold and resume reset. `device-removed` is a synthetic
status control: it latches native fallback until stage recreation but does not
remove the actual DXGI device. A rejected capture is accepted only when its
manifest has no public/external output and source-color and final-composited
SHA-256 hashes match; actual device-loss recovery is not implied.

`runtime-unavailable` accepts an injection count of exactly one. Once its
accepted-evaluation threshold is reached and outstanding ring work has retired,
it releases the live public-NGX session and backend-owned objects without a
flush, clears the candidate output, and latches native fallback. Performance
telemetry records the terminal status and the corresponding object-count drop.
This is controlled active-unavailability evidence, not physical removal of a
loaded runtime binary.

During Phase 1, the test-only D3D11 fixture driver writes the implemented subset:
`manifest.json`, `color.png`, `color.raw`, and `report.md`. The manifest sets
`production_renderer` to `false` and the report explicitly labels depth and
motion as `no data`. Passthrough additionally writes `prepared-color.png`,
`neural_output.png`, `ngx-status.json`, and a command report. Missing artifacts
are not synthesized or represented as production evidence.

`dx11` and `dx11-oit` in Phase 1 test-driver reports are requested contract
lanes, not claims that the production renderer classes were invoked. That gate
remains open until the harness is connected through `rend_context` and the
production DX11/OIT implementations.

Depth visualization inverts the legacy encoding with
`w = exp2(depth * 34) - 1`, followed by the path-specific 100000 scale. This is
diagnostic only and is not used for motion or matching.

The inverse is unit-checked for both shader branches: legacy returns
`(exp2(depth * 34) - 1) / 100000`; `DIV_POS_Z` returns
`100000 / (exp2(depth * 34) - 1)`. A zero/underflow input maps to zero for
visualization rather than infinity.

The production seam exposes bounded draw metadata, deterministic snapshot
hashes, frame/history identity, and a three-deep atomic export set: converted
RGBA8 scene color, OP/PT-only R32 depth, RG16F motion, R8 bias mask, R8
confidence, and R16_UINT draw ID. Exact-topology accepted positions are bound
as a second vertex stream. The export shader writes previous-minus-current
motion in render pixels and gates it by draw confidence, per-vertex validity,
and a 128-pixel magnitude limit. Any rejection writes zero motion/confidence
and bias one. Draw IDs cover OP/PT replay only. Production-path artifact capture
now covers both DX11 surface types and normal/OIT renderers. Gates 13-15 are
green; Gate 17 title coverage and Gate 18 performance/stability remain pending.

Draw-history diagnostics refer to the last stage-accepted frame rather than
the previous emulated frame. A FramebufferDirect package carries color,
dimensions, content rect, frame/history identity, and an explicit reset, but no
geometry or temporal input views. RTT passes produce no stage submission.

Each captured draw separates structural topology and UV identity from pose
(centroid, bounds, and depth range). Texture TCW identity is accompanied by
decoded-content, palette-content, and rendered-texture generations. Small
repeated exact and topology-compatible buckets record deterministic minimum-cost
assignment plus best/second-best costs; buckets above eight are reported as
ambiguous with zero confidence instead of receiving manufactured motion. Match
diagnostics also retain similarity-fit residual, accepted-history age,
skipped-frame count, and scene-cut state.

Exact-topology matches also build a bounded per-current-vertex previous-position
stream from the last stage-accepted geometry snapshot. Each element stores XYZ
and an explicit validity bit. Mapping follows index position, including repeated
strip indices. Reindexed geometry is accepted only through a similarity fit with
at most 0.25 render-pixel RMS residual and bounded scale; deformation above that
threshold remains invalid. Naomi 2 until transform history is implemented,
out-of-range indices, reset/truncation, and conflicting mappings of one current
vertex remain invalid. The history caps are 1,048,576 vertices and indices;
overflow resets history rather than allocating without bound.

History age advances from the last successfully accepted frame. Confidence is
attenuated for one and two skipped evaluations and rejected after three. An
unmatched-area scene cut invalidates the complete previous-position stream and
sets an explicit reset. Gate 14 then reprojects current depth against only the
last successfully accepted depth/draw-ID surfaces in the production post-pass.

The guidance replay writes an internal expected-previous R16 draw ID in addition
to the captured current draw ID. The disocclusion pass point-loads accepted
depth and draw ID at `current pixel + motion`, protects out-of-raster and clear
samples, requires the expected accepted identity, and applies the documented
encoded-depth threshold. Its R8 output, not the pre-pass base mask, is submitted
as public `BiasCurrentColorMask`. The internal expected-ID surface is not a
consumer input. A retained accepted ring slot is excluded from new exports until
a later submission becomes the accepted reference.

The dedicated neural input layout drives both normal DX11 and the base guidance
replay used after an OIT scene resolve. Current and previous unjittered screen
positions are separate interpolants; raster position is not used to derive
motion. Naomi 2 remains invalid until accepted historical model/projection
matrices are available.

The ROM-free Gate 11 command is `neuraltest depth-contract --api
d3d11|d3d11on12 --out DIR`. It compiles the production pixel shader, writes
`correct-color.png`, `reversed-color.png`, `wrong-polarity-color.png`, exact
R32 files for each control, and `depth-contract.json`. These are synchronous
harness artifacts, not emulator performance evidence. Public NGX polarity A/B
uses `neural --depth-polarity inverted|normal`; the override exists only in
`neuraltest` and does not expose a production user setting.

The ROM-free Gate 12 command is `neuraltest motion-contract --out DIR`. It
writes previous/current color, correct/reversed/doubled RG16F motion, and a JSON
report containing analytic samples and reprojection MAE. Public DLAA consumes
the same pair through harness-only `neural --previous-in ... --motion-x ...
--motion-y ...`; this temporal input override is not a production setting.

The Q1 SDR command is `neuraltest color-contract --out DIR`. It writes
`source-color.png`, `roundtrip-color.png`, and `color-contract.json`. The test
compiles Flycast's production DX11 quad shaders, point-samples a deterministic
RGBA chart through `R8G8B8A8_UNORM`, and requires byte-exact RGB, grayscale,
alpha-independent RGB, and alpha output. It also verifies the exact requested
4:3/16:9 rectangles and a sweep of odd output sizes. Public DLAA color runs
must set `FLYCAST_NGX_FEATURE_PATH` to the separately supplied public feature
DLL directory when it is not beside `neuraltest.exe`.

The ROM-free Gate 14 command is `neuraltest disocclusion-contract --api
d3d11|d3d11on12 --out DIR`. It compiles the production post-pass and writes
`resolved-mask.png`, `wrong-disocclusion-mask.png`, and
`disocclusion-contract.json`. The report separates trusted static/camera/depth-
tolerant regions from protected outside, depth-disagreement, crossing,
newly-visible, revealed-background, and scene-cut regions, and records trail
energy for the deliberately omitted-pass control.

The ROM-free Gate 15A command is `neuraltest transparency-contract --api
d3d11|d3d11on12 --out DIR`. It compiles and executes the production OIT final
resolve with the production u2/u3 UAV layout and writes `reactive-mask.png`,
`wrong-omitted-mask.png`, and `transparency-contract.json`. The fixture labels
empty/modifier-only pixels, single-layer translucency, and a visible two-layer
stack. It proves mask coverage only; translucent geometry never becomes
authoritative depth or motion. Normal sorted translucency is protected by a
separate production list replay into the same base mask.

The ROM-free Gate 15B command is `neuraltest overlay-contract --api
d3d11|d3d11on12 --out DIR`. It compiles and executes the production late
composite and writes `original-scene.png`, `neural-scene.png`,
`overlay-mask.png`, `composited.png`, and `overlay-contract.json`. Acceptance
requires every protected output byte to equal original PVR color, every
unprotected output byte to equal the neural scene, and the deliberately omitted
composite to mismatch every protected pixel.

That synthetic compositor result is not real-game classifier coverage. Bounded
captures now include schema-2 `overlay-draws.json` with `screen_size`, the active
`overlay_profile`, PVR-native-screen bounds, resource generations, structural
signatures, accepted-frame stability, texture-use counts, topology-proven
`screen_aligned_primitive_count`, and classification for every captured draw.
`manifest.json` separately records `pvr_screen_size` and `overlay_profile`.
For sorted translucency the original PolyParam slots are empty placeholders; actual
SortedTriangle submissions follow all three original list ranges, in sorted
submission order, with flag 64 (triangle list). Their motion is always reactive.
The live replay uses those same triangle ranges; it must never reinterpret
sorted PolyParam vertex offsets as index offsets. OIT and per-strip paths keep
their indexed-strip contract.

`hud_protected_pixel_count` and `hud_protected_pixel_percentage` quantify mask
coverage. `hud_comparison_available` requires matching source/native/final
dimensions; when unavailable, `hud_pixel_mismatch_count` is null.
`hud_protected_pixels_verified` requires a nonempty mask and zero mismatches.
It proves only those selected pixels, not complete HUD coverage or correct
classification. The initial diagnostic-before and sorted-fix-on12 development
captures used the older provisional name `hud_preservation_verified` for this
same limited check. Historical zero mismatches with an empty mask are vacuous.

The exact game ID `T1401N` selects `soulcalibur-t1401n-hud-v1`. It does not
write an external consumer configuration. The profile is restricted to late,
bounded, stable translucent geometry wholly inside the top fifth of the native
PVR screen. Generic single quads retain exact UV/topology continuity. Proven
quad batches may tolerate atlas churn, while a bounded duplicate name-plate
bucket is paired one-to-one by depth; a changed or oversized bucket resets to
untrusted. Every active profile is visible in runtime notices and captures.

`NeuralOverlayPolicy` is stored through Flycast's existing per-game option
system: 0 is the strict automatic classifier, 1 restores the complete PVR frame
as a safe title override, and 2 disables post-composition. Runtime notices name
the game ID, policy, active protected-draw count, and conservative 2D/menu
bypass transitions. Those transitions require three consecutive classifications
and are active only for experimental generative mode; public DLAA remains a
separately selectable feature.

`NeuralMatchOutputResolution=yes` applies only to target-native DLAA and
experimental external-consumer modes. The runtime `Neural raster contract`
notice records input, output, global content rectangle, and whether match mode
was actually active. SR modes intentionally log `match=0` and retain the manual
PVR resolution.

The public preset harness form is `neuraltest neural --in PNG --out DIR
--backend dlaa --api d3d11|d3d12 --preset auto|j|k --frames N`. Its JSON and
Markdown reports record the selected hint. The production log likewise records
Auto/J/K and states that external Neural Rendering model selection is
independent. A successful preset test proves public feature creation/evaluation;
it does not prove that the external consumer changed models.

The real save-state lifecycle form is `neuraltest performance --game PATH
--out DIR --frames N --warmup N --savestate-roundtrip-after N
--savestate-load-delay N` plus the normal API/renderer/feature-path options.
Both thresholds are main-frame counts. The control is hidden, default-off, and
mutually exclusive with renderer reinit, renderer switch, surface switch, and
same-media reload. It serializes into process memory, never the platform save
directory. Acceptance requires `savestate-roundtrip-complete.json` to report
the exact save/load frames, `in_memory=true`, and a nonzero byte count, plus a
completed performance report with no missing/accepted-but-unpresented frame,
identity mismatch, stale output repeat, or frame latency. Source-frame gaps and
explicit native fallback at the load boundary are retained rather than hidden.

The real pause lifecycle form is `neuraltest performance --game PATH --out DIR
--frames N --warmup N --pause-roundtrip-after N --pause-duration N` plus the
normal API/renderer/feature-path options. The hidden, default-off control is
mutually exclusive with other developer lifecycle transitions. Acceptance
requires `pause-roundtrip-complete.json` to record the exact main-frame pair,
an observed `GuiState::Pause`, and an observed return to `GuiState::Closed`.
The performance interval spans the pause and must retain explicit source gaps
without stale output, identity errors, accepted-output loss, or frame latency.
`--evidence-start-frame` delays evidence marking and readback until that exact or
next emulated frame ID. It is intended for deterministic input-replayed gameplay;
frames before it evaluate and present normally and remain outside the evidence set.
