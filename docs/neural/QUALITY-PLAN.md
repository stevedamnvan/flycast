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
5. **Q5 / FC-050, FC-055, FC-065:** retain the completed live status and
   presentation-only guidance debug UI; add faithful, enhanced-materials, and
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
- Gate 12: green at LOG #122. LOG #62 established the synthetic guidance
  contract: RG16F motion
  matches analytic static/translation/camera/deformation truth; jitter-only is
  zero; pixel reprojection and public DLAA both reject reversed/doubled motion.
  LOG #67 additionally proves the real production PVR vertex/pixel shader pair
  emits `[-4,+3]` render-pixel motion identically on native D3D11 and
  D3D11On12. Invalid and over-limit controls emit zero motion/confidence and
  full current-color bias. LOG #117 adds the production Naomi 2 permutation:
  accepted exact topology and matrices emit the same analytic `[-4,+3]` on
  both surfaces, while missing history fails closed. LOG #122 proves bounded
  Halton jitter on content-bearing normal-DX11 production frames while the
  unjittered native framebuffer remains byte-identical. An active injected
  failure returns exact native output with no stale public artifact; automatic
  HUD protection remains zero-jitter and byte-exact; OIT remains explicitly
  zero-jitter pending its separate scene replay. LOG #123 extends the same
  public-DLAA contract to DLSS5 experimental and indexes phase by accepted
  neural-history count, eliminating policy-on/off startup-phase drift.
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
  reindex fits are bounded by scale and 0.25-pixel RMS residual. LOG #117 adds
  accepted-matrix, exact-topology Naomi 2 correspondence and keeps reindexed or
  missing-matrix draws deliberately invalid. Pixel disocclusion remains Gate 14.
  LOG #94 adds a real deterministic Soulcalibur combat control: it exposed and
  corrected rejection of `0xFFFFFFFF` primitive-restart indices during the
  final previous-position validity check. The fixed 30-frame target-DLAA run
  retained valid history on 29 frames, averaged 82.81% trusted pixels, and
  used one conservative reset on a genuine topology transition. The prior
  all-reactive run and the failed candidate/validated area diagnostics are
  retained rather than counted as passing evidence.
- Gate 14: green at LOG #69. The production post-pass uses only the last
  accepted depth/draw-ID ring slot, expected accepted identity, and
  current-to-previous motion. Static, depth-tolerant, and camera-pan regions
  remain trusted; outside, depth mismatch, crossing, newly visible, reveal, and
  scene-cut regions are protected. Native D3D11/D3D11On12 masks are exact; the
  omitted-pass control misses 192 pixels and raises trail energy from 0 to
  12,288.
- FC-032 previous-position stream: partial through LOG #117. The bounded CPU
  stream is owned by last accepted history, maps exact topology by strip/index
  position, supports deformation, accepts only bounded rigid reindex fits, and
  rejects non-rigid residuals and shared-vertex conflicts. It is bound to a
  dedicated DX11 input layout and drives the normal and OIT guidance replay.
  Exact-topology Naomi 2 now carries accepted model/projection history through
  the production HLSL permutation; missing history and reindexing remain
  validity zero rather than using current matrices on prior pose.
- Gate 15A: partial through LOG #105. The synthetic
  LOG #70-#71 controls remain green and actual sorted-submission indexing now
  participates in the bounded Soulcalibur normal/OIT capture path.
  Normal DX11 translucent lists replay only into
  current-color bias with depth disabled. The OIT final visible fragment stack
  emits separate R8 reactive coverage, merged without creating depth or trusted
  motion. The exact production resolve passes empty/modifier-only, single-layer,
  and multi-layer controls on native D3D11 and D3D11On12 with byte-identical
  masks; omitted coverage fails. Punch-through remains in the proven physical
  depth path and framebuffer-direct rendering retains native fallback. LOG #71
  records and corrects an RT1-versus-`SV_Target0` merge-slot bug and adds a
  direct base-mask-union regression on both surfaces.
