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
hashes, frame/history identity, and existing color ownership. When enabled it
also exposes the current slot of an isolated three-deep R32 depth ring populated
by OP/PT-only replay. No emulator-path readback exists. Runtime depth artifacts
and Gate 3 remain pending; motion, mask, confidence, and draw-ID artifacts are
not synthesized while their exports are absent.

Draw-history diagnostics refer to the last stage-accepted frame rather than
the previous emulated frame. A FramebufferDirect package carries color,
dimensions, content rect, frame/history identity, and an explicit reset, but no
geometry or temporal input views. RTT passes produce no stage submission.
