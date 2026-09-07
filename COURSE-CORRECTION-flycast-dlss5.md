# Flycast DLSS 5 course correction

## Current priority -- reuse-first remake feasibility (2026-09-07)

`docs/neural/REMAKE-REPLAY-AUDIT.md` is the latest diagnostic checkpoint:
decoded GPU replay runs, but a one-step repeated-native/on-off pixel discrepancy
is unresolved. M2 remains open; do not restart the already-built decoder.

`docs/neural/REMAKE-M2-AUDIT.md` records the snapshot/decoder prerequisite.
Actual projected packets reach isolated raster replay; recovered camera and
Remix rendering remain unproven.

M1 now has a tested CPU scene contract and real-header/mock adapter; see
`docs/neural/REMAKE-M1-AUDIT.md` and LOG #133/#135. Its bounded corrections are
self-reviewed. No synthetic GPU or real-game reconstruction is proven.

The next implementation is FC-067 / M2 decoded native replay in
`docs/neural/REMAKE-FEASIBILITY-PLAN.md`, routed to GPT-6 Astra at low reasoning
(the user's requested Astra light, not Sol/high).
Evaluate a narrow public Remix SDK scene adapter and synthetic harness before
custom lighting/rendering or replacement-asset investment. RTX Remix is a
candidate, not a proven Flycast integration; scene reconstruction and later
DLSS 5 chaining are explicitly unproven. `AGENTS.md` is the agent entry point.
This priority supersedes older next-task scheduling, not the evidence below.
Keep unavailable-title/hardware tests recorded but do not let them block M2.
The new remake lane is explicitly transformative; Faithful stays public Auto
and all existing rendering/fallback/provenance protections remain intact.

## Rebaseline

Experimental transport and provenance are proven for the named D3D11On12 plus
user-supplied RenoDX route by FC-056/FC-066 and Gate 10. D3D11On12 is the
selected proven route because it is the route the supplied consumer actually
intercepted; it is not the only theoretically valid route. A compatible direct
D3D11 consumer or contract-preserving bridge could still be valid if separately
supplied and proven.

The critical path is no longer repeated Feature 18 reachability work. It is:

`PVR scene color -> depth truth -> geometry motion -> disocclusion/reactive protection -> exact target-resolution DLAA -> external Neural Rendering -> protected Dreamcast overlays -> Present`

Gate 10 is rerun only as a focused regression when a later change can affect
transport, resource identity, evaluation, or presentation. Its synchronous
sentinel/readback mode remains developer-only, off by default, and excluded
from performance measurements.

## Quality direction

The default is **Faithful Dreamcast Remaster**, not maximum photorealism. It
retains game filtering, fog, modifier volumes, dithering policy, shadows,
color, silhouettes, and protected HUD/text. Menus, FMV, direct-framebuffer,
sprite-heavy, and predominantly 2D scenes bypass generative Neural Rendering
when that is more faithful.

Work continues under the existing FC identifiers. FC-024, FC-031 through
FC-036, FC-044, FC-045, FC-053 through FC-055, FC-059, and FC-063 through
FC-065 own the remaining depth, motion, protection, resolution, capture,
quality, failure, and performance work. The detailed execution and acceptance
map is in `docs/neural/QUALITY-PLAN.md`.

## Boundaries retained

- Native rendering is always available and neural modes remain off by default.
- Public NGX DLAA/SR remains separate from experimental external consumption.
- Flycast adds no private Feature 18 implementation or undocumented parameters.
- Proprietary neural binaries are not inspected, modified, bundled, downloaded,
  or redistributed. FC-067 may inspect public open-source renderer sources and
  build them in isolation after dependency/license review; no proprietary
  dependency auto-downloads. No automatic external configuration writes;
  D-083's explicit user Apply and authorized byte-restored test sweeps remain
  narrow exceptions and do not authorize M1 to edit live settings.
- RTT never evaluates; direct-framebuffer content uses native fallback; accepted
  history advances only after successful neural submission.
- PVR game overlays are protected before Flycast OSD and ImGui, which remain
  after the neural scene result.
- Geometry-derived motion is the production source. Ambiguous content receives
  zero motion and current-color bias rather than fabricated confidence.

No claim of production readiness or highest fidelity is permitted until Gates
11 through 18 and the representative moving-title matrix are complete.
