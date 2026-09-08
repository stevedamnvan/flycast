# FC-067 bounded opaque source collection

## Scoped result

ACCEPTED: eighteen indexed-gather -> physical SQ -> actual TA-copy -> decoded
packet XYZ links, spanning six selected vertices in three producer frames.
This is not original RAM-producer, matrix semantics, camera, raster visibility,
Remix GPU or combined DLSS 5 presentation proof. Production remains unchanged.

Starting checkpoint: c6450eafd2c443a3b4a148a4dc4b26073ff913d7. Temporary hooks
were retained as an ignored patch/header and removed before checkpoint builds.
No third-party binary/configuration or game media changed.

## Observed source mapping

Each row repeats across producer epoch3, ordinals1781/1782/1783, with actual
context generations1790/1791/1792. Values change between frames; addresses alone
are not a temporal identity or original-transform proof. Y/Z are X+4/X+8.

| Opaque draw | Decoded vertex | TA byte offset | X RAM address |
|---|---:|---:|---|
| 1 | 4 | 32 | 8ce74250 |
| 1 | 5 | 64 | 8ce74230 |
| 1 | 6 | 96 | 8ce74240 |
| 26 | 146 | 4608 | 8ce6e460 |
| 26 | 147 | 4640 | 8ce6e470 |
| 26 | 148 | 4672 | 8ce6e480 |

The actual gather is block8c03cc68:27 operations,6 live scalar inputs,8 reads,
20 live results and7 SQ stores per invocation. The verifier independently
recomputes indexed integer address arithmetic and SSA dependencies. XYZ reads
at cc7c/cc7e/cc80 feed stores at cc82/cc84/cc86. All12 position bytes have the
expected last writer. Both SQ slots are checked, including actual source-buffer
base, copy destination, context generation, decoder pointer and all32 copied
bytes. Header/color/material ancestry is deliberately not claimed.

Selection is bounded to18 invocations,128 operations/32 live words/512 events
per invocation. Context reset, duplicate/overwritten slots, incomplete frame
coverage, unexpected interpreter/bulk-write paths and identity disagreement
reject evidence. Collection completion is recorded at the actual final queued
producer frame; subsequent clean shutdown is not a reset of an active lease.

## Retained experiments

Private evidence directory names (outside Git):

- camera-sources-a: first build's three-frame capture, exit0/clean close;
  rejected because normal shutdown was mislabeled emulator-reset.
- camera-sources-b: first correction, exit0/clean close; still rejected because
  completion depended on a later next-frame hint. Retained, not normalized into
  accepted evidence.
- camera-sources-c: corrected source capture,18 independently checked links,
  54 returned XYZ loads, exit0/clean close.
- camera-sources-d-negative: same binary, only expected copy ID increased by1;
  all18 reject. An explicitly offline expected-only repair reproduces C's
  actual observations; the real negative remains rejected.
- camera-sources-e-disabled: same binary without probes, no probe tags,
  exit0/clean close.

All names have the `fc067-` prefix. C/D/E preserve complete scene packets and
producer identity,27 PNG planes and9 raw guidance files exactly against each
other and the retained0e095eb75 native reference. Against the latest c6450eafd
hook-free capture, each has the same retained discrepancy: depth.f32 differs
by1023/5905 bytes in frames1782/1783, and frame1783 native/source/final color
each differs by8 one-level components. Do not claim universal GPU repeatability
or a cause; the disabled control has the same discrepancy. The parked strict
raster replay remains failed/parked.

## Reproducible checks and next dependency

`python neuraltest/camera_source_inspect.py --capture CAPTURE` verifies the
source chain, reusing the packet and scalar-operand checkers. Six synthetic
methods test complete coverage and wrong identity/generation/pointer/epoch,
bytes/writers/physical slot, indexed address/input/instruction, missing or
duplicate records, resets, early completion, scene mutation and clock order.
The complete inspector suite passes150 methods plus5 binary32 oracle methods.
Build/selftest exact-checkpoint outcomes are recorded separately in LOG.

Next: reuse the existing FTRV/initial-store/reciprocal/projection probes for
these six RAM records across the same three actual producer frames. Require
actual last-writer and generation association through the now-proven gather
chain; never join matrices by matching values/cycles. Preserve nonunit W and
test per-draw/shared-projection hypotheses without forcing world semantics.
The backlog owns the next bounded execution card.
