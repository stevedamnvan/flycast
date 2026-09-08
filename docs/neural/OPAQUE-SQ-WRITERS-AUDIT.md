# FC-067 selected opaque SQ filling-store audit

## Disposition and scope

Restored working-tree validation passes all four serial configure/builds,
three enabled selftests298/298 each, SDK mock56 with runtime/GPU/Present false,
and Python84 inspector plus5 binary32 methods. Core diff is empty; exact-commit
verification follows checkpoint. No runtime binary or user configuration touched.

R2 filling-store witness ACCEPTED in bounded self-review, not position-calculation
or camera recovery. Starting checkpoint c6a8ee668; normal Dreamcast/native D3D11,
same retained Soulcalibur target from OPAQUE-REVERSE-PLAN.md. No production
rendering, guest memory, external consumer configuration or runtime changes remain.
Temporary hooks and header removed; each exact ignored patch must be paired with
its matching fc067-sq-writers-{a,b,c,d}-header.h in build-neural-automation.

## Actual evidence

Capture fc067-sq-writers-c starts at its first observed physical SQ slot1 store.
All eight stores and the target flush share scheduler cycle7602643776. Temporal
order is the actual callback event sequence, not inferred from equal timestamps.
Eight post-store reads match their executed SHIL store operands. Per-byte last
writer coverage is32/32; the final flush is event9 and its bytes agree with R1's
original source/copy/destination, decoder packet32/member36 and opaque vertex4.
The accepted producer remains epoch3/ordinal1781/cycle7605222912.

| Packet offset | Executed store PC | Field in selected type3 packet | Event |
|---|---|---|---|
| 0 | 8c03cc40 | PCW | 1 |
| 4 | 8c03cc82 | projected X | 2 |
| 8 | 8c03cc84 | projected Y | 3 |
| 12 | 8c03cc86 | raw PVR Z | 4 |
| 16 | 8c03cc8c | U | 5 |
| 20 | 8c03cc96 | V | 8 |
| 24 | 8c03cc8e | packed base color | 6 |
| 28 | 8c03cc90 | packed offset color | 7 |

All observed target stores are32-bit. The temporary emitter handles both32/64-bit
SHIL writes after their actual memory operation, including immediate addresses;
its callbacks cover E0-E3 aliases of physical SQ slot1. MMU mode is unsupported.
Eight-byte alias/overwrite behavior has synthetic verifier coverage, not a claim
of actual64-bit target-game execution. No static descriptor alone is called an
executed store; the callback uses that executed op's guest offset for its PC.

Physical SQ flushes copy but do not clear the queue. The corrected observer and
verifier retain observed byte writers across flushes. Reset invalidates the
observation; the first window cannot rearm. Limits remain256 store/flush events
and two emulated seconds, with incomplete/cap/clock-reset rejection.

## Failed and falsifying attempts

- A: no earlier slot1 flush in the narrow start window; no writer observation.
  R1 copy still captured. Rejected for missing SQ boundaries.
- B: starting at list initialization captures earlier sprite-to-RAM physical
  aliases and hits256 events before the target. Rejected, cap not increased.
  This also prompted covering memory-directed flush routes, not only TA writes.
- C: start at an actual store within the narrow pre-target cycle window; all32
  bytes have observed writers. Do not seed unknown bytes from the old SQ contents.
- D-negative: deliberately flip the expected X-store word by one bit without
  altering the operand or memory. Actual store at8c03cc82 remains unchanged;
  exact=0 and store-mismatch terminate the trace. Final original/copied packet
  words equal C and the verifier rejects D.

All A-D captures exit0/clean close. Each matches27 unique native image planes
and three complete producer identities of fc067-copy-c6a8ee668-native:108 unique
plane comparisons and12 producer comparisons total. No performance claim.
sq_writer_inspect.py accepts C and rejects A/B/D; five actual-input offline
controls mutate generation, PC, coverage, last-writer attribution and expected
word. Eight synthetic methods additionally cover aliases, wide/overlapping
stores, retained writers across flushes, missing/reordered events and bounds.

## Next bounded operand-source task

R2 permits scoping the observed operand producer after filling-store proof.
Observe only the actually executed block(s) containing XYZ stores8c03cc82/84/86
in this same target invocation. Record their source operands and the required
entry values, returned loads and arithmetic that produce those three words.
Bind dynamic block identity to execution; static descriptors are only supporting
metadata. If values are already computed on entry, report their exact register
or observed memory source and stop before recursively tracing callers.

Bound to8 executed blocks,128 SHIL descriptors and512 witness events within two
emulated seconds. Reject missing values, unsupported paths, reset, cap or wrong
word. Preserve the same packet/producer/native comparison and a falsifying
operand control. Do not repeat the completed SQ copy/writer census, general
matrix census, arbitrary caller tracing, parked replay precision or GPU transport
proof. Whole-scene camera, world coordinates and real Remix GPU remain open.
