# FC-067 lossless transform ledger

The next draw277 original-transform collection reuses the existing validated
event vocabulary and Flycast's already linked zlib dependency. A zlib stream
replaces repeated text on disk; it is not a new semantic event format. No
proprietary component or download is involved.

Runtime limits:921 selected addresses/frame,2763 target instances over3frames,
32 descriptors,128 operations and32 live input words/descriptor,64 writes per
record,8MiB compressed output and128MiB total formatted input. The compressor
holds bounded output plus a16KiB scratch buffer and zlib state; it does not
accumulate expanded text. Output storage is reserved once at8MiB. Separate
consumer/copy buffers retain their previously recorded limits. Budget failure
cannot finalize a successful ledger. The offline decoder caps expanded input
before parsing and rejects truncation, trailing/concatenated streams, malformed
text and expansion beyond the cap.

Compression preserves all diagnostic events, including rejection markers. It
does not make those events truthful or satisfy arithmetic/provenance gates.
The next callback integration must bind descriptor identity, source/matrix
inputs, accumulation, actual writes, X-load continuity and each final consumer.
Unknown producer records remain unsupported; do not manufacture source state.

The accepted consumer tape independently supplies a stable921-address target
set and2745-vertex mapping. It contains gaps; no contiguous address interval is
substituted. Generated selectors remain ignored evidence. Context generations
are not treated as proven original-transform generations.

Checks run: C++ empty-stream,128MiB raw-cap,8MiB incompressible-cap and terminal
controls; independent Python recovery of45000 golden event bytes from346
compressed bytes; Python corrupt/truncated/trailing/concatenated/expansion
controls. Runtime callback routing and a live transform ledger are pending.
Do not repeat format-only work; next integrate the existing producer seams.

Live A retained906 four-seam records and15 unsupported addresses in its first
frame, then rejected a duplicate producer after the5069-copy selected prefix.
It is failed evidence, despite clean capture shutdown. B closes observation
immediately after the5069th actual successful copy, records its generation,
ordinal and cycle, and requires this boundary at frame queue. All selected
decoder bindings remain mandatory. Writes after the last retained copy cannot
change its copied bytes; later geometry is explicitly outside this evidence.
Arithmetic, selected-consumer chronology and camera acceptance remain pending.

B completed three frames with no observer rejection:3336952 compressed bytes,
37865040 expanded bytes,425653 lines. Independent same-capture compact scene
binding passes. The indexed selected-consumer checker joins8097 consumers to
2718 traced address instances and leaves138 consumers/45 address instances
unsupported. Selected-copy, identity, prefix and one-bit read controls reject.
An initial read mutation touched unselected copy162 and was outside this
checker's selected-consumer assertion; the corrected copy1294 control rejects.
Original arithmetic and calibrated reconstruction remain pending, not passed
by compression, producer counts or this binding check.

Next offline calibration uses the verified2718 primary and1287 accumulation
matrices, without a new runtime capture. The shared analyzer's existing0.001
normalized-scale and1e-9 reconstruction tolerances stay unchanged. Its explicit
opt-in matrix bound is19341 (2763 targets times at most seven contributions);
legacy21/144-matrix defaults remain unchanged. Wrong shared scale must reject.
Physical world scale and a unique world camera are not implied by factorization.
