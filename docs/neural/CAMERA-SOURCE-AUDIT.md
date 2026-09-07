# FC-067 camera source-witness audit

Baseline: `b7e621b48144a95d69b3b346c89d9386b1bf1ba8`, clean checkout.
Disposition: **ACCEPTED source-seam audit and execution census (self-review)**. Camera recovery
is **NOT_REVIEWABLE**: no executed Soulcalibur transform-to-vertex witness has
been captured. This is not proof that recovery is impossible.

## What the source actually exposes

- `core/hw/sh4/interpr/sh4_fpu.cpp`, FTRV opcode handler: live input FR vector,
  XF matrix, and four computed output values. These are executed arithmetic,
  not matrices found by searching memory. Their semantic role is unknown.
- `core/hw/sh4/interpr/sh4_interpreter.cpp`, ReadNexOp/ExecuteOpcode and
  ExecuteDelayslot: interpreter fetch advances PC before dispatch. An observer
  must record instruction identity at dispatch and test delay-slot behavior,
  rather than applying that PC assumption to the dynarec.
- `core/hw/sh4/dyna/shil_canonical.h`, ftrv: canonical helper takes destination,
  input and matrix pointers, but no guest PC or submission identity. On x64,
  rec_x64.cpp falls through genBaseOpcode to shil_chf; xbyak_base.h has no FTRV
  specialization. canonCall materializes pointer arguments and calls the helper.
  SHIL has guest_offs, but the existing FTRV helper does not receive it.
- `core/hw/sh4/storeq.cpp`: both translated sqWrite and optimized sqWriteTA
  submit to TAWriteSQ. A hook on only one path misses supported submissions.
  The current XF matrix at queue flush is not necessarily the matrix used to
  create the queued vertex.
- `core/hw/sh4/modules/dmac.cpp`, DMAC_Ch2St: RAM-backed polygon packets can
  arrive through TAWrite, including RAM-wrap handling. Their computation may
  precede DMA by many instructions; the submission-time matrix is insufficient.
- `core/hw/pvr/pvr_mem.cpp`, TAWrite/TAWriteSQ, and ta.cpp, ta_thd_data32_i:
  packet bytes are copied into the active TA buffer. CPU operation provenance
  is not attached. The TA source explicitly separates producer and render/TA
  threads; a global "last matrix" read on the render thread is invalid lineage.
- `core/hw/pvr/ta_vtx.cpp`, vert_cvt_base_: ordinary Dreamcast input is already
  projected x/y plus invW. It does not carry the missing transform. Naomi 2
  projection/normal fields are not evidence for this Soulcalibur route.

`config::DynarecEnabled` exists with key `Dynarec.Enabled` and default true.
Interpreter instrumentation would therefore be a separately labeled diagnostic
lane, not evidence that the production x64 lane was instrumented or unchanged.

## Concrete next experiment, not a matrix extractor

Implement one off-by-default, bounded **executed-transform witness** experiment
for the legal deterministic Hoko interval. First establish whether FTRV is used
there at all; do not assume it is the title's projection operation. Retain exact
instruction identity, input/matrix/output bits and execution sequence. Any
bounded overflow must reject the witness rather than silently truncate it.

Tie a candidate to actual packet words by tracing its executed arithmetic and
stores into SQ or DMA source storage, with a TA-context epoch and packet offset.
Do not accept nearest-in-time or numerical similarity as causal association.
If that lineage cannot be established in this bounded experiment, return an
explicit uncorrelated/unsupported result and stop; do not build a general game
decompiler or expand into arbitrary memory scans.

Before game evidence, test known matrix/vector operations, in-place register
aliasing, delay-slot instruction identity, SQ and DMA routing, overwritten
values, context reset, and cap overflow. Wrong operation/frame/matrix controls
must reject association. Preserve native on/off output in the diagnostic lane.
If interpreter execution is used, separately prove production-lane correspondence
before claiming production camera provenance. No observation changes emulated
registers, PVR depth, neural history, user configuration or rendering defaults.

Even a causally matched FTRV may be a model, normal, lighting or combined
transform. Only subsequent moving reprojection and semantic separation can
justify camera/world-space claims. No runtime integration is part of this task.

## Executed census, not transform or camera recovery

A temporary environment-opt-in counter was inserted at the canonical x64 FTRV
helper, without reading or changing its vector/matrix arguments. It records
cumulative calls and SH4 scheduler cycles, bounded at 120 emulated seconds or
100 million calls. There is no packet association, guest-PC attribution or
matrix extraction. It is diagnostic instrumentation, not production code.

Working B automation build succeeds. The legal deterministic native D3D11 Hoko
capture closes cleanly with frames 1804-1806. Its log records the first FTRV at
cycle 537104512 (emulated second 2), and 45,001,051 calls by cycle 7600021632
(second 38). No cap marker occurs in the retained log. This establishes actual
execution in this game run, not that any counted operation produced a captured
vertex, and not a final whole-run count. FTRV remains **uncorrelated**.

The nine explicitly named image planes match the separate opt-out run 27/27
(three frames each). Synchronous capture/census is excluded from performance
claims. Source emu.cfg remains SHA256
`1EF718689784DCE64CAD1CE8BEC776E710B4A3705ECFF2E5A276A1A3B2F92992`.
The probe is removed from both source files; its exact B patch is retained only
in ignored `build-neural-automation/fc067-ftrv-census.patch`. Raw evidence stays
in sibling `flycast-evidence/fc067-ftrv-census-working-*` directories, with B's
execution.log retained before another launch can overwrite the stage log.

Failures: attempt A completes but INFO_LOG is compiled out at Release's warning
ceiling, so it supplies no positive or negative execution evidence. B uses
NOTICE_LOG and a fresh capture. A first comparison glob sees 29 identical PNGs
but asserts an expected 27 and fails; corrected explicit nine-plane selection
passes 27/27. Neither failure is silently labeled a successful test.

Next: the instruction-site/value-lineage part of the bounded witness above.
Do not repeat the usage census. Presence is now proven; camera semantics are not.

After removing the temporary probe, automation, NGX, no-NGX and feature-off
configure/build serially with exit 0; enabled selftests each pass 243/243.
SDK mock passes 56/56 with runtime_loaded/gpu_rendered/presented all false.
The disposable stage executable is replaced by the restored-source build.

## Source checks performed for this audit

Read-only source inspection and symbol searches preceded the census above;
no matrix capture or camera test. Windows wildcard directory searches failed and were
rerun against the concrete core directory. Prior material evidence is retained
in MATERIAL-CAPTURE-AUDIT.md; it is not relabeled as camera evidence. The strict
M2 source residual remains parked and unpassed.
