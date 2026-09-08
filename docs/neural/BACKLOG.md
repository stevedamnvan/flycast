# Neural rendering backlog

## Active execution authority - 2026-09-08

This is the single current queue. User authorization now covers autonomous,
bounded engineering progression through it, not repeated approval per trace.
D-114 records this authority and supersedes procedural permission stops.
Old audit stops are historical scope, not active permission gates. AGENTS.md
retains the hard boundaries; the remake plan retains architecture/acceptance.
Do not create replacement FC IDs or another task-state system.

### Short standing goal

> Follow docs/neural/BACKLOG.md autonomously until Flycast has a verified working experimental RTX Remix + externally supplied DLSS 5 gameplay pipeline. Implement, test, record evidence, and advance through the backlog without routine human approval; preserve its safety boundaries and stop only for a genuine external blocker or the documented working-pipeline acceptance.

Run with Astra low as requested. On 2026-09-08 the app reported no existing
goal, and the short objective above was successfully registered as active.
Use the goal tools to verify current state; this document is not live app
telemetry. Do not falsely complete an objective or edit app storage to replace
it. The tracker carries the objective; this backlog carries the work plan.

### Current checkpoint

- Last exact tested checkpoint: `d7eb260adff8ea469057d6d1df160549ce686ab7`, pushed (LOG199). Later owned work remains uncommitted.
- FC-067: scene/material export, public-header mock, selected opaque FTRV-to-TA
  lineage and native preservation are proven only in their recorded scopes.
- Actual transform `8c03c944` in block `8c03c93a` supplies opaque vertex4.
  Preserve nonunit W. Source coordinate space, matrix semantics and broader
  coverage are unproven. Use OPAQUE-INITIAL-STORES-AUDIT.md, LOG #175.
- Real Remix GPU rendering, moving relighting and combined Remix/DLSS 5
  presentation are NOT proven. M2 strict source equality remains failed/parked.
- Existing standalone neural transport/provenance and runtime coverage remain
  valid evidence; do not redo them just because a new task starts.

### Ordered queue

Current card: **FC-067 / M2-camera**.
Usable camera contract: **pending**.
Statuses: `todo`, `doing`, `blocked(reason -> next action)`, `done`.
A blocked card does not block independent rows. "Done" requires linked evidence,
not a plan or successful API call. The labels below are FC-067 substeps, not
new FC IDs or implicit acceptance of old M1-M5 requirements.

| Card | Status | Dependencies | Deliverable / acceptance | Evidence |
|---|---|---|---|---|
| FC-067 / M2-camera | doing | LOG #175 | Bounded opaque sample coverage and explicit usable camera/coordinate contract, or precise unsupported domains. See next-card bounds below. | LOG #175/#177/#178/#179 partial; camera pending |
| FC-067 / M1-GPU | todo | public-header adapter already tested | Isolated public Remix runtime harness with real synthetic GPU output/readback, moving camera, overlap and wrong-camera/light controls. A mock remains a mock. This row can proceed if M2-camera is blocked. | pending |
| FC-067 / M2-scene | todo | M2-camera usable contract | Export actual geometry/material generations for the supported scene domains; reconstruct/compare moving opaque coverage and depth with falsifying controls. Quantify omissions; do not silently promote the parked strict replay gate. | pending |
| FC-067 / M3-relighting | todo | M1-GPU, M2-scene | Actual moving Soulcalibur fighters/arena rendered through Remix with source assets, controlled lighting, stable camera/occlusion and explicit baked-light/material limitations. Preserve moving native/Remix comparison. | pending |
| FC-067 / M4-presentation | todo | M3-relighting | Return actual Remix output to Flycast with explicit ownership, synchronization, frame identity, bounded latency, native fallback and protected HUD/OSD. Source scene/guidance must describe the new image. | pending |
| FC-067 / M4-DLSS5 | todo | M4-presentation | Prove that returned Remix scene reaches the supplied external DLSS 5 consumer and the combined result reaches Present. Exact-input ON/OFF, active consumer tuple, native/public/Remix/combined distinction and focused Gate 10 negative controls. | pending |
| FC-045, FC-054, FC-055, FC-063, FC-064 / combined hardening | todo | M4-DLSS5 | Capture/overlay/transition/failure/cadence checks on the changed route; asynchronous performance and repeatable launch. Satisfy the working-pipeline checklist below. | pending |
| FC-067 / M5 and FC-065 / style expansion | todo | working pipeline; legal content where needed | Optional further art direction and title coverage. Not substitutes for making the combined route work, and not factory-default promotion without the existing quality gates. | pending |

### Next-card bounds: FC-067 / M2-camera