- Gate 15B: partial through LOG #105. The LOG #71 synthetic production-composite
  mechanism remains green. Protected pixels are restored from original PVR color
  after neural scene presentation but before Flycast OSD/ImGui; the exact GPU
  fixture restores 33/33 pixels byte-for-byte and changes 0 unclassified
  pixels on native D3D11 and D3D11On12. Strict world-geometry negatives, a
  per-game full-frame protection override, framebuffer-direct native fallback,
  and a three-frame-latched 2D/menu bypass are covered. Representative-title
  visual acceptance remains Gate 17 and is not inferred from this gate.
  The earlier Soulcalibur zero-mismatch reports had zero protected pixels and
  did not prove HUD coverage. LOG #105 closes that named bounded defect with
  topology-proven batches, PVR-native coordinate classification, and an exact
  `T1401N` profile: 30 moving normal-DX11 and 30 moving-OIT frames protect text,
  bars, timer, counters, and both character names with zero byte mismatches and
  zero mask pixels at or below y=100. Native D3D11 and D3D11On12 short-sequence
  buffers are exact per renderer. Gate 15B remains partial until representative
  titles and uncertain-overlay controls establish that profiles neither miss
  other HUDs nor remove world geometry. This does not reopen Gate 10.
- Legacy integration Gate 8 is green at LOG #109. Exact same-frame production
  captures on normal DX11 and DX11 OIT, each on native D3D11 and D3D11On12,
  retain the neural/game-overlay composite before Flycast UI and then capture
  the ImGui-rendered FPS OSD afterward. All eight positive frames contain a
  nonzero late-only pixel delta; the no-OSD control contains zero delta and is
  rejected. This proves ordering and buffer exclusion, not broader game-HUD
  classification or title quality.
- Gate 16: partial through LOG #118. The production match-output option and public
  Auto/J/K selector are implemented. A 2560x1440 Soulcalibur fullscreen run
  rasterized 4:3 content at exactly 1920x1440; the manual 2x Quality-SR lane
  remained 1280x960 into 1920x1440. Auto and K were pixel-identical over the
  240-frame synthetic fixture on D3D11/D3D12, while J was observably distinct,
  so Auto remains the default. A pixel-aligned 30-frame Soulcalibur combat
  matrix now shows target-native Auto at average temporal variance 9.67 versus
  native 31.93, with zero repeats, drops, or HUD mismatches. J reached 9.51 but
  increased trail, silhouette, saturation, black-level, and thin-line error;
  K materially matched Auto and did not justify promotion. Public NGX rejected
  the legacy 426x320 Quality-SR raster while requesting 427x320; the corrected
  exact-width path submitted 427x320 into 640x480 successfully without changing
  ordinary manual even-width modes. A current-SHA ten-frame 5120x3840 8x-native
  reference completed with exact 640x480 output, nine valid-history frames, one
  conservative scene cut, zero repeats/drops, and 77.07% average trusted pixels.
  It differs materially from target-native Auto (`PSNR 33.02` at frame 1802), so
  it remains a reference rather than the default. A current-SHA 30-frame
  external-versus-public comparison joins frames by exact color/depth/motion/
  mask hashes, not nominal frame numbers. The supplied external setting lowered
  raw temporal delta from 6.098338 to 5.852924, but worsened source PSNR from
  30.219362 to 24.391432, gradient MAE from 3.637432 to 4.404373, edge recall
  from 92.029170% to 89.370230%, color drift from 0.233438 to 3.547196, and
  saturation drift from 3.334620 to 11.999162. It therefore does not displace
  public Auto as the Faithful baseline. That earlier comparison recorded only
  Flycast's recommendation. The supplied consumer's current host log reports
  upscaling off, intensity 1, global tone 1, diffuse white 203 nits, preset 0,
  style 0, and enabled on. LOG #106 adds fail-closed capture binding for this
  tuple. LOG #107 then widened and rebuilt the late ON/OFF evidence window,
  confirmed 30/30 fresh post-fix frames, and exact-input paired them to public
  Auto. External/public raw temporal RGB MAE was 2.752928/2.774712, but
  source PSNR was 25.677898/38.543206 dB and source RGB MAE was
  10.256009/0.932040. External also lost the trail, edge, thin-line, color,
  saturation, and black-level components. The interval contained no protected
  HUD pixels, so it is settings/source evidence rather than a second real-HUD
  proof. LOG #118 then performs the authorized, fail-closed external settings
  sweep on the same exact-input interval. Seven nonzero tuples are accepted:
  intensity 1, 0.5, 0.25, and 0.125; global tone 1/0; Default/Natural style;
  and external preset 0/2 where applicable. Global tone 0 is byte-identical to
  tone 1 on this SDR interval, and preset 2 is byte-identical to preset 0 while
  upscaling is off. Natural plus intensity 0.125 is the least damaging accepted
  external result, but still records trail 2.08 versus 1.45, edge displacement
  0.65 versus 0.60, thin-line continuity 82.69 versus 83.95, color drift 0.82
  versus 0.62, saturation drift 2.11 versus 1.51, black drift 1.39 versus 1.26,
  and raw temporal RGB MAE 2.7836 versus 2.7747 for public Auto. Moving review
  shows a barely visible effect with softer hair/face detail and no clear
  material benefit. Intensity 0 correctly fails external provenance because it
  cannot be distinguished from policy-OFF. Public Auto therefore remains the
  Faithful baseline; broader legal titles, completed raster jitter, and any
  Enhanced/Photoreal winner remain open, so Gate 16 is not green.
