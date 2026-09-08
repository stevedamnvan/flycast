# FC-067 final opaque coordinate calculation

## Disposition

ACCEPTED in bounded self-review: one selected executed block computes the final
X/Y values and copies the Z factor for the previously identified opaque record.
Its17 SHIL operations, seven live-in scalar words and18 dynamic events replay
exactly. This is not the origin of those live-ins, a recovered world camera,
universal memory observation, or completion of M2. Starting SHA8531ed2bb.

Temporary probes are removed. Ignored record-calc-a/b patch/header pairs retain
the experiment; B and C-negative share one build. Only the verifier, synthetic
tests and governing documents are checkpointed. See LOG #172 for validation.

## Actual calculation

The block begins at8c03c9c0, cycle7602640640. Entry r4=8ce7425c; actual predecrement
stores8c03c9ca/cc/ce address8ce74258/54/50. Diagnostic data is buffered until
the actual first store qualifies this record, so a neighboring iteration at
the same timestamp cannot become the selected witness. Unselected buffers are
discarded, not accepted as extra samples. One invocation is retained within
128-op/32-live-word/512-event/64KiB bounds.

Let a,b,q be the observed FR0,FR1,FR3 live-ins. The block performs:

    X = round32(round32(a * q) + 320)
    Y = round32(round32(b * q) + 240)
    Z = q

The two multiplies and two additions each have an actual intermediate result.
MXCSR=fffd at entry/exit, rounding control3 (toward zero); FPSCR=40001 is retained.
Exact rational arithmetic with independently rounded binary32 intermediates
matches all four returned words. This is not fused multiply-add or an assumed
round-to-nearest computation. The bounded oracle rejects nonfinite/subnormal
operands/results and unknown operations; it is not a general SH4 emulator.

| Live-in | Actual bits | Role in this block |
|---|---|---|
|FR0|c42bc1dc|a, multiplied by q|
|FR1|c2252f6c|b, multiplied by q|
|FR3|3e630bb6|q, multiplied into X/Y and copied to Z|
|FR4|43a00000|X offset320|
|FR5|43700000|Y offset240|

Final XYZ bits4327ab3f/4366d7f6/3e630bb6 match the actual RAM last writers, actual
consumer loads, SQ stores, packet copy and TA decode for opaquevertex4. The
block's final load prefetches the next record at8ce74260; that value is not
misattributed to the selected vertex. Pointer/count/decrement/comparison/branch
SSA also replays, including the predecrement addressing of the three stores.

FR0/FR1 are bit-consistent with the earlier overwritten record, but this block
does not load those live-ins. Their earlier transfer is not thereby proven.
There is no divide or matrix operation here. Do not call q reciprocal world Z,
assume a projection matrix/FOV, or import fish/petal calibration from the shared
buffer. Full position-calculation provenance remains false; only this final
coordinate block is now proven. RAM observer limitations from LOG #171 remain.

## Controls and preservation

A initially omitted floating-point mode and is retained/rejected by the final
verifier. B includes actual mode at both ends and passes exact replay. C-negative
changes only expected FR4 from43a00000 to43a00001; the actual register, arithmetic,
RAM and packet remain unchanged. Runtime entry-operand-mismatch and verifier
rejection occur. Five offline controls mutate a self-consistent offset input,
multiply opcode, SSA factor, store address and event sequence.

The first address-control implementation accidentally changed the earlier RAM
observer's line, leaving the CALC block unchanged. The control was accepted;
this failed harness attempt is retained. Mutations now target CALC lines only,
with a regression test containing the shadowing earlier RAM line. All five
controls then reject on the actual B capture. A failed register-header path
search was corrected to sh4_if.h; it is not evidence of a missing source seam.

Eight synthetic methods use independent golden values a=-100,b=-20,q=1/4 and
X295,Y235,Z1/4. They cover controls, shadowed mutation targets, directed rounding,
unsupported floating-point values/ops, missing modes, live rejection, wrong
SSA/addresses/PCs, duplicate invocations, missing events and mode changes.

All A/B/C captures exit0/clean close. Each matches27 unique image planes, three
producer identities and nine raw guidance hashes against hook-free8531ed2bb:
81/9/27 comparisons. All pass the prior RAM/SQ/XYZ/copy verifier and its controls.
No performance or multi-title claim is made. No external configuration changed.

## Next bounded task

Do not repeat this final block or start an arbitrary caller census. Identify
one actual executed predecessor edge supplying FR3=q to this exact invocation
at8c03c9c0. Qualify it by the actual destination PC, entry record pointer and
factor value, with reset/overwrite termination. Capture that predecessor's
operations, live-in values and intermediate results only: one accepted edge,
128 SHIL operations,32 live-in scalar words,512 events. If the factor is not
produced within that bounded predecessor, report that limitation rather than
recursively expanding. Retain a wrong-factor/edge control and native comparison.

This is a focused missing operand producer, not authorization for whole-scene
reverse engineering or renderer integration. Do not infer the factor's depth
semantics from its numeric range. Camera/world/material/Remix GPU requirements,
representative moving coverage and parked strict replay remain unchanged.
