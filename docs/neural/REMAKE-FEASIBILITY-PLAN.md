# Reuse-first remake feasibility

## Current M2 checkpoint -- 2026-09-07

Current: TRANSFORM-SEMANTICS-AUDIT.md (LOG #154, D-096). One linked composite
passes algebra/projection controls; model/view split is not unique. Next test
shared per-context projection/depth calibration across distinct linked geometry.
A labeled camera-relative frame need not recover a unique world origin; it must
have proven calibration. World reconstruction/Remix GPU remain unproven.

Latest: TRANSFORM-CONSUMER-AUDIT.md, LOG #150, D-092. One executed buffer-consumer
chain is verified and stops at overwrite. Next prove the identified derived
coordinates and bounded TA lineage; camera/Remix GPU remain unproven.

Current: TRANSFORM-STORE-AUDIT.md (LOG #149, D-091). One actual FTRV result is
linked to four verified guest-RAM stores with a falsifying control. Next follow
that bounded span's executed consumers toward TA. Camera/world-space and Remix
GPU execution remain unproven; earlier checkpoints below are historical.

Implemented checkpoint: `MATERIAL-CAPTURE-AUDIT.md` (LOG #146, D-089).
The source-texture/palette sidecar and 30-frame native-preservation evidence are
accepted for that slice only. Review the material artifacts, then pursue the
approved camera source-witness track; do not reimplement extraction or tune
the rejected framebuffer-light preview. Camera/Remix/full M2 remain unproven.
Older launch instructions below are historical scope, not repeat assignments.

New user-approved scope: `CAMERA-MATERIAL-PLAN.md` (LOG #145, D-088).
Implement bounded source-texture/palette export first; actual resources and
vertex color are separable before final shading. Camera remains unknown. Stop
at the material-sidecar review; no pixel probes or framebuffer-light tuning.
This supersedes older stopping/launch text below, not rendering safety gates.

Latest visible decision: `REMAKE-PREVIEW-AUDIT.md`. A 30-frame camera-relative
approximation exists, but faceting and baked-lighting conflicts reject its
promotion. Deliver the comparison and stop that branch. The prior assignment
below is retained for scope; actual camera/material reconstruction and runtime
execution are not completed or silently authorized by this preview.

Current disposition and next task: `REMAKE-DISPOSITION.md`. The user approved
parking the small replay residual. Direct current-packet/current-Remix-adapter
handoff is a source-contract no-go. Next produce one explicitly labeled moving
camera-relative relighting approximation or a precise input no-go; do not resume
precision diagnosis. M2 strict native-source equality remains failed/parked.

Continuation: bounded M1 preflight correction is self-reviewed and pushed;
M2 snapshot/decoder implementation is in `REMAKE-M2-AUDIT.md`; implemented
GPU replay and failing mapping controls are in `REMAKE-REPLAY-AUDIT.md`.
Neither raster alignment nor wrong-viewport controls recover a world camera.

See `REMAKE-M1-AUDIT.md`: the bounded CPU scene contract and real-header/mock
Remix adapter are implemented (current enabled selftests 243/243, SDK mock 56/56).
Synthetic GPU rendering, game reconstruction, and combined presentation are
not proven. The missing runtime/GPU harness remains explicitly open. The M1
task body below is historical scope, not the current next implementation.

## Authority, baseline, and priority

User course correction, 2026-09-07: investigate and implement a bounded
open-source reuse path before designing a custom renderer or commissioning
replacement assets. Planning baseline is
`49c96a09841bf8c0d5df6f3bc8b04d0398d5566d`; record actual implementation HEAD.
Work in the existing branch and preserve all newer work.

FC-067 is a new feasibility item, not a replacement for FC-048 transport,
FC-056/066 provenance, or FC-044/054/055/065 quality and protection work.
It has scheduling priority over unavailable-title expansion and hardware-only
tests. Public Auto remains the Faithful default. Uncanny remains user-selected.
The transformative remake lane may deliberately change materials, lighting,
color, and appearance; unstable geometry, broken HUD, trails, frame-identity
errors, emulation slowdown, or crashes are not aesthetic freedoms.

## Reuse candidates and evidence limits

Primary references inspected during planning (2026-09-07):

- NVIDIA Remix SDK: https://github.com/NVIDIAGameWorks/dxvk-remix/blob/main/documentation/RemixSDK.md
  Direct C/C++ API for meshes/materials/lights/cameras with a D3D9-independent
  example. Leading candidate to evaluate, not an accepted Flycast backend.
- PCSX2 experiment: https://github.com/Aelthien/pcsx2-rtx-remix
  Reference for depth/projection reconstruction and texture categorization;
  README and RemixConfig header inspected, no build or gameplay verified here.
- RTGL1: https://github.com/sultim-t/RayTracedGL1
  Alternative path-tracing library with material overrides and existing game
  ports. Do not integrate both renderers in the first slice.
- RT64: https://github.com/rt64/rt64
  Useful deferred-frame/dual-renderer architecture reference; the inspected
  public README says ray tracing and emulator plugin are not yet available.

Pin actual source revisions and inspect applicable licenses/dependencies before
reuse. Link behavior to real source/API symbols. Do not treat online claims as
runtime proof. Source builds must be isolated; do not download proprietary
neural runtimes or silently enable binary-fetching dependency scripts.

## The actual unknown

Normal Dreamcast PVR submissions contain projected positions and renderer depth,
not an automatically complete world-space scene, camera, skeleton, or lighting
model. Naomi 2 matrix/normal fields do not prove these exist for Soulcalibur.
Missing off-screen/culled geometry and baked lighting can invalidate apparent
path-tracing success. A visually plausible depth extrusion is only a labeled
view-space approximation. Do not assign arbitrary near/far/FOV values and call
them recovered camera truth; do not change the proven neural logarithmic depth
contract to accommodate another renderer.

## M1 -- historical bounded implementor task

Model routing: GPT-6 Astra, low reasoning (user: Astra light, explicitly not
Sol/high). No further model comparison phase.

1. Record HEAD/worktree, inspect current TA vertex/index/list/state and texture
   generation paths, existing capture seams, and SDK source interfaces. Produce
   a compact source/API/dependency audit. Do not clone every candidate.
2. Implement a bounded, renderer-neutral scene packet and validator in a
   developer harness, isolated from ordinary frame submission. It must identify
   coordinate space, projection provenance (known/supplied/unknown), frame/game
   IDs, topology, texture identity/generation, transforms when actually known,
   and unsupported/omitted geometry. Enforce explicit vertex/index/byte limits.
   Unknown normals, lights, transforms, and materials remain unknown, not
   invented engine facts. This schema is harness-only and versioned.
3. Add deterministic synthetic triangles/overlap/camera-motion fixtures with
   analytic 3D truth. Validate strip winding and degenerate breaks if using PVR
   topology. Test invalid indices, NaN/Inf, truncation, missing projection,
   wrong depth/unprojection, and frame-identity mismatch. Negative controls must
   fail for their intended reason. Keep raster-depth semantics separate from
   any explicitly derived view-space positions.
4. Build a thin adapter to the pinned public Remix API where its dependencies
   are safely available. Prefer a standalone developer target (explicit opt-in,
   off by default), not a new production D3D9 backend. First render the synthetic
   scene with a controlled light and verify readback/occlusion/camera behavior.
   If unavailable, complete the packet/validator/source adapter work and report
   the exact missing build/runtime dependency. A mock tests adapter calls only;
   it is never GPU/path-tracing success. Do not stall all implementation on SDK
   availability or silently switch to a second architecture.
5. Record outcomes and stop at the M1 review boundary. Do not install hooks,
   change production presentation, activate consumer settings, create a factory
   preset, or begin a texture/mesh replacement campaign in this slice.

Allowed owned scope: new `neuraltest` feasibility sources/fixtures, narrow
build-system wiring, and governing docs. Prefer reuse of existing source seams;
any production capture hook must be separately justified and remain disabled
with zero ordinary-mode allocations/work. No renderer rewrite in M1.

## Later milestones -- ordered, not pre-accepted

- **M2: one bounded Soulcalibur arena packet.** Use the existing deterministic
  Hoko Temple replay and legal user-supplied media. Export actual geometry and
  texture references/generations with explicit projection assumptions. First
  prove native raster replay alignment through camera movement/overlap. Record
  omitted surfaces and multiple projection domains. Wrong-camera and wrong-
  depth controls must fail. No proprietary game data enters Git.
- **M3: moving scene relighting.** Original assets first, simple materials and
  one controlled light. Check camera-consistent geometry, moving fighters,
  occlusion, surface stability, missing geometry, and baked-lighting conflicts.
  A camera-relative approximation cannot claim full scene reconstruction.
- **M4: integration decision.** Audit resource ownership, synchronization,
  returned image identity, latency, presentation, and possible DLSS 5 chaining.
  Keep standalone Remix proof distinct from existing external neural proof.
  Reuse Gate 10 only if the actual changed route requires a focused regression.
  Run relevant normal/OIT cadence and feature-off checks before production use.
- **M5: artistic investment.** Only after measured moving benefits decide
  whether material overrides, authored lights, textures, or meshes add enough
  value. No requirement to preserve original colors/materials in this explicit
  transformative lane, but keep gameplay timing, readable UI, and stable motion.

## Verification and handoff

For code changes, build `build-neural-automation`, `build-neural-baseline`,
`build-neural-no-ngx`, and `build-neural-off` serially using the established MSVC
environment. Run each enabled `neuraltest selftest` (planning baseline 172/172;
report actual counts), plus the new fixtures. If source changes affect neither
renderer nor presentation, do not repeat the full old real-game gate matrix.
Run the relevant synthetic GPU case only when its runtime is available; record
unavailability precisely. Synchronous capture is not performance evidence.

Commit independently proven slices with explicit staging and verify remote SHA
when pushing to the existing fork. Retain failed commands/negative controls and
external evidence paths in local reports; committed docs use portable paths.
Update LOG/BACKLOG/DECISIONS and tester handoff, distinguishing:

- scene packet/analytic contract proven;
- public SDK adapter built or blocked;
- synthetic GPU rendering verified or not run;
- real-game scene reconstruction pending;
- moving relighting/presentation/combined DLSS 5 pending.

M1 success is useful tested implementation plus an honest adapter disposition,
not a declaration that Remix is viable for all Dreamcast games. If M2 disproves
scene reconstruction, document the limitation and compare a labeled view-space
experiment or RTGL1 next; never fabricate a complete scene to keep the plan green.
