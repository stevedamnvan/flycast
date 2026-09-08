# FC-067 opaque-draw reverse provenance scope

## R1 working checkpoint

Restored working-tree validation: all four configure/builds pass; three enabled
selftests298/298 each; SDK mock56 (runtime/GPU/Present false); Python70 inspector
and5 binary32 tests pass. Exact-commit verification/delivery follows checkpoint.

2026-09-08: R1 is PARTIAL. Temporary decoder probe B links this target's
type3/part0 32-byte TA packet at offset32 to actual vertex input offset36 and
final opaque draw1/index4/vertex4. The four-byte PCW prefix is outside the
TA_Vertex3 member. Probe A incorrectly started a full packet at member offset36;
retain A as rejected evidence, not a valid packet. Both native captures exit0
and close cleanly; each matches27 unique planes and three producer stamps of
the retained84231a2af baseline. No earlier transfer or CPU producer is proven.

Retained captures: fc067-reverse-decode-a and fc067-reverse-decode-b outside
Git. reverse_decode_inspect.py rejects A and accepts B. Five synthetic methods
exercise offset/member alignment, packet variant, generation, order, duplicate,
packet/material and final-index controls. It only proves the selected normal
type3 decoder-buffer association; no general compaction/sprite/split support.
Temporary ta_vtx.cpp changes removed; exact B patch retained ignored as
build-neural-automation/fc067-reverse-decode-b.patch.

Helper review found reused/backwards Begin generations could revive identity.
RED build passes with selftest293 passed/2 failed; corrected monotonic high-water
and explicit expected-generation remap yield298/298. These are harness tests,
not a production generation observer. Logs reverse-range-b-red/green retained.

NEXT: witness the actual copy of this offset32 packet in ta_thd_data32_i,
with per-context nonreused generation carried to the accepted producer and
decode record. Record both original source and copied destination bytes and
transfer identity; distinguish TAWrite, TAWriteSQ and the separate pvr_sb_regs
bulk caller rather than labeling every bulk call DMA. One selected context,
one packet remains the target. Wrong offset/generation must reject. Do not
infer the copy from the later TA contents or proceed to R2 until this is proven.

Historical initial helper checkpoint:

Harness-only ta_provenance.h models generation-qualified destination intervals
and explicit final-to-decoded vertex mapping. Automation build passes and
selftest293/293 includes nine new interval/remap checks: interior offsets,
stale generation, exclusive end, split rejection, capacity invalidation,
overlap, arithmetic overflow, explicit reorder and missing/restart mapping.
No production hook, actual TA provenance or camera success is claimed.

Source review: normal vert_cvt_base_ appends vertices; sprite decode allocates
four at once; final index creation/sorting is separate from source draw ranges.
The initial scan of ta_vtx/ta_util found no vertex erase/memmove operation, but
this is not proof that all renderer paths preserve indices. Next wire a bounded
copy/decode witness for the selected normal-Dreamcast target and verify its
actual final index association. Keep sprite/split variants unsupported unless
their packet extent and mapping are explicit. The interval helper is a small
reference model with linear lookup, not a profiled production hot-path design.

User approved scoping this route with Proceed after the explicit proposal to
start from one captured opaque draw. Planning baseline7a683e2fc, clean source.
This supersedes the request to choose a route, not previous evidence limits.

## Concrete target from retained evidence

Use frame1782 of fc067-opaque-84231a2af-native, gameT1401N. Scene SHA256 is
a05e136a388cc8293014f60f216ed53f30629b46441bde1f8e0a7522f54dd57e.
Producer stamp: epoch3, ordinal1781, SH4 cycle7605222912. Epoch is process-local;
require fresh actual correspondence, never infer it from an equal epoch alone.

Target list0 opaque draw ordinal1, index range first4/count166. Its first
referenced vertex is4. TCW392880, TSP543696109, upload1, RTT0, no palette.
Material slot0 binds asset0: DXGI86,256x256,131072 bytes, FNV64
99D8DB31C96EAC5F. No actor/arena/HUD/visible-pixel role is asserted.
Some following draws have count0 and the range contains restart tokens; never
treat draw ordinal as an original primitive identity or0xffffffff as a vertex.

## R1: one decoded vertex to one original TA packet

1. Add temporary opt-in diagnostic metadata at actual TA copy boundaries and
   vertex decode. Track source-to-destination byte intervals for one bounded
   context generation, including reset/recycle and capacity rejection.
2. At vert_cvt_base_, associate the actual input pointer and packet length with
   its containing copied interval. Carry a diagnostic vertex identity through
   any compaction/remapping into the selected final draw. Record actual mapping;
   matching XYZ, nearest timestamp or global last packet is insufficient.
3. At the exact accepted context's scene capture, resolve final vertex4/draw1
   back to the original packet offset, its input transfer ordinal and path.
   TAWrite and TAWriteSQ are distinct; account for32/64-byte vertex variants,
   split packets, restart/degenerate strips and merged draws explicitly.
4. Retain exact packet bytes privately and reject ambiguity, missing mapping,
   changed material/producer identity, wrong offset and wrong generation.

Hard bounds: one selected context generation, at most65536 vertex associations,
262144 packet/transfer records,32MiB metadata and two emulated seconds around
the selected producer; earlier of generation end/reset/limit ends observation.
No full-memory scan, runtime binary inspection or CPU site census. These are
caps, not permission to hide dropped records; hitting a cap rejects the run.

Success is a same-run causal packet-to-final-opaque-vertex link with failing
wrong-offset/generation controls and unchanged native images/producer inputs.
Failure is a precise unsupported mapping/path result. R1 does not recover a
camera and does not authorize arbitrary caller tracing.

## Conditional R2, only after R1 evidence review

Use the observed transfer path and actual source storage to scope one producer
witness. RAM-backed DMA requires generation-aware write provenance; SQ requires
the actual filling stores. A later submission cannot reveal past writes by
looking backwards at current RAM. Plan an earlier bounded observation with
explicit start/termination and require independent lineage on that run.
Do not automatically pursue another matrix solely because its numbers match.

## Validation and preservation

Use existing decoder/material checks and producer-stamped no-reset capture.
Test interval boundary, split input, duplicate/overlapping association, reset,
wrong offset, compaction mapping and overflow before gameplay acceptance.
Run four serial builds, three enabled selftests, SDK mock and Python suites
for production-hook changes; SDK mock remains non-GPU evidence. Remove temporary
hooks before the checkpoint, retain failed attempts and exact patches ignored,
then commit/push only owned verifier/docs/source. Never stage raw assets/media,
third-party configuration or personal paths. Strict replay remains parked.
