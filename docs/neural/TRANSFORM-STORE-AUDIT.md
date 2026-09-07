# FC-067 executed transform-to-RAM witness

Baseline: `23e3d416d262d5d3f54a405b3cfe25eb7cc344e3`.
**ACCEPTED one executed transform-to-guest-RAM witness (self-review).**
TA submission lineage, camera semantics, moving reprojection and world-space
reconstruction remain NOT_REVIEWABLE. This is not a camera recovery gate pass.

## Actual observation

Temporary x64 canonCall instrumentation passes `block->vaddr + op.guest_offs`
as an extra argument to a wrapper around the existing canonical FTRV function.
The original function computes the result; no alternate matrix arithmetic or
guest-state mutation is substituted. Input/matrix bits are copied before the
in-place operation, outputs afterward. The decoder assigns guest_offs from the
current instruction PC. These observed sites are not delay slots; no general
delay-slot or other-CPU provenance claim is made.

The corrected targeted run observes FTRV at guest PC `8c070c3c`, SH4 cycle
`6000468096`, inside the explicit 30-40-emulated-second window. It produces
output words `457dcb43,c50958f1,4163a016,3f800000`. Its translated block stores
the same SSA outputs in reverse component order at PCs `8c070c3e` through
`8c070c44`. A post-store observer captures the actual effective addresses and
source values, then reads those four aligned guest-RAM words directly:

| Component | Store PC | Guest address | Written/readback bits |
|---|---|---|---|
| 3 | 8c070c3e | 8c00f308 | 3f800000 |
| 2 | 8c070c40 | 8c00f304 | 4163a016 |
| 1 | 8c070c42 | 8c00f300 | c50958f1 |
| 0 | 8c070c44 | 8c00f2fc | 457dcb43 |

All four match the recorded transform output, address progression and execution
order. This proves a specific transform feeds a specific intermediate RAM
buffer. It does NOT prove the buffer is consumed by PVR or identify the matrix
as camera/model/projection. Do not assign a physical depth scale or inferred FOV.
The sample precedes the retained image frames; it is not asserted to belong to
frame 1804 or to be representative of all moving geometry.

## Falsification and native preservation

Run F records four exact stores. Run G flips one bit only in the observer's
expected component zero, never the guest data or renderer. G correctly reports
three exact stores and one mismatch, while retaining identical executed matrix,
input/output bits, SH4 cycle and actual RAM destination/value.

Both runs close cleanly, three native D3D11 frames 1804-1806 each. The nine
named image planes match the restored `23e3d416d` baseline 27/27 for each run
(54 comparisons). These are synchronous diagnostic captures, not performance
evidence. Source configuration and external consumer settings are not changed.

`neuraltest/transform_store_inspect.py --positive F --negative G --baseline B`
verifies only the final launch in each append-only execution.log, the bounded
sample, exact source contract, ordered RAM stores, falsifying result, and native
images. Passing F as both positive and negative rejects with exit 1. No parsed
log success can upgrade the output's explicit unknown TA/camera provenance.

## Failed and limited attempts retained

- Initial A/B builds fail because canonCall does not own the compile method's
  local block pointer. A compiler-local context pointer corrects the diagnostic.
- C observes 32 sites and hits its cap: incomplete, rejected as whole-window
  evidence. Its limited site observations guide narrowing only. Compile-time
  block excerpts are not dynamic instruction traces.
- D targets `8c048dcc/8c048dde`; their output stores and surrounding arithmetic
  do not establish a PVR connection. They are not promoted as camera evidence.
- E narrows to `8c070c3c/8c0610b4`; the former's immediate contiguous stores
  justify the actual post-store F check. The latter remains uncorrelated.
- The first F/G verifier sees appended earlier-launch store records and fails.
  Selecting the final explicit DX11 initialization boundary yields exactly four
  records per run and the expected positive/negative dispositions. Old log
  records are retained, not erased or silently mixed into accepted evidence.

Raw evidence is outside Git under sibling `flycast-evidence/fc067-ftrv-sites-*`.
Exact F and G patches are retained in ignored build-neural-automation files.
All temporary changes to rec_x64.cpp are removed. No guest code, captured
matrices/assets, proprietary binaries or external configuration is committed.

Restored-source automation, NGX, no-NGX and feature-off configure/build serially
with exit 0. Enabled selftests each pass 243/243; SDK mock passes 56/56, with
runtime_loaded/gpu_rendered/presented false. The parser's six unittest methods
pass, including ten malformed-record subcases, launch-boundary isolation,
overflow and missing-negative controls. These test evidence parsing, not SH4
arithmetic or camera recovery. The final inspector passes the real F/G pair.

## Next bounded task

Follow reads/overwrites of this dynamically witnessed 16-byte output span after
the observed stores, with an execution-sequence identity and a fixed event cap.
Use actual executed effective addresses, not an arbitrary memory scan or
nearest-value matching. Determine whether its values reach a TA packet through
further arithmetic, SQ or DMA; stop on overwrite/reset/cap and report unresolved
when necessary. Do not repeat the FTRV census or call intermediate-RAM lineage
a camera. A real TA association still needs moving/wrong-frame/matrix controls.
