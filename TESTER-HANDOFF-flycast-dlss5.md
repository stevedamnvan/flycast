# Flycast DLSS 5 tester handoff

## Current assignment -- 2026-09-07

Latest: `docs/neural/CAMERA-SOURCE-AUDIT.md` (LOG #147, D-090). Next is a bounded
executed-transform-to-TA witness, with an explicit uncorrelated/unsupported exit.
No camera has been recovered or newly tested. Do not repeat material extraction,
approximation tuning or parity probes. Older assignments below are historical.

LOG #148 now confirms actual x64 FTRV execution with a temporary removed
counter and three-frame native preservation (27/27 image planes). Next trace
bounded guest-instruction/value lineage; do not rerun the presence census.

Implemented checkpoint: `docs/neural/MATERIAL-CAPTURE-AUDIT.md` (LOG #146, D-089).
The source-texture/palette sidecar and 30-frame native-preservation evidence are
accepted for that slice only. Review the material artifacts, then pursue the
approved camera source-witness track; do not reimplement extraction or tune
the rejected framebuffer-light preview. Camera/Remix/full M2 remain unproven.
Older launch instructions below are historical scope, not repeat assignments.

New user-approved scope: `docs/neural/CAMERA-MATERIAL-PLAN.md` (LOG #145, D-088).
Implement bounded source-texture/palette export first; actual resources and
vertex color are separable before final shading. Camera remains unknown. Stop
at the material-sidecar review; no pixel probes or framebuffer-light tuning.
This supersedes older stopping/launch text below, not rendering safety gates.

Latest: `docs/neural/REMAKE-PREVIEW-AUDIT.md`. The 30-frame labeled moving
comparison is implemented. Review it as an approximation only; it exposes
faceting and baked-lighting conflicts and is not promoted. Deliver it, then
stop this branch. No more pixel probes or light tuning; a genuine remake step
needs a separately scoped camera/material and actual GPU-runtime task.
The implementor instructions below describe the completed preview assignment.

Read `docs/neural/REMAKE-DISPOSITION.md` first. The user approved parking the
13-pixel replay residual and ending the precision-diagnostic loop. Strict M2
source equality remains failed/parked; decoded/native-buffer replay is exact
for the retained 30-frame interval. Do not resume trace-53 probes.

Next implementor (Astra/low): complete the bounded moving camera-relative
relighting approximation specified there, using existing captured geometry
and matched source frames. Label assumed projection, derived normals, baked
lighting, image-projected materials, and missing geometry. Preserve HUD or
limit to a clearly labeled world crop. Deliver a native/relit animation with
light-off identity and falsifying depth/normal controls, or an exact input no-go.

The current real packet cannot directly feed the current world-only Remix
adapter. This is not proof all Remix integration is impossible. No recovered
world camera, real Remix GPU rendering, combined DLSS 5 presentation, or new
quality winner is claimed. Temporary renderer probes are removed; failed and
invalid attempts remain in the disposition and local evidence. No live config
changes, proprietary downloads, production rewrite, or default promotion.

### Historical M1/export launch notes (superseded by assignment above)

Follow-up correction: topology preflight rejects a later all-degenerate strip
before external resource calls. Four builds and three 217/217 selftests pass;
the SDK mock now passes 56/56. This is self-reviewed bounded acceptance, not GPU
proof. Continue M2 export/disk round-trip/native alignment next, retaining
unknown game camera and normal provenance and explicit unsupported omissions.

M1 implementation checkpoint: review `docs/neural/REMAKE-M1-AUDIT.md` and the
new `neuraltest/remake_*` sources. All four builds pass; enabled selftests are
217/217 and the opt-in public-header/mock adapter test is 55/55. No runtime was
loaded, no GPU rendering occurred, and no game geometry was captured in M1.
On review acceptance, M2 is bounded Soulcalibur packet export/native alignment;
the synthetic GPU/runtime gap remains a separate explicitly unproven slice.
The original launch instructions below are retained as scope, not current
claims that no implementation exists.

**FC-067 M1 takes priority.** Read `AGENTS.md` and
`docs/neural/REMAKE-FEASIBILITY-PLAN.md`. Implement the bounded reuse-first
scene contract/synthetic harness and public Remix API adapter disposition,
using GPT-6 Astra/low (Astra light). The audited implementation baseline before this plan
is `49c96a09841bf8c0d5df6f3bc8b04d0398d5566d`; record actual HEAD.
The original planning change built no adapter; M1 now has the header/mock
adapter evidence above, but no game reconstruction. Independent review must distinguish analytic packet tests, adapter
build, synthetic GPU output, and real-game reconstruction. The last two are
not implied by the first two. Use the M1 acceptance/return contract in the plan.

Next implementor task after accepted M1: M2 bounded Soulcalibur Hoko Temple
scene export and native replay alignment, with explicit camera/depth provenance
and wrong-camera controls. If M1 is not reviewable, first resolve its exact
dependency or contract failure instead. Do not proceed directly to production
path tracing, asset replacement, or combining DLSS 5 and Remix.

The UI/title assignments below are retained follow-ups, not permission to
override FC-067 priority. Current established selftest baseline is 172/172 in
all three enabled configurations; older counts below are historical.

## Retained UI and quality follow-ups

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

## Historical handoff retained for provenance

You are the independent tester/reviewer for Flycast's post-jitter Gate 16/17 visual-quality work.

Repository:

    <workspace>\flycast

Branch:

    feat/neural-rendering

Audited parent baseline SHA:

    a95c5ef78d971e42792db6fc7887fbe95b08cf81

Fork remote:

    https://github.com/stevedamnvan/flycast.git

Do not reset, clean, stash, rebase, discard, or overwrite the working tree. Resolve the current branch head with `git rev-parse HEAD`; the parent baseline above is retained only as provenance.

## Proven committed baseline

Commit `a95c5ef78` is `FC-032: rasterize accepted-history Naomi 2 motion`.

The exact committed SHA was rebuilt in all four required configurations:

- `build-neural-baseline`
- `build-neural-no-ngx`
- `build-neural-off`
- `build-neural-automation`

Both NGX and no-NGX selftests passed `153/153`. The actual production Naomi 2 HLSL permutation produced analytic `[-4,+3]` render-pixel motion on native D3D11 and D3D11On12. Missing matrix history and reindexed Naomi 2 geometry remained current-color protected. The commit is pushed and the worktree was clean before the current WIP began.

## Proven working-tree closure pending discrete commit

The working tree closes FC-035 and quality Gate 12 without perturbing the native framebuffer or native fallback. LOG #122 contains the complete active-jitter, failure, HUD, OIT, build, and selftest evidence. The authorized Gate 16 sweep and deliberately uncanny follow-up in LOG #118-#120 did not produce a Faithful external winner. `NRLocalStructure=2.0` is proven active through an exact isolated A/B. LOG #121 also adds Uncanny Cinematic as a persistent user-selectable profile; Faithful remains the factory default.

Modified files at pause time:

    core/rend/dx11/dx11_naomi2.cpp
    core/rend/dx11/dx11_renderer.cpp
    core/rend/dx11/dx11_renderer.h
    core/rend/dx11/dx11_shaders.cpp
    core/rend/neural/instrumentation.h
    core/rend/neural/quality_capture.cpp
    core/rend/neural/quality_capture.h
    core/rend/neural/quality_profile.cpp
    core/rend/neural/quality_profile.h
    core/ui/settings_video.cpp
    docs/neural/BACKLOG.md
    docs/neural/DECISIONS.md
    docs/neural/DIAGNOSTICS.md
    docs/neural/LOG.md
    docs/neural/QUALITY-PLAN.md
    neuraltest/guidance_contract.cpp
    neuraltest/harness.h
    neuraltest/main.cpp
    neuraltest/unit_tests.cpp

The intended design is:

1. Keep Flycast's ordinary PVR framebuffer unjittered and untouched for fallback and protected-overlay composition.
2. For normal DX11 public DLAA/SR and the DLSS5 experimental public-DLAA contract, rerender the PVR scene into the separate neural color target with a Halton render-pixel jitter.
3. Apply the same jitter to current raster coverage in the normal, modifier-volume, and Naomi 2 vertex permutations.
4. Compute motion from current and previous unjittered positions, then report the raster jitter separately through `InJitterOffsetX/Y`.
5. Keep hook-compatible DLAA at zero jitter; the selected DLSS 5 experimental route uses the ordinary public-DLAA jitter contract before interception.
6. Fail conservatively to zero jitter for DX11 OIT, predominantly 2D frames, retained-framebuffer content, and frames containing protected overlay draws.
7. Record the exact jitter and conservative reason in every quality-capture manifest.

The capture CLI now accepts a developer diagnostic option:

    --overlay-policy auto|full|disabled

This does not change the default. It exists so a bounded diagnostic can explicitly disable overlay protection without silently weakening Faithful Remaster behavior.

## Evidence already run on the WIP

The baseline build linked after the renderer changes. The expanded selftest passed `155/155`.

The two added production GPU controls passed on native D3D11 and D3D11On12:

- A one-render-pixel jitter shifted standard PVR coverage by exactly one pixel while static motion stayed exactly `[0,0]`.
- The Naomi 2 production permutation shifted by the same exact amount while static motion stayed exactly `[0,0]`.

Retain this failed attempt in the evidence record: the first fixture compile used `std::min/std::max` while Windows macros were active and failed at `guidance_contract.cpp`; changing the calls to `(std::min)` and `(std::max)` corrected the harness-only issue.

A deterministic Soulcalibur run completed cleanly:

    build-neural-automation\jitter-dlaa-normal-working

It used the supplied legal media and replay:

    <user-supplied legal Soulcalibur CHD outside the repository>
    build-neural-automation\scripts\Soulcalibur (USA).input

The command was:

    .\build-neural-automation\neuraltest\neuraltest.exe capture --game "<legal Soulcalibur CHD>" --frames 3 --skip 1802 --out build-neural-automation\jitter-dlaa-normal-working --flycast .\build-neural-automation\flycast.exe --lane dlaa --api d3d11 --renderer dx11 --preset auto --profile faithful --style auto --overlay-policy disabled --render-height 480 --feature-path "<public NGX feature path>" --input-replay yes --timeout-ms 180000

The process closed cleanly and all three public-DLAA evaluations were accepted. This run is **not** active-jitter proof. Frames 1804-1806 each recorded:

    "raster_jitter": [0, 0]
    "raster_jitter_applied": false
    "raster_jitter_reason": "retained-framebuffer-content"

That is the intended conservative guard because Soulcalibur did not clear the retained PVR framebuffer during those frames. Do not relabel this run as jitter success, failure, or title-quality evidence.

## Next bounded implementor task — Gate 17 style-family expansion

LOG #124 completes the exact-commit post-jitter Soulcalibur comparison. The
conservative and both Uncanny lanes pass 30/30 external provenance. Public Auto
remains Faithful; Uncanny is a valid user-selected transformative preset whose
automatic-HUD lane has zero protected-pixel mismatches.

Required implementation and evidence:

1. Preserve every LOG #118-#124 artifact. Do not reset, clean, stash, rebase, or overwrite them.
2. When the user supplies another legal title, choose one deterministic moving interval that fills a missing style family.
3. Capture native, public DLAA Auto, Uncanny automatic-HUD, and maximum-coverage experiment lanes with exact candidate/marker/policy-off provenance.
4. Require zero protected-pixel mismatch for the candidate-default lane and keep maximum coverage separately labeled.
5. Judge the title independently. Do not infer a factory-default win from Soulcalibur or still images.
6. Keep accepted-history-indexed jitter. Absolute-frame jitter phase is a rejected design.
7. Restore any authorized external text config byte-for-byte and keep all third-party files, captures, media, and user paths outside Git.

LOG #125 records one additional rejected control. The exact 30-frame OIT
candidate/marker/policy-off inputs match, but protected HUD composition covers
35 pixels of the fixed proof sentinel in 26 frames. The verifier correctly
rejects 989/1024 as presentation proof. Do not accept, compare, or present
those external outputs unless a focused future diagnostic change proves a
non-overlapping or explicitly overlay-aware sentinel without weakening Gate 10.

## Required independent test sequence

1. Read `docs/neural/BACKLOG.md`, `docs/neural/DECISIONS.md`, `docs/neural/LOG.md`, `docs/neural/DIAGNOSTICS.md`, and `docs/neural/QUALITY-PLAN.md`.
2. Record current HEAD and worktree state. Confirm the FC-035/profile slice is committed before beginning another production change.
3. Re-run the four-build matrix and all available selftests if code changes. Expected current count is `159/159`; report the actual count.
4. Verify eligible public and external frames use accepted-history production jitter and exact matching input hashes; protected-overlay frames may conservatively report zero jitter.
5. Verify automatic overlay protection still has zero protected-pixel mismatches in any default-candidate run.
6. Compare native PVR, public Auto, conservative external, and Uncanny with source, temporal, trail, edge, thin-line, color, saturation, black-level, repeat/drop, and HUD metrics plus moving visual review.
7. Reject any run whose requested external settings cannot be bound to the exact consumer output under the existing fail-closed provenance rules.

## Acceptance boundary

Uncanny Cinematic is now a persistent user-selectable candidate default, not the factory default. It may become the user's preferred DLSS 5 profile immediately, but changing the automatic default requires representative Gate 17 moving evidence. Faithful remains public DLAA Auto until an external candidate wins its stricter identity constraints.

Do not claim “production-ready,” “optimal,” or “highest fidelity.” Do not alter, inspect, bundle, or patch third-party Neural Rendering binaries. Do not treat public-NGX success as external DLSS 5 presentation proof.

Return a concise disposition with the exact tested SHA, settings provenance, matched-input count, full component metrics, moving-comparison paths, configuration-restoration hash, `ACCEPTED`, `CORRECTIONS_REQUIRED`, or `NOT_REVIEWABLE`, and the next concrete assignment.
