# FC-067 linked composite-transform semantics

## Opaque extension - LOG #177

Starting checkpoint `60196ee3a`. The existing factorizer now accepts the
separately linked opaque initial-supply capture via `--opaque-capture`, after
the entire actual source/record/gather/SQ/TA chain passes. No new GPU capture
or production hook was needed for this retained-evidence algebra step.

The opaque axes have norms 614.7143127511063, 565.5371443085618 and
0.9999998092535347; normalized X/Y scales are 614.7144300057109 and
565.5372521827936. Orthogonality residual is 6.716574753448559e-08.
These are a compatible scaled-axis factorization, not proven game intrinsics.
The algebraic screen point is 167.66892408168997, 230.84360127436491.
Actual rounded reciprocal then separate multiply/add reproduces all three
decoded words exactly. Importing the earlier translucent 1.04 depth multiplier
fails. The actual nonunit W is retained; replacing it with one fails the
observed transform. Doubled X scale and uncompensated decomposition errors
are 152.33107591831003 and 663.7678249808051 pixels. A compensated alternate
model/view basis still agrees within 2.842170943040401e-14 pixels.

The retained positive passes; the same-build live-negative capture rejects
at its predecessor witness as intended. Four new synthetic methods pass,
including independent opaque projection goldens, nonunit W, corrupt pixels/
viewport, and an explicit case where fused versus split arithmetic differs.
The four historical methods and accepted historical translucent capture B
also pass. Historical capture A was tried first and rejected its incomplete
copy/decode witness; that failed attempt remains retained, not relabeled.

ACCEPTED: the opaque algebra substep and per-domain depth distinction.
Still pending: six actual opaque vertices across three producer-identified
frames, multiple draw coverage, a usable shared camera contract, and Remix GPU
output. The three neighboring algebra probes are synthetic, not game samples.
Do not copy the translucent scale to opaque geometry or treat similar axis
norms across old captures as proof of common frame/scene ownership.

## Historical translucent result

Baseline: `55976c40da7433a60c85e4e8a542c7752553542f`.
**ACCEPTED one linked composite-transform algebra check (self-review).**
This is not a unique world camera, all-draw calibration, moving reconstructed
scene, physical scale, or Remix GPU result. No production rendering changes.

## Independent checks actually run

The inspector first verifies the full source-to-decoded-vertex observation in
TRANSFORM-DECODE-AUDIT.md. Recorded matrix columns follow innerProduct<4> in
shil_canonical.h: fm/fm+1/fm+2/fm+3 with a four-element column stride. The actual
canonical implementation uses double products/sum and converts to float.

An exact-rational mathematical dot product followed by toward-zero binary32
rounding matches all four recorded FTRV output words for this sample. It is
explicitly not an instruction-by-instruction binary64 emulator. The separately
verified reciprocal, scale and fused screen-offset operations reproduce the
actual decoded xyz words 44175b90/42ab1024/3d95b6cd exactly.

The captured 3x3 rows have norms 614.7143607284762, 565.5371226578062 and
0.9999998203860782. Normalizing them gives maximum Gram-matrix residual
3.318944212666328e-08 and positive determinant. The diagonal scale times the
remaining affine composite reconstructs the matrix within the declared 1e-9
algebraic tolerance. These norms are NOT labeled proven camera intrinsics:
unresolved model scale/basis conventions may contribute.

The unrounded algebraic screen point is (605.4307137705855, 85.53151469155915).
Wrong column/row layout fails the observed output-word check. Doubling only the
x scale changes projection by 285.4307137705854 pixels. Changing the inferred
view without the corresponding model compensation changes it by
254.55257093453673 pixels. These negative controls reject those incorrect uses.

Conversely, a distinct rigid transform H can be assigned to the model and its
inverse absorbed in the view. K*V and K*(V*inverse(H))*H give the same projected
coordinates for the actual point and three analytic neighboring probe points;
maximum measured error is 0 in this run, required below 1e-9. The three extra
points are synthetic algebra tests, not additional captured game vertices.
Thus this one composite cannot uniquely identify the model/view split.

Four independent synthetic test methods pass: known analytic projection with
ambiguity, wrong output, wrong layout, and unsupported shear/projective/
degenerate/nonfinite matrix rejection. The real linked sample inspector passes.
No failed attempt occurred in this algebra tranche. It reuses immutable game
evidence and does not claim a newly captured game sequence.

Four working configure/builds return 0; three enabled selftests pass 243/243
each and SDK mock passes 56/56 with runtime/GPU/Present false. The combined
transform inspector tests pass 29 methods, plus five binary32 tests. Exact-SHA
rebuild and regression results are retained in the ignored postcommit note.

## Disposition and next experiment

Supported now: a projection-bearing, approximately scaled-orthogonal affine
composite for one causally linked decoded vertex, plus its actual reciprocal
and screen-offset arithmetic. Unsupported: shared calibration across geometry,
per-frame camera extraction, physical world scale, off-screen geometry and a
complete relightable scene.

Model/view gauge ambiguity alone is NOT a reason to endlessly search for one
unique world origin: a consistently calibrated camera-relative coordinate frame
could support a clearly labeled scene experiment. What is still missing is
evidence that this calibration applies to more than the one linked sample and
is attached to the right scene/context generation. No arbitrary FOV is allowed.

Next acquire the minimum additional causally linked transform/decoded-vertex
samples needed to test a shared per-context projection and reciprocal-depth
calibration across distinct geometry. Record source matrix, actual projection
operations and actual TA context ownership. Test a wrong calibration and a
stale/mismatched context as falsifying controls. If it is not shared, return the
specific per-draw/unsupported contract rather than averaging unlike matrices.
If it is shared, assess an explicitly camera-relative scene contract; do not
claim unique world reconstruction. No repeated lineage census, framebuffer-light
tuning or real Remix runtime integration is authorized by this algebra result.
