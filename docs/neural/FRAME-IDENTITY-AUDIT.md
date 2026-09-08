# FC-067 capture identity qualification

## Working-tree validation follow-up

The additive producer stamp and capture save/load harness are implemented, not
yet committed. Fresh `fc067-identity-final` builds pass serially for automation,
baseline NGX, no-NGX and feature-off. All three enabled selftests pass284/284;
public-header SDK mock passes56/56 with runtime/GPU/presentation false.
Python inspector tests pass61 and binary32 tests pass5. Three invalid capture
requests reject before launch with exit2 (save-after10001, load-delay0, and
save/load with the DLAA lane).

Actual `fc067-producer-savestate-c` exercises the strict marker helper: exit0,
three frames, clean close. In-memory save/load completes at main frames1000
and1030 with27884590 state bytes. Retained frames1782-1784 carry epoch4,
ordinals756-758 and cycles7505131648/7508467904/7511804160. All27 unique image
planes match save/load run B exactly (54 duplicated helper comparisons).
This proves this bounded reset capture, not arbitrary cross-run determinism or
the cause of the original shifted run. The invalid locale-formatted marker in
run A remains failed evidence; classic-locale formatting fixes its JSON output.

Next: review/stage this identity slice and record exact-SHA confirmation before
promotion. Main-scene camera provenance, replay alignment and Remix GPU remain
open; this metadata does not manufacture missing identity for older captures.

Starting source29e4a66de. CORRECTIONS_REQUIRED cross-run alignment, not proved
rendering regression. The first exactcommit capture differs by one game frame;
unchanged-executable repeat I and diagnostic B-F match restored H. Neither
successful repeats nor shifted matching authorize accepting the failed run.
See COMBAT-PACKET-AUDIT.md postcommit qualification and LOG160-161.

## Executed bounded evidence

Temporary DX11 source trace observes direct frames0/136 and geometry frames.
Writer trace shows skip1780 counts geometry capture attempts, retaining neural
frames1782-1784. Producer trace independently records accepted context cycles,
then carries them on that context to the capture attempt:

| Neural frame | Producer submission | Emulated cycle |
|---|---|---|
|1782|1781|7605222912|
|1783|1782|7608559168|
|1784|1783|7611895424|

F capture exits0/clean close and matches27 unique image planes against H.
frame_identity_inspect.py validates submission/dequeue/retention identity and
rejects four offline changes. Five synthetic methods cover duplicate, missing,
wrong-context and reordered observations; total inspector suite60 passes.
The old failed run has no producer witness. Its cycle cannot be inferred from
a later matching image. No original-root-cause or determinism claim follows.

First shared-instrumentation trace fails linking neuraltest due to GenericLog;
moving logging to DX11 fixes the build. A failure and B-F raw runs remain in
ignored evidence; exact F patch retained. All temporary core edits removed.
This trace is not timing/performance evidence or production telemetry.

## Source review and next bounded implementation

QueueRender is the producer seam, before rqueue publication; FrameCount instead
increments at dequeue and is not an emulated identity. rend_context::Clear is
also used for render-data clearing, so do not invalidate a newly submitted
witness there without reviewing call order. TA_context::Reset recycles data;
deserializeContext reads address and TA bytes, not the C++ rend_context layout.
Identity must be reset at lifecycle boundaries and freshly stamped per accepted
submission, never restored as old game-state provenance. DX11 developer replay
temporarily swaps rendContext, so capture identity must come from the original
live context, not a reconstructed replay packet.

Implement optional capture-only producer metadata, not a frame-ID replacement:

- Distinguish unavailable from cycle zero; name clock domain explicitly.
- Include a reset/session epoch, submission ordinal and emulated producer cycle.
- Assign before queue publication; preserve through decode to capture metadata.
- Invalidate on recycle, reset/load, unsupported/direct source and synthetic
  replay. A new submission after load receives a new epoch/identity.
- Keep existing frame_id/history counters and save-state byte format unchanged.
- Add fields additively to capture manifests; old artifacts are unverified for
  this identity, not implicitly equal or silently relabeled.
- Equality checks require matching replay, source epoch/sequence semantics and
  producer timing in addition to images. Include wrong-cycle, stale-epoch,
  absent-identity and reordered-submission controls and native preservation.

First implement/test lifecycle contract and manifest validation, then an exact
producer-bound moving capture. Do not repeat identical captures or substitute
index offsets for proof. No change to game speed, rendering or neural acceptance.
