# Flycast DLSS 5 tester and implementor handoff

## Resume from the backlog

Read [AGENTS.md](AGENTS.md), then the active queue in
[docs/neural/BACKLOG.md](docs/neural/BACKLOG.md). Do not copy this handoff into a
new standing goal or treat old audit stopping language as current authority.
The user's2026-09-08 authorization permits routine bounded implementation,
review, checkpoint and automatic advancement without human signoff.

Current first card: FC-067 / M2-camera. FULL-DRAW-TRANSFORM-AUDIT.md records
426 verified vertex observations feeding one complete92-triangle opaque draw.
Its calibrated reprojection is verified;8247 other opaque triangles/frame,
world-camera semantics and runtime rendering remain unproven. Temporary hooks
are removed. Next follow the backlog's remaining-domain coverage priority,
not another single-point/source-gather trace. Preserve nonunit W, the older
draw26 accumulation requirement and opaque/translucent depth distinctions.
Latest live wrong-X control rejects but timing/images differ: no exact-input
preservation claim. Earlier successful and failed controls retain their scopes.
If the card is genuinely blocked, take FC-067 /
M1-GPU. The standalone public-SDK bring-up now builds but needs a legal compatible
runtime; read REMAKE-RUNTIME-BRINGUP.md. Its63-check mock and API success are not GPU
proof. It does not require a recovered game camera. Neither synthetic GPU output nor the existing mock is gameplay
or combined DLSS 5 presentation evidence.

Latest build/test outcomes and exact checkpoint identities are in LOG, not
duplicated here. Before advancing, inspect actual HEAD/status and preserve any
newer work. Historical exact-SHA results are not substitutes for new checks.

For each slice report ACCEPTED, CORRECTIONS_REQUIRED or NOT_REVIEWABLE, update
the current backlog card and LOG/DECISIONS, preserve failed artifacts, and
continue. Ask only for genuine external dependencies/authority after all useful
safe alternatives are exhausted. Keep native and external settings untouched
unless the applicable implementation stage explicitly covers the change and
the user has authorized the external action.

The final deliverable is the backlog's reproducibly working combined gameplay
pipeline, with moving comparisons, provenance, protected HUD and asynchronous
cadence/failure evidence. Full M2/world camera, the parked replay residual,
additional titles and production quality are separate claims.

## Retained UI and quality follow-ups

These do not override the active queue or justify repeating completed work.



LOG #131 adds the user-requested external intensity panel under DLSS 5 mode.
Review Video > Neural Rendering > External consumer intensity controls. Set
the installed companion and consumer INI paths, choose pending controls, then
Apply. Use Uncanny values only fills the controls. Test any writes on a copy
of the consumer INI first, check the adjacent exact backup and helper result,
then restart the app. Initial sliders do not claim to reflect current config.
Manual click/layout and consumer restart coverage remain to be exercised;
the production launcher has passed a real-companion disposable-file test.

For OIT history, consult `docs/neural/OIT-JITTER-IMPLEMENTATION-PLAN.md`. LOG #127 closes
the focused OIT replay/jitter slice in the current source: isolated color and
A-buffer ownership, aligned reactive coverage, exact production-shader jitter,
byte-identical native PVR controls on D3D11 and D3D11On12, 163/163 selftests in
all three enabled configurations, and a linked feature-off build. An earlier
wrong-pointer clear and its bounded 1-LSB direct-D3D11 controls are retained as
failed evidence rather than hidden.

LOG #128 adds the requested lightweight neural status OSD. It is off by
default, toggled in Video settings, and rendered only through Flycast's late
OSD path. Normal/OIT on/off captures prove nine pre-OSD artifacts remain exact;
direct D3D11 and failure-state smokes also pass. It deliberately says external
DLSS 5 output is unverified even when the public contract submits. Manual
settings-click and resize coverage remain a small follow-up.

The first two post-OIT external recapture attempts were rejected rather than
phase-shifted into a false comparison. Candidate startup accepted a different
number of pre-capture evaluations than marker/policy-off, so nominally equal
frame IDs carried different accepted-history jitter and different complete
inputs. LOG #129 adds a capture-only, one-shot discontinuity immediately before
the first retained frame. All four builds link, all three enabled selftests pass
168/168, and a real OIT capture records the reset on its first retained frame.

LOG #130 completes that exact-SHA rerun on commit `9466e1c2b`. Conservative
0.125/Natural, maximum-coverage Uncanny, and automatic-HUD Uncanny each pass
30/30 exact-input external provenance. The HUD-safe Uncanny lane protects an
average 15,803 pixels with zero mismatch/repeat/drop, but it remains far outside
Faithful source, trail, edge, thin-line, color, saturation, and black-level
constraints. Faithful remains public DLAA Auto; Uncanny remains a valid
user-selected transformative default, not the factory default.

The next image-quality assignment is Gate 17 style-family expansion when the
user supplies another legal title. The next bounded local UI check is a manual
Video-setting toggle plus resize smoke for the optional neural-status OSD.
