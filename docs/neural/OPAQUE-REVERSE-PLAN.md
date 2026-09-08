# FC-067 opaque-draw reverse provenance scope

Current R2 result/next task: OPAQUE-SQ-WRITERS-AUDIT.md, LOG #169. All32 bytes
have actual filling-store witnesses; XYZ source calculation remains unknown.
The next bounded operand-source task supersedes the completed R2 writer task
below, not the full objective or original scope restrictions.

## R1 working checkpoint

Restored R1-copy checkpoint: four serial configure/builds pass, three enabled
selftests298/298 each, SDK56 with runtime/GPU/Present false, Python76 inspector
plus5 binary32 methods pass. Core diff is empty. Exact-commit validation follows.

2026-09-08 R1 bounded acceptance (LOG #168): the actual32-byte source copy is
TAWriteSQ, address0xe0000020/slot1, cycle7602643776. Its before/after eight words
agree with the decoder's packet at offset32. The destination pointer is carried
on the same context to decode and final opaque vertex4, not matched by position.
The fresh accepted producer is epoch3/ordinal1781/cycle7605222912.

One observation generation1 is selected for the entire process in the preceding
one-frame cycle window; it cannot rearm. Context reset/recycle/deserialization
clears its metadata, target overwrite invalidates it. This is a bounded packet
lease, not a new general-purpose renderer context-generation implementation.
Only one packet record/eight word records and one vertex association are retained.

Capture fc067-reverse-copy-a passes reverse_copy_inspect.py and five actual-input
offline controls (generation, offset, destination, byte, path). Six synthetic
methods pass. In fc067-reverse-copy-b-negative the live decoder requests generation2
while actual metadata remains1: link exact=0 and verifier rejects, with identical
original/copied words. Both captures exit0/clean close and each preserves27 unique
native planes plus three producer stamps against the hook-free fd79db700 capture.
The first focused Python invocation also discovered five imported decoder tests;
import changed to a module to avoid counting those duplicates as new coverage.

R1 self-review ACCEPTED ONLY for the selected normal type3 opaque vertex. No
sprite/split/64-byte/general compaction claim. Material generation, final index,
packet/vertex values and actual copy membership are checked; original CPU filling
stores and camera remain unknown. Temporary core hooks removed; exact patches
fc067-reverse-copy-a.patch and fc067-reverse-copy-b.patch retained ignored.

## R2 next bounded implementation

R1 evidence review above permits the previously conditional R2 scope. Trace only
the stores filling physical SQ slot1 that are consumed by this same target flush.
The x64 StoreQueue handler masks the address with0x3f after checking the E0-E3
region; include these physical aliases, not only guest address0xe0000020.
Both32- and64-bit stores must be covered. Carry actual executed SH4 store PCs
and per-byte last-writer coverage, not a stale generic SH4 context PC or static
instruction membership. Preserve normal register allocation and memory effects.

Start a bounded interval before the known flush, retain actual preceding slot1
flush/reset boundaries, and stop at cycle7602643776 only when its32-byte payload,
TA destination, decode and producer identity match R1. Reject missing earlier
writers, unsupported write paths, incomplete coverage, reset, ordering ambiguity
or cap exhaustion. Bound to at most256 SQ writes/flush records and two emulated
seconds; do not expand into a CPU site census or caller trace. Prove last writers
for all32 bytes and falsify a wrong-word/generation control. Only then scope the
observed operand producer; R2 does not itself establish a world camera.

Historical decoder-only checkpoint:

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
