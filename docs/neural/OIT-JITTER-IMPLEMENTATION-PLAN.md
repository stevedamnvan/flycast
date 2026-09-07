# OIT jitter assessment and implementation handoff

## Implementation status — 2026-09-07

The focused OIT replay slice is implemented and verified in the working tree.
Neural OIT replay now owns dedicated opaque/multipass textures, a separate
A-buffer/pointer texture, and an aligned resolve-time reactive target. A scoped
restore returns every native texture/view binding on exit. The production OIT
vertex shader fixture proves an exact +1 render-pixel coverage shift on native
D3D11 and D3D11On12, and the complete selftest suite passes 163/163 in the
automation, NGX, and no-NGX configurations. Feature-off also links.

Deterministic Soulcalibur frames 1804-1806 use nonzero accepted-history Halton
jitter on both D3D11 surfaces and retain byte-identical native PVR output versus
their native controls. An intermediate control exposed that replay initially
cleared the native pointer texture; switching the clear to the dedicated replay
pointer texture made direct-D3D11 parity exact. The earlier 1-LSB variable runs
remain retained outside Git as falsifying evidence.

The next implementation slice is step 9, the lightweight neural status OSD,
followed by the affected exact-SHA Gate 16/17 recapture. Steps below remain the
historical acceptance plan and evidence checklist.

Assessed 2026-09-07. Implementor: GPT-5.6 Sol, high reasoning, in the existing
task and checkout. This is the current bounded assignment for FC-035/FC-036,
followed by affected FC-044/FC-054/FC-065 visual comparisons.

## Current state and evidence

- Branch: `feat/neural-rendering`.
- Local HEAD and live `fork/feat/neural-rendering` both resolve to
  `fcd5bc220dd9efefc11b8add02f15872e3d99274`. The fork remote is
  `https://github.com/stevedamnvan/flycast.git`; `origin` is upstream.
- Five preserved source files contain the paused OIT replay draft:
  `core/rend/dx11/dx11_renderer.cpp`, `core/rend/dx11/dx11_renderer.h`,
  `core/rend/dx11/oit/dx11_oitrenderer.cpp`,
  `core/rend/dx11/oit/dx11_oitshaders.cpp`, and
  `neuraltest/shader_contract.cpp`.
- On September 4 that draft linked in the automation build and passed 162
  selftests. On September 7 the existing automation test executable was run
  again: 162 passed, zero failed. No fresh build or gameplay test was run
  during this assessment. Shader source compilation is covered; OIT jitter
  coverage, native history isolation, and gameplay fallback are not yet proven.
- `git diff --check` passed, with line-ending warnings only.
- LOG #126 and the saved external-confirmation artifact establish 30/30
  matched OIT Uncanny frames at tested SHA `ca9ec174b`: intensity 2, global tone
  0.75, diffuse white 203, preset 0, style 2, upscaling off, enabled on.
  This is the prior zero-jitter OIT result, not evidence for the new draft.
  It preserves protected HUD pixels but loses the Faithful comparison.
- Public DLAA Auto remains the Faithful default. Uncanny Cinematic remains a
  persistent user-selected transformative preset. No host fork is needed to
  repeat the already-proven Structure-200/Tone-75 tuple.
- Supplied Soulcalibur media and the deterministic `.input` replay are still
  available. Find their current local paths; keep them out of tracked files.
- Prior evidence lives in the sibling evidence directory under
  `gate17-uncanny-oit-ca9ec174b`. Preserve all earlier accepted and rejected runs.

## Findings requiring correction or proof

1. **Persistent OIT color state is overwritten by replay.** The draft prepares
   the native `opaqueRenderTarget`, calls OIT `drawStrips()`, and leaves its
   modified opaque/multipass textures in place. Those targets persist across
   frames, clear only conditionally, and can swap during multipass resolve.
   Routing final output away from `fbRenderTarget` protects the current output
   but does not establish isolation of the next retained native frame.
   Replacing native retained opaque state with the previous fully composited
   framebuffer can also change translucency accumulation. This is a concrete
   ownership defect; its visible impact has not yet been measured.
2. **Reactive coverage is from the wrong raster.** The replay branch binds only
   color at final resolve; `renderNeuralReactiveCoverage()` still merges the
   original unjittered `oitReactiveView`. Replay must produce matching visible
   translucent-stack coverage at its jittered positions, or provide a proven
   conservative union. Do not drop native modifier/translucency protection.
3. **A-buffer reset needs an explicit boundary.** `Buffers::bind()` resets the
   allocation counter, while final resolve clears only visited pixel pointers.
   Verify scissors, retained frames, changing extents, multipass, and interrupted
   replay cannot expose stale pointers. Audit u2/u3 bindings before guidance MRTs.
4. **Cleanup and dimensions need proof.** The raw replay-target member resets
   only after normal return. Use scoped ownership/cleanup. OIT scratch can be
   larger than current content after RTT or resize; verify viewport, retained
   copy region, and sampling dimensions. Preserve RTT bypass and native state.
5. **Feature-off shader safety needs verification.** The draft initializes
   render dimensions outside the neural preprocessor guard because shared
   shaders divide by them. Verify positive dimensions on native and RTT paths,
   zero-jitter native parity, and the feature-off build rather than assuming
   a successful compile proves pixels.

