# FC-067 observed derived-coordinate arithmetic

Baseline: `adff200afc4d525bb9ff4b5defdc5cbce86e2ffd`.
**ACCEPTED one bounded arithmetic/store observation (self-review).**
Camera recovery, physical depth calibration, TA submission and Remix GPU
execution remain unproven. This does not close the parked native replay gate.

## Actual observations

The disposable x64 probe continues the original transform/store witness at
SH4 cycle 6000468096. It observes actual register operands/results around four
existing generated arithmetic operations, without replacing their computation.
Post-store checks read the actual aligned guest-RAM destinations. Probe source
is removed from production; positive and negative patches remain ignored.

| Instruction | Operation | Actual operand words | Actual result |
|---|---|---|---|
| 8c070c74 | divide a/b | 3f800000, 4163a016 | 3d8ff4b2 |
| 8c070c88 | multiply a*b | 3f851eb8, 3d8ff4b2 | 3d95b6cd |
| 8c070c9a | fused a+b*c | 43a00000, 457dcb43, 3d8ff4b2 | 44175b90 |
| 8c070ca2 | fused a+b*c | 43700000, c50958f1, 3d8ff4b2 | 42ab1024 |

All operations report MXCSR 0000fffd: round toward zero, with DAZ/FTZ enabled.
The observed nonzero operands/results are normal finite binary32 values. The
exact-rational reference intentionally rejects subnormals, overflow/nonfinite
values and does not model signed-zero subtleties. It is not a full SH4 emulator.
The fused marker follows the actual existing FMA/configuration branch.

For this sample, original Z feeds 1/Z; its rounded result is shared by the
depth scale (3f851eb8, approximately 1.04) and x/y operations. The x/y offsets
are exactly 320 and 240; the multiplicands equal original transformed X/Y.
These recorded operands and compiled operations establish this local arithmetic
observation, not an independently reconstructed whole-program dataflow graph.
The resulting stores/readbacks are:

- 8c070c92 -> 8c00f304: 3d95b6cd (old generation ends here).
- 8c070c9c -> 8c00f2fc: 44175b90.
- 8c070ca4 -> 8c00f300: 42ab1024 (candidate generation 2 complete).

The intermediate overwrite is not silently treated as unchanged geometry.
No TA packet or render-frame ownership is attached. The source computation
precedes retained frames 1804-1806; image equality is preservation evidence only.

## Controls and retained attempts

Working A captured multiply and two fused operations but not the division.
Working B adds the executed division at 8c070c74. Working C-negative flips only
the x-offset sign in the diagnostic shadow expectation (320 to -320). Actual
guest operands, arithmetic and writes are unchanged. B/C captures both return
exit 0, three frames and clean close. The verifier rejects exactly one shadow
operation; deliberately wrong nearest rounding also fails the reciprocal by
one ULP. Actual four results and three stores remain identical across B/C.
Both runs match the restored adff200af baseline for all nine named native image
planes on three frames: 54 comparisons total.

Five exact-arithmetic unit tests and four derived-parser test methods pass;
the latter include ten field mutations, reordered events and missing-negative
rejection. Parser tests mock the previously verified span and are not guest
execution evidence. Real capture verification also runs the earlier complete
source-store/span checks and isolates the final launch of append-only logs.
No failed build or capture occurred in this arithmetic tranche. Limited A is
retained rather than mislabeled as complete division evidence.

Raw runs: sibling `flycast-evidence/fc067-derived-working-a`, `-b`,
`-c-negative`. Exact probe patches: ignored
`build-neural-automation/fc067-derived-positive.patch` and `-negative.patch`.
No proprietary binary/configuration or game asset is staged.

After removing the probe, automation, NGX, no-NGX and feature-off builds all
return 0. The three enabled selftests each pass 243/243; SDK contract passes
56/56 with runtime/GPU/presentation explicitly false. All 15 store/span/derived
parser methods pass together, and the real B/C verifier passes after restoration.
The source configuration hash remains unchanged. Exact-commit rebuild/capture
results are recorded separately in the ignored postcommit evidence note.

## Next concrete task

Follow this completed derived RAM generation through executed reads/copies to
one specific SQ/DMA/TA submission, or return a bounded uncorrelated result.
Use effective addresses, operation dependencies and actual committed/readback
words; similar values alone are not causal lineage. Bound trace lifetime and
events, stop on overwrite/reset, and include a failing association control.
Do not repeat the arithmetic census, expand to arbitrary memory scanning,
guess a camera/FOV, or start a Remix runtime integration. Only after a submitted
vertex is linked can camera/material reconstruction feasibility be assessed.
