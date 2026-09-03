# Neural diagnostics

The production capture command is:

`neuraltest capture --game PATH --frames N --skip M --out DIR [--flycast EXE]
[--lane native|dlaa|sr-quality|dlss5] [--api d3d11|d3d11on12]
[--renderer dx11|dx11-oit] [--preset auto|j|k]
[--profile faithful|enhanced|photoreal] [--style FAMILY]
[--render-height N] [--feature-path DIR] [--timeout-ms N]`

It launches Flycast with transient command-line configuration, limits a run to
1 through 240 frames, waits for a completion marker, requests a normal window
close, and reports whether forced termination was required. Missing media,
invalid arguments, a completed destination, early emulator exit, and timeout
return nonzero. It does not copy media or record the media path.

Each production `frame-NNNNNN` directory contains:

- `manifest.json`: Flycast SHA, game ID, frame/history/reset identity, render and
  output dimensions, exact content rectangle, API, mode, preset, profile,
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
  actually evaluated; module/readiness detection alone cannot create it.
- `final-composited.png`, optional `native-versus-output-difference.png`, and
  an optional `temporal-flicker.png` beginning with the second captured frame.
- `metrics.json`: temporal variance, motion reprojection error, reactive-region
  trail energy, silhouette/line measures, color/saturation/black drift, HUD
  mismatch, repeat/drop counts, invalid guidance coverage, and trusted/reactive
  percentages. GPU timings remain `null` until asynchronous production queries
  are implemented.

Raw game data and user paths are excluded. Normal emulator execution performs
no capture readback. This synchronous developer-only path is disabled unless an
explicit destination and positive frame limit are supplied, and every package
is marked ineligible for performance measurements.

`neuraltest capture-index --root DIR [--out HTML]` recursively discovers only
production packages containing both source and final images. It writes a lazy-
loading HTML contact sheet and a machine-readable JSON manifest using relative
paths. Every card shows game/profile, actual renderer/API, acceptance, external-
evaluation state, and submit status; available native/guidance/public/external/
final/difference/flicker artifacts are linked in place. The index explicitly
sets `winner_declared=false` and does not convert still images into a title-
quality decision. An empty root writes an empty diagnostic index and exits 3.

Production performance measurement is separate:

`neuraltest performance --game PATH --frames N --warmup N --out DIR
[--lane native|dlaa|sr-quality|dlss5] [--api d3d11|d3d11on12]
[--renderer dx11|dx11-oit] [--preset auto|j|k] [--render-height N]
[--feature-path DIR] [--inject none|create|evaluate|ring-busy|device-removed]
[--inject-count N] [--inject-after N]
[--transition none|resize-minimize-restore|fullscreen-roundtrip] [--transition-delay-ms N]
[--renderer-reinit-after N]
[--renderer-switch-after N]
[--surface-switch-after N]
[--game-reload-after N]
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
