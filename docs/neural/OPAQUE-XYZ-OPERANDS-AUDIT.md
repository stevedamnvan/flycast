# FC-067 opaque XYZ operand-source audit

## Bounded disposition

Restored working-tree validation: four configure/builds pass; enabled selftests
3x298/298, SDK56 with runtime/GPU/Present false, Python92 inspector plus5
binary32 tests pass. Core diff is empty. Exact-commit verification follows.

ACCEPTED in self-review: one actual indexed RAM gather supplies the selected
XYZ SQ stores. This is not the calculation of those coordinate values, camera
recovery, world-space reconstruction, or completion of M2. Starting checkpoint
d98ac822b. Production probes/header removed; ignored patches and matching headers
fc067-xyz-operands-a/b retain the experiment. B and C-negative use the same probe
build with only the diagnostic expected-base control changed.

## Actual source relationship

The selected executing block starts at8c03cc68. B contains one context-qualified
entry,27 scalar SHIL operations, six observed live-in registers, eight actual
load addresses,20 live results and28 value/load events. Entry selection requires
the known cycle, SQ address and actual TA context offset, not a timestamp alone.
Every recorded arithmetic result and SSA dependency is checked, as are seven
stores against R2's actual post-store values. The PCW store precedes this block.

| Evidence | Observed value / relationship |
|---|---|
| Entry destination | r4=e0000020, TA context00509700, packet offset32 |
| Index stream | r5=8c8ebfb4; first returned signed word is05df |
| Position base | r6=8ce6e460 |
| Index mask and shift | r9=3fff, r14=4; masked index is shifted left4 |
| Calculated position record | 8ce6e460 + (05df <<4) = 8ce74250 |
| Returned XYZ loads | PCs8c03cc7c/7e/80 read8ce74250/54/58 |
| Actual XYZ stores | PCs8c03cc82/84/86 write SQ offsets4/8/12 unchanged |
| Color path | index0633 and base8ce78460 select8ce7e790/94 |

The27-op block does integer indexed addressing and copying/packing; it does not
project world positions. The values already exist in RAM. The16-byte stride does
not prove the meaning of the unused fourth word. Sharing base8ce6e460 with an
earlier effect record does not transfer that effect's calibration to this opaque
record. No whole-scene camera interpretation is allowed from this evidence.

## Falsification and preservation

A initially records five loop iterations sharing the same scheduler cycle.
It is retained as a limited locator and rejected by the one-target verifier.
B adds actual context/offset/destination selection and accepts exactly one entry.
C-negative requests base8ce6e461 against actual8ce6e460 without changing registers
or memory; entry-operand-mismatch rejects it. Original/copied packet words remain
identical to B. Five offline controls independently mutate the internally
consistent base, mask, shift, actual load address and store SSA source. The
base/mask/shift controls preserve reported input self-consistency and must fail
against actual arithmetic/addresses, not merely an expected-value field.

Eight synthetic test methods cover scalar index arithmetic, signed shifts and
comparisons, missing/reordered dynamic events, stale SSA, wrong returned loads,
wrong target and the controls. Fixture values are synthetic, not captured assets.
Verifier scope is the observed scalar operations; unknown operations fail closed.

A/B/C each exit0 and close cleanly. All27 unique image planes and three producer
identities per capture match fc067-writers-d98ac822b-native:81 image comparisons
and nine producer comparisons. Additionally27 raw depth.f32/motion.rg16f/draw-ID
files (three files x three frames x three captures) match SHA256 byte-for-byte.
No performance claim. Broad shell-glob probes for core/rec* and xbyak* failed on
Windows; corrected rg -g queries were used, with no success claimed for failures.

## Next bounded record-producer task

The current block's value calculation is outside this invocation. Stop recursive
caller tracing. Next scope only the actual writes producing guest RAM span
8ce74250..8ce7425b in this target frame, with physical RAM aliases accounted for.
The retained list-init boundary is cycle7601901888/context00509700; use a fresh
actual lifecycle boundary, not a later snapshot or a remembered matching value.

Observe relevant CPU scalar/wide stores and SQ/bulk-copy paths that can touch
these12 bytes. Unsupported/unobserved write paths must be explicit rejection or
a limitation, not silently treated as absent. Retain per-byte last writers,
source values, actual destination bytes and reset/overwrite boundaries through
the exact observed XYZ reads. Bound to64 write/copy witnesses and two emulated
seconds; no whole-buffer watch, site census or arbitrary caller trace. If no
complete writer generation is seen, report it rather than inferring a producer.

Acceptance requires one same-run complete producer generation linked through
the observed XYZ reads, SQ packet, TA decode, producer stamp and native inputs,
plus a falsifying word/generation control. Do not call a matching write a
projection or camera until its calculation and semantics are separately proven.
