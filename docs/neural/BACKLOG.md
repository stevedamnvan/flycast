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
| FC-016 | 1 | Compare/capture | Threshold exit and legal capture path | doing | LOG #13-#14; compare green, capture blocked on FC-054 |
| FC-019 | 1 | Phase gate | Commands run for both renderers | todo | |
| FC-020 | 2 | Public structures | Renderer-neutral atomic frame contract | doing | LOG #20; C++17 neutral contract and bounded views implemented, GPU exports incomplete |
| FC-021 | 2 | Runtime probes | Disabled path has zero probe work | doing | LOG #20; guarded runtime branch/default-off metadata path builds, pixel-parity gate pending |
| FC-022 | 2 | Determinism / Gate 1 | Disabled equals baseline | todo | |
| FC-023 | 2 | Genuine scaling / Gate 2 | 4x/8x edge samples differ from nearest | todo | |
| FC-024 | 2 | Depth / Gate 3 | OP+PT only, correct ordering | doing | LOG #25; production guarded R32_FLOAT OP/PT replay export builds in DX11/OIT; runtime ordering artifacts pending |
| FC-025 | 2 | Draw records and IDs | Fixed history; R16_UINT ID | doing | LOG #20; fixed 8192-entry double history, overflow flag, deterministic snapshot implemented; draw-ID MRT pending |
| FC-026 | 2 | History generation | All structural reset sources increment | doing | LOG #20; enable/mode/resize/overflow resets wired locally; global reset/save-state/renderer-switch call sites pending |
| FC-027 | 2 | Atomic package | Once per display frame; never RTT evaluate | doing | LOG #20; Geometry package is submitted once in normal DX11/OIT Render branches; FramebufferDirect/RTT package evidence pending |
| FC-029 | 2 | Phase gate | Gates 1-3 and deterministic snapshots | todo | |
| FC-030 | 3 | Previous selection | Last successfully evaluated frame | doing | LOG #17; CPU history tracker implemented, renderer wiring pending |
| FC-031 | 3 | Draw matching | Three tiers, strips, rigid fit, reactive/N2 | doing | LOG #17; tiers/one-to-one/rigid/reactive implemented, strips/N2 pending |
| FC-032 | 3 | Motion rasterization | RG16F render-pixel current-to-previous | todo | |
| FC-033 | 3 | Confidence/mask | R8 confidence and required bias rules | todo | |
| FC-034 | 3 | Reset rules | All cadence and scene-cut resets | doing | LOG #17; CPU tracker/scene-cut implemented, call sites pending |
| FC-035 | 3 | Jitter | All VS variants; Halton; unjittered motion | doing | LOG #17; sequence/phase utility implemented, shaders pending |
| FC-036 | 3 | Gates 4-6 | Fixture thresholds pass | todo | |
| FC-039 | 3 | Phase gate | CPU/HLSL evidence and unit tests | todo | |
| FC-040 | 4 | Stage API | Nonblocking submit/status/output contract | doing | LOG #18; passthrough, frame dedupe, source bypass implemented; device backends pending |
| FC-041 | 4 | Resource rings | Three deep; fixed; deferred retirement | doing | LOG #25; fixed three-slot D3D11 R32 depth ring implemented; remaining inputs/output, busy tracking, deferred retirement pending |
| FC-042 | 4 | NGX D3D11 lifecycle | RAII, SEH leaves, readable capability | todo | |
| FC-043 | 4 | Recovery/timing | State machine, removal, async timings | doing | LOG #18; sliding-window hold controller green, NGX retry/device/timing pending |
| FC-044 | 4 | D3D11 DLAA/SR | 240-frame public NGX matrix | todo | |
| FC-045 | 4 | D3D11On12 surface | Same renderer, dedicated D3D12 lists | todo | |
| FC-046 | 4 | Cross-API parity | Identical inputs within 1 LSB | todo | |
| FC-047 | 4 | Hook-compatible DLAA | Zero jitter standard D3D12 NGX shape | todo | |
| FC-048 | 4 | Fallback transport | Bridge only if 11On12 gate fails | todo | |
| FC-049 | 4 | No-RTX behavior | WARP/no-NGX green on both APIs | todo | |
| FC-050 | 5 | Settings/UI | Modes, reason, metrics, debug view | doing | LOG #21; guarded mode/surface config and renderer requirement UI implemented; live status/metrics/debug selector pending |
| FC-051 | 5 | Presentation | Evaluate before OSD/ImGui, once/frame | todo | |
| FC-052 | 5 | Reset/cadence wiring | Actual emulator call sites connected | todo | |
| FC-053 | 5 | Internal resolution | Set/restore and resize rules | todo | |
| FC-054 | 5 | Capture CLI | Rate-limited artifact package | todo | |
| FC-055 | 5 | Optional layer classes | Only after FC-044 green | todo | |
| FC-056 | 5 | Experimental DLSS 5 | Unsupported until public contract | todo | |
| FC-059 | 5 | Phase gate | Runtime toggles and Gate 8 | todo | |
| FC-060 | 6 | Unit tests | Fifteen specified behaviors pass | doing | LOG #17-#21; 26 checks pass, strip/N2/content-rect/HLSL pending |
| FC-061 | 6 | Harness acceptance | Gates 1-8 and cross-API/debug checks | todo | |
| FC-062 | 6 | Public NGX acceptance | Live matrix or exact hardware blocks | todo | |
| FC-063 | 6 | Failure acceptance | No crash/stall/poison/leak | todo | |
| FC-064 | 6 | Performance | Invariants and measured targets | todo | |
| FC-065 | 6 | Manual game matrix | Legal available images; gaps stated | todo | |
| FC-066 | 6 | Optional hook test | User-supplied real-emulator components | todo | |
| FC-069 | 6 | Definition of done | All non-contingent requirements green | todo | |
