# FC-067 initial record store boundary

## Disposition

ACCEPTED in bounded self-review: actual live FR0/FR1/FR2 operands are copied
into the earlier XYZ record by block8c03c94c. NOT_REVIEWABLE within this block:
the calculation and coordinate system of those precomputed operands. No
transform, floating-point arithmetic or matrix operation is present. This is
a stopping boundary, not another automatic predecessor assignment. Starting
SHAd38c25f76. Overall M2 remains incomplete.

Temporary hooks/header removed; ignored initial-record-a/b patch/header pairs
retain the experiment. Only verifier/tests/docs are checkpointed. B and
C-negative share one build. See LOG #174 for restored and exact validation.

## Actual bounded witness

One invocation at cycle7602512960 has ten SHIL operations, five live-in scalar
words and ten dynamic events. Actual store destination8ce74258 qualifies the
buffered invocation; neighboring iterations are not accepted by timestamp.
The128-op/32-live-word/512-event bound is not exceeded or expanded.

| Live operand | Actual bits | Actual store |
|---|---|---|
|FR2|409052c6|PC8c03c94e ->8ce74258 (earlier Z)|
|FR1|c2252f6c|PC8c03c950 ->8ce74254 (earlier Y)|
|FR0|c42bc1dc|PC8c03c952 ->8ce74250 (earlier X)|

Entry r0=8ce7425c is predecremented for each store. Entry r6=78 is decremented
and compared to zero; the branch-condition result and final pointer increment
also replay exactly. There are no loads and no modifications to the three
floating-point coordinate operands. FPSCR/MXCSR are metadata only here; no
floating-point calculation is inferred from their presence.

The source operands match the actual initial RAM generation, which precedes the
already proven reciprocal-Z and final-coordinate blocks. The full prior chain
still passes through the selected opaque vertex. Initial-coordinate-calculation
and world-camera flags explicitly remain false. Sharing a RAM array with an
earlier effect does not supply missing coordinate-space provenance.

## Controls and preservation

A and B both pass the store verifier. B changes the diagnostic query target
from unobserved r4 to actual FR2; no negative run on the unused A selector is
claimed. C-negative changes only expected FR2 from409052c6 to409052c7. Actual
register, RAM, reciprocal calculation and scene remain unchanged; live and
offline rejection occur. Four offline controls mutate a self-consistent input,
store SSA source, destination and event sequence.

Five synthetic methods verify independent register/store goldens, controls
despite shadowing earlier RAM log lines, stale SSA/PC/pointer faults, duplicate
or missing events, live rejection and refusal to promote the block to float/
transform arithmetic. All A/B/C exit0/clean close. Each matches27 unique image
planes, three producer identities and nine raw guidance hashes against
hook-freed38c25f76:81/9/27 comparisons. Prior factor/calculation/RAM/TA verifiers
and the source-generation control pass all three. No performance claim.

## Scope stop and remaining goal

Do not start another predecessor trace, caller census, transform-site search,
camera guess or repeat capture automatically. The current bounded record
producer assignment is exhausted: coordinates are live-ins, not calculated in
this block. Further tracing requires a newly approved concrete target and stop
condition; this audit does not grant that authority to itself.

The remaining-goal audit still distinguishes implemented bounded scene/material
export and tested public-header adapter from real reconstruction. Current
remake_scene.cpp rejects unknown projection, transforms and normals; the public
adapter reports api-submitted-not-rendered-or-presented. The original M2 plan
requires moving native alignment plus camera/depth controls. Strict source
equality remains user-parked, not passed; this operand chain does not replace
whole-scene camera provenance or moving coverage. Real GPU/relighting/chaining
remain later unproven milestones, not successes inferred from this trace.

After checkpoint verification, audit current M2 requirements against actual
artifacts and return the exact unmet scope. If there is no remaining safe
implementation under current authority, request a new bounded route. Keep the
full goal incomplete; do not broaden tracing or shrink acceptance to the
already proven one-vertex path merely to close it.
