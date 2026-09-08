# FC-067 combat packet and fish-sprite association

Starting HEAD: `916d0cc19b589223ef3530c8be814c2d7983fff6`.
ACCEPTED bounded observation (self-review), not whole-scene camera or Remix GPU.

## Actual chain

The previously witnessed projection produces xyz at RAM8ce6e460. Actual reads
8c06fc2e/32/36 and compiled SSA-linked stores8c06fc30/34/38 copy those three
words into the first SQ packet at e0de71a0. A second copy is distinct and is not
substituted for this first generation. Original RAM later ends at CPU overwrite
event14567. Actual pref8c06fc4c calls the unchanged handler with QACR0 area3,
writing all32 bytes exactly to RAM0cde71a0 at cycle7601914432.

An independent packet generation survives until actual TAWrite input: count1086,
offset128, cycle7603704192,634413 memory events. Full bytes match. Guards stop
at CPU/SQ overwrite, reset, detected byte change, one-second age or five-million
event cap. This is not arbitrary DMA/ABA coverage. The actual input pointer is
copied into context00509700 offset516800, epoch1790; decoder returns vertex14770
with exact xyz and context. Same render-context snapshot is frame1782, not a
later capture inferred from matching coordinates.

Scene v2 contains15967vertices/19197indices/3347draws. Vertex14770 belongs to
translucent source draw0, four vertices starting14768. Sorted command10 uses
material draw4; triangles at17580 and17586 are [14770,14769,14771] and
[14768,14769,14770]. The source and material draw bind asset31/palette320,
TCW713839104,upload1,palette hash4029862303,RTT0. The decoded source image was
visually inspected: it depicts a koi fish on transparent background. This is a
fish-textured environmental sprite, not established opaque fighter/arena data.
Pixel visibility/occlusion and physical world camera remain unproven.

## Evidence and failed attempts

Raw runs remain outside Git under sibling flycast-evidence: combat-watch-working
A-F, combat-watch-f-scene, combat-material-working-g and combat-restored-material-h
(each prefixed fc067-). Ignored build logs and exact removed patches A-F scope
are catalogued in fc067-combat-watch-validation-progress.md. No raw assets,
third-party code/configuration, local user paths or temporary core hooks are staged.

A/B observe reads and stores; C adds actual flush; D follows independent packet
generation; E adds actual copy/decode; F captures same context; G captures matching
materials. All named C-H captures complete three frames with clean close.
The initial B projection verifier failed because a negative replacement hit the
new WATCH_BEGIN line instead of the intended buffer. Fixed by targeting the exact
step/address/tag; regression included. Earlier guessed source/frame lookup paths
failed and were corrected; no build failure in C-H. Do not erase those failures.

F projection/native checker passes five controls and27 unique native planes
against restored916d0cc19. Packet verifier passes ten separate offline controls:
flush flag, bulk/copy offsets, termination, vertex, epoch, context, SSA source,
first-versus-second copy address and triangle index. These are offline evidence
mutations, not runtime injected faults. Three new tests cover record cardinality
and tag selection; they are not synthetic whole-scene tests.

## Restored-source validation

All six temporary core diffs and probe header were removed. Four serial configure/
builds pass. Three enabled selftests pass262/262 each; SDK mock56/56 reports
runtime/GPU/Present false. All55 inspector tests and five binary32 tests pass.
Fresh uninstrumented H frames1782-1784 exit0/clean close, contain no probe logs,
match G on27 unique native planes (54 duplicated comparisons), and frame1782
scene equals F exactly including SHA. Material inspector verifies three packages,
39 first-frame assets and40 previews. Native replay was disabled for these runs;
the historical strict replay residual remains failed/parked. Capture is not
performance evidence. Exact-commit validation follows in an ignored note.

## Next disposition

### Postcommit qualification: exact-frame check failed

Commit29e4a66de exact builds/selftests pass, but its three-frame native capture
does NOT match restored H at equal frame IDs. Frame1782 differs at241148 color
pixels,1783 at238775,1784 at236615. Subsequent read-only comparison finds new
1782 exactly equals old1783 in native color, and new1783 equals old1784. This is
an observed one-frame offset, not a passed equal-frame gate or1-LSB residual.
Launch records/replay hash match. Root cause of the offset remains unproven.
The shell sequence continued after Python validation failed and pushed the
commit; this procedural failure is retained, not retroactively called success.
Investigate frame identity/capture alignment before promoting exact-SHA evidence.
Do not silently shift acceptance frames or overwrite failed captures.

Repeat I uses the same29e4a66de executable, same capture settings/replay and
restored save inputs. It exits0/clean close and matches restored H at equal
frame IDs on all27 unique image planes. It differs from the first29e4a66de run.
Thus the offset is not a consistent commit-dependent scene change. Failed run
remains failed; repeat success does not establish run-to-run determinism.
Source inspection shows instrumentation frameId increments per CaptureGeometry
or CaptureSource invocation; it is not an emulated-vblank identifier. Which
invocation differed remains unknown; next inspect startup/source/render skipping.

Source review narrows diagnostic seams: QueueRender can reject disabled/skipped
or occupied-queue work; rend_vblank can enqueue direct-framebuffer work.
CaptureGeometry and CaptureSource share frameId_, while QualityCaptureWriter
separately increments seen_ per Write attempt. No evidence yet identifies which
path differed in the failed run. Next bounded source/capture-attempt counters
and submitted emulated identity, not a global frame-ID offset correction.

The second source witness is now explained; neither it nor the old petal sample
establishes opaque main-scene calibration. Before further source tracing, review
the existing captured opaque draw/material coverage and source seams to select
one explicit opaque producer or record unsupported scope. No blind site census,
guessed world camera, effect-to-world promotion, replay precision loop, or Remix
runtime claim. M2 camera/main-scene feasibility remains open, not complete.
