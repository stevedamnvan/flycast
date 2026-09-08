# Flycast DLSS 5 course correction

## Autonomous backlog-driven delivery - 2026-09-08

Use [docs/neural/BACKLOG.md](docs/neural/BACKLOG.md) as the single live queue
and source of the short standing goal. The user now authorizes continuous,
bounded implementation toward working RTX Remix plus supplied DLSS 5 gameplay,
without approval at every trace or milestone. D-114 replaces those old
permission stops; technical acceptance and hard safety boundaries remain.

AGENTS.md gives the execution loop. The remake feasibility plan gives
architecture, reuse candidates and claim limits. LOG/audits preserve history.
Do not append competing current assignments here. On a tested checkpoint,
update the backlog in place and continue to the next unblocked dependency.

The selected opaque FTRV-to-TA chain is verified at0e095eb75 (LOG #175).
Next is bounded camera/coordinate coverage, with an independent real Remix GPU
harness as a useful alternative while scene dependencies are unresolved.
The actual combined scene-to-Remix-to-external-DLSS5-to-Present route is still
unproven; unrelated standalone successes cannot be combined into a claim.

The transformative remake lane may be deliberately unfaithful, but geometry,
HUD, temporal correctness, frame ownership, stability and emulation/audio speed
remain requirements. Public DLAA Auto stays the Faithful factory baseline.
Uncanny is user-selectable. Do not conflate experimental working scope with
the full quality/title/hardware matrix or silently close parked failures.

Historical current-task paragraphs are removed from this launch document,
not from evidence: their exact versions remain in Git at0e095eb75 and the
referenced LOG/audits. This is a routing correction, not a renderer change.

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
