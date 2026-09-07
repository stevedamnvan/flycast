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

### Per-draw causal probe (a62fb12c2 plus temporary diagnostic)

The temporary probe samples native pixel (322,265) after each normal-list,
sorted-translucency, and final modifier-composite draw into a bounded GPU atlas.
It runs only for the first explicit replay capture and its original-buffer
control. It is **not production code** and was removed after testing; the
reproducible patch is retained locally as
`build-neural-automation/fc067-pixel-trace.patch`. Raw data stays outside Git in
`flycast-evidence/fc067-pixel-trace-a` through `-f`. Each probe built successfully
in the automation configuration. These are working-tree diagnostics: the
incremental executable retains the earlier `688d63e05` generated stamp, which
does not identify the added probe. Do not label them exact-SHA acceptance runs.

- A reproduces the 13-pixel failure and records 145 draw samples. The selected
  pixel first differs at zero-based trace command 53 and stays different through
  the remaining 92 samples. Its incoming color at command 52 is identical.
- B adds command identity and passes 3/3; command 53 is a sorted-translucency
  DrawIndexed with first index 17497 and count 18. This is the sorted index
  buffer, not an exported original-list ordinal.
- C passes 3/3 while dumping constants and state. Its first texture dumper did
  not support native B5G5R5A1/A8 formats, so absent texture dumps are **not**
  evidence of texture equality. D adds those formats and reproduces failure.
- E adds viewport/scissor, shader/layout/geometry-buffer object identities,
  and depth-surface descriptors; it also fails. The first depth dumper handles
  typeless formats only; the actual surface is typed D24_UNORM_S8_UINT, so no
  depth-byte comparison is claimed for E.
- F adds typed D24 readback and reproduces the same 13-pixel failure. Thirty
  captured state files are byte-identical: VS b0, PS b0/b1, all eight mip levels
  of texture 0, palette/fog texels, texture descriptors, all three samplers,
  blend/factor/sample mask, depth/stencil descriptor/reference, raster state,
  viewport/scissor, IA stride/offset/format, and bound shader/layout/VB/IB object
  identities. Object equality is not a claim about internal driver compilation.
  The only differing dump is depth storage: **1178 D24 samples differ by one
  integer step**, with zero stencil differences. Depth at (322,265) is identical
  (5373102) in both dumps, so depth variation is not yet the cause of that color
  mismatch. Color changes from BGRA word 3275052116 to 3275052373 at command 53;
  both enter that command with word 4282600521.

A/D/E/F capture commands exit 1; B/C exit 0. All close cleanly. No failed run
is accepted as alignment success. No proprietary binary/configuration was
inspected or changed. The probe, environment opt-in, and per-draw copies are
absent from the restored renderer; these timings are ineligible for performance.

**ACCEPTED:** opt-in diagnostic mechanism and bounded decoded-geometry versus
native-buffer replay evidence. **CORRECTIONS_REQUIRED:** full M2 completion.

### Repeated production-shader fixture

`neuraltest repeat-raster` now provides a durable bounded GPU fixture; see
DIAGNOSTICS.md for lane/iteration mapping. The production VS and PS render
sloped raw-depth and Gouraud-color geometry into BGRA8 with D24 or D32 depth,
with/without alpha blending and with/without a procedural eight-mip B5G5R5A1
texture. Each lane compares 64 repeats, alternating newly allocated and reused
targets, and retains wrong-viewport/raw-depth controls plus all image/raw data.
No production renderer changes or game data enter this implementation.

Working-tree native and On12 runs each pass 512/512 strict color/depth repeats.
Viewport mutation changes at least 6493 color pixels and 8272 depth samples;
wrong raw-depth changes 8190 depth samples while preserving color in this
specific uniform-scale control. Existing-output rejection exits 1; invalid API
exits 2. Existing depth-contract and motion-contract commands exit 0. All four
working builds pass. Initial untextured-only 256-repeat and textured 512-repeat
attempts used the older harness's shader model 5/optimized settings; retained
but superseded by the final runtime-equivalent shader model 4/flags-zero runs.
Evidence directories are `fc067-repeat-*`; final working outputs are
`fc067-repeat-final-native/on12`. A textured blended PNG was visually inspected
and contains the intended gradient/checker geometry rather than a blank pass.

This fixture **does not reproduce** the Soulcalibur failure. It excludes a
simple repeat/new-target failure for these synthetic inputs, not all driver
precision behavior. It omits the game's particular geometry, fog, modifier
history, prior scene color/depth, and draw batching. Do not promote its success
to the M2 original-source equality gate.

Resolve the repeat-render differences without assuming the decoder or a
particular shader constant is responsible. Native-only cross-run variation is
now reproduced, and late framebuffer mutation is excluded in a failing run.
The selected color divergence is now localized to a sorted-translucency command
with matching captured inputs/state, alongside separate one-step D24 variation.
The generic repeated-shader fixture above is exact. Next retain the actual
trace-53 command's pre-draw color/depth surfaces, indexed geometry, shader
variant and uniforms/resources, then run an isolated same-command replay with
the matching inputs and failing mutation controls. Trace the first depth
divergence separately if needed. Do not attribute the result to
driver precision, relax the gate, or change production arithmetic without a
falsifiable control. Then rerun exact-SHA moving pairs. Keep the current
zero-tolerance gate and failed attempts. Extend the packet's retained-state
disposition honestly; do not claim an offline complete scene, recovered camera,
path tracing, or Remix/DLSS 5 combined output. Only close M2 after its full
native-alignment and preservation evidence is stable, or record an explicit
falsifying feasibility disposition covering the actual requirement.
