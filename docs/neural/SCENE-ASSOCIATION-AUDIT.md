# FC-067 linked samples are translucent effect primitives

Starting HEAD: `eb1b67bcf768317f7dfd80868ab10b712b6f5314`, clean.
ACCEPTED bounded offline association verifier and matched source-material
association (self-review). Not opaque-world calibration or full M2 acceptance.

## Executed verification

`scene_association_inspect.py` reuses the complete prior source/arithmetic/TA/
decoded-vertex chain and shared-calibration test. It invokes the compiled bounded
packet decoder before inspecting each v2 snapshot. It checks snapshot epoch,
context, frame, vertex and geometry counts; exact sampled xyz bits; unique
three-vertex source membership; exact sorted triangle index order; material
state and texture generation equivalence. Merged material state is not treated
as geometry identity. Other source primitive families are explicitly unsupported
by this small association checker, not silently mapped as these triangles.

The two immutable `fc067-sorted-working-original` / `different` packets remain
byte-identical. Twelve offline in-memory wrong-frame/epoch/vertex/xyz/material-
reference/index controls reject. They do not mutate the retained evidence and
are not runtime fault-injection tests. Nine synthetic test methods pass,
including ambiguous source membership, duplicated GPU references, wrong range
space/topology/generation, unsupported viewport, offscreen bounds, and matched
frame/material identity changes. Initial RED is the expected missing-module
import failure; seven initial tests then pass, followed by eight/nine as viewport
and matched-material checks are added. No test is inferred from code inspection.

`--matched-frame` requires every field of the captured scene to equal the
witnessed scene except the explicitly different Git SHA of the removed probe
and restored executable. Frame/game/SHA must agree across the restored scene,
capture manifest and material manifest. Source and sorted-state bindings must
resolve to the same captured asset/palette with exact generation metadata.
Raw texture verification remains the separate existing `material_inspect.py`
step; the association report does not pretend to verify pixels by reading IDs.

## Actual matched frame and material

New capture `fc067-associated-frame-eb1b67bcf` requests frames1302-1304 with
native rendering, source materials and diagnostic replay. Frame1302's packet
matches the witnessed snapshot in every field except git_sha. Material inspector
verifies all three packages: 36 assets in frame1302, 35 texture/palette previews.
Source translucent draws5/6 and merged material-state draw20 all bind asset31,
a 64x64 indexed source texture, palette base336 in palette asset35, format0.
The actual decoded preview `asset-31-palette-336.png` was visually inspected:
it is a pale pink, petal-like sprite on a transparent background.

The actual native frame1302 was also inspected. The game displays **Emperor's
Garden**, with Xianghua in an introductory pose. The existing combat frame1804
was inspected separately; it shows Kilik/Xianghua in the same-looking arena.
Do not replace the observed on-screen title with the historical replay nickname
"Hoko Temple", or infer that the early effect transforms cover the later fighters.
That nickname is retained only to locate the existing deterministic replay.

The first sampled triangle spans approximately x604.763..609.792 and
y81.908..85.532 after the captured affine viewport mapping. The other spans
x300.633..306.661 and y-175.356..-168.462, outside the viewport. Geometric bounds
are not an alpha/depth/occlusion visibility result. The source texture identifies
the sampled primitive family much more narrowly than opaque characters/world.

## Important failed replay result

The new capture command returns **1**, because frame1302 fails strict replay.
Decoded versus native differs at 1 pixel; retained-buffer versus native at
7 pixels; decoded versus retained at 6 pixels. Maximum channel delta is 1.
Wrong viewport/depth controls differ at 304629/307193 pixels. Frames1303/1304
replay exactly and both falsifying controls differ materially, but that does
not make the three-frame run a pass. All artifacts remain retained.

The native image and matching packet/material contents support their scoped
association checks; the failing replay is not hidden by those results. Do not
reopen the parked precision loop or claim whole-frame strict parity. No new
renderer/core/configuration changes were made in this association slice.

## Next concrete task

Precommit validation: all four serial configure/builds return0; enabled
selftests pass262/262 each and SDK mock56/56 with runtime/GPU/Present false.
Nine association test methods and the matched actual-input report pass after
the final strict field-type comparison change. Logs/reports use ignored
`fc067-association-working-*` and `fc067-association-matched-report.json` names.
Exact-commit checks will be recorded in the ignored postcommit note.

Seek a bounded executed transform-to-TA association for an actual opaque fighter
or arena primitive in the combat interval, or a precise unsupported disposition
for that source path. Use the established packet identity and generation checks;
do not repeat a matrix-presence census, average effect and world projections,
apply the petal calibration to the entire scene, or start GPU path tracing on a
guessed camera. The source packet/native harness work is usable; broader camera
coverage, strict M2 source alignment and Remix GPU execution remain unproven.
