# Flycast DLSS 5 tester handoff

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
2. For normal DX11 public DLAA/SR only, rerender the PVR scene into the separate neural color target with a Halton render-pixel jitter.
3. Apply the same jitter to current raster coverage in the normal, modifier-volume, and Naomi 2 vertex permutations.
4. Compute motion from current and previous unjittered positions, then report the raster jitter separately through `InJitterOffsetX/Y`.
5. Keep hook-compatible DLAA and DLSS 5 experimental modes at zero jitter.
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

## Next bounded implementor task — post-jitter Gate 16/17 comparison

FC-035 and quality Gate 12 are accepted at LOG #122. The next task is to rerun the leading external candidates because the production jitter contract materially changes their exact inputs.

Required implementation and evidence:

1. Preserve every LOG #118-#122 artifact. Do not reset, clean, stash, rebase, or overwrite them.
2. Capture the same deterministic moving Soulcalibur interval through target-native public DLAA Auto with active production jitter.
3. Capture the conservative intensity-0.125/Natural candidate and Uncanny Cinematic candidate with exact matching color/depth/motion/mask inputs.
4. Uncanny uses Cinematic, Structure 200 percent, Tone 75 percent, and maximum scene coverage as its requested external recommendation. Keep automatic HUD protection enabled in the candidate-default lane; any disabled-overlay maximum-coverage run is a separately labeled visual experiment.
5. Require the consumer-reported active tuple for fields the host reports. Continue labeling local structure as requested plus isolated-output-proven unless the host positively reports it.
6. Retain moving comparisons and all component metrics. Judge Faithful, Enhanced, and Uncanny separately; a dramatic image is not automatically a faithful winner.
7. Restore the supplied text configuration byte-for-byte and verify its original hash after every bounded sweep.
8. Do not acquire proprietary media, inspect or patch third-party binaries, or stage configurations, captures, game media, or user paths.

## Required independent test sequence

1. Read `docs/neural/BACKLOG.md`, `docs/neural/DECISIONS.md`, `docs/neural/LOG.md`, `docs/neural/DIAGNOSTICS.md`, and `docs/neural/QUALITY-PLAN.md`.
2. Record current HEAD and worktree state. Confirm the FC-035/profile slice is committed before beginning another production change.
3. Re-run the four-build matrix and both selftests if code changes. Expected current count is `156/156`; report the actual count.
4. Verify the public and external captures use nonzero production jitter and exact matching input hashes.
5. Verify automatic overlay protection still has zero protected-pixel mismatches in any default-candidate run.
6. Compare native PVR, public Auto, conservative external, and Uncanny with source, temporal, trail, edge, thin-line, color, saturation, black-level, repeat/drop, and HUD metrics plus moving visual review.
7. Reject any run whose requested external settings cannot be bound to the exact consumer output under the existing fail-closed provenance rules.

## Acceptance boundary

Uncanny Cinematic is now a persistent user-selectable candidate default, not the factory default. It may become the user's preferred DLSS 5 profile immediately, but changing the automatic default requires representative Gate 17 moving evidence. Faithful remains public DLAA Auto until an external candidate wins its stricter identity constraints.

Do not claim “production-ready,” “optimal,” or “highest fidelity.” Do not alter, inspect, bundle, or patch third-party Neural Rendering binaries. Do not treat public-NGX success as external DLSS 5 presentation proof.

Return a concise disposition with the exact tested SHA, settings provenance, matched-input count, full component metrics, moving-comparison paths, configuration-restoration hash, `ACCEPTED`, `CORRECTIONS_REQUIRED`, or `NOT_REVIEWABLE`, and the next concrete assignment.
