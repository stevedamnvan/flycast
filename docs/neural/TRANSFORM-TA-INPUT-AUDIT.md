# FC-067 one transform-to-TA-input chain

Baseline: `6ad78a7bb9c6f0545ca1196b042cf398c7f7c085`.
**ACCEPTED one bounded source-transform to bulk TA input observation (self-review).**
Not a recovered camera, physical-depth calibration, decoded-vertex/render-frame
association, complete world scene, or Remix GPU evidence. Strict native replay
equality remains failed/parked. All instrumentation was temporary and removed.

## Executed chain

The preceding TRANSFORM-DERIVED-AUDIT.md source, arithmetic and stores are
verified again by the combined inspector. The completed 12-byte derived span
is read at 8c070cb4 (z), then 8c070ea0/ea4/ea8 (x/y/z). Actual loaded bits match
the witnessed RAM bytes. Compiled SHIL binds r2.1/r3.1/r2.2 directly to following
stores 8c070ea2/ea6/eaa; actual destinations and post-store queue readbacks are:

| Component | Store queue address | Word |
|---|---|---|
| x | e0de73e4 | 44175b90 |
| y | e0de73e8 | 42ab1024 |
| z | e0de73ec | 3d95b6cd |

The following pref at 8c070eb0 invokes the existing doSqWrite handler, which
is not replaced with diagnostic behavior. Observed QACR area is 3. This is a
**RAM flush, not direct TA output**. The exact 32-byte queue becomes the RAM
packet at 0cde73e0; every byte agrees after the actual handler returns. The first
word is e0000000 and the coordinate words occupy byte offsets 4/8/12.

That exact RAM span later lies inside the actual data pointer/count passed to
TAWrite: address 00000000, count 928, byte offset 704 (packet index 22). The
actual bytes still match all 32 saved flush bytes. The address selects the
polygon branch, which calls ta_vtx_data; this probe observes **TAWrite entry**,
not subsequent per-vertex decoding, context acceptance, or presentation.
The inclusion check uses the actual pointer range, not a search for similar
coordinates. The source code's normal RAM channel-2 path calls this entry,
but no separate DMA register/caller instrumentation is claimed.

The flushed generation reaches TA input at cycle 6002105984, 1,637,888 cycles
after the source witness, following 584,739 generated CPU memory events.
The old derived-coordinate span independently stops on overwrite at event 381,
instruction 8c070c40; that does not invalidate its already copied RAM packet.

## Controls and scope

Positive G3-C and negative G3-D each capture three native frames 1804-1806 and
close cleanly with exit 0. D changes only the diagnostic expected packet offset
from 704 to 736. The association check correctly fails while the actual source,
arithmetic, queue, RAM packet, bulk-input bytes/count/offset/cycle remain equal.
Both runs match nine named native image planes against the restored 6ad78a7bb
baseline, 27 each / 54 comparisons. These images preserve rendering; they do
not associate the earlier source computation with retained frame 1804.

The G2 source span stops at overwrite, 50,000 generated CPU memory events, or
one emulated second. G3 stops on observed CPU/SQ overwrite of its packet,
5,000,000 generated CPU memory events, a backwards cycle, or one emulated
second. Accepted input lies within these bounds. Source/queue/read/write checks
are x64, MMU-disabled and title/sample-specific. Interpreter-internal accesses,
all possible DMA/non-CPU writers, reset/save-state coverage, and stale-generation
ABA cases are not proven. Full-byte equality at flush/input is observed; do not
upgrade these diagnostics into a production lifetime/taint-tracking system.
Only the first coordinate group's copy has compiled dependency verification;
the adjacent group's stores are context, not another accepted source witness.

Five new synthetic parser test methods cover the positive/negative pair,
ten malformed fields, missing-negative, invalidated generation and reordered
copy rejection. They mock the prior derived inspector and are not SH4 tests.
The real verifier executes the full earlier source/span/arithmetic inspectors.

## Retained attempts

- G2-A: four exact reads before overwrite; no following-store observation.
- G2-B: actual queue stores plus compiled load/store dependency.
- G2-C: confirms QACR area 3 and full queue-to-RAM flush.
- G3-A: stops at 500,001 events before TA input; inconclusive, not a no-use result.
- G3-B: expanded 5,000,000-event bound reaches exact TA input at event 584,739.
- G3-C/D: explicit positive/wrong-offset association controls, unchanged images.
- Initial synthetic positive test failed because its fixture omitted the fifth
  (terminating) access. Added that access and explicitly validated its fields;
  all five tests then pass. No build/game-capture failures in this tranche.
- Two Windows rg wildcard path errors were corrected to directory-scoped search.

Raw captures remain in sibling flycast-evidence/fc067-g2-working-* and
fc067-g3-working-*. Exact C/D probes remain ignored as
build-neural-automation/fc067-ta-positive.patch and fc067-ta-negative.patch.
No proprietary binary, runtime configuration, raw capture or asset is staged.

After probe removal, all four restored-source configure/builds return 0.
Three enabled selftests pass 243/243 each; SDK mock passes 56/56 with runtime,
GPU and presentation false. The combined store/span/derived/TA parser suite
passes 20 methods, plus five binary32 tests. Exact-commit build/capture results
are retained separately in the ignored postcommit note.

## Next concrete task

Carry this exact TA input packet into the actual TA context/decoded vertex with
an explicit packet index and frame/context lifetime. Require an intentionally
wrong packet/context association to fail. Then assess what the linked source
matrix/vector proves about projection versus model/view and available camera
semantics; do not relabel a composite transform as a recovered world camera.
If the necessary decomposition is unavailable, produce that precise feasibility
disposition. Do not repeat source/SQ/TA-entry censuses, widen to arbitrary memory
scans, tune the rejected lighting approximation, or start runtime integration.
