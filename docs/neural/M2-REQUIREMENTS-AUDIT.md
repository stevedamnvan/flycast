# FC-067 M1/M2 requirements audit

2026-09-08 scope update: user approved OPAQUE-REVERSE-PLAN.md. LOG #167 proves
only the selected type3 TA-buffer-to-final-opaque-vertex association. Actual
original transfer and CPU/world provenance remain missing; the requirements
below are not promoted. R1 copy witness is next; R2 awaits full R1 review.

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
| Explicit real camera/depth provenance | Current packet says camera/world unknown, raw PVR depth before log shader; verified petals/fish are limited effect domains; third scalar candidate remains uncorrelated | Whole-scene/opaque camera not proved |
| Native/configuration preservation | Temporary CPU hooks removed; restored builds/selftests and hook-free J images/stamps match; no external configuration edits in this investigation | Scoped preservation accepted; not a new full transition/performance matrix |
| Real Remix GPU/render/presentation | Adapter returns api-submitted-not-rendered-or-presented, no loader/readback path; mock explicitly runtime/GPU/present=false | Not implemented or proved; no proprietary runtime acquisition authorized |
| Commit/push and governing disposition |84231a2af exact-SHA four builds,3x284 selftests,SDK56,Python65+5 pass; hook-free capture matches27 unique planes and producer timing; fork ref verified | Evidence checkpoint delivered; overall M2 incomplete |

Read-only checks in this audit inspected current source predicates and actual
frame1804 proof JSON, not just prior summaries. New probes are not authorized by
this table. A failed PowerShell rg wildcard lookup was corrected using file
inventory; it provided no source evidence.

## Next decision boundary

The approved bounded source investigation has produced two effect domains and
one uncorrelated scalar routine. Its plan explicitly forbids indefinite guest
decompilation/caller tracing. Continue checkpoint validation and delivery, but
do not automatically expand that investigation to satisfy a completion label.

The unresolved reconstruction requirement needs a new bounded route: either
title-specific authoritative camera/mesh information (with legal provenance),
or an explicitly approved further producer investigation with a concrete target
and stopping criterion. A guessed-FOV depth extrusion, standalone synthetic
Remix demo, or altered shader cannot substitute for the missing real-game proof.
The parked strict replay requirement also remains open unless the user chooses
to reprioritize it; no tolerance change is implied. Do not mark the goal complete.
