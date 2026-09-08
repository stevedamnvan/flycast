# FC-067 two linked instances share a bounded calibration

Baseline: `0f60b82613c60e9d3e8e16cb080800a176bb7194`.
**ACCEPTED two linked transform instances in one replay context (self-review).**
Not whole-scene/all-draw calibration, semantic geometry-family coverage, visible
frame ownership, physical scale, world camera or Remix GPU evidence.

## Selection and actual identity

The removed probe selects the original executed transform or the first different
matrix at the same proven instruction site. Selection is bounded by 200,000
candidates and 0.1 emulated seconds after the first eligible call. The different
matrix appears at candidate 4, only 1,792 SH4 cycles after the original.
This is a second actual executed matrix, not a perturbation of the first.

A CPU-side list-initialization epoch is recorded at source selection and actual
TA copy; its saved value is carried into decoded-vertex observation. The original
and different captures both report source/copy/decode epoch 1310 and context
00509700. They use the same deterministic replay and restored starting saves.
The following identity is checked, not inferred from context address reuse:

| Observation | Original | Different matrix |
|---|---:|---:|
| Source SH4 cycle | 6000468096 | 6000469888 |
| Polygon TA bulk cycle | 6002105984 | 6002105984 |
| Bulk packet count | 928 | 928 |
| Packet byte offset | 704 | 800 |
| Copied TA byte offset | 507616 | 507712 |
| Decoded vertex index | 14526 | 14529 |

Bulk RAM origins computed from destination minus packet offset agree. The copied
TA slot spacing also agrees with the bulk packet spacing (96 bytes). Each sample
passes the full source/arithmetic/queue/RAM/TA-copy/decoded-xyz chain. Actual
source input vectors are the same local point; the matrices and decoded vertices
are different. No character/background/material-family identity is claimed.

## Calibration result and controls

Normalize each matrix's x/y row norms by its z row norm to avoid silently
identifying a common uniform model scale with a projection scale. The observed
normalized pairs are:

- Original: 614.7144711397532, 565.537224236165.
- Different: 614.7145022696874, 565.5372647819627.

Both observed projection operations retain the exact reciprocal-depth multiplier
word 3f851eb8 and screen offsets 320/240. Their independent transform algebra
and actual rounded decoded coordinates verify. Applying the original normalized
calibration to both composites has maximum reprojection error
2.995132695104985e-05 pixels, below the declared 0.001-pixel tolerance, over the
two actual points plus six analytic neighboring points. The neighboring points
are synthetic, not additional game observations. Physical depth scale is unknown.

Offline parsed-input mutation controls double the shared x scale or change the
second sample's epoch by one; each is rejected. These are validator controls,
not runtime failure injection or a real stale-context transition test. Five
independent synthetic shared-calibration test methods also pass, covering scale,
epoch, bulk/context mismatch and duplicate-instance rejection.

Both three-frame captures close cleanly with exit 0. Nine native image planes
match the restored 0f60b8261 baseline for frames 1804-1806: 27 each / 54 total.
Those later images prove preservation, not that the sampled vertex belongs to
retained frame 1804. The second decoded point is outside the 640x480 viewport;
no on-screen world-object coverage follows merely from successful decoding.

## Retained rejected assumptions and implementation limits

The first different-sample verification rejects because nearest and toward-zero
rounding happen to give the same reciprocal for that input. The original sample
still supplies the required discriminating rounding control. A new explicit
internal option allows additional samples without demanding that every input
distinguish those rounding modes; exact actual arithmetic/output checks remain.
The next attempt rejects the original fixed memory-event numbers. The second
sample's observed profile is 33/48/50/52/377 instead of 33/58/60/62/381. Expected
profiles are explicit, bounded and ordered; original CLI behavior remains strict.
Instruction, value, dependency, destination and termination checks are retained.
No compiler or game-capture failure occurred.

Temporary production edits/header are removed. Exact patch/header snapshots are
ignored build-neural-automation/fc067-calibration-probe.patch and
fc067-calibration-probe-header.h. Raw runs remain in sibling flycast-evidence/
fc067-calibration-working-original and fc067-calibration-working-different.
No proprietary binaries, external configuration, media or raw captures staged.

Four restored-source configure/builds return 0; three enabled selftests pass
243/243 each and SDK mock passes 56/56 with runtime/GPU/Present false. Transform
inspector tests pass 31 methods, including explicit new-profile acceptance with
default-profile rejection and bad-arithmetic rejection when the extra rounding
control is not applicable. Five binary32 and five shared-calibration tests pass.
Exact-commit rebuild/capture results are retained in the ignored postcommit note.

## Next concrete task

Bind this witnessed epoch/calibration to its actual bounded scene snapshot and
record the sampled draw/list/material/primitive associations and visibility.
Do not attach the calibration to a later retained frame just because its numeric
context address matches. Determine whether the two samples represent relevant
3D world geometry or a narrower primitive family before claiming scene coverage.
Then test a labeled calibrated camera-relative scene contract with explicit
unsupported coverage and wrong/stale-calibration controls. Do not re-run matrix
presence censuses, guess FOV, average unlike projections, or promote to a whole
scene/world camera/Remix runtime without the corresponding evidence.