Current next action: finish the LOG202 checkpoint checks, then capture the
first producer family for all15 currently unsupported addresses in one bounded
batch. B's2718 original records,1287 accumulations and8097 consumer bindings
now pass arithmetic; calibrated reconstruction supports1529 triangles/frame,
omitting62 draw277 triangles and6810 whole-scene opaque triangles. Preserve
the same calibration and fail-closed arithmetic; do not repeat verified906
records, draw1 or emitter-format work. Temporary core hooks are retained as
ignored evidence and removed (LOG203); restored checks pass. Exact-commit
rebuild/native capture remain required before the next probe.

Accepted bounded evidence:
- LOG180..184 / TRANSFORM-COVERAGE-AUDIT.md:15 selected samples including actual
  accumulation where present; older draw26 slot3 remains unsupported.
- LOG192 / FULL-DRAW-TRANSFORM-AUDIT.md: draw1's142 vertices and92 triangles
  have original-transform/RAM/gather lineage and calibrated reprojection.
  It omits8247 opaque triangles/frame; physical scale/world camera unknown.
- LOG194..197 / COMPACT-CONSUMER-TAPE.md: draw277's2745 vertices,3321 indices,
  576 restarts and1591 triangles have a complete compact copy/decoder/scene
  map across3frames. Tape has8235 records/1317616 bytes and references921
  XYZ addresses/frame. No differing values at reused addresses were observed.
  Address reuse is not original-transform identity or authoritative generation.
- C++ emitter13 checks, cross-language bytes, old-domain same-capture exact
  verbose/tape equality, large-domain scene-binding tests and live wrong-pointer
  controls pass. Both wrong-pointer captures created no tape.188 Python
  inspector tests pass. Temporary core hooks are removed; retained patches
  remain ignored evidence. Four restored builds, three298-test selftests,
  SDK63-check mock and compact13-check cross-language test pass. The committed
  exact-SHA build/test/native capture checkpoint is verified in LOG199.

Failures retained, not retried away: large A/B encountered a never-copied
completed gather after5069 actual copies. C identified a later unsupported
copy beyond the selected domain. D deliberately limits acquisition to the
observed5069-copy prefix, and all8235 selected vertices independently find
their actual copies. E wrong-pointer rejects. Later geometry is omitted,
not accepted. No paired native/image-preservation or performance claim.

Next original-transform collection must:
1. Derive the921-address/consumer map from the accepted tape, preserving
   frame/producer/generation/packet identity. Never assume a contiguous range.
2. Reuse known executed producer seams only where actual descriptors and
   observed source pointers agree. Preserve nonunit W, all matrix/input bits,
   accumulation, X-load continuity, actual writes and every consumer's final
   bytes. Unknown producers/writers reject or remain explicitly unsupported.
3. Replace repeated text with a bounded compact transform ledger. Keep3frames,
   at most4096 records/frame,32 descriptors with128 operations/32 live words,
   64 writes/record and8MiB emitted ledger. Record the exact layout and memory
   bounds before running; source consumer tape is not arithmetic evidence.
   LOG200 provides a tested lossless zlib envelope using the existing dependency:
   8MiB compressed/128MiB formatted-input cap and bounded streaming buffers.
   The actual921-address target table is derived and retained ignored. Next
   producer callbacks are now integrated in temporary hooks (LOG201); no further format-only phase. Read
   COMPACT-TRANSFORM-LEDGER.md before running the new ledger.
4. Independently verify arithmetic and original-transform-to-consumer binding,
   then reconstruct the supported1591-triangle domain with shared-calibration
   controls. Quantify omissions and never silently close parked strict replay.
5. Preserve the prior no-progress/pivot rule and advance the highest-coverage
   supported family. Top10 draws contain6570 of8339 triangles; do not spend
   another phase walking small draw ordinals one by one.

Independent M1-GPU: standalone runtime bring-up builds;63/63 public-header
mock tests include120-frame immutable-resource reuse and failure invalidation.
The configured cache contains only header/licenses. Real runtime startup,
GPU output/readback/completion and combined presentation remain unproven.
Use REMAKE-RUNTIME-BRINGUP.md when a legal compatible runtime is available.
Do not expand mocks further merely to avoid that dependency; no proprietary
binary fetch or configuration workaround is authorized.

### Autonomous progression and no-progress rule

Each card records its hypothesis, owned scope, dependency, success/failure
artifact, fixed bounds and next transition before execution. Review, test and
advance without routine human signoff. Do not stop after each isolated store
or turn a self-written task card into a claim that an unmet gate passed.

After two consecutive tranches produce no new causal fact, supported scene
coverage, working capability, or diagnosed/fixed regression: stop that tactic,
record why, and select a different concrete hypothesis or ready dependency.
An informative negative is progress only if it eliminates a named hypothesis
and changes the next action. Do not count larger logs, repeated audits or
unchanged captures as progress. Do not silently expand sample/caller bounds.

