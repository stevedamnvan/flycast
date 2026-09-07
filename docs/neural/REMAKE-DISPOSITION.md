# FC-067 current-input disposition and next visible milestone

## User-approved course correction -- 2026-09-07

Starting HEAD: `9fafcb873eef8945976f5c76ffaa57f9016cea8d`.
The user asked whether the project was spinning its wheels and approved
parking the small replay residual. Stop trace-53/pixel-parity investigations
unless a new change creates a concrete regression or the user reprioritizes it.
The strict gate is not relaxed, passed, or erased by this scheduling decision.

**NO-GO for a direct handoff of the current real-game packet to the current
Remix adapter.** This is a source-contract finding, not a failed Remix GPU run
and not proof that Dreamcast relighting or Remix integration is impossible.

## What is established

- Bounded actual Hoko Temple export and disk decoding exist. The moving
  decoded-buffer versus original-buffer GPU replay is exact for 30/30 frames.
- Strict original-source equality is only 29/30 in the retained exact-SHA
  moving run: frame 1804 has 13 differing pixels, maximum channel delta one.
  Both wrong viewport and wrong raw-depth controls materially fail. These
  falsify raster mappings, not an independently recovered world camera.
- The public-header adapter and analytic scene tests exercise validation,
  topology, call mapping, and failure cleanup. They do not execute a renderer.
- Production rendering, neural defaults, and external configuration are not
  changed by this disposition. Faithful remains public DLAA Auto; Uncanny
  remains an explicitly selected transformative profile.

## Why the direct handoff is not ready

| Captured or implemented fact | Consequence |
|---|---|
| `pvr_scene_capture.cpp` labels positions `pvr-projected`, camera and world transform unknown, normals unknown for Dreamcast | Native raster alignment cannot supply missing world-camera provenance |
| Packet omissions include texture pixels, lights, offscreen geometry, modifier geometry, and sorted resolve state | The saved packet is not a complete material/lighting scene; successful replay uses retained same-frame supplements |
| `ReadyForAdapter` in `neuraltest/remake_scene.cpp` requires known projection, world space, no omissions, identity transforms, normals, and untextured materials | Relabeling the real packet or filling guessed values would evade rather than satisfy the current contract |
| `RemixScene::Submit` ends with `api-submitted-not-rendered-or-presented` | A mock call sequence proves neither GPU completion nor image ownership |
| Configured `.cache/remix-m1` contains the pinned public header and licenses only; no runtime loader/readback/completion path is implemented | Supplying a runtime alone is insufficient; isolated GPU lifetime and image validation remain implementation work |

This audit does not search for or inspect proprietary binaries elsewhere on the
machine. No dependency-fetch script or external-configuration edit is authorized
by this checkpoint. Switching to RTGL1 would not itself recover missing camera,
surface, or texture data; do not begin a second renderer integration to evade
these scene-contract gaps.

## Parked diagnostic evidence

The temporary single-draw probe is preserved outside tracked source as
`build-neural-automation/fc067-single-draw-probe.patch`; raw evidence is under
the sibling `flycast-evidence/fc067-single-draw-a` through `-g` directories.
It has been removed from the production renderer. These were working-tree
diagnostics, not exact-SHA acceptance or performance runs.

- A/E capture exit 0; B/C/D/F/G exit 1 with the known 13-pixel residual.
  Immediate same-target and new-target redraws match their own original draw;
  the wrong-viewport control changes 12,671 pixels.
- C's incomplete shader-input signature yields implausible UV data and is
  rejected as UV evidence. D corrects the full signature. E/F match full-image
  UV, interpolated color, and sampled-texture values, including F's failing run.
- G's original pixel shader rendered unblended into FP32 differs between native
  and decoded contexts. Therefore a final 8-bit-store-only explanation is not
  established. Root cause remains unknown; no production arithmetic fix follows.

M2 full native-alignment acceptance remains **CORRECTIONS_REQUIRED / parked**.
M1 synthetic GPU and real-game Remix presentation remain **NOT_REVIEWABLE**.
The current-input no-go is accepted only as a feasibility/scheduling disposition.

## Next implementor task: one visible, labeled approximation

Use Astra/low in the existing checkout. Do not repeat M1, packet decoding,
transport proof, or the parked precision investigation. Complete one bounded
offline visual experiment using the already captured moving Hoko Temple
interval before investing in production integration:

1. Reuse exported indexed PVR geometry and the existing captured native frames.
   Produce a camera-relative surface approximation in a developer harness,
   with explicit user-supplied/assumed projection parameters in every artifact.
   Derive geometric face normals only where valid. Never label these recovered
   world normals or change production logarithmic depth. Reject invalid,
   degenerate, discontinuous, or unsupported surfaces conservatively.
2. Apply one controlled light to trusted visible surfaces; keep source texture
   appearance from the matched native image and explicitly call out baked
   lighting, image-projected material, missing geometry and screen-space limits.
   This is not Remix, path tracing, a texture replacement, or DLSS 5 output.
   Preserve HUD and uncertain coverage unchanged; missing trustworthy protection
   is a precise reason to limit the experiment to a visibly labeled world crop.
3. Export a native/approximation moving comparison over the same 30-frame
   interval, plus coverage/normal diagnostics and complete assumption metadata.
   Light-off must reproduce source bytes. Wrong depth/projection and inverted
   normals must fail their geometric/lighting controls. Keep rejected attempts.
4. Visually review camera motion, silhouettes, fighters, water, and baked-light
   conflicts. Deliver the animation to the user. Report whether this limited
   prototype warrants an isolated real Remix GPU harness, needs title-specific
   camera information, or is a no-go. Do not declare a perceptual winner from
   numeric scores or one still image.

Stop after that visible decision. No live consumer settings, production renderer
replacement, factory-default promotion, proprietary dependency download, or
combined neural presentation belongs to this task. Any expansion requires its
own bounded acceptance. If the inputs cannot support even this approximation,
report the exact absent field and a falsifying example instead of adding more
general-purpose diagnostic harnesses.
