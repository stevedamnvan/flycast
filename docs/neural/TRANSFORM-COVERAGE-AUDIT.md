# FC-067 bounded transform coverage

Implementation baseline1043fa4e3. Accepted scope: mathematical and tracked RAM
lineage for15 selected samples, not full-scene rendering or a recovered camera.

Draw1 vertices4/5/6 and draw26 vertices147/148 are linked across producer
ordinals1781..1783, context generations1790..1792. Draw26 vertex146 (slot3)
has an accepted later RAM/gather map but no observed initial transform; it
remains unsupported here. No missing record is assigned an invented origin.

The known initial FTRV/store and reciprocal/projection paths account for60
selected block invocations. Draw26 additionally executes six contributions:
FTRV at8c03c96a followed by three additions into existing XYZ. The fourth
source word is preserved exactly, including nonunit values and its observed
use in integer address selection. This is not a claim about bones or weights.

The checker verifies108 observed record writes,45 final ledger reads and the
existing SQ/copy/decoder association. Fifteen X loads at8c03c9d6 are bound to
the current ledger, next JIT entry and the reciprocal block's preserved FR0.
Initial transforms, accumulation, projection and pointer arithmetic use the
existing independent binary32/SSA checkers. No matrix is called a world camera.

## Evidence

- `fc067-extra-writer-c`: one arithmetic witness; original full-chain rejection
  remains active. Wrong-target D rejects; C/D/E imagery and scene agree.
- `fc067-accumulation-a`: rejected at unsupported slot3. B covers15 supported
  samples; C wrong-target rejects. Retain the8 one-level color components and
 1023/5905 raw-depth-byte differences, including derived flicker differences.
- `fc067-xload-a`: rejected diagnostic callback gated one block late.
- `fc067-xload-b`:15 supported links pass after correction.
- `fc067-xload-c-negative`: wrong expected X rejects; actual loads and29 PNGs,
 3 raw depth and3 motion files are byte-identical to B.

Captures are three retained Soulcalibur frames with exit0 and clean close.
They are synchronous diagnostics, never performance measurements. All failed
attempts remain in LOG180..184 and ignored local evidence; no retry erases them.

Synthetic tests distinguish responsibilities: seam tests validate arithmetic
with independent goldens; aggregate tests mock those already-tested arithmetic
decoders and validate composition/ownership with independent synthetic metadata.
Real end-to-end captures exercise the actual decoders together. These scopes
must not be conflated into an independently generated full GPU fixture.

Temporary core hooks were removed after preserving the complete patch/header
under ignored `fc067-transform-final-probe` files. Restored four builds pass;
automation/NGX/no-NGX selftests298/298 each; Python inspector suite158 tests;
SDK mock63/63 remains runtime/GPU/Present false. Exact-commit checks follow the
commit; this audit does not preclaim them.

## Next decision

Stop retracing these links. Use their observed transforms and existing algebra
to determine whether a consistent camera-relative coordinate contract can be
defined without invented FOV, scale or world semantics. Quantify unsupported
geometry; do not silently promote15 samples to an entire draw or scene. The
independent runtime bring-up executable is available, but actual Remix GPU,
readback/lifetime and combined DLSS5 presentation remain unproven.
