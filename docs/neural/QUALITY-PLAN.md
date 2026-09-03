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
- Gate 15A: green at LOG #70-#71. Normal DX11 translucent lists replay only into
  current-color bias with depth disabled. The OIT final visible fragment stack
  emits separate R8 reactive coverage, merged without creating depth or trusted
  motion. The exact production resolve passes empty/modifier-only, single-layer,
  and multi-layer controls on native D3D11 and D3D11On12 with byte-identical
  masks; omitted coverage fails. Punch-through remains in the proven physical
  depth path and framebuffer-direct rendering retains native fallback. LOG #71
  records and corrects an RT1-versus-`SV_Target0` merge-slot bug and adds a
  direct base-mask-union regression on both surfaces.
- Gate 15B: green at LOG #71 for the production mechanism and conservative
  default classifier. Protected pixels are restored from original PVR color
  after neural scene presentation but before Flycast OSD/ImGui; the exact GPU
  fixture restores 33/33 pixels byte-for-byte and changes 0 unclassified
  pixels on native D3D11 and D3D11On12. Strict world-geometry negatives, a
  per-game full-frame protection override, framebuffer-direct native fallback,
  and a three-frame-latched 2D/menu bypass are covered. Representative-title
  visual acceptance remains Gate 17 and is not inferred from this gate.
- Gate 16: partial at LOG #72. The production match-output option and public
  Auto/J/K selector are implemented. A 2560x1440 Soulcalibur fullscreen run
  rasterized 4:3 content at exactly 1920x1440; the manual 2x Quality-SR lane
  remained 1280x960 into 1920x1440. Auto and K were pixel-identical over the
  240-frame synthetic fixture on D3D11/D3D12, while J was observably distinct,
  so Auto remains the default. Lanes A-E, moving title comparisons, and external
  consumer quality evidence remain open; Gate 16 is not green.
- Q5 profiles: partial at LOG #73. Faithful Dreamcast Remaster is the default;
  Enhanced Materials and explicitly non-faithful Photoreal Experimental are
  selectable; style families, user-controlled external recommendations, and an
  explicit sprite-heavy generative bypass are visible in UI/capture metadata.
  Per-title tuning and evidence-driven broader trust remain open.
- Gate 17: partial at LOG #73. The bounded capture CLI writes the production
  source, complete guidance set, public output when present, final composite,
  differences/flicker, manifest, and component metrics on normal DX11, DX11
  OIT, and D3D11On12. Soulcalibur intro frames and native/no-NGX controls are
  covered. LOG #74 adds a relative-path HTML/JSON comparison index which
  explicitly refuses to declare a still-frame winner. GPU timings, external-
  output capture, moving gameplay, all profile lanes, and every other legally available title
  remain open, so no title-quality winner is declared.
