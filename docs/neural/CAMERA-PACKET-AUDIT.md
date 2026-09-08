# FC-067 bounded opaque packet map

Starting source: `c47f26b11996f223663dacf39e3629d75769d8db`.
LOG #178, 2026-09-08. Temporary decoder probe retained as an ignored patch,
then removed. No production rendering or third-party changes.

## Observed scope

The actual parser records six selected vertices in each of three consecutive
producer frames. This is decoded-buffer/draw ownership, not CPU transform
provenance, raster visibility, recovered camera, or Remix rendering.

| Vertices | Opaque draw | TA byte offsets | Final index offsets |
|---|---|---|---|
| 4, 5, 6 | 1 | 32, 64, 96 | 4, 5, 6 |
| 146, 147, 148 | 26 | 4608, 4640, 4672 | 170, 171, 172 |

Those mappings hold separately in all three captured frames, not by an assumed
cross-frame ordinal match. Producer epoch3 has ordinals1781/1782/1783, cycles
7605222912/7608559168/7611895424 and exported frames1782/1783/1784.
Root/child TA contexts alternate00509700/00109700/00509700. All selected
packets are actual type3, 32-byte, part0 textured packed-color vertices.
Draws1/26 have index ranges4+166 and170+103 and distinct texture words.
Packet XYZ, decoded vertices, indexed membership, draw state and producer
metadata agree exactly. Each sample belongs to one active opaque draw.

`camera_packet_inspect.py --capture CAPTURE` requires all18 samples in exactly
three frames, actual producer metadata, unique root-owned packet offsets and
three vertices from each of two opaque draws. Its eight offline controls reject
bad epoch, coordinate, range, index, offset, begin identity, duplicate and
missing records. Six synthetic test methods cover these and scene/layout/
membership/limit errors. Camera and upstream-transform flags remain false.

## Captures and failures retained

- A: positive capture passes all18 packet mappings and eight controls. All27
  unique image planes, three producer identities and nine raw guidance files
  match the prior hook-free baseline exactly.
- B: same-binary expected-epoch-only negative rejects, with all actual packet
  fields and complete scene vertices/indices/draws/viewport unchanged. It is
  NOT a graphics-preservation pass: frame1783 differs by eight one-level color
  components in native/source/final images; raw depth differs by1023 bytes in
  frame1782 and5905 bytes in frame1783. These failures are retained as measured,
  not waived or attributed to a proven cause. Other sampled raw guidance and
  producer metadata match.
- C: one bounded same-binary probe-disabled control closes cleanly, emits no
  probe records, and matches all27 planes, three producer identities and nine
  raw guidance files. This does not retrospectively turn B into a pass.

All three captures return0 and clean close. Initial inspector rejection was
an incorrect type4 assumption; actual parser code and records identify type3,
and the checker was corrected before acceptance. The first strict paired
preservation script fails on B and is retained unchanged. No rerun-until-pass
loop or reopening of the parked strict raster-replay problem was performed.

## Disposition and next use

ACCEPTED: A's bounded decoded-packet map, epoch rejection and scoped A/C native
preservation. CORRECTIONS_REQUIRED: claiming complete A/B GPU preservation.
The packet map may locate the next actual source observations; it cannot
activate a production camera or close the broader M2-camera card.

Next observe the actual indexed gather and SQ-to-TA copy for these same six
packet offsets over the same three producer identities. Record source record
addresses and executed memory/register/SQ ownership with bounded descriptors,
per-sample event limits and overwrite/reset rejection. Do not select a source
by similar float values or cycle alone. Then bind the corresponding FTRV and
projection instances using the existing proven code seams. Execution bounds
and progress live in BACKLOG.md; this audit is evidence, not a second queue.
