# FC-067 selected opaque RAM writer audit

## Disposition

ACCEPTED in bounded self-review: the observed twelve-byte record has two sets
of executed CPU writes. Per-byte last-writer replay links the second set through
the actual XYZ loads, SQ writes, SQ copy and TA decode to opaque draw1/vertex4.
This does not prove the arithmetic producing those coordinates, a world camera,
all possible host write paths, or full M2. Starting SHA:1a9e4ab2d.

Temporary core hooks are removed; ignored ram-record-a/b/c/d patch/header pairs
retain the exact experiments. D and E-negative use the same probe build. No
production rendering change or external configuration change is part of this
slice. Restored and exact-checkpoint validation is recorded in LOG #171.

## Observed record

One fresh actual list-init at cycle7601901888/context00509700 starts a nonrearmed
lease for RAM8ce74250..8ce7425b. Six writes fit the64-witness/two-emulated-second
bound. The first set occurs at7602512960; the final set at7602640640. Actual
consumer loads occur at7602643776. Event order, not timestamp alone, distinguishes
the writes. All observed stores are32-bit; wider/partial alias handling is
synthetic coverage, not an actual64-bit game-store claim.

| Event | Executed store PC | Record component | Previous writer | Final consumer |
|---|---|---|---|---|
|1|8c03c94e|Z|unobserved|overwritten by4|
|2|8c03c950|Y|unobserved|overwritten by5|
|3|8c03c952|X|unobserved|overwritten by6|
|4|8c03c9ca|Z|1|8c03cc80|
|5|8c03c9cc|Y|2|8c03cc7e|
|6|8c03c9ce|X|3|8c03cc7c|

The final twelve byte-writer IDs are6,6,6,6,5,5,5,5,4,4,4,4. The corresponding
returned words are4327ab3f/4366d7f6/3e630bb6. Each actual CPU operand is compared
with the post-store RAM bytes. Read checks use the actual effective load address
and returned value, and appear directly before the linked XYZ result event.
The16-byte record stride still says nothing about the unused fourth word.

## Write-path scope and limitations

The diagnostic observes x64 non-MMU SHIL scalar/wide stores (including immediate
and memory-handler lowering), generic and fast/non-vmem RAM-directed SQ copies,
direct DMA/pointer copies and DMA handler writes. Copy witnesses snapshot only
the source bytes overlapping this record before the copy, then compare actual
destination bytes. There is no whole-buffer scan. Only CPU writes touched the
selected record in D; other paths are inspected/instrumented, not live positive
coverage. Physical Dreamcast16MiB RAM aliases are replayed independently.

Unknown interpreter fallbacks terminate the lease. The actual fallback is the
Reios GD-ROM HLE opcode085b at8c001006. D observes command4/drive status exactly
once. Its two actual status-write operations are instrumented; neither overlaps
the record. Idle EXEC_SERVER and GET_CMD_STAT are guarded/instrumented but not
positively exercised in this interval. Other HLE commands and full interpreter
execution terminate observation. Reset/reinit, mismatch, time/cap and a write
during the three-load consumption terminate the lease as well.

Do not promote this to a universal memory observer: MMU-translated stores,
strict-cache writeback, arbitrary debugger/cheat/direct host-pointer writes,
and other titles/platforms are not covered. The verifier reports this limitation
and keeps cpu_producer_provenance and position_value_calculation_proven false.
No equal-byte unobserved overwrite may be inferred absent merely from final
equality. Any follow-up using those modes must add focused rejection/coverage.

## Falsification and retained attempts

- A stops at an unspecified interpreter fallback with zero record writes.
- B identifies opcode085b/PC8c001006 and stops at that same uncovered path.
- C guards idle/status calls but rejects actual command4 (drive status).
- D includes the actual drive-status write seam and covers six writes/three
  linked loads. Five offline controls reject wrong lease generation, previous
  writer, coverage, terminal reason and expected byte.
- E-negative flips only the diagnostic expected byte at event3 fromdc todd;
  source and RAM remain dc. Runtime write-mismatch and offline rejection occur.

All five captures exit0/clean close. Each matches27 unique native image planes,
three producer identities and nine raw depth/motion/draw-ID SHA256 comparisons
against the hook-free1a9e4ab2d baseline:135/15/45 respectively. The complete prior
XYZ/SQ/copy verifier and its five controls pass for all five captures. A/B/C/E
are rejected as RAM writer evidence, not mislabeled successful producer traces.

Ten synthetic methods cover aliases/partial overlap, wide stores, per-byte
overwrites, missing/reordered/duplicate bytes, stale IDs, unknown paths, wrong
reads, malformed schemas, no rearm, caps and writes during consumption. The
first preservation script encountered KeyError for C's earlier HLE schema;
explicit field validation now rejects it with ValueError, and the full script
was rerun successfully. Wrong source-path searches (dmac/interpreter/cache
locations) failed before corrected paths; no success is claimed for them.

## Next bounded task

Do not repeat the RAM/SQ/operand locator or expand into arbitrary callers.
Observe the actual executing block containing final stores8c03c9ca/cc/ce at
cycle7602640640, selected by its effective destination overlapping this exact
record. Retain its live-in values, actual operations, loads, intermediate
results and source operands for those three final stores. Bound to one accepted
invocation,128 SHIL operations and32 live-in scalar words; reject if insufficient
instead of silently expanding. Include a wrong-operand control and same native
preservation checks. Keep existing write-path limitations explicit.

This next task proves or rejects the selected coordinate calculation only.
Do not import fish/petal calibration because of the shared buffer base. Unknown
floating-point or vector operations must fail closed in the evaluator; no
guessed camera/FOV, further caller tracing, renderer replacement or real Remix
GPU claim follows automatically. Full camera/material/Remix requirements and
parked strict replay remain unchanged.