- Q5 profiles: partial at LOG #73. Faithful Dreamcast Remaster is the factory
  default; Enhanced Materials, explicitly non-faithful Photoreal Experimental,
  and explicitly non-faithful Uncanny Cinematic are
  selectable; style families, user-controlled external recommendations, and an
  explicit sprite-heavy generative bypass are visible in UI/capture metadata.
  LOG #119 captures the Photoreal profile at consumer-reported intensity 1.5,
  global tone 1.5, preset 2, Default style. It visibly changes face, skin,
  lighting, hair mass, and color, but exact-input comparison proves all 30
  outputs byte-identical to the intensity-1/tone-0/preset-0 Default-style run.
  The available Default-style path therefore saturates at intensity 1 for this
  interval; the stronger numbers do not create a stronger image. LOG #120 then
  exercises the requested uncanny lane: Cinematic style, intensity 2,
  global/local tone 0.75, local structure 2, automatic mask off, and UI
  correction on. Cinematic intensity 2 is byte-identical to intensity 1 when
  structure remains absent, but an isolation A/B changing only
  `NRLocalStructure=2.0` changes 30/30 exact-input outputs. Structure is
  functionally active even though the host log omits it; captures label the
  field requested plus isolated-output-proven, not consumer-reported. The result
  deliberately leaves the Faithful envelope and remains Photoreal Experimental.
  LOG #124 repeats the leading conservative and Uncanny lanes after production
  jitter on exact commit `c11ba8312`. All three candidate-default experiments
  pass 30/30 exact-input provenance. Conservative still loses public Auto on
  Faithful constraints. Uncanny is valid as a user-selected transformative
  profile; its automatic-HUD lane protects an average 15,313 pixels with zero
  mismatch/repeat/drop, but its trail, edge, thin-line, saturation, and black-
  level results prohibit promotion to the Faithful factory default.
  Per-title tuning and broader trust remain open. LOG #121 makes that exact
  uncanny tuple a persistent user-selectable Flycast profile and capture-CLI
  value. It is eligible to be the user's chosen default, but is not promoted to
  the factory/automatic default without representative Gate 17 evidence.