## Implementation sequence

1. Read current docs and diff, record actual HEAD, preserve all dirty files,
   and reproduce ownership/mask failures with bounded production or GPU
   controls before fixing them. Keep failures in the evidence log. Do not
   describe the current draft as an accepted FC-035 OIT closure.
2. Give neural OIT replay isolated color history and resolve ownership. Prefer
   dedicated replay scratch initialized from the correct pre-native OIT base;
   a scoped save/restore alternative must preserve both pixels and texture/view
   identities through multipass swaps. Do not seed it from an assumed equivalent
   fully resolved frame. Keep allocations bounded and account for new resources.
3. Generate aligned OIT reactive coverage, ensure clean A-buffer state, and
   restore bindings and replay ownership on every exit. Keep current/previous
   motion positions unjittered and phase indexed by accepted evaluations.
   Protected overlays and predominantly 2D frames retain conservative guards.
4. Prove production OIT shader coverage shifts by the known render-pixel amount
   on D3D11 and D3D11On12. Include standard interpolation variants and the
   shared modifier/Naomi 2 variants where supported. Static exported motion
   must remain zero. Use omitted/reversed jitter and stale-mask controls.
5. Run deterministic content-bearing Soulcalibur captures on both surfaces:
   native control, active public DLAA with overlays disabled diagnostically,
   active-frame evaluate failure, and automatic HUD protection. Compare native
   PVR bytes over multiple successive retained frames, including the frame
   after jitter, a rejected evaluation, and disabling neural mode. A single
   capture taken before replay cannot prove future native-history isolation.
   Failure output must equal that same native frame and emit no stale public
   artifact; protected HUD mismatch must be zero. Exercise multipass/extent
   controls synthetically where the title does not supply them.
6. Build automation, NGX baseline, no-NGX, and feature-off configurations; run
   all available selftests and relevant fixtures. Use the established VS2022
   Developer PowerShell environment. The harness command is `selftest`, not
   `--selftest`. Retain failed build/CLI attempts rather than erasing them.
7. Commit the independently proven slice with explicit file staging. Rebuild
   the committed SHA and perform the required post-commit verification. Run
   600-frame normal/OIT D3D11On12 public-DLAA regressions with synchronous
   evidence disabled; report cadence, resource growth, timings, and actual
   jitter eligibility. If protected HUD suppresses replay throughout, add a
   separately labeled bounded active-replay performance run. Avoid unrelated
   hardware-blocked Gate 18 matrices.
8. Recapture affected leading Gate 16/17 candidates at the committed shader
   contract: public Auto, conservative external, and Uncanny cinematic. Keep
   automatic-HUD and maximum-coverage lanes labeled separately. Require exact
   color/depth/motion/mask matching, verified active settings, and existing
   candidate/marker/policy-off presentation proof. Inspect moving sequences
   plus source, temporal, trail, edge, thin-line, color, saturation, black,
   protected-pixel, and repeat/drop metrics. Do not claim a visual winner from
   a lower frame delta alone. Reuse older evidence only for unaffected claims.
9. Add a lightweight, toggleable in-game neural status overlay after the OIT
   jitter slice is proven. Keep it off by default and render it through the
   existing late Flycast OSD path, after neural scene output and protected-game
   overlay composition, so it never enters color/depth/motion/mask inputs.
   Show the resolved neural mode, quality profile, public preset, D3D11 versus
   D3D11On12 route, active/bypass/fallback state, render/output resolution,
   current jitter, FPS/frame time, and compact accepted/busy/fallback/drop
   counters. Use `unavailable` for measurements outside Flycast's timing scope.
   Provide both a Video settings toggle and the existing configuration system;
   reuse the current live-status snapshot instead of querying the external
   consumer. Verify toggling during gameplay, resize, OIT/normal rendering,
   neural off/on, and failure fallback. With the toggle off, final output must
   remain byte-identical. With it on, capture proof must show the overlay only
   in the late-OSD artifact and final presentation, never in source/public/
   external neural artifacts. Keep the text compact at 480p and scale it with
   the existing OSD policy. Do not make it evidence of external DLSS 5 output.

## Completion and boundaries

Update LOG, BACKLOG, DECISIONS, and the tester handoff with exact tested SHAs,
commands, failures, acceptance, and remaining gaps. Push proven commits to
`fork` and verify the remote SHA, as previously authorized. No broad staging,
reset, cleaning, stashing, or replacement of newer work.

Use only supplied legal media and external components. No third-party binary
inspection or modification. Temporary external text configuration changes are
authorized only for bounded tests with exact backup/hash and byte-for-byte
restoration; never commit those files. Preserve strict Gate 10 proof. Neural
Rendering stays experimental/off by default and native rendering stays fallback.

If evidence falsifies the OIT replay, keep its conservative zero-jitter route
and record the precise remaining defect. Do not promote an unproven patch.
Finish the available visual comparisons before waiting for additional legal
titles or unrelated hardware validation. Gates 16/17 remain profile/title
decisions, not a requirement to manufacture an external winner.
