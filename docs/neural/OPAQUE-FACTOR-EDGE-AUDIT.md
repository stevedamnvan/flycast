# FC-067 reciprocal record-depth predecessor

## Disposition

ACCEPTED in bounded self-review: the actual predecessor8c03c9a4 loads the earlier
Y/Z record and computes FR3=1/Z, then hands that factor unchanged to the selected
final-coordinate block8c03c9c0. Starting SHA20e3d3eab. This establishes reciprocal
record-depth provenance for one opaque vertex, not the coordinate system of the
earlier Z, a world camera, universal write coverage, or completion of M2.

Temporary core probes/header removed; ignored factor-edge-a patch/header pair
retains both A and B-negative's identical probe build. Only verifier/tests/docs
are checkpointed. Restored/exact validation is recorded in LOG #173.

## Actual source and edge

One accepted predecessor has seven SHIL operations, two live-in scalar words
and nine dynamic events, within the128-op/32-live-word/512-event bound. Actual
next PC, entry pointer and factor qualify the edge. A pending guard checks the
very next executed JIT block; a different block, reset lease or factor rejects.
The source and destination block observations both record cycle7602640640.

| Operation | Actual evidence |
|---|---|
|Load Y|PC8c03c9a4 reads8ce74254, returnsc2252f6c; byte writers2,2,2,2|
|Load Z|PC8c03c9a6 reads8ce74258, returns409052c6; byte writers1,1,1,1|
|Unit numerator|PC8c03c9a8 assigns binary32 1.0 to FR3|
|Comparison|PC8c03c9aa compares loaded Z against observed FR7=0, returns true|
|Divide|PC8c03c9ac computes1/Z, returns3e630bb6 in FR3|
|Actual successor|PC8c03c9c0, r4=8ce7425c, FR3=3e630bb6|

Exact-rational binary32 division matches the actual result under MXCSRfffd,
toward-zero rounding; FPSCR40001 is retained. The prior record writers precede
these loads, and the final-record overwrite follows the edge. The verifier
replays the earlier source bytes rather than comparing only the final record.
The loaded Y also reaches the final block unchanged. FR0's earlier load is not
inside this predecessor and is not invented from matching bytes.

Together with LOG #172, the selected final output is X=a*(1/Z)+320,
Y=Yrecord*(1/Z)+240, and TA vertex Z=1/Zrecord, with separate actual binary32
rounding steps. The coordinate system and calculation producing Zrecord remain
unknown. The observed positive comparison alone does not prove clipping policy.
Do not generalize this one vertex to all draws, infer FOV/camera axes, reuse
effect calibration, or treat the numeric reciprocal as proof of world-space Z.
The RAM/MMU/cache/debugger/direct-host limitations from LOG #171 remain explicit.

## Controls and preservation

A passes. B-negative changes only the expected successor factor from3e630bb6
to3e630bb7; actual factor, registers, record and scene remain unchanged. The
live next-entry-mismatch rejects and the offline verifier rejects B. Five
offline edge/program controls reject wrong successor, numerator, denominator
SSA, load address and event sequence.

A sixth source-generation control consistently changes the earlier overwritten
Z byte's source/expected/actual fields. The prior final-record/calculation
verifier still accepts that mutation; the new actual-source-load association
rejects it. This prevents final-value equality from hiding a false denominator
generation. Six synthetic methods use independent Y=-20,Z=4,q=1/4 goldens and
cover edge/factor/pointer faults, arithmetic/nonfinite inputs, mode/duplicate
events, load ordering and before-overwrite source generation.

Both A/B exit0/clean close. Each matches27 unique native image planes, three
producer identities and nine raw guidance hashes against hook-free20e3d3eab:
54/6/18 comparisons. Both pass the prior calculation/RAM/SQ/TA chain and its
controls. No production renderer/configuration changes or performance claims.

## Next bounded task

The missing value calculation is now at an already observed producer, not an
unknown caller. Capture the one actual block producing the initial record via
stores8c03c94e/50/52 at cycle7602512960, qualified by the actual effective
destinations8ce74258/54/50. Retain its operations, live-ins, loads/intermediates
and exact store operands with one accepted invocation,128 SHIL operations,
32 live-in scalar words and512 events. Include a wrong-input control and the
same native preservation checks. Reject if the bound is insufficient.

Do not repeat reciprocal/final-block tracing, recursively expand predecessor
edges, or start a transform/matrix census. A matrix operation, if actually
observed, proves only its own operands and calculation until coordinate-space
semantics and broader scene coverage are independently established. World
camera/material/Remix GPU, representative moving coverage and parked strict
replay remain unchanged and incomplete.
