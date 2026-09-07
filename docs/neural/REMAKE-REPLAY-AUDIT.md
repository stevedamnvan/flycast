# FC-067 M2 GPU replay diagnostic checkpoint

Base: `857d63963`, clean worktree before implementation. **M2 remains open.**
This is an accepted developer diagnostic harness, not accepted production
presentation or complete standalone scene replay.

## What runs

`neuraltest capture ... --lane native --api d3d11 --renderer dx11
--remake-packet yes --remake-replay yes` enables a bounded experiment only.
The replay option defaults false and rejects other renderer/lane combinations.
The diagnostic caps raster dimensions at 4096x2160 and retained supplemental
geometry/counts before cloning. The constant-copy hook excludes RTT and direct
framebuffer paths; this was checked during source review, not a new RTT matrix.
Normal native rendering, public NGX, external consumer configuration, accepted
history, and presentation ownership are not replaced.

Before native drawing, retain the actual framebuffer base (after any requested
clear). Hoko Temple uses retained framebuffer content. Freeze the native
vertex and pixel shader constant blocks. At the existing synchronous capture
boundary, write the PVR packet, decode it from disk, validate live texture
control words/generations and draw/pass state, and create separate vertex,
index, depth/stencil, and color resources. Do not reapply
`setFirstProvokingVertex`: the exported data already contains that adjustment.
Restore renderer context/buffer ownership, constant binding, target and viewport
afterward. The experiment never invokes another neural evaluation.

Four outputs are retained per frame:

1. Decoded vertex/index/draw/pass replay.
2. The same replay with a deliberately wrong viewport translation (+0.05 in
   the matrix translation term).
3. The same replay with raw PVR z incorrectly replaced by its reciprocal.
4. Replay using the original retained GPU vertex/index buffers and original
   render context, as a causal control for decoding versus repeated drawing.

`pvr-pre-frame.png` records the prior framebuffer. `pvr-replay-proof.json`
records exact changed-pixel counts, maximum channel deltas, decoded-versus-
retained-buffer equality, and the original-source equality separately. The
strict overall gate still requires exact source pixels plus material failures
for both wrong controls; it has not been relaxed to tolerate small errors.

This is **same-frame resource-backed replay**. Texture pixels, global/pixel
state, modifier-volume geometry and sorted resolve order remain retained
supplements rather than a complete portable scene package. Game camera/world
transforms remain unknown; matching PVR rasterization does not recover them.
The wrong-depth image is an intentional failure, not relighting evidence.

## Executed evidence and falsification

- All four working-tree builds pass; enabled selftests remain 243/243 each,
  public-header/mock SDK 56/56. Real cases use the isolated native stage and
  existing legal Hoko Temple replay, not the user's active consumer install.
- First three-frame replay: frame 1804 differs by 13 pixels, maximum channel
  delta one; 1805 and 1806 are exact. Wrong viewport changes 264181/264231/264301
  pixels; wrong depth changes 307159/307144/307135 pixels. Launcher correctly
  exits 1. Initial proof JSON accidentally inherited grouping separators from
  the global locale; fixed with classic locale. Those malformed reports are
  retained, not accepted as valid JSON.
- Freezing pixel constants alone did not remove the discrepancy. A subsequent
  full-vertex-constant run was exact 3/3, but repeating it restored the 13-pixel
  failure. Thus the apparent correction did **not** establish causality.
  A diagnostic compared every vertex-constant word: only the intentional wrong
  viewport term differed. The temporary diagnostic logging was then removed.
- Original-buffer replay reproduces exactly the same 13 differing pixels in
  the failing frame; decoded versus original-buffer replay is exact 3/3.
  This localizes the residual away from packet decoding but does not explain it.
- Working moving interval `fc067-m2-replay-moving-yes`: 30/30 frames pass the
  original-source exact gate, retained-buffer exact gate, and exact decoded-
  versus-retained-buffer comparison. Both wrong controls fail materially in
  all frames (minimum 264181 and 307135 changed pixels respectively).