- Gate 18: partial at LOG #75 and LOG #77. Asynchronous production D3D11 timestamp queries,
  Present-call intervals, stage counters, ring pressure, and post-warmup VRAM
  growth are now available without synchronous capture. Initial Soulcalibur
  native, normal-DLAA, OIT-DLAA, and D3D11On12 intervals are bounded and clean;
  D3D12-queue evaluation is honestly unavailable rather than inferred. The full
  transition/failure matrix, external timing, longer runs, resource-object
  counts, latency, resize/fullscreen/device-removal cases, and title coverage
  remain open. LOG #76 adds bounded injected feature-create, evaluate,
  output-ring/delayed-fence-busy, and synthetic device-removed-status coverage
  on native D3D11 and D3D11On12. Rejected frames after accepted output fall
  back to byte-identical source color rather than stale public output; the
  recoverable controls resume after one hold/reset and removed status remains
  latched. Real device removal and the rest of the transition matrix remain
  open. LOG #77 adds per-sample source, accepted, and displayed frame identity
  at the actual Present boundary. Sixty-frame native-D3D11 and D3D11On12 DLAA
  intervals each showed 60 accepted/60 neural presents, zero missing or
  accepted-but-unpresented frames, zero identity errors, repeats, source gaps,
  native/neural alternations, and zero-frame latency. A mid-window injected
  evaluation-failure control detected the expected neural-to-native transition
  without stale output, while an experimental DLSS 5 run without an external
  consumer counted all 30 accepted public candidates as not presented and all
  30 final frames as native. A no-NGX build likewise recorded 30 native
  presents with zero accepted or neural frames and no cadence/identity error.
  Longer runs, active runtime removal, real device
  loss, and the remaining transition matrix are still open, so Gate 18 is not
  closed.
  LOG #78 adds an OS-observed resize/minimize/restore/resize-back sequence
  delayed five seconds into active rendering. Normal and OIT D3D11 and
  D3D11On12 each completed 600 measured Soulcalibur frames and clean shutdown.
  All four runs had zero missing Presents, accepted-output drops, identity
  mismatches, output repeats, or frame latency; each recorded the single
  source-ID gap at the minimize boundary. Normal D3D11 conservatively used 35
  native frames and one explicit native/neural transition while its public
  feature/resources recovered; the other three lanes presented neural output
  for all 600 samples. Fullscreen, monitor move, alt-tab, renderer restart,
  load/unload, real device loss, and long-run coverage remain open.
  LOG #79 adds paired 10000-sample normal-renderer soaks at committed
  `b3399f96c`. Native D3D11 and D3D11On12 each presented 10000/10000 accepted
  neural frames with zero missing, dropped, repeated, gapped, mismatched, or
  alternating frames, zero measured frame latency, zero query-ring pressure,
  and clean close. D3D11 local VRAM moved from 306855936 to 303792128 bytes
  (-3063808); D3D11On12 remained exactly 391602176 bytes. This closes the
  bounded normal-renderer long-run growth/cadence check, not OIT long-run,
  resource-object accounting, external-consumer timing, or the remaining
  transition/failure matrix.
  LOG #80 adds exact-frame renderer/API-context teardown and recreation on
  normal DX11 and DX11 OIT across native D3D11 and D3D11On12. Each replacement
  renderer completed a fresh 60-frame warmup and 600 measured frames with
  600/600 neural Presents, zero native fallback, missing or accepted-but-not-
  presented frames, identity errors, repeats, latency, or query-ring pressure,
  and clean close. Three runs had flat measured-window VRAM; native-D3D11 OIT
  decreased by 3612672 bytes. Renderer/API switching, fullscreen, game
  load/unload, active runtime removal, and real device loss remain open.
  LOG #81 adds real normal-DX11 to DX11-OIT and DX11-OIT to normal-DX11
  switches on both native D3D11 and D3D11On12. Every destination renderer
  completed a fresh 60-frame warmup and 600 measured frames with 600/600 neural
  Presents, no native Presents, missing/accepted-output drops, identity errors,
  repeats, latency, or query-ring pressure, and clean close. The destination
  renderer identity matched every switch marker and report. Three measured
  windows had flat VRAM and normal-D3D11 to OIT released 37703680 bytes. The
  D3D11/D3D11On12 surface switch and other listed Gate 18 gaps remain open.
  LOG #82 adds real native-D3D11 to D3D11On12 and D3D11On12 to native-D3D11
  surface switches while retaining normal DX11 or DX11 OIT. Every replacement
  tracker identified the requested destination API and completed a fresh
  60-frame warmup plus 600 measured frames with 600/600 neural Presents, zero
  native Presents, missing/accepted-output drops, identity errors, repeats,
  latency, fallback, or query-ring pressure, and clean close. Both normal-DX11
  windows had flat VRAM; D3D11-to-On12 OIT grew by 131072 bytes and On12-to-
  D3D11 OIT by 3334144 bytes, so longer OIT/resource-object evidence remains
  required.
  LOG #83 adds paired 10000-sample OIT soaks after 600 warmup frames. Native
  D3D11 and D3D11On12 each presented 10000/10000 accepted neural frames with
  zero native, missing, dropped, repeated, gapped, mismatched, alternating, or
  latent frames, zero query-ring pressure, and clean close. Native D3D11 OIT
  local VRAM grew by a bounded 647168 bytes; D3D11On12 OIT remained exactly
  flat. This closes the bounded OIT long-run cadence check, not resource-object
  accounting, external-consumer timing, or broader-title stability.
  LOG #84 adds Flycast-native F11 borderless-desktop-fullscreen enter/exit on
  normal DX11 and DX11 OIT across native D3D11 and D3D11On12. All four runs
  positively observed monitor-sized fullscreen, windowed exit, and exact
  original-rectangle restoration, then completed 600 measured neural Presents
  with zero native Presents, missing/accepted-output drops, identity errors,
  output repeats, alternation, latency, or query-ring pressure and clean close.
  Each retained one source-frame gap at the mode boundary. Flycast does not
  expose exclusive fullscreen in this path, so no exclusive claim is made.
  LOG #85 adds real same-media `Emulator::unloadGame`/`loadGame`/`start`
  transitions at main frame 300 on normal DX11 and DX11 OIT across native D3D11
  and D3D11On12. Every run observed cleared content state and the same game ID
  and media identity after reload, then completed 600 measured neural Presents
  with zero native Presents, missing/accepted-output drops, identity errors,
  output repeats, alternation, latency, or query-ring pressure and clean close.
  Three source-ID gaps and explicit resets expose the lifecycle discontinuity.
  Cross-title loading and save-state runtime validation remain open.