- Gate 17: partial through LOG #103. The bounded capture CLI writes the production
  source, complete guidance set, public output when present, final composite,
  differences/flicker, manifest, and component metrics on normal DX11, DX11
  OIT, and D3D11On12. Soulcalibur intro frames and native/no-NGX controls are
  covered. LOG #74 adds a relative-path HTML/JSON comparison index which
  explicitly refuses to declare a still-frame winner. LOG #93 first confirmed
  two moving frames. LOG #99 adds capture-only base-PVR, guidance,
  accepted evaluation, overlay/presentation-blit, and overall-frame GPU spans
  with exact D3D12 frame-ID retirement where available. External profile lanes
  and every other legally available title remain open, so no title-quality
  winner is declared. LOG #94 adds a repeatable real combat
  sequence and makes the developer-only launcher retain the exact input script,
  hash, and byte count. Two independent frame-1802 native scouts were
  pixel-identical and all four initial native/Auto/J/K sequences had identical
  source frames before output comparisons were accepted. LOG #95 adds a current
  8x combat reference and promotes one frame of active Kilik-versus-Taki combat
  only after exact five-hash ON evidence, exact four-input policy-OFF evidence,
  1024/1024 same-frame sentinel pixels, and completed Present. A 671-package
  comparison index retains every accepted and rejected attempt. LOG #100 then
  confirms 60 consecutive restored-presentation external frames with 60 unique
  outputs, zero rejects, zero HUD mismatches, 48 valid-history frames, and 12
  conservative scene cuts. LOG #101 retains 30 exact-input public outputs under
  policy OFF, proves every final composite byte-identical to native, and creates
  an accurate moving side-by-side comparison. That comparison rejects the
  supplied external setting as the Faithful winner on source identity despite
  slightly lower raw temporal delta. Other legally available Dreamcast titles
  remain unavailable, so Gate 17 and the representative title matrix remain open.
  LOG #103 replaces the ad-hoc comparison with a bounded command that verifies
  every saved input/output hash and actual input-byte equality, rejects ambiguous
  matches and chronology gaps, and writes separate RGB/alpha/temporal metrics.
  It reproduces all 30 pairs; raw temporal change is explicitly not a stability
  score because it includes object motion and cuts. Actual depth and PNG mutation
  controls reject without creating a report; both selftests now pass 125/125.