- The matching replay-off run closes cleanly but native PNGs are identical only
  28/30 across the on/off runs. Frame 1805 differs by 3 pixels and frame 1812 by
  11 pixels, maximum channel delta one in each. Exported vertices, indices,
  draw records, passes and viewport are exact for both differing frames.
  This is **not** a 30/30 noninterference pass. Source configuration hash remains
  unchanged. The actual cause of the run-to-run one-step differences is open.
- Local evidence directories retain `decoded-replay-01/02/03`,
  `replay-diagnostic-yes/no`, `native-buffer-control`, and
  `replay-moving-yes/no` under the FC-067 M2 prefix. Build/test logs are
  `fc067-replay-*`. Synchronous captures provide no performance evidence.

## Disposition and next task

### Exact-SHA causal follow-up (688d63e05)

No renderer source changed for these checks. Two fresh native-only 30-frame
runs, `fc067-replay-688d63e05-native-repeat-a/b`, both exit 0 and close cleanly.
Run A matches the prior replay-off run 30/30. A versus B matches 29/30:
frame 1805 differs at three pixels, maximum channel delta one. Thus replay is
not necessary for this particular cross-run variation. This does not explain
the separate 13-pixel same-frame mismatch or establish that all GPU inputs
were identical across runs.

The existing NativeParityCapture hook was then armed in the disposable native
stage, alongside quality capture, to read the framebuffer immediately after
native drawStrips and again at the late quality-capture boundary. No new
production hook or synchronization was added. A three-frame paired run exits 0
with exact early/late and source/replay pixels. Its 30-frame repeat exits 1:
early/late native pixels are exact **30/30**, decoded/original-buffer replay
pixels exact **30/30**, but source/replay exact only **29/30**. Frame 1804 again
has the same 13 one-step differences in both replay lanes. Both wrong controls
still fail materially. This falsifies an early-readback-as-fix hypothesis and
rules out late framebuffer mutation for the observed failing run.

Evidence prefixes are `fc067-replay-688d63e05-early-{native,paired}` and
`fc067-replay-688d63e05-early-moving-{native,paired}`. The native capture index
1802 maps to quality frame 1804; all 30 metadata pairs verify their expected
indices and identical build SHA. BGRA readback bytes were compared to decoded
PNG BGRA pixels with explicit 640x480/2560-byte row checks. An initial diagnostic
passed PowerShell PathInfo instead of a string to Bitmap, causing constructor
errors and invalid all-pixel counts; those counts are rejected. The corrected
Stop-on-error comparison exits 0 and reports the exact counts above.

Only the disposable stage's capture settings were temporarily changed and then
removed. Source emu.cfg SHA-256 remains
`1EF718689784DCE64CAD1CE8BEC776E710B4A3705ECFF2E5A276A1A3B2F92992`.
No external consumer configuration, production setting, or gate threshold
changed. These synchronous runs provide no performance evidence.

**ACCEPTED:** opt-in diagnostic mechanism and bounded decoded-geometry versus
native-buffer replay evidence. **CORRECTIONS_REQUIRED:** full M2 completion.

Resolve the repeat-render differences without assuming the decoder or a
particular shader constant is responsible. Native-only cross-run variation is
now reproduced, and late framebuffer mutation is excluded in a failing run.
Next capture/compare the remaining per-draw shader constants, bound resource
contents and pipeline descriptors between the native and original-buffer
replays; localize the first differing draw/pixel before changing production
rendering. Then rerun exact-SHA 30-frame replay/on-off/repeat pairs. Keep the current
zero-tolerance gate and failed attempts. Extend the packet's retained-state
disposition honestly; do not claim an offline complete scene, recovered camera,
path tracing, or Remix/DLSS 5 combined output. Only close M2 after its full
native-alignment and preservation evidence is stable, or record an explicit
falsifying feasibility disposition covering the actual requirement.
