# Neural rendering quality plan

## Objective and claim boundary

The quality target is a **Faithful Dreamcast Remaster**: correct PVR scene
color, renderer-authentic depth, geometry-derived motion, conservative history
protection, exact target-resolution DLAA, optional externally supplied Neural
Rendering, protected game overlays, then Flycast OSD/ImGui and Present.

The D3D11On12 plus user-supplied RenoDX route has experimental transport and
provenance evidence (FC-056/FC-066, Gate 10). That result does not prove temporal
quality, title suitability, stability, or performance. Native rendering remains
the fallback; neural modes remain experimental and off by default; public NGX
DLAA/SR remains independently supported. The synchronous evidence path is never
used for performance measurement.

## Execution order

1. **Q1 / FC-024, FC-032, FC-035, FC-036, FC-044, FC-060, FC-061:** prove the
   public guidance contract with deterministic GPU fixtures before production
   motion controls the shader.
2. **Q2 / FC-025, FC-031 through FC-036:** separate structural identity from
   pose, add resource generations, deterministic minimum-cost assignment,
   previous-position streams, and evidence-based confidence.
3. **Q3 / FC-033, FC-034, FC-055:** add depth/identity disocclusion checks,
   conservative sorted/OIT translucency coverage, and protected in-game
   overlays. Modifier volumes never become neural depth.
4. **Q4 / FC-044, FC-053:** add exact content-rectangle raster sizing and
   compare target-native DLAA, lower-resolution SR, 8x downsample, DLAA-only,
   and native lanes. Public preset Auto/J/K is evaluated separately from the
   external consumer.
5. **Q5 / FC-050, FC-055, FC-065:** add faithful, enhanced-materials, and
   photoreal-experimental profiles. Recommendations are displayed and captured;
   Flycast does not write third-party settings.
6. **Q6 / FC-054, FC-065:** implement bounded captures, component metrics, and
   moving title sequences. A title that does not win defaults to a safer profile
   or bypass.
7. **Q7 / FC-045, FC-063, FC-064:** exercise transitions, injected failures,
   latency, frame pacing, VRAM, and long-run growth with synchronous evidence
   disabled.

## Gates

| Gate | Existing work | Acceptance |
|---|---|---|
| 11 | FC-024, FC-044, FC-060, FC-061 | Production-shader R32 samples prove near direction, clear/no-geometry, OP/PT agreement, submission-order stability, native D3D11/D3D11On12 parity, and the deliberately wrong depth polarity fails. NGX flag A/B is required before changing feature flags. |
| 12 | FC-032, FC-035, FC-036, FC-044 | Static motion is exactly zero; analytic translations, camera motion, deformation, and jitter separation pass; reversed sign and doubled scale fail. |
| 13 | FC-025, FC-031, FC-033, FC-036 | Repeated objects, reordered draws, changed texture/palette/RTT generations, topology changes, and particle ambiguity cannot produce confident false motion. |
| 14 | FC-033, FC-034, FC-036 | Newly revealed/background/crossing/scene-cut pixels bias current color; a wrong-disocclusion control measurably fails. |
| 15A | FC-025, FC-055 | Translucency/OIT/modifier-volume tests introduce no false physical depth, persistent trail, or opaque particle silhouette. |
| 15B | FC-050, FC-055, FC-065 | Protected game HUD/text is byte-identical after composition, and the default classifier removes no world geometry. |
| 16 | FC-044, FC-053, FC-065 | The selected resolution/preset lane matches or beats public DLAA temporal stability while preserving more source identity/detail than alternatives. |
| 17 | FC-054, FC-065 | Moving representative-title evidence shows the default preserves style, identity, text, silhouette, and color; losing titles use a safer default or bypass. |
| 18 | FC-045, FC-063, FC-064 | Transition/failure/performance matrix has no crash, device loss, stale output, unbounded wait, leak, frame-identity error, or silent native/neural alternation. |

## Guidance contract rules

- Preserve the renderer's logarithmic PVR depth unless controlled evidence
  proves a better representation. Set `DepthInverted` only if Gate 11 proves it.
- Motion is current-to-previous in render-pixel units. Current and previous
  positions are unjittered; current raster jitter is reported separately.
