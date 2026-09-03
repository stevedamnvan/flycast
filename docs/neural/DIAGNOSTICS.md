# Neural diagnostics

Each captured frame directory is immutable evidence and contains:

- `manifest.json`: Flycast SHA, fixture/game identifier, frame, renderer,
  scale, output size, mode, jitter, history state, and config hash.
- `color.png` and `color.raw`: scene color before OSD/ImGui.
- `depth.raw` and `depth.png`: opaque/punch-through depth and visualization.
- `motion.raw` and `motion.png`: render-pixel current-to-previous vectors and
  HSV visualization.
- `mask.png`, `confidence.png`, `draw_id.png`, `list_type.png`.
- `history_valid.png`; optional Phase 5 `overlay_class.png`.
- `neural_output.png`, `diff_native_vs_neural.png`, `output-flicker.png`.
- `ngx-status.json`: device, SDK/runtime, feature parameters/results, failures,
  frame/history IDs, fallback counters, and asynchronous GPU timings.
- `report.md`: assertions and metrics for the command.

Raw game data and user paths are excluded. Emulator capture is rate limited.
Normal emulator execution performs no readback; readback helpers are confined
to `neuraltest`.

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
and bias one. Draw IDs cover OP/PT replay only. Emulator-path artifact capture
and Gates 13-18 remain pending.

Draw-history diagnostics refer to the last stage-accepted frame rather than
the previous emulated frame. A FramebufferDirect package carries color,
dimensions, content rect, frame/history identity, and an explicit reset, but no
geometry or temporal input views. RTT passes produce no stage submission.

Each captured draw separates structural topology and UV identity from pose
(centroid, bounds, and depth range). Texture TCW identity is accompanied by
decoded-content, palette-content, and rendered-texture generations. Small
repeated structural buckets record deterministic minimum-cost assignment plus
best/second-best costs; buckets above eight are reported as ambiguous with zero
confidence instead of receiving manufactured motion.

Exact-topology matches also build a bounded per-current-vertex previous-position
stream from the last stage-accepted geometry snapshot. Each element stores XYZ
and an explicit validity bit. Mapping follows index position, including repeated
strip indices; topology changes, Naomi 2 until transform history is implemented,
out-of-range indices, reset/truncation, and conflicting mappings of one current
vertex remain invalid. The history caps are 1,048,576 vertices and indices;
overflow resets history rather than allocating without bound.

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