- Gate 18: partial through LOG #110. Asynchronous production D3D11 timestamp queries,
  Present-call intervals, stage counters, ring pressure, and post-warmup VRAM
  growth are now available without synchronous capture. Initial Soulcalibur
  native, normal-DLAA, OIT-DLAA, and D3D11On12 intervals are bounded and clean;
  LOG #98 adds accepted-frame-filtered asynchronous D3D12 queue timestamps for
  public and intercepted evaluation without waits or per-frame misassociation.
  LOG #102 confirms the post-restoration normal/OIT public-DLAA regressions each
  present 600/600 frames with no cadence or identity fault, zero owned-object
  growth (127 including two optional timing resources), and clean close. All
  synchronous evidence remains off in those measurements.
  LOG #110 keeps one continuous 600-sample interval across a live exact-frame
  neural-mode round trip on normal DX11 and DX11 OIT over native D3D11 and
  D3D11On12. Every case records mode 2 -> 0 at main frame 300 -> mode 2 at 360,
  60/60 native mode-zero Presents, zero off-mode evaluations, and a reset on
  the first accepted re-entry. All four have zero missing Presents, accepted-
  output loss, identity mismatch, output repeat, frame latency, query-ring
  pressure, or Flycast-owned object growth, and close cleanly. Native D3D11
  normal/OIT each retain one explicit nonblocking native fallback while mode 2
  is active; both On12 cases return 540/540 requested-mode neural Presents.
  LOG #114 adds a real Windows foreground-focus loss/restore cycle between the
  launched Flycast window and a visible top-level control window. Public DLAA
  normal/OIT on native D3D11 and D3D11On12 each accepted and presented 600/600
  outputs with zero missing or accepted-but-unpresented frames, identity
  mismatch, source/output repeat, native/neural alternation, frame latency, or
  Flycast-owned object growth, and every process closed cleanly. Each run
  retains the single source-frame gap and reset at the focus boundary. The
  harness does not synthesize the Alt+Tab keyboard gesture, but the foreground
  ownership lifecycle relevant to that boundary is now proven on the four
  normal-Dreamcast production paths.
  LOG #115 exercises the production D3D11 and D3D12 NGX evaluate SEH leaves
  with three application-defined software exceptions after warmup. Native-
  D3D11 normal and D3D11On12 OIT each record exact exception `0xE0424E47`,
  three evaluation failures, one bounded hold, 62 native Presents, and recovery
  to 538 neural Presents. Both have zero missing or accepted-but-unpresented
  frames, identity mismatch, source/output repeat, latency, query pressure, or
  object growth and close cleanly. This closes the Flycast-owned SEH boundary,
  not a fault originating inside a supplied runtime. LOG #116 separately
  exercises controlled actual D3D12 device removal.
  At exact main frame 300, normal and OIT D3D11On12 runs each call
  `ID3D12Device5::RemoveDevice`, observe `DXGI_ERROR_DEVICE_REMOVED`, rebuild a
  D3D11On12 context, and start a fresh post-recovery sampling interval. Each
  fresh interval accepts and presents 600/600 neural frames with zero native,
  missing, accepted-but-unpresented, identity, source/output repeat, source-gap,
  alternation, latency, query-ring-pressure, or Flycast-owned object-growth
  counts and closes cleanly. This proves controlled removal/reconstruction of
  Flycast's actual process device, not a spontaneous driver reset or TDR.
  The full transition/failure matrix, isolated external timing, longer runs,
  resource-object
  counts, latency, remaining transition cases, and title coverage
  remain open. LOG #76 adds bounded injected feature-create, evaluate,
  output-ring/delayed-fence-busy, and synthetic device-removed-status coverage
  on native D3D11 and D3D11On12. Rejected frames after accepted output fall
  back to byte-identical source color rather than stale public output; the
  recoverable controls resume after one hold/reset and removed status remains
  latched. LOG #116 supersedes the earlier actual-removal gap with controlled
  device removal on normal/OIT D3D11On12; spontaneous driver/TDR coverage and
  the rest of the transition matrix remain open. LOG #77 adds per-sample source,
  accepted, and displayed frame identity
  at the actual Present boundary. Sixty-frame native-D3D11 and D3D11On12 DLAA
  intervals each showed 60 accepted/60 neural presents, zero missing or
  accepted-but-unpresented frames, zero identity errors, repeats, source gaps,
  native/neural alternations, and zero-frame latency. A mid-window injected
  evaluation-failure control detected the expected neural-to-native transition
  without stale output, while an experimental DLSS 5 run without an external
  consumer counted all 30 accepted public candidates as not presented and all
  30 final frames as native. A no-NGX build likewise recorded 30 native
  presents with zero accepted or neural frames and no cadence/identity error.
  Longer runs, physical loaded-runtime removal, spontaneous driver/TDR
  evidence, and the remaining transition matrix are still open, so Gate 18 is not
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
  load/unload, controlled device removal, and long-run coverage were open at
  that point; LOG #116 now closes the controlled-removal item.
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
  load/unload, active runtime removal, and controlled device removal were open
  at that point; LOG #116 now closes the controlled-removal item.
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
  LOG #86 adds an exact-frame, cross-platform in-memory save/load round trip on
  normal DX11 and DX11 OIT across native D3D11 and D3D11On12. All four cases
  restored the same 27797870-byte production state at main frames 200 and 230,
  completed 600 measured Presents with zero missing or accepted-output drops,
  identity errors, output repeats, or frame latency, and retained one source
  gap plus explicit reset counts. Native-D3D11 OIT conservatively presented one
  fresh native frame at the load boundary in both repeated runs and recorded
  both path transitions; the other cases presented 600/600 neural frames.
  Cross-title loading, monitor move, active runtime removal,
  controlled device removal, external timing, and broader-title coverage were
  open at that point; LOG #116 now closes the controlled-removal item.
  LOG #87 adds an exact-frame `gui_togglePause`/resume round trip on normal DX11
  and DX11 OIT across native D3D11 and D3D11On12. Every marker observed
  `GuiState::Pause` at frame 200 and `GuiState::Closed` at frame 260, and every
  case completed 600 measured Presents with zero missing/accepted-output drops,
  identity errors, source or output repeats, or frame latency. Each retained one
  source-frame gap. Native-D3D11 OIT used one explicit fresh native fallback;
  the other cases presented 600/600 neural frames. Process-owned foreground
  transfer was also attempted but Windows rejected focus ownership in that
  non-interactive desktop. LOG #114 supersedes that attempt with observed
  foreground ownership in the current interactive desktop.
  LOG #88 adds per-sample Flycast-owned neural GPU-object accounting and four
  fresh 10000-sample Soulcalibur soaks across normal/OIT native D3D11 and
  D3D11On12. Native D3D11 remained exactly 91 objects (82 renderer, 9 backend)
  and D3D11On12 exactly 125 (115 renderer, 10 backend): initial, minimum,
  maximum, and final were identical in every run. All 40000 accepted outputs
  were presented with zero native fallback, missing/accepted-output drops,
  identity errors, repeats, source gaps, alternation, latency, or query-ring
  pressure; local VRAM was non-increasing. The host exposes only one monitor,
  and media discovery found only the supplied Soulcalibur Dreamcast image, so
  monitor-move and cross-title evidence remain blocked rather than inferred.
  LOG #89 adds a default-off active-runtime-unavailable control that waits
  nonblockingly for submitted work, retires the live public-NGX session and
  backend objects, clears output, and latches native fallback. Four 120-sample
  mid-window Soulcalibur runs across normal/OIT D3D11 and D3D11On12 each
  recorded one terminal status and one explicit neural-to-native transition,
  with zero missing/accepted-output drops, identity errors, output repeats, or
  latency. D3D11 released 9 backend objects and D3D11On12 released 10. This is
  controlled active-unavailability coverage; physical loaded-DLL removal and
  controlled device removal were separate gaps there. LOG #116 now closes the
  controlled-removal gap, while spontaneous driver/TDR evidence remains unavailable.
  LOG #90 adds current-SHA Soulcalibur frames 302 through 331 for native
  640x480 presentation, target-resolution 880x660 DLAA under Faithful,
  Enhanced, and Photoreal profile metadata, and a 5120x3840 8x reference
  downsampled to the same 880x660 output. All five lanes contain 30 unique
  consecutive outputs with zero reported repeat, drop, or HUD mismatch. The
  three DLAA profile image sequences are byte-identical because profiles do not
  write external settings and no external consumer was present; no winner is
  declared. An initial 8x capture exposed locale-grouped dimensions, was
  retained and rejected, and a fresh 30-frame recapture plus strict index
  validation produced 150 accepted and 30 rejected packages. Gameplay scenes,
  external output, and other legally supplied titles remain required for Gate
  17.
  LOG #91 attempted ten current-build frames through the unchanged supplied
  Gate 10 staging route. The contract and consumer components were observed,
  but every status explicitly said returned neural output was unconfirmed.
  Those candidate files are retained but rejected as presentation evidence;
  Feature 18 activity alone does not satisfy Gate 17.
  LOG #92 separates contract evaluation from external-output confirmation. A
  three-frame live negative control observed the contract and public output but
  correctly emitted no external label or file. Capture-specific external
  mutation/presentation proof remains required.
  LOG #93 implements that proof without marking the quality artifact: schema-3
  captures carry exact raw contract/output hashes, and a separate verifier
  requires a same-build ON sentinel replay, 1024/1024 swapchain marker pixels,
  completed same-frame Present, and an exact-input policy-OFF output
  difference. Two moving Soulcalibur frames were confirmed through the
  unchanged supplied route, while a zero-mask mismatch was rejected without
  promotion. This closes the capture-provenance mechanism and a bounded live
  sample, not Gate 17's gameplay/profile/title matrix.
  LOG #96 adds a deterministic production-performance replay spanning
  Soulcalibur frames 1802 through 2401 with all synchronous capture/evidence
  disabled. The supplied consumer ON run presented 600/600 accepted outputs;
  the explicit-policy-OFF control presented 600/600 native frames and counted
  all accepted public candidates as unpresented. Both had zero missing Presents,
  identity errors, source gaps/repeats, output repeats, alternations, frame
  latency, or query-ring pressure; both closed cleanly and retained the same
  `0A96F4E2FB52C75C` replay. ON kept 125 Flycast-owned neural objects constant
  and grew local VRAM by 196608 bytes; OFF kept 116 constant and grew by 131072
  bytes. ON Present-interval P50/P95/P99 was 13.8265/14.8639/15.6154 ms versus
  OFF 13.4586/14.6828/16.1706 ms. The D3D12/external evaluation remains outside
  Flycast's D3D11 timestamp domain and is still reported null rather than
  inferred from end-to-end intervals, so external timing remains partial.
  LOG #97 repeats that external ON/OFF combat measurement through DX11 OIT.
  ON presented 600/600 accepted outputs; OFF presented 600/600 native frames
  and retained all public candidates as unpresented. Both covered source frames
  1802-2401 with zero missing Presents, identity errors, source gaps/repeats,
  output repeats, alternations, latency, or query pressure and clean close.
  ON/OFF kept 125/116 Flycast-owned objects constant and each grew local VRAM
  by 131072 bytes. ON Present P50/P95/P99 was 13.8293/15.2448/16.5339 ms versus
  OFF 13.4453/14.6009/15.0220 ms. LOG #98 directly measures the inclusive D3D12
  evaluation span: ON normal/OIT P50/P95/P99 is
  2.653568/2.916768/2.961088 ms and 2.487072/2.896224/3.016384 ms, while explicit
  policy-OFF is 0.171040/0.195104/0.627168 ms and
  0.225024/0.290720/0.319744 ms. These separately reported runs are not treated
  as an isolated external-cost subtraction.