Missing additional games, monitor movement, spontaneous TDR or physical runtime
removal are scoped coverage gaps, not reasons to delay the available prototype.
Missing legal runtime/build dependencies are checked once and revisited only
when the environment changes; finish synthetic/mock/interface work meanwhile.
If every useful card genuinely needs unavailable resources or new external
authority, report one precise blocker and required action. "Without human
intervention" does not authorize prohibited binaries, media acquisition,
external configuration writes, spending or unrelated system changes.

### Working-pipeline acceptance (standing-goal exit)

All of the following are required together; this is an experimental supported-
scope gameplay prototype, not universal Dreamcast/Naomi or production readiness.

- A reproducible opt-in launch/build route, with exact source/API/runtime
  provenance and explicit supported renderer/game/domain restrictions.
- Actual original Soulcalibur moving fighters and arena processed by RTX Remix
  on the GPU, then by the supplied DLSS 5 consumer, with the combined pixels
  presented by Flycast. Publish synchronized native, public DLAA, Remix-only
  and combined moving evidence for at least 300 consecutive gameplay frames.
  A synthetic-only scene, two unrelated demos, depth extrusion or module/log
  detection cannot satisfy this requirement.
- Scene/guidance/content-rectangle contracts validated for the combined route;
  exact frame/history/provenance and deliberately wrong-input/output controls
  fail. Consumer settings must be positively reported; no undocumented keys.
- Zero protected HUD/text mismatch, correct late OSD/ImGui, explicit safe
  treatment of transparency and native bypass for unsupported 2D/FMVs/RTT/
  direct-framebuffer cases. Unsupported world geometry cannot silently vanish.
- Separate non-synchronous 600-frame normal and OIT runs on the selected
  D3D11On12 route at a recorded resolution/quality lane. After at most120
  declared warm-up frames, at least99 percent of steady eligible frames must
  use actual combined output; no missing/incorrect frame identity, drops,
  unbounded waits, stale fallback output or growing owned-resource count.
  Report latency and P50/P95/P99 per-pass/frame timing and VRAM, not pass alone.
  Do not lower these criteria after seeing results to force a success.
- Emulation/audio timing ownership unchanged; compare emulated-cycle advance
  per wall time to paired native control (within1 percent over the same bounded
  run). A slow lane must fall back explicitly, not slow emulation. Fallback-only
  success does not meet the steady combined-output requirement.
- Focused mode-off/on, resize, renderer restart, game reload, save-state load,
  injected create/evaluate failure, ring-busy/device-loss recovery and shutdown
  checks on affected code, preserving prior coverage instead of claiming a
  full fresh hardware matrix. Native/no-NGX/feature-off builds remain green.
- Evidence index, configuration instructions, known limitations, tested source
  checkpoint committed/pushed to the fork, and clean worktree. The old strict
  replay residual and unavailable-title/hardware gates stay visible, not passed.
  A user-selected transformative style is allowed; factory Faithful stays
  public DLAA Auto unless its separate quality acceptance is met.

### Validation and checkpoint discipline

Use existing build/capture/selftest procedures in REMAKE-FEASIBILITY-PLAN.md
and DIAGNOSTICS.md. Build all four configurations serially and run all enabled
selftests plus relevant fixtures after code commits. Follow the original
per-commit matrix; document-only changes additionally need live-queue/link/
safety consistency checks, not a fabricated gameplay result. Record actual
counts. Keep the LOG's exact-checkpoint addenda distinct from fresh tests.

Update the current card in place. Keep detailed evidence in LOG/audits and
private raw artifacts outside Git. Commit each independently proven capability
or falsifying result, not an indistinguishable pile of unproven quality edits.
Do not add more historical "current task" headers to this active section.

## Existing FC registry and evidence

The table below retains existing IDs and historical evidence. The active queue
above determines priority; old "pending" phrases in evidence are dated claims,
not requests to repeat closed gates. Verify the relevant latest audit when a
card actually touches that behavior.

