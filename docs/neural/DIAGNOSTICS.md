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

Depth visualization inverts the legacy encoding with
`w = exp2(depth * 34) - 1`, followed by the path-specific 100000 scale. This is
diagnostic only and is not used for motion or matching.
