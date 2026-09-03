# Neural rendering backlog

Status values are `todo`, `doing`, `blocked(reason -> next action)`, and `done`.

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
| FC-011 | 1 | Render/determinism | Five-run hashes and deltas | doing | LOG #11-#13; test driver green, production renderers pending |
| FC-012 | 1 | Scaling/depth commands | Genuine 4x/8x samples; depth ground truth | doing | LOG #11-#12; genuine-sampling gate green in test driver, production depth pending |
| FC-013 | 1 | Motion command | Reports error, trust, tiers, reactive pixels | doing | LOG #14; explicit no-data result, matcher pending |
| FC-014 | 1 | Neural passthrough | Shared stage and artifact package | doing | LOG #13; shared-stage color identity green, full export package pending |
| FC-015 | 1 | Harness NGX | D3D11/D3D12 live or precise unsupported | doing | LOG #14; precise unsupported result, live APIs pending |
| FC-016 | 1 | Compare/capture | Threshold exit and legal capture path | done | LOG #13-#14,#73; compare thresholds and the bounded legal-media production capture launcher return explicit success/failure codes |
| FC-019 | 1 | Phase gate | Commands run for both renderers | todo | |
| FC-020 | 2 | Public structures | Renderer-neutral atomic frame contract | doing | LOG #20,#29; C++17 neutral contract and complete DX11 GPU texture set attached atomically; D3D12 ownership pending |
| FC-021 | 2 | Runtime probes | Disabled path has zero probe work | doing | LOG #20; guarded runtime branch/default-off metadata path builds, pixel-parity gate pending |
| FC-022 | 2 | Determinism / Gate 1 | Disabled equals baseline | todo | |
| FC-023 | 2 | Genuine scaling / Gate 2 | 4x/8x edge samples differ from nearest | todo | |
| FC-024 | 2 | Depth / Gate 3 | OP+PT only, correct ordering | done | LOG #25,#29,#61; production-shader Gate 11 proves zero clear, greater-is-near ordering, OP/PT agreement, reverse-order stability, exact D3D11/On12 parity, and a failing wrong-polarity control; both public NGX APIs create/evaluate with the corrected flag |
| FC-025 | 2 | Draw records and IDs | Fixed history; R16_UINT ID | doing | LOG #20,#29,#64,#69; fixed 8192-entry history separates topology/UV/resource identity, content generations, and pose; current and expected-accepted OP/PT R16_UINT IDs now gate pixel reprojection; translucent coverage remains pending |
| FC-026 | 2 | History generation | All structural reset sources increment | doing | LOG #20,#26,#49; enable/mode/resize/overflow/framebuffer-source plus global render reset/save-state deserialize are wired; explicit renderer-switch evidence pending |
| FC-027 | 2 | Atomic package | Once per display frame; never RTT evaluate | doing | LOG #20,#26,#29; complete DX11 TextureRef set is attached to one Geometry package; runtime cadence/RTT evidence pending |
| FC-029 | 2 | Phase gate | Gates 1-3 and deterministic snapshots | todo | |
| FC-030 | 3 | Previous selection | Last successfully evaluated frame | done | LOG #17,#26; renderer commits reference history only after `Submitted`, skip-reference unit control green |
| FC-031 | 3 | Draw matching | Three tiers, strips, rigid fit, reactive/N2 | doing | LOG #17,#27,#64,#66,#68; exact and structural buckets use deterministic minimum-cost assignment; exact strips retain per-vertex history and reindexed rigid geometry uses a bounded similarity fit; Naomi 2 accepted matrix history remains pending |
| FC-032 | 3 | Motion rasterization | RG16F render-pixel current-to-previous | doing | LOG #27,#29,#62,#66,#67; accepted previous positions are bound as a second DX11 vertex stream and the production PVR shaders rasterize exact `[-4,+3]` truth on native D3D11/D3D11On12; invalid and excessive motion is protected; prior Naomi 2 transform history remains pending |
| FC-033 | 3 | Confidence/mask | R8 confidence and required bias rules | done | LOG #27,#29,#64,#67-#69; evidence-based confidence, validity/magnitude rejection, and accepted-depth/draw-ID reprojection all feed the final public current-color bias mask; wrong-disocclusion control has measurable trail energy |
| FC-034 | 3 | Reset rules | All cadence and scene-cut resets | doing | LOG #17,#26,#68-#69; accepted-frame age/skips and unmatched-area scene cuts conservatively invalidate motion, increment history generation, and produce full current-color protection; remaining global renderer-switch call-site evidence is pending |
| FC-035 | 3 | Jitter | All VS variants; Halton; unjittered motion | doing | LOG #17,#62,#67; production export separately interpolates current/previous unjittered positions so future raster jitter cannot enter motion; applying and proving production raster jitter remains pending |
| FC-036 | 3 | Gates 4-6 | Fixture thresholds pass | doing | LOG #62,#67-#69; quality Gates 12-14 are green on normal Dreamcast geometry; production jitter application and broader phase closure remain pending |
| FC-039 | 3 | Phase gate | CPU/HLSL evidence and unit tests | todo | |
| FC-040 | 4 | Stage API | Nonblocking submit/status/output contract | done | LOG #18,#31,#34; 720 live D3D11 submissions use shared stage and return output without production waits |
| FC-041 | 4 | Resource rings | Three deep; fixed; deferred retirement | doing | LOG #25,#29,#31; fixed D3D11 input and NGX output/query rings implemented; deferred retirement pending |
| FC-042 | 4 | NGX D3D11 lifecycle | RAII, SEH leaves, readable capability | done | LOG #31,#33-#35; exact project identity/version, external feature path, live create/evaluate/cleanup and readable unsupported paths verified |
| FC-043 | 4 | Recovery/timing | State machine, removal, async timings | doing | LOG #18,#31; sliding-window hold, nonblocking ring readiness, exception and device-removal mapping implemented; GPU timing/retry evidence pending |
| FC-044 | 4 | D3D11 DLAA/SR | 240-frame public NGX matrix | doing | LOG #34,#37-#40,#61,#63,#72; public Auto/J/K hints are now exercised on both APIs with expected output distinction and cross-API identity; complete Lane A-E/title evidence and flicker thresholds remain pending for Gate 16 |
| FC-045 | 4 | Conditional D3D11On12 surface | Same renderer, dedicated D3D12 lists, selected only by measured route need | doing | LOG #42-#44,#52-#53,#78,#80-#81; production queue/device, wrapped input/output rings, flip-model backbuffer ring, active-render resize/minimize/restore, same-renderer API-context recreation, and bidirectional normal/OIT renderer switching are green for Soulcalibur on both surfaces; fullscreen, monitor-move, D3D11/D3D11On12 surface switch, game-load/unload, and device-loss matrix remains open |
| FC-046 | 4 | Cross-API parity | Identical inputs within 1 LSB | done | LOG #45; all 12 final D3D11/D3D12 DLAA/hook/SR fixture pairs are byte-identical |
| FC-047 | 4 | Hook-compatible DLAA | Zero jitter standard D3D12 NGX shape | done | LOG #44-#45,#53,#59; three 240-frame zero-jitter standard D3D12 evaluations and the production Soulcalibur interception route are green |
| FC-048 | 4 | D3D11 bridge transport | Genuine D3D11 contract mirrored through a private D3D12 consumer and returned with frame identity | blocked(no compatible contract-preserving bridge runtime supplied -> obtain a compatible package or authorize a bounded local bridge build) | LOG #51; Feeder v0.10.0-beta.2 constructs its own ReShade image-derived contract and is transport reference only, while standalone NIGos is absent |
| FC-049 | 4 | No-RTX behavior | WARP/no-NGX green on both APIs | done | LOG #46; SDK and no-SDK WARP GPU texture allocation plus D3D11On12 surface creation return clean explicit unsupported status on both APIs |
| FC-050 | 5 | Settings/UI | Modes, reason, metrics, debug view | doing | LOG #21; guarded mode/surface config and renderer requirement UI implemented; live status/metrics/debug selector pending |
| FC-051 | 5 | Presentation | Evaluate before OSD/ImGui, once/frame | doing | LOG #49; accepted D3D11 output is selected for the final content-rect blit and native output remains fallback; live emulator capture/cadence evidence pending |
| FC-052 | 5 | Reset/cadence wiring | Actual emulator call sites connected | doing | LOG #49,#80-#81; production render/reset/save-state and source-transition call sites are connected; same-renderer context recreation and normal-DX11/DX11-OIT switches in both directions complete fresh cadence-clean intervals on both surfaces; live game-load and D3D11/D3D11On12 surface-switch validation remain pending |
| FC-053 | 5 | Internal resolution | Set/restore and resize rules | done | LOG #63,#72; production Match Neural Output uses exact post-aspect content dimensions for target-native DX11 lanes, excludes bars, follows fullscreen size, leaves RTT/direct-framebuffer paths untouched, and preserves manual/SR sizing |
| FC-054 | 5 | Capture CLI | Rate-limited artifact package | doing | LOG #73-#74; bounded production D3D11/D3D11On12 normal/OIT capture writes source/guidance/public/final/difference/flicker artifacts, manifests, component metrics, and a provenance/status-aware comparison index; GPU timings, external-output capture, and full title matrix remain |
| FC-055 | 5 | Optional layer classes | Only after FC-044 green | done | LOG #70-#71; conservative normal/OIT translucency, modifier-volume exclusion, strict stable-HUD classification, exact late composite, safe full-frame title override, and latched 2D/menu bypass close Gates 15A/15B; representative-title acceptance remains FC-065/Gate 17 |
| FC-056 | 5 | Experimental DLSS 5 consumer mode | Route-neutral public contract, readiness ladder, native fallback, no private implementation | done | LOG #50-#59; the selected D3D11On12 route passes Gate 10 with full-contract ON/OFF hashes, per-frame sentinel presentation, zero display-frame latency, and native fallback; direct D3D11 and bridge remain unselected candidate routes |
| FC-059 | 5 | Phase gate | Runtime toggles and Gate 8 | doing | LOG #49; build-time presentation/fallback/reset structure is green, runtime toggles and pixel Gate 8 pending |
| FC-060 | 6 | Unit tests | Fifteen specified behaviors pass | doing | LOG #50,#61-#64,#66-#73,#76-#77; 100 checks pass in both NGX and no-NGX builds, including profile/bypass policy, presentation cadence, removed-device latching, and exact target-raster, motion, disocclusion, OIT, and overlay contracts on both surfaces |
| FC-061 | 6 | Harness acceptance | Gates 1-8 and cross-API/debug checks | todo | |
| FC-062 | 6 | Public NGX acceptance | Live matrix or exact hardware blocks | doing | LOG #34,#38-#40,#44-#45; public RTX harness matrix green on both APIs; production captures/cadence remain pending |
| FC-063 | 6 | Failure acceptance | No crash/stall/poison/leak | doing | LOG #35,#75-#76,#78,#80-#81; missing runtime, WARP/non-NVIDIA, and no-NGX return clean unsupported; injected feature-create, evaluate, ring/fence-busy, and device-removed-status controls retain byte-exact native fallback on D3D11 and D3D11On12, bounded holds recover, removed status latches, and active window/context/renderer-variant transitions close cleanly on normal/OIT surfaces; real device loss, SEH, runtime removal while active, and broader transitions remain |
| FC-064 | 6 | Performance | Invariants and measured targets | doing | LOG #75,#77-#81; asynchronous no-flush production telemetry reports per-pass GPU P50/P95/P99, exact source/accepted/displayed frame identity, present-call intervals, repeat/drop/gap/alternation/latency counters, stage/fallback counts, query-ring pressure, and post-warmup VRAM growth; four 600-sample active-window-transition, four post-context-restart, and four bidirectional post-renderer-switch intervals cover normal/OIT D3D11 and D3D11On12, and paired 10000-sample normal-renderer soaks are cadence-clean with nonpositive VRAM growth; OIT long-run, title, external, and D3D12-queue timing coverage remains |
| FC-065 | 6 | Manual game matrix | Legal available images; gaps stated | doing | LOG #73; Soulcalibur intro packages cover public DLAA on normal DX11, DX11 OIT, and D3D11On12 plus native/no-NGX controls; gameplay characters, other profiles, moving sequences, external output, other legal titles, and comparison index remain |
| FC-066 | 6 | Mandatory DLSS 5 provenance test | User-supplied real-emulator route passes all Gate 10 items | done | LOG #53-#59; all 120 full input contracts matched across ON/policy-OFF, 118 returned outputs differed, frame 9 distinguished native/public-DLAA/external hashes and carried 1024/1024 sentinel pixels through successful same-frame Present, and negative controls retained native fallback |
| FC-069 | 6 | Definition of done | All non-contingent requirements green | todo | |

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