| ID | Phase | Item | Acceptance | Status | Evidence |
|---|---:|---|---|---|---|
| FC-000 | 0 | Pinned branch and baseline | Build, tests, hashes, timing, present notes | doing | LOG #1-#4; hashes/timing pending harness |
| FC-001 | 0 | Verify source facts | Exact paths and symbols recorded | done | 00-render-path.md |
| FC-002 | 0 | Document render path | TA through Present before code changes | done | 00-render-path.md |
| FC-003 | 0 | Build options and skeleton | OFF, instrumentation, NGX configs build | done | LOG #5-#9 |
| FC-004 | 0 | Planning documents | Required docs and full backlog exist | done | docs/neural |
| FC-005 | 0 | Licensing note | No binaries; maintainer review stated | done | THIRD_PARTY_NOTICES.md |
| FC-009 | 0 | Phase gate | Three builds and backlog complete | done | LOG #5-#9 |
| FC-010 | 1 | Synthetic fixtures | All named scenes plus TA-stream fixture | doing | LOG #12; test-only geometry exists, `rend_context`/TA parser and analytic truth pending |
| FC-011 | 1 | Render/determinism | Five-run hashes and deltas | done | LOG #11-#13,#111; all 14 fixtures are exact across five runs under both renderer labels, and deterministic production normal/OIT captures plus material wrong-frame controls are green |
| FC-012 | 1 | Scaling/depth commands | Genuine 4x/8x samples; depth ground truth | done | LOG #11-#12,#61,#112; production PVR scaling and production-shader depth truth are both captured with failing controls |
| FC-013 | 1 | Motion command | Reports error, trust, tiers, reactive pixels | doing | LOG #14; explicit no-data result, matcher pending |
| FC-014 | 1 | Neural passthrough | Shared stage and artifact package | done | LOG #13,#29,#73,#113; passthrough color identity is exact, the shared stage carries the complete GPU export set, and production native-lane packages do not fabricate public/external output |
| FC-015 | 1 | Harness NGX | D3D11/D3D12 live or precise unsupported | done | LOG #14,#34-#46; public D3D11 and D3D12 create/evaluate matrices are live on RTX while WARP/no-NGX paths return precise unsupported status |
| FC-016 | 1 | Compare/capture | Threshold exit and legal capture path | done | LOG #13-#14,#73; compare thresholds and the bounded legal-media production capture launcher return explicit success/failure codes |
| FC-019 | 1 | Phase gate | Commands run for both renderers | todo | |
| FC-020 | 2 | Public structures | Renderer-neutral atomic frame contract | doing | LOG #20,#29; C++17 neutral contract and complete DX11 GPU texture set attached atomically; D3D12 ownership pending |
| FC-021 | 2 | Runtime probes | Disabled path has zero probe work | done | LOG #20,#111; mode-off production markers prove zero instrumentation, draw records, previous-position history, neural input-layout/export/backend allocation, and guidance replay on normal/OIT D3D11 and D3D11On12 |
| FC-022 | 2 | Determinism / Gate 1 | Disabled equals baseline | done | LOG #111; deterministic Soulcalibur replay is bit-exact for 5/5 1440x1080 production PVR frames across normal/OIT native D3D11 and enabled-build D3D11On12 versus compile-time feature-off; 28 synthetic renderer/fixture five-run cases are deterministic and material wrong-frame controls fail |
| FC-023 | 2 | Genuine scaling / Gate 2 | 4x/8x edge samples differ from nearest | done | LOG #112; exact Soulcalibur frame 30 production captures prove material full-frame and edge-only differences plus subpixel-diverse edge blocks at 4x/8x on normal/OIT native D3D11 and D3D11On12; the zero-difference nearest control fails |
| FC-024 | 2 | Depth / Gate 3 | OP+PT only, correct ordering | done | LOG #25,#29,#61; production-shader Gate 11 proves zero clear, greater-is-near ordering, OP/PT agreement, reverse-order stability, exact D3D11/On12 parity, and a failing wrong-polarity control; both public NGX APIs create/evaluate with the corrected flag |
| FC-025 | 2 | Draw records and IDs | Fixed history; R16_UINT ID | doing | LOG #20,#29,#64,#69; fixed 8192-entry history separates topology/UV/resource identity, content generations, and pose; current and expected-accepted OP/PT R16_UINT IDs now gate pixel reprojection; translucent coverage remains pending |
| FC-026 | 2 | History generation | All structural reset sources increment | doing | LOG #20,#26,#49,#80-#82,#85-#86; enable/mode/resize/overflow/framebuffer-source plus global render reset/save-state deserialize are wired; context, renderer, surface, same-media reload, and real in-memory save/load transitions retain explicit resets and discontinuities without false output continuity; cross-title runtime evidence remains pending |
| FC-027 | 2 | Atomic package | Once per display frame; never RTT evaluate | doing | LOG #20,#26,#29; complete DX11 TextureRef set is attached to one Geometry package; runtime cadence/RTT evidence pending |
| FC-029 | 2 | Phase gate | Gates 1-3 and deterministic snapshots | todo | |
| FC-030 | 3 | Previous selection | Last successfully evaluated frame | done | LOG #17,#26; renderer commits reference history only after `Submitted`, skip-reference unit control green |
| FC-031 | 3 | Draw matching | Three tiers, strips, rigid fit, reactive/N2 | doing | LOG #17,#27,#64,#66,#68,#117; exact and structural buckets use deterministic minimum-cost assignment; exact strips retain per-vertex history, reindexed ordinary geometry uses a bounded similarity fit, and exact-topology Naomi 2 retains accepted matrices; reindexed Naomi 2 remains conservatively rejected |
| FC-032 | 3 | Motion rasterization | RG16F render-pixel current-to-previous | done | LOG #27,#29,#62,#66-#67,#94,#117,#122; accepted previous positions are bound as a second DX11 vertex stream and the production PVR shaders rasterize exact `[-4,+3]` truth on native D3D11/D3D11On12; Naomi 2 does the same with accepted model/projection history and a failing missing-history control; the primitive-restart validator defect found by deterministic Soulcalibur gameplay is corrected, yielding 82.81% average trusted pixels over the 30-frame sample; active production jitter remains separate from motion and preserves the unjittered native framebuffer. |
| FC-033 | 3 | Confidence/mask | R8 confidence and required bias rules | done | LOG #27,#29,#64,#67-#69; evidence-based confidence, validity/magnitude rejection, and accepted-depth/draw-ID reprojection all feed the final public current-color bias mask; wrong-disocclusion control has measurable trail energy |
| FC-034 | 3 | Reset rules | All cadence and scene-cut resets | doing | LOG #17,#26,#68-#69,#80-#82,#85-#86; accepted-frame age/skips and unmatched-area scene cuts conservatively invalidate motion, increment history generation, and produce full current-color protection; renderer/context/surface, same-media reload, and real in-memory save/load call-site evidence is green; broader-title scene transitions remain pending |
| FC-035 | 3 | Jitter | All VS variants; Halton; unjittered motion | done | LOG #17,#62,#67,#118,#122-#123,#127; production standard-PVR, OIT, and Naomi 2 shader fixtures prove exact +1-pixel coverage movement with zero jitter-only motion on native D3D11 and D3D11On12. Deterministic Soulcalibur frames 1804-1806 exercise nonzero Halton jitter in both normal and isolated OIT scene replays while every native PVR frame remains byte-identical to its unjittered control. Injected failure returns native output with no stale public artifact; protected overlays remain conservatively zero-jitter. DLSS5 experimental uses the same accepted-history-indexed phase as public DLAA. |
| FC-036 | 3 | Gates 4-6 | Fixture thresholds pass | done | LOG #27,#62,#67-#69,#113,#117,#122,#127; legacy Gates 4-6 and quality Gate 12 are green: static motion is exactly zero, analytic translation/deformation and sign/scale negatives pass, current/previous positions remain unjittered, and production Halton jitter affects only the isolated normal/OIT neural raster without perturbing native output. |
| FC-039 | 3 | Phase gate | CPU/HLSL evidence and unit tests | todo | |
| FC-040 | 4 | Stage API | Nonblocking submit/status/output contract | done | LOG #18,#31,#34; 720 live D3D11 submissions use shared stage and return output without production waits |
| FC-041 | 4 | Resource rings | Three deep; fixed; deferred retirement | doing | LOG #25,#29,#31; fixed D3D11 input and NGX output/query rings implemented; deferred retirement pending |
| FC-042 | 4 | NGX D3D11 lifecycle | RAII, SEH leaves, readable capability | done | LOG #31,#33-#35; exact project identity/version, external feature path, live create/evaluate/cleanup and readable unsupported paths verified |
| FC-043 | 4 | Recovery/timing | State machine, removal, async timings | doing | LOG #18,#31,#98,#116; sliding-window hold, nonblocking ring readiness, exception/device-removal mapping, and controlled actual D3D12 removal/reconstruction are green; native D3D11 and asynchronously retired D3D12 queue timings are measured without waits, while spontaneous driver/TDR evidence remains unavailable |
| FC-044 | 4 | D3D11 DLAA/SR | 240-frame public NGX matrix | doing | LOG #34,#37-#40,#61,#63,#72,#94-#95,#101,#103,#106-#107,#118,#120,#124,#130; deterministic Soulcalibur combat covers target-native Auto/J/K, exact 427x320-to-640x480 Quality SR, and a 5120x3840 8x-native reference. A fail-closed settings sweep proves intensity 0.5, 0.25, and 0.125 plus tone/style/preset variants: lower intensity and Natural style reduce damage, global tone 0 is output-inert on this SDR interval, and external preset 2 is byte-identical to preset 0 with upscaling off. The exact capture-reset OIT rerun confirms that 0.125/Natural slightly lowers raw temporal MAE but still loses public Auto on source, trail, thin-line, color, saturation, and black constraints. Public Auto therefore remains the Faithful baseline. Uncanny Cinematic/Structure-200 is valid as an explicit transformative profile but remains non-faithful. |
| FC-045 | 4 | Conditional D3D11On12 surface | Same renderer, dedicated D3D12 lists, selected only by measured route need | doing | LOG #42-#44,#52-#53,#78,#80-#87,#114,#116; production queue/device, wrapped input/output rings, flip-model backbuffer ring, active-render resize/minimize/restore, borderless-fullscreen roundtrip, observed foreground focus loss/restore, same-renderer API-context recreation, bidirectional normal/OIT renderer switching, bidirectional D3D11/D3D11On12 surface switching, same-media unload/reload, in-memory save/load, pause/resume, and controlled actual D3D12 removal/reconstruction are green for Soulcalibur; monitor-move, cross-title load, other-API switching, and spontaneous driver/TDR coverage remain open |
| FC-046 | 4 | Cross-API parity | Identical inputs within 1 LSB | done | LOG #45; all 12 final D3D11/D3D12 DLAA/hook/SR fixture pairs are byte-identical |
| FC-047 | 4 | Hook-compatible DLAA | Zero jitter standard D3D12 NGX shape | done | LOG #44-#45,#53,#59; three 240-frame zero-jitter standard D3D12 evaluations and the production Soulcalibur interception route are green |
| FC-048 | 4 | D3D11 bridge transport | Genuine D3D11 contract mirrored through a private D3D12 consumer and returned with frame identity | blocked(no compatible contract-preserving bridge runtime supplied -> obtain a compatible package or authorize a bounded local bridge build) | LOG #51; Feeder v0.10.0-beta.2 constructs its own ReShade image-derived contract and is transport reference only, while standalone NIGos is absent |
| FC-049 | 4 | No-RTX behavior | WARP/no-NGX green on both APIs | done | LOG #46; SDK and no-SDK WARP GPU texture allocation plus D3D11On12 surface creation return clean explicit unsupported status on both APIs |
| FC-050 | 5 | Settings/UI | Modes, reason, metrics, debug view | done | LOG #21,#108,#121,#128; the thread-safe live snapshot now feeds both the settings diagnostics and an off-by-default, lower-right in-game status OSD. It reports mode/profile/preset, D3D11 or D3D11On12, submit/bypass/HUD state, raster/output size, jitter, FPS/frame interval, and accepted/busy/fallback counters while labeling unavailable drop data honestly. DLSS 5 mode identifies public-contract status and keeps external output explicitly unverified. The seven-view guidance selector and Uncanny profile remain separately available. |
| FC-051 | 5 | Presentation | Evaluate before OSD/ImGui, once/frame | done | LOG #49,#77,#109; accepted output is selected once per emulated frame with native fallback, while exact pre/post production captures on normal/OIT D3D11 and D3D11On12 prove Flycast's ImGui-rendered OSD appears only after the saved neural/game-overlay composite |
| FC-052 | 5 | Reset/cadence wiring | Actual emulator call sites connected | doing | LOG #49,#80-#82,#85-#87; production render/reset/save-state and source-transition call sites are connected; context recreation, renderer/surface switches, same-media unload/reload, real in-memory save/load, and pause/resume are identity-safe across normal/OIT D3D11 and D3D11On12; cross-title load remains pending |
| FC-053 | 5 | Internal resolution | Set/restore and resize rules | done | LOG #63,#72; production Match Neural Output uses exact post-aspect content dimensions for target-native DX11 lanes, excludes bars, follows fullscreen size, leaves RTT/direct-framebuffer paths untouched, and preserves manual/SR sizing |
| FC-054 | 5 | Capture CLI | Rate-limited artifact package | doing | LOG #73-#74,#90-#95,#99-#103,#106-#107,#118,#124-#126,#128-#130; bounded production D3D11/D3D11On12 normal/OIT capture writes the full guidance/output package and can now select the neural-status late-OSD proof independently from the legacy FPS proof. Exact on/off Soulcalibur pairs keep nine pre-OSD artifacts byte-identical on normal and OIT while the late presented artifact alone changes. A one-shot capture-start discontinuity normalizes accepted-history jitter across candidate/marker/policy-off lanes after warm-up; exact-SHA conservative, maximum-coverage Uncanny, and HUD-safe Uncanny OIT lanes now each confirm 30/30. The full legal-title matrix remains. |
| FC-055 | 5 | Optional layer classes | Only after FC-044 green | doing | LOG #70-#71,#104-#105,#125-#127; actual sorted submissions retain reactive-only motion; topology-proven strip/triangle-list batches, PVR-native coordinate classification, and exact `T1401N` profile continuity protect Soulcalibur text, bars, timer, counters, and names across moving normal/OIT frames with zero protected-pixel mismatches. OIT neural replay now owns a separate A-buffer, opaque/multipass history, and aligned resolve-time reactive target, leaving native PVR output byte-exact on both D3D11 surfaces. Broader-title/uncertain-overlay Gate 15A/15B coverage remains open. |
| FC-056 | 5 | Experimental DLSS 5 consumer mode | Route-neutral public contract, readiness ladder, native fallback, no private implementation | done | LOG #50-#59; the selected D3D11On12 route passes Gate 10 with full-contract ON/OFF hashes, per-frame sentinel presentation, zero display-frame latency, and native fallback; direct D3D11 and bridge remain unselected candidate routes |
| FC-059 | 5 | Phase gate | Runtime toggles and Gate 8 | done | LOG #49,#109-#110; production pixel Gate 8 is green with a failing no-OSD control, and a continuous exact-frame neural-off/on round trip proves native fallback, zero off-mode evaluation, fresh reset re-entry, and clean identity on normal/OIT D3D11 and D3D11On12 |
| FC-060 | 6 | Unit tests | Fifteen specified behaviors pass | doing | LOG #50,#61-#64,#66-#73,#76-#77,#105-#106,#108,#121-#124,#126-#129; 168 checks pass in automation, NGX, and no-NGX builds, including exact production OIT raster-jitter coverage, one-shot capture-start reset ownership, and the prior profile, history, motion, disocclusion, overlay, provenance, marker, and live-status contracts; all seven developer visualization shaders compile from production source |
| FC-061 | 6 | Harness acceptance | Gates 1-8 and cross-API/debug checks | doing | LOG #11-#14,#27,#45,#61-#63,#67-#71,#108-#113,#117; legacy Gates 1-8 are green with current falsifying controls, including accepted-history Naomi 2 HLSL motion; a final debug-layer-clean aggregation remains blocked by unavailable host debug-layer components |
| FC-062 | 6 | Public NGX acceptance | Live matrix or exact hardware blocks | doing | LOG #34,#38-#40,#44-#45; public RTX harness matrix green on both APIs; production captures/cadence remain pending |
| FC-063 | 6 | Failure acceptance | No crash/stall/poison/leak | doing | LOG #35,#75-#76,#78,#80-#89,#110,#114-#116; missing runtime, WARP/non-NVIDIA, and no-NGX return clean unsupported; injected failures, production-leaf SEH containment, controlled active-runtime-unavailable, and controlled actual D3D12 removal/reconstruction retain explicit safe behavior; bounded holds recover, terminal failures latch, and active window/focus/context/renderer/surface/game/save-state/pause/mode lifecycles close cleanly on normal/OIT paths; spontaneous driver/TDR, physical loaded-runtime removal, and broader transitions remain |
| FC-064 | 6 | Performance | Invariants and measured targets | doing | LOG #75,#77-#89,#96-#98,#102,#110,#114,#116; asynchronous no-flush production telemetry reports per-pass GPU P50/P95/P99, exact source/accepted/displayed frame identity, present-call intervals, repeat/drop/gap/alternation/latency counters, live mode, accepted-reset state, stage/fallback counts, query-ring pressure, post-warmup VRAM, and scoped Flycast-owned neural GPU-object counts; transition and four 10000-sample normal/OIT soaks cover both surfaces; continuous 600-sample off/on and foreground-focus intervals on all four paths and fresh post-device-removal normal/OIT intervals have zero missing/drop/identity/repeat/latency faults and zero owned-object growth; external ON/OFF spans remain separate rather than an isolated external-cost claim, and broader-title/spontaneous-device evidence remains |
| FC-065 | 6 | Manual game matrix | Legal available images; gaps stated | doing | LOG #73,#90,#94-#95,#100-#103,#106-#107,#118-#121,#124,#126,#130; Soulcalibur has a pixel-repeatable Hoko Temple combat sequence across native, target-native DLAA Auto/J/K, accepted Quality SR, 8x native, conservative external tuples, and explicitly non-faithful Photoreal/Cinematic captures. The exact capture-reset OIT rerun again finds no Faithful external winner. Cinematic/Structure-200/Tone-75/max coverage and automatic-HUD lanes complete 30/30 exact contracts on both normal DX11 and OIT; the current OIT HUD-safe lane protects an average 15,803 pixels with zero mismatch/repeat/drop while remaining visibly and numerically non-faithful. Uncanny Cinematic is therefore a valid user-selected candidate default, not the factory default; other legal titles remain unavailable. |
| FC-066 | 6 | Mandatory DLSS 5 provenance test | User-supplied real-emulator route passes all Gate 10 items | done | LOG #53-#59; all 120 full input contracts matched across ON/policy-OFF, 118 returned outputs differed, frame 9 distinguished native/public-DLAA/external hashes and carried 1024/1024 sentinel pixels through successful same-frame Present, and negative controls retained native fallback |
| FC-067 | R | Reuse-first remake feasibility | Proven scene contract, real Remix GPU/gameplay and separately proven combined external DLSS 5 presentation | doing | D-114 and active queue above. LOG #175 links an opaque FTRV to actual TA output; camera domains/coverage, real Remix GPU, moving relighting and combined presentation remain open. No routine human approval between bounded cards. Strict old replay residual remains failed/parked. Does not replace FC-048 or FC-056/066. |
| FC-069 | 6 | Definition of done | All non-contingent requirements green | todo | |

