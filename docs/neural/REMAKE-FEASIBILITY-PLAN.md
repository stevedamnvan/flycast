# Reuse-first remake feasibility

## Current authority

The active queue and short standing goal are in [BACKLOG.md](BACKLOG.md).
The user's2026-09-08 autonomous-delivery authorization, recorded in D-114,
supersedes old per-block/M1/M2 permission stops. Agents may implement, test,
review and advance through the bounded queue without routine human signoff.
Safety and unproven acceptance remain intact. This file defines architectural
constraints; it must not carry a second competing current assignment.

Current evidence anchors: LOG467-474 demonstrate isolated moving real-scene
Remix output; LOG484/509 establish owned live snapshots and a bounded observed
transform-dependency subset. LOG510-511 retain a measured calibration candidate,
not recovered world/camera truth. LOG525 proves bounded live paired returned
input delivery; LOG526 confirms three source-qualified retained-input external
outputs through matched ON/clean/OFF controls. LOG529 adds capture-independent
ordinary-frame scene feed:60 matched moving sources and56 bounded retained paired
replies. LOG542 subsequently records600 paired OIT returns and595 distinct
displayed sources; LOG544 proves25 consecutive exact-input external results
through clean HUD composition and completed Present. Returned presentation is
also verified after native OIT effect restoration: LOG564 proves28 consecutive
exact-input outputs with full effect identity, marked/clean/OFF controls, original
HUD and completed Present. This closes that changed-path regression only. It is
therefore experimentally implemented, not absent. Full combined ordinary-gameplay
acceptance, returned-scene temporal guidance and camera acceptance remain pending.
The strict replay residual stays
failed/parked; neither approximation nor a mock closes that gate.

Immediate work is the backlog's live M2-scene integration card. The runtime is
available; do not repeat historical dependency inventories. The standing objective closes
only on the backlog's working-pipeline checklist, not an individual milestone.
Historical launch snapshots remain in Git at0e095eb75 and in their source
audits/LOG entries; they are not continuing orders to pause.

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
errors, emulation slowdown, or crashes are not aesthetic freedoms. Intentional
source trails and translucent gameplay effects must be preserved; only added
reconstruction persistence is a temporal defect.

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
5. Historical M1 stop superseded by D-114: record/review outcomes and take the
   next ready backlog card without routine human approval. Do not install hooks,
   change production presentation, activate consumer settings, create a factory
   preset, or begin a texture/mesh replacement campaign in this slice.

Allowed owned scope: new `neuraltest` feasibility sources/fixtures, narrow
build-system wiring, and governing docs. Prefer reuse of existing source seams;
any production capture hook must be separately justified and remain disabled
with zero ordinary-mode allocations/work. No renderer rewrite in M1.

## Milestone architecture -- execution order lives in BACKLOG

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
  Under D-224 the bounded Soulcalibur pilot (BACKLOG pilot substeps A-E) may
  run material-channel proof, shading/opacity correction, a standalone
  higher-resolution reference and an authored temple light on this one scene
  alongside unfinished hardening; it needs only its immediate dependencies.
- **M4: integration decision.** Audit resource ownership, synchronization,
  returned image identity, latency, presentation, and possible DLSS 5 chaining.
  Keep standalone Remix proof distinct from existing external neural proof.
  Reuse Gate 10 only if the actual changed route requires a focused regression.
  Run relevant normal/OIT cadence and feature-off checks before production use.
- **M5: artistic investment.** Only after measured moving benefits decide
  whether material overrides, authored lights, textures, or meshes add enough
  value. The transformative lane may change appearance, but the D-224 pilot's
  faithful candidate preserves recognizable designs, costume artwork,
  silhouettes, animation, stage identity, original effects and HUD; its scoped
  authoring (curated material palette, temple light) is limited to the Hoko
  Temple scene and two fighters. User direction 2026-09-10: the pilot's
  material set is AI-reimagined (local ComfyUI, ComfyUI-RTX-Remix nodes,
  PBRify models; results ingested through the Toolkit MCP) while designs,
  silhouettes, animation, effects and HUD stay original. Keep gameplay timing, readable UI and stable
  motion. Broad asset replacement and further titles remain later work.

## What the playable remaster must deliver

The current execution order is in BACKLOG; this plan defines the result.
Actual fighters and arena must participate in path-traced lighting, with
correct geometry, smooth normals, opacity and material response. Demonstrate
cast shadows, indirect illumination and appropriate reflections through
controlled scene changes and runtime provenance. Texture upgrades alone are
insufficient. Preserve artwork identity and remove baked-light conflicts rather
than amplifying them with generated normals or excessive gloss.

The result must remain coherent during player-controlled combat: moving hair,
occlusion, camera changes and round transitions, with native HUD/effects and
uninterrupted audio/emulation. Verify supplied DLSS5 contribution separately;
it does not establish that the upstream scene is correctly path traced.

Target sustained60 fps with fresh output at the recorded resolution, measured
pacing/latency and bounded resources. Preserve the existing300-frame visual and
600-frame normal/OIT acceptance gates. Deliver through the existing reversible
opt-in launcher/layer, with lifecycle tests and separate human visual approval.
A testable intermediate build is not final acceptance.

Keep Package D's full captured-set coverage and cost controls. Strand hair
requires supported moving geometry attachment and measured rendering cost.
Use the existing pipeline; the authoritative queue governs prioritization.

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