## Pre-quality Gate 1 closure

LOG #111 closes FC-021/FC-022 before further quality tuning. Five consecutive
1440x1080 Soulcalibur PVR scene frames are byte-identical between the
neural-capable mode-off build and compile-time feature-off build on normal and
OIT DX11. Repeating the enabled side on a marker-verified D3D11On12 device is
also byte-identical. Every enabled marker reports zero neural instrumentation,
draw/history population, input-layout/export/backend allocation, and guidance
replay. The material wrong-frame controls change more than 569000 of 1555200
pixels, and 14 synthetic fixtures under both renderer labels remain
deterministic across five runs. The proof capture is synchronous and excluded
from all performance evidence.

## Pre-quality Gate 2 closure

LOG #112 closes FC-023 with the production PVR scene capture rather than the
test-only rasterizer. One exact replayed Soulcalibur source frame is captured
at 640x480, 2560x1920, and 5120x3840 on normal and OIT DX11 under both native
D3D11 and D3D11On12. The command compares each 4x/8x sample against the exact
nearest-neighbor enlargement of the 1x frame. It also isolates 1x source edges
with an RGB-neighbor threshold of eight and requires material differing edge
samples and subpixel-diverse high-resolution blocks.

Normal DX11 changes 713193/4915200 pixels at 4x and 2842727/19660800 at 8x;
160244 and 639317 of those changes lie in source-edge blocks, with 14888 and
15020 subpixel-diverse edge blocks. OIT changes 714179 and 2842153 pixels,
including 160653 and 642093 edge samples and 14959/15083 diverse blocks. The
native-D3D11 and D3D11On12 results are exact per renderer. A zero-difference,
zero-diversity nearest control is executed through the same acceptance predicate
and rejected. Capture is synchronous, developer-only, and performance-ineligible.

## Legacy Gates 4-7 aggregation

LOG #113 refreshes the original FC-061 thresholds without conflating them with
quality Gates 11-18. Legacy Gate 4 is green: the current GPU fixture returns
exact static `[0,0]` motion. Gate 6 is green through the matcher/generation/
ambiguity controls that emit zero confidence and current-color bias for wrong
history. Gate 7 is green: current-SHA passthrough runs on packages rendered
under both DX11 renderer labels report byte-identical stage color.

Legacy Gate 5 is green: normal Dreamcast +4X is exactly `[-4,0]`,
camera/deformation truth is within the analytic bound, changed-count edge clips
retain at least 90 percent in the CPU control, topology changes are rejected,
and particles remain reactive. LOG #117 carries accepted prior Naomi 2 model-
view/projection matrices through the production HLSL export and proves analytic
`[-4,+3]` motion on native D3D11 and D3D11On12. A missing-history control and
reindexed geometry remain zero-trust, so legacy Gate 5 is now closed without
manufacturing correspondence.
