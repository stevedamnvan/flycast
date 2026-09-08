# FC-067 compact consumer tape v1

Developer evidence transport only. No production hook, source-transform proof,
camera recovery or presentation acceptance is implied by decoding a tape.

## Fixed format

All integers little-endian. Header:8-byte magic `FC067C01`, unsigned32 record
count, unsigned32 CRC32 of payload. Each160-byte record contains:

- Eight unsigned64 values: sample, epoch, ordinal, copy cycle, generation,
  actual copy source pointer, actual destination pointer, decoder pointer.
- Eight unsigned32 values: decoded vertex, draw ordinal, TA byte offset, SQ
  address, actual X/Y/Z RAM addresses, zero reserved word.
- Eight unsigned32 packet words before copy, then eight after copy.

The parser rejects width errors, nonzero reserved words, pointer disagreement,
changed copy bytes, bad vertex packet headers, misalignment, sample gaps,
corruption, truncation, trailing data and counts above12288. Maximum tape size
is1,966,096 bytes. CRC is corruption detection, not authentication. No filename,
user path, binary runtime or proprietary asset is part of the schema.

## Integration boundary

C++ in-memory emission is implemented in compact_consumer_tape.h and tested;
live callback migration and draw277 scene association are now verified in the
recorded scopes below. The observer must bind records
to actual copy/decoder callbacks, retain reset/overflow rejection, and emit a
separate frame/producer/Git manifest. Do not infer observed callbacks merely
from caller-supplied fields. Validate scene vertex/index membership separately.
This tape does not carry instruction descriptors, arithmetic inputs or full
RAM-writer history; those remain required before original-transform acceptance.

The next selected draw277 has2745 vertices,3321 indices,576 strip restarts and
1591 triangles in each of three retained frames. Its8235 consumer records fit
in1,317,616 bytes. Runtime observation is bounded to4096 vertices/frame for
three frames, not every scene draw. Before execution, specify the accompanying
instruction/transform ledger budgets and prove the emitter/reader roundtrip.
Never discard source evidence to obtain a smaller tape.

## Checks run

Python format tests cover independent byte offsets/64-bit cycle preservation,
8235-record roundtrip, caps, corrupt/truncated/trailing data, and semantic
copy/identity controls. The426 previously verified consumer records roundtrip
exactly in68,176 bytes. These tests prove transport only; live checks follow.

Migration capture `fc067-compact-live-a` completed3frames with a68176-byte tape
matching every same-run verbose record exactly, including actual executed RAM
addresses, copy pointers, before/after bytes and decoder association. Live
wrong-pointer B completed3frames but rejected the association and created no
tape. Both closed cleanly. This proves the new transport path in the prior
domain; it does not expand transform coverage or prove paired image preservation.
Temporary callback hooks remain ignored evidence, not a production feature.

The opt-in NEURALTEST_COMPACT_TAPE CMake target compact-consumer-contract
passes13 checks, including partial completion and fail-closed invalidation.
Its176-byte independent golden is byte-identical to Python including CRC and
64-bit fields. Reproduce with compact_consumer_contract_inspect.py and the
built executable path. No format-only expansion is needed before integration.

Large-domain D completes8235 records/1317616 bytes and binds all2745 vertices
and1591 triangles per frame to captured scene bytes/producer identity. It finds
921 XYZ addresses/frame with no differing values at reused addresses. The
original transforms for these addresses remain unproven. Acquisition is limited
to the first5069 actual copies per frame; all selected decoder associations
are independently mandatory. Later geometry is deliberately omitted.
Large A/B/C failures are retained in LOG197. E wrong-pointer rejects with no
tape. Temporary hooks are removed after retaining their patch/header. No
paired native-image preservation, new camera contract or performance claim.
