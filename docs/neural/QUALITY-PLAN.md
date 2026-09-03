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

