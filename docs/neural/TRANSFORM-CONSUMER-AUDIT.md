# FC-067 bounded CPU buffer-consumer observation

Baseline: `fcc8dc84f706fe7d04638754f16e17139972dc38`.
**ACCEPTED one executed CPU read/copy/overwrite chain (self-review).**
Not TA submission, camera recovery, physical depth calibration, or Remix GPU
evidence. No source-world reconstruction gate is closed by this observation.

## Observed continuation of the prior witness

The removed temporary x64 probe activates after the four exact stores in
TRANSFORM-STORE-AUDIT.md. It watches only the dynamically witnessed 16-byte
guest-RAM span. It counts at most 5,000 generated SHIL CPU memory-access events,
records overlapping accesses, and stops before the first overlapping write.
The source sample remains at SH4 cycle 6000468096. No arbitrary memory search,
global-last-matrix assumption or numeric-nearest matching is used.

Run C records:

| Event | Guest instruction | Observation |
|---|---|---|
| 1 | 8c070c4a | Reads 457dcb43 from 8c00f2fc; loaded register bits agree |
| 3 | 8c070c52 | Reads c50958f1 from 8c00f300; loaded register bits agree |
| 5 | 8c070c5a | Reads 4163a016 from 8c00f304; loaded register bits agree |
| 7 | 8c070c64 | Reads 4163a016 again; loaded register bits agree |
| 10 | 8c070c92 | Writes the original third component; terminates this span generation |

The first three reads feed stores at 8c070c4e/56/5e. Compiled SHIL explicitly
binds each loaded SSA value f3.1/f3.2/f3.3 to the following store. Actual effective
addresses are 8c8b6d00/04/08. Post-store RAM readbacks equal the corresponding
original transform outputs. This is a three-word copy, not just similar values
found elsewhere in memory. The recorded register-load results and committed
destination bytes corroborate that specific compiled dependency.

The terminating store replaces 4163a016 with 3d95b6cd at 8c00f304; its post-store
readback agrees. That value is **not labeled reciprocal/linear/physical depth**.
The block containing this write starts at 8c070c88 and includes multiplication,
then x/y multiply-add operations and writes at 8c070c9c/a4. Their executed
operands/results have not been captured. The old span is not silently considered
unchanged after the write, and the copied buffer is not asserted to reach TA.

## Controls, bounds and limitations

Run D changes one bit only in the first read checker's expected value. That
read correctly fails while the other three read checks pass. Actual source
input/matrix/output/cycle, four initial stores, three copied values/destination,
and the terminating write remain identical between C/D. Both runs close cleanly
with three native D3D11 frames 1804-1806; nine image planes each match the restored
fcc8dc84f baseline 27/27 (54 comparisons). Synchronous probes are not performance
evidence. The sampled computation precedes these images; no frame-1804 ownership
is inferred from their preservation.

The verifier requires final-launch log isolation, initial transform/store
identity, cross-event order, actual loaded values, compiled copy dependency,
copied RAM readback, correct stop boundary, negative result and native images.
Five new parser test methods pass, including ten malformed-field subcases,
reordered-copy rejection and missing-negative rejection. The earlier six store
parser methods remain separate regression coverage.

Scope is x64, MMU-disabled, generated SHIL CPU accesses in this disposable game
run. DMA/SQ bulk copying, interpreter/fallback-internal memory operations,
save-state/reset transitions and all-title coverage are NOT proven. Only the
observed CPU chain is accepted; no lack-of-DMA/TA-use claim follows. The 5,000-event
cap was not reached here, and its runtime failure injection is not claimed.

Attempt A records pre-access addresses/bytes only; B adds actual loaded-register
checks. C adds copy/terminal-write readback and enforces stopping after a failed
initial witness or at the event cap. D is the failing diagnostic-expectation
control. No failed build or capture occurred in this tranche. All earlier
limited attempts remain in sibling flycast-evidence/fc067-span-working-*.
Exact C/D temporary patches are retained in ignored build-neural-automation.
All rec_x64.cpp instrumentation is removed; no live consumer settings or native
rendering defaults change. Captured matrices/game data remain outside Git.

Restored-source automation, NGX, no-NGX and feature-off configure/build serially
with exit 0. Three enabled selftests pass 243/243 each; SDK mock passes 56/56,
runtime_loaded/gpu_rendered/presented false. Both parser suites pass (six prior
store methods, five new consumer methods). The prior F/G store verifier remains
green after shared-parser refactoring. Passing C as its own negative control
rejects with exit 1. Source emu.cfg remains SHA256
`1EF718689784DCE64CAD1CE8BEC776E710B4A3705ECFF2E5A276A1A3B2F92992`.

## Next bounded task

Capture actual arithmetic operands/results at 8c070c88/9a/a2 and committed
derived-coordinate stores at 8c070c92/9c/a4 for this witnessed execution. Treat
the resulting span as a NEW generation only after proving its arithmetic links,
with a wrong-operand control. Then follow that derived span toward actual SQ/DMA
TA submission with execution/context/packet identity and explicit event limits.
Do not infer FOV from images, call the copied attribute buffer a camera, repeat
the census, or start renderer/runtime integration. Native PVR depth is unchanged.
