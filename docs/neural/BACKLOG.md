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
| FC-020 | 2 | Public structures | Renderer-neutral atomic frame contract | doing | LOG #20,#29; C++17 neutral contract and complete DX11 GPU texture set attached atomically; D3D12 ownership pending |
| FC-021 | 2 | Runtime probes | Disabled path has zero probe work | doing | LOG #20; guarded runtime branch/default-off metadata path builds, pixel-parity gate pending |
| FC-022 | 2 | Determinism / Gate 1 | Disabled equals baseline | todo | |
| FC-023 | 2 | Genuine scaling / Gate 2 | 4x/8x edge samples differ from nearest | todo | |
| FC-024 | 2 | Depth / Gate 3 | OP+PT only, correct ordering | doing | LOG #25,#29; production guarded R32_FLOAT OP/PT replay export builds in DX11/OIT; runtime ordering artifacts pending |
| FC-025 | 2 | Draw records and IDs | Fixed history; R16_UINT ID | doing | LOG #20,#29; fixed 8192-entry history plus OP/PT R16_UINT ordinal MRT build; translucent coverage/runtime evidence pending |
| FC-026 | 2 | History generation | All structural reset sources increment | doing | LOG #20,#26; enable/mode/resize/overflow/framebuffer-source resets wired; global reset/save-state/renderer-switch call sites pending |
| FC-027 | 2 | Atomic package | Once per display frame; never RTT evaluate | doing | LOG #20,#26,#29; complete DX11 TextureRef set is attached to one Geometry package; runtime cadence/RTT evidence pending |
| FC-029 | 2 | Phase gate | Gates 1-3 and deterministic snapshots | todo | |
| FC-030 | 3 | Previous selection | Last successfully evaluated frame | done | LOG #17,#26; renderer commits reference history only after `Submitted`, skip-reference unit control green |
| FC-031 | 3 | Draw matching | Three tiers, strips, rigid fit, reactive/N2 | doing | LOG #17,#27; CPU tiers/one-to-one/strip coverage/rigid/reactive/N2 matrix path green; renderer vertex correspondence pending |
| FC-032 | 3 | Motion rasterization | RG16F render-pixel current-to-previous | doing | LOG #27,#29; RG16F target exists but safely emits zero masked as untrusted; real previous-position rasterization pending |
| FC-033 | 3 | Confidence/mask | R8 confidence and required bias rules | doing | LOG #27,#29; R8 targets exist and incomplete motion is forced to bias 1/confidence 0; trusted/untrusted HLSL classification pending |
| FC-034 | 3 | Reset rules | All cadence and scene-cut resets | doing | LOG #17,#26; CPU tracker/scene-cut and framebuffer transitions implemented, remaining global call sites pending |
| FC-035 | 3 | Jitter | All VS variants; Halton; unjittered motion | doing | LOG #17; sequence/phase utility implemented, shaders pending |
| FC-036 | 3 | Gates 4-6 | Fixture thresholds pass | todo | |
| FC-039 | 3 | Phase gate | CPU/HLSL evidence and unit tests | todo | |
| FC-040 | 4 | Stage API | Nonblocking submit/status/output contract | done | LOG #18,#31,#34; 720 live D3D11 submissions use shared stage and return output without production waits |
| FC-041 | 4 | Resource rings | Three deep; fixed; deferred retirement | doing | LOG #25,#29,#31; fixed D3D11 input and NGX output/query rings implemented; deferred retirement pending |
| FC-042 | 4 | NGX D3D11 lifecycle | RAII, SEH leaves, readable capability | done | LOG #31,#33-#35; exact project identity/version, external feature path, live create/evaluate/cleanup and readable unsupported paths verified |
| FC-043 | 4 | Recovery/timing | State machine, removal, async timings | doing | LOG #18,#31; sliding-window hold, nonblocking ring readiness, exception and device-removal mapping implemented; GPU timing/retry evidence pending |
| FC-044 | 4 | D3D11 DLAA/SR | 240-frame public NGX matrix | doing | LOG #34,#37-#40; three DLAA plus Quality/Performance fixtures pass 240/240 with zero invalid frames; DLAA/reference downsample and full flicker thresholds pending |
| FC-045 | 4 | D3D11On12 surface | Same renderer, dedicated D3D12 lists | todo | |
| FC-046 | 4 | Cross-API parity | Identical inputs within 1 LSB | todo | |
| FC-047 | 4 | Hook-compatible DLAA | Zero jitter standard D3D12 NGX shape | todo | |
| FC-048 | 4 | Fallback transport | Bridge only if 11On12 gate fails | todo | |
| FC-049 | 4 | No-RTX behavior | WARP/no-NGX green on both APIs | doing | LOG #31; no-NGX build and explicit unsupported factories green; GPU export WARP and D3D12 paths pending |
| FC-050 | 5 | Settings/UI | Modes, reason, metrics, debug view | doing | LOG #21; guarded mode/surface config and renderer requirement UI implemented; live status/metrics/debug selector pending |
| FC-051 | 5 | Presentation | Evaluate before OSD/ImGui, once/frame | todo | |
| FC-052 | 5 | Reset/cadence wiring | Actual emulator call sites connected | todo | |
| FC-053 | 5 | Internal resolution | Set/restore and resize rules | todo | |
| FC-054 | 5 | Capture CLI | Rate-limited artifact package | todo | |
| FC-055 | 5 | Optional layer classes | Only after FC-044 green | todo | |
| FC-056 | 5 | Experimental DLSS 5 | Unsupported until public contract | done | LOG #31; backend factory returns explicit public-contract unsupported reason; no private IDs or module inspection |
| FC-059 | 5 | Phase gate | Runtime toggles and Gate 8 | todo | |
| FC-060 | 6 | Unit tests | Fifteen specified behaviors pass | doing | LOG #17-#31; 41 checks pass in NGX and no-NGX builds, including explicit DLSS5/D3D12 stubs; numerical HLSL equivalence pending |
| FC-061 | 6 | Harness acceptance | Gates 1-8 and cross-API/debug checks | todo | |
| FC-062 | 6 | Public NGX acceptance | Live matrix or exact hardware blocks | doing | LOG #34,#38-#40; D3D11 RTX matrix running; D3D12/hook and production captures pending |
| FC-063 | 6 | Failure acceptance | No crash/stall/poison/leak | doing | LOG #35; missing runtime, WARP/non-NVIDIA, and explicit no-NGX return clean unsupported status; injected create/evaluate/SEH/busy controls pending |
| FC-064 | 6 | Performance | Invariants and measured targets | todo | |
| FC-065 | 6 | Manual game matrix | Legal available images; gaps stated | todo | |
| FC-066 | 6 | Optional hook test | User-supplied real-emulator components | todo | |
| FC-069 | 6 | Definition of done | All non-contingent requirements green | todo | |
