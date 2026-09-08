# FC-067 M1/M2 requirements audit

LOG #174 scope stop: initial block8c03c94c only stores precomputed coordinates.
Their calculation is outside the approved block. No automatic predecessor/
caller/site expansion. Audit unmet M2 scope after checkpoint and request a
new bounded route if necessary; OPAQUE-INITIAL-STORES-AUDIT.md governs.

LOG #173 update: selected predecessor proves1/Zrecord, linking the earlier RAM
generation to the actual final-coordinate input. Record Z's coordinate system
and initial calculation remain unknown; no world camera/M2 promotion. Next
bounded initial-record producer in OPAQUE-FACTOR-EDGE-AUDIT.md.

LOG #172 update: actual final coordinate block8c03c9c0 proves its separate
binary32 multiply/add steps and Z-factor copy under measured rounding. The
factor's producer/depth semantics and world camera remain unknown. Next one
bounded predecessor edge in OPAQUE-COORDINATE-CALC-AUDIT.md; no broader M2 claim.

LOG #171 update: bounded byte-writer replay now observes the final record stores
8c03c9ca/cc/ce and their overwritten predecessors. Actual loads link the final
values to opaquevertex4. This is not coordinate calculation, universal memory
observation, camera or GPU reconstruction. Next selected block calculation in
OPAQUE-RAM-WRITERS-AUDIT.md; no whole-scene/M2 requirement is promoted.

LOG #170 update: actual indexed addressing and returned RAM loads now explain
the inputs to XYZ stores. They are precomputed at8ce74250/54/58; their producer
calculation and world camera remain unknown. Next bounded record generation in
OPAQUE-XYZ-OPERANDS-AUDIT.md; no whole-scene/M2 requirement promoted.

LOG #169 update: actual SQ filling stores now bind all32 packet bytes to the
selected opaque vertex. XYZ stores8c03cc82/84/86 are observed, but their operand
calculation and world camera are not recovered. Next bounded operand-source
task is OPAQUE-SQ-WRITERS-AUDIT.md; no whole-scene/M2 requirement is promoted.

2026-09-08 scope update: user approved OPAQUE-REVERSE-PLAN.md. LOG #168 proves
the selected type3 SQ-to-TA copy-to-final-opaque-vertex association, including a
failing live generation query. Original SQ filling stores and CPU/world
provenance remain missing; requirements below are not promoted. R1 selected-path
review accepts; next bounded R2 physical SQ last-writer witness is scoped.

Audit source84231a2af. Overall objective remains INCOMPLETE. A bounded no-go
disposition is not a whole-scene reconstruction pass or completion of strict M2.

| Requirement | Current authoritative evidence | Disposition |
|---|---|---|
| M1 bounded scene contract and public adapter review | remake_scene.cpp rejects unknown projection/world/normal/transform and omissions; adapter preflights topology before API resources; SDK mock56 checks pass | CPU/public-header slice accepted; no independent or GPU review claim |
| Bounded real-game packet export/decoder | pvr_scene_capture.cpp emits v2 bit-preserving projected data; limits/decoder exercised by selftests; current native captures contain real packets | Implemented, not a portable complete scene |
| Source materials and sorted list correctness | Material sidecar captures separate textures/palettes; v2 separates source vertex ranges from sorted GPU commands | Bounded supplements accepted; older omission tables are historical |
| Native moving replay alignment | Retained frame1804 replay proof says decoded-versus-retained0 pixels, source13 pixels/max1LSB; source_frame_exact=false, passed=false | Strict equality failed/parked by user; do not relabel or restart precision loop |
| Falsifying raster controls | Same proof reports wrong viewport264181 pixels and wrong depth307159 pixels | Raster controls fail as intended; not recovered camera controls |
| Deterministic input identity | New producer metadata exposes one-step differences despite equal host frame/reset counters; separate no-reset runs have exact matching timing/images | Bounded exact runs accepted; blanket cross-run determinism unproven |
| Explicit real camera/depth provenance | LOG171-174 link initial register stores, RAM generations,1/Zrecord, final viewport arithmetic and TA output for one opaque vertex. Initial coordinate calculation/space and packet camera/world remain unknown; prior effect domains do not generalize | Selected reciprocal-record-depth proven; whole-scene/opaque camera not proved |
| Native/configuration preservation | Temporary CPU hooks removed; restored builds/selftests and hook-free J images/stamps match; no external configuration edits in this investigation | Scoped preservation accepted; not a new full transition/performance matrix |
| Real Remix GPU/render/presentation | Adapter returns api-submitted-not-rendered-or-presented, no loader/readback path; mock explicitly runtime/GPU/present=false | Not implemented or proved; no proprietary runtime acquisition authorized |
| Commit/push and governing disposition |d38c25f76 exact-SHA four builds,3x298 selftests,SDK56,Python116+5 pass; hook-free capture matches27 unique planes/three producer stamps/nine raw guidance files; fork ref verified. LOG174 checkpoint verification follows | Evidence checkpoints delivered; overall M2 incomplete |

Read-only checks in this audit inspected current source predicates and actual
frame1804 proof JSON, not just prior summaries. New probes are not authorized by
this table. A failed PowerShell rg wildcard lookup was corrected using file
inventory; it provided no source evidence.

## Next decision boundary

The approved bounded source investigations have produced two effect domains,
one uncorrelated scalar routine and a selected opaque register-to-RAM-to-TA
chain including reciprocal record depth. The initial XYZ calculation is still
outside the final approved block. The plan forbids indefinite guest/caller
tracing. Continue checkpoint validation and requirement audit, but do not
automatically expand that investigation to satisfy a completion label.

The unresolved reconstruction requirement needs a new bounded route: either
title-specific authoritative camera/mesh information (with legal provenance),
or an explicitly approved further producer investigation with a concrete target
and stopping criterion. A guessed-FOV depth extrusion, standalone synthetic
Remix demo, or altered shader cannot substitute for the missing real-game proof.
The parked strict replay requirement also remains open unless the user chooses
to reprioritize it; no tolerance change is implied. Do not mark the goal complete.