## Retained UI and title follow-ups (not the current assignment)

FC-050 explicit intensity panel extension (LOG #131): DLSS 5 mode now exposes
pending 0-200 percent Overall/Structure/global/local Tone, style, mask, and UI
correction controls with Use Uncanny values and an explicit Apply action. Apply
invokes the installed settings companion, shows its backup/result, and requires
a restart. It does not establish consumer-active provenance. The helper launch
and exact backup/preserved-keys tests use a disposable configuration; manual
settings-panel click/layout coverage remains pending.

LOG #127 closes the focused OIT replay/jitter implementation and LOG #128 adds
the late neural-status OSD with exact normal/OIT layer isolation. A manual
settings-click and resize smoke remains useful UI coverage but does not block
the image-contract work.

LOG #129 rejects two recapture attempts whose pre-capture external-host startup
accepted different evaluation counts and therefore selected different Halton
phases. The bounded capture writer now issues one discontinuity before the
first retained frame so every comparison lane begins at accepted-history phase
zero. This reset is synchronous-capture-only and must not be enabled for
performance measurements.

LOG #130 completes the affected Gate 16/17 OIT recapture. Public Auto,
conservative external, maximum-coverage Uncanny, and automatic-HUD Uncanny now
have reset-aligned moving comparisons with exact provenance. Preserve those
artifacts and do not rerun them absent a focused regression.

When another user-supplied legal title becomes available, fill one missing
style family with native, public Auto, automatic-HUD Uncanny, and separately
labeled maximum-coverage evidence. Until then, a manual Video-setting toggle
and resize smoke for the optional neural-status OSD remains a bounded local
UI check; it does not block the accepted image contract.

## Subsequent assignment — Gate 17 style-family expansion

LOG #124 completes the exact-commit post-jitter Soulcalibur rerun. Preserve its
three accepted external lanes, moving comparisons, rejected controls, and
restored config hash.

1. Keep public DLAA Auto as Faithful for Soulcalibur and retain Uncanny as a
   persistent, user-selected, explicitly non-faithful profile.
2. When additional user-supplied legal titles become available, capture one
   deterministic moving interval per missing style family before considering
   any factory-default change.
3. Run automatic HUD protection for every candidate-default lane and require
   zero protected-pixel mismatch; label disabled-overlay runs maximum-coverage
   experiments only.
4. Keep accepted-history-indexed jitter and exact candidate/marker/policy-off
   provenance. Never reuse the rejected absolute-frame phase design.
5. Preserve completed OIT scene-replay jitter and reset-aligned evidence from
   LOG #127-#130; rerun only if a new change creates a focused regression.

## FC-056 / FC-066 route sub-items

- Direct D3D11: blocked for the exact supplied add-on; LOG #51 proves Flycast's D3D11 contract evaluates while the add-on arms D3D12 NGX hooks only. Retest only with a compatible direct-D3D11 consumer.
- D3D11 bridge: blocked pending a compatible contract-preserving runtime or an authorized bounded build; Feeder's image-derived contract is not Flycast provenance.
- Conditional D3D11On12/native D3D12: selected experimental consumer route; LOG #52-#59 prove stable queue-owned presentation, public-contract interception, repeated feature-18 evaluation, exact full-contract ON/OFF pairing, three-way pixel-hash differentiation, per-frame public-output sentinels, and same-frame Present. This completes Gate 10 for the named supplied-component route. FC-045's resize/fullscreen/device-loss/OIT/OSD/ImGui and production-performance matrix remains open and prevents a general production-readiness claim.

## Quality-phase gate mapping

The quality rebaseline does not replace existing FC identifiers. Detailed
acceptance is in `QUALITY-PLAN.md`.

- Gate 11: FC-024, FC-044, FC-060, FC-061 — depth polarity, clear semantics,
  OP/PT agreement, API parity, and NGX flag A/B.
- Gate 12: FC-032, FC-035, FC-036, FC-044 — analytic render-pixel motion and
  explicit sign/scale/jitter negative controls.
- Gate 13: FC-025, FC-031, FC-033, FC-036 — structural matching, generations,
  ambiguity, and confidence.
- Gate 14: FC-033, FC-034, FC-036 — reprojection/disocclusion protection.
- Gate 15A/15B: FC-025, FC-050, FC-055, FC-065 — transparency, OIT, modifier
  volumes, and byte-identical overlay composition.
- Gate 16: FC-044, FC-053, FC-065 — exact target resolution and preset lanes.
- Gate 17: FC-054, FC-065 — capture CLI, metrics, and moving title matrix.
- Gate 18: FC-045, FC-063, FC-064 — transitions, failures, performance, and
  long-run stability.