- Keep `InMVScaleX/Y = 1` only after Gate 12 proves one stored unit equals one
  render pixel. `MVLowRes` is SR-only unless evidence requires otherwise;
  `MVJittered` remains off for unjittered motion.
- SDR uses `InPreExposure = 1`, `InExposureScale = 1`, and no HDR flag after
  the color/exposure chart proves that description.
- The public `BiasCurrentColorMask` protects temporal reconstruction. It is not
  treated as a private semantic art-direction mask.
- The neural rectangle is the exact content rectangle and excludes black bars.

## Capture and evidence policy

Every acceptance record names the Git SHA, renderer/API, fixture or game ID,
profile, frame/history/reset IDs, dimensions/content rectangle, guidance hashes,
output hashes, numeric thresholds, command, and exit code. Failed and falsifying
controls stay in `LOG.md`. Legal game media and user-supplied components stay
outside Git; captures never record user paths. No still-frame-only result may
close a temporal or title-quality gate.

## Current state at quality rebaseline

- Starting SHA: `c0e0d0f0dcda9e8328ccfd1235972a4a424ebcea`.
- Transport/provenance: proven experimentally for the named D3D11On12 route.
- Gates 11 through 18: pending at rebaseline.
- Available legal media: Soulcalibur CHD outside the repository; no claim is
  made for other titles until media is available and the capture lane exists.

## Progress

- Gate 11: green at LOG #61. Production-shader ordering and clear semantics are
  exact across native D3D11 and D3D11On12; the wrong-polarity control fails;
  public NGX D3D11/D3D12 create and evaluate both polarity declarations. The
  static NGX chart is polarity-invariant, which is recorded as a limitation.
- Gate 12: green at LOG #62 for the synthetic guidance contract. RG16F motion
  matches analytic static/translation/camera/deformation truth; jitter-only is
  zero; pixel reprojection and public DLAA both reject reversed/doubled motion.
  LOG #67 additionally proves the real production PVR vertex/pixel shader pair
  emits `[-4,+3]` render-pixel motion identically on native D3D11 and
  D3D11On12. Invalid and over-limit controls emit zero motion/confidence and
  full current-color bias.
- Q1 SDR/color/rectangle contract: green at LOG #63. The production quad path
  is byte-exact for the deterministic chart; public DLAA is exact in 214,320
  constant-interior RGB/alpha samples and byte-identical across D3D11/D3D12.
  Content-rectangle examples and odd-size centering are exact. Black-border
  exclusion still depends on the production target-resolution work in FC-053.
- Gate 13/Q2 correspondence: green at LOG #68 for normal Dreamcast geometry.
  Structural identity is pose-independent; exact and compatible repeated
  buckets use minimum-cost one-to-one assignment with best/second cost;
  texture/palette/RTT revisions, large/reactive particle buckets, rejected
  reindex fits, shared-vertex conflicts, excessive motion, stale accepted
  history, and scene cuts cannot produce trusted production motion. Rigid
  reindex fits are bounded by scale and 0.25-pixel RMS residual. Naomi 2 remains
  deliberately invalid pending accepted matrix history and does not weaken the
  normal Dreamcast gate. Pixel disocclusion remains Gate 14.
- Gate 14: green at LOG #69. The production post-pass uses only the last
  accepted depth/draw-ID ring slot, expected accepted identity, and
  current-to-previous motion. Static, depth-tolerant, and camera-pan regions
  remain trusted; outside, depth mismatch, crossing, newly visible, reveal, and
  scene-cut regions are protected. Native D3D11/D3D11On12 masks are exact; the
  omitted-pass control misses 192 pixels and raises trail energy from 0 to
  12,288.
- FC-032 previous-position stream: partial at LOG #66-#68. The bounded CPU
  stream is owned by last accepted history, maps exact topology by strip/index
  position, supports deformation, accepts only bounded rigid reindex fits, and
  rejects non-rigid residuals and shared-vertex conflicts. It is bound to a
  dedicated DX11 input layout and drives the normal and OIT guidance replay.
  Naomi 2 transform history is still required and is
  deliberately validity zero rather than using current matrices on prior pose.
- Gate 15A through Gate 18: pending.
