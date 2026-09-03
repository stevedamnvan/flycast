# Neural rendering evidence log

#1 2026-09-02 12bb436 | `cmake -S . -B build-neural-baseline -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=artifact-neural-baseline -DENABLE_CTEST=ON -DUSE_DISCORD=ON` | MSVC 19.44.35228, SDK 10.0.26100.0; configure succeeded | baseline configured

#2 2026-09-02 12bb436 | `cmake --build build-neural-baseline --config Release --target install --parallel` | stopped at 1008/1099; missing `d3dx9shader.h`; June 2010 DirectX SDK absent | failed, retained

#3 2026-09-02 12bb436 | reconfigure with `-DUSE_DX9=OFF -DENABLE_CTEST=ON`, build | stopped at 661/707; `tests/src/HttpTest.cpp` missing `curl/curl.h` on Windows | failed, retained

#4 2026-09-02 12bb436 | reconfigure with `-DUSE_DX9=OFF -DENABLE_CTEST=OFF -DBUILD_TESTING=OFF`, then build/install | `flycast.exe` linked and installed; exit 0 | baseline Windows DX11 build pass

Toolchain: Windows 11 10.0.26220; CMake 4.4.3; Ninja 1.13.2; Visual Studio
2022 Build Tools 17.14.37516.0; MSVC 19.44.35228 / 14.44.35207; Windows SDK
10.0.26100.0.

#5 2026-09-02 working tree | configure/build `FLYCAST_NEURAL=ON`, `FLYCAST_NEURAL_NGX=OFF`, `FLYCAST_NEURALTEST=ON` | `flycast.exe`, `flycast-neural.lib`, and `neuraltest.exe` linked; exit 0 | instrumentation configuration pass

#6 2026-09-02 working tree | configure/build with `FLYCAST_NEURAL_NGX=ON`, SDK `C:/Game Dev/Emulators/NVIDIA-DLSS-v310.7.0` | dynamic-CRT release/debug imports validated; full build exit 0 | NGX configuration pass

#7 2026-09-02 working tree | configure/build `FLYCAST_NEURAL=OFF`, `FLYCAST_NEURAL_NGX=OFF` | full `flycast.exe` link exit 0; no neural target in graph | feature-off configuration pass

#8 2026-09-02 working tree | configure NGX with `C:/definitely-missing-sdk` | generation stopped with the required `include/nvsdk_ngx.h` path diagnostic | negative configuration pass

#9 2026-09-02 working tree | `build-neural-baseline/neuraltest/neuraltest.exe --version` | `neuraltest phase-0`, exit 0 | harness skeleton pass

#10 2026-09-02 e5c88da | MSVC x64 configure and `cmake --build build-neural-baseline --target neuraltest -j 8`, neural ON / NGX OFF | five harness translation units and `flycast-neural.lib` linked; exit 0 | Phase 1 target build pass

#11 2026-09-02 working tree | `render`, 5-run `determinism`, and `scaling` smoke on `static-triangle` / `textured-checker-edge` | hash `f38656d535ada799`; 5/5 exact; 4x 204915 and 8x 850539 pixels differ from nearest, max delta 249 | test-only D3D11 driver pass, not production renderer evidence

#12 2026-09-02 working tree | 14 fixtures x `dx11`,`dx11-oit`: 5-run `determinism` and 1x/4x/8x `scaling` | 28 deterministic and 28 scaling commands passed; axis-aligned non-gate scenes reported informational zero differences | Phase 1 command matrix pass within test-driver boundary

#13 2026-09-02 working tree | render all 14 fixtures in both requested lanes; passthrough; exact compare; wrong-history negative control | 28 packages; passthrough max delta 0; wrong-history 32325 differing pixels, max delta 248, PSNR 9.935606, expected exit 1 | artifact/threshold controls pass

#14 2026-09-02 working tree | `depth`, `motion`, no-NGX `neural --backend dlaa`, and `capture` probes | depth/motion report no data with exit 0; DLAA reports unsupported with exit 0; capture exits 3 | missing production instrumentation and FC-054 are explicit, no false pass

#15 2026-09-02 working tree | full MSVC x64 builds with neural ON/NGX OFF, neural ON/NGX ON at SDK v310.7.0, then neural OFF | `flycast.exe` linked in all three configurations; `neuraltest.exe` linked in both enabled configurations | configuration matrix remains green

#16 2026-09-02 working tree | WARP `render --fixture rotate-quad --scale 4` and 5-run determinism | Microsoft Basic Render Driver; 5/5 exact at 1x; JSON manifest parsed successfully | WARP test-driver path pass, not FC-049 export evidence

#17 2026-09-02 0caaeeb | build `motion_reference.cpp` and run `neuraltest selftest` | 17/17 checks pass: signature, tiers 1-3, one-to-one, reactive/unmatched, rigid fit, history, Halton, phase count, scene cut | CPU reference subset pass; strip, Naomi 2, HLSL, full recovery, and renderer wiring remain open

#18 2026-09-02 f271894 | build recovery/stage changes and run `neuraltest selftest` | 23/23 checks pass; three failures at frames 1/30/60 enter one hold, 60 presents alone cannot exit before 1000 ms, resume emits one reset, repeated frame submits once | fallback-hold and emulated-frame cadence unit subset pass; NGX retry/device/timing remain open

#19 2026-09-02 working tree | compile guarded production `rend_context` instrumentation seam | first compile lacked `nowide` includes, second lacked `glm`, third found non-const Flycast `ComPtr::get`; dependencies and accessor qualification corrected without bypassing diagnostics | failed attempts retained; no acceptance claim

#20 2026-09-02 working tree | MSVC x64 full `flycast` + `neuraltest` build, neural ON / NGX OFF; `neuraltest selftest` | full link exit 0; 26/26 checks pass including a real `rend_context` metadata snapshot, first-frame reset, repeated-frame tier-1 match, deterministic hash, and atomic overflow state | production DX11/OIT metadata/cadence seam build pass; MRT exports and production pixel gates remain open

#21 2026-09-02 working tree | rebuild guarded config/settings/runtime-mode changes | `flycast.exe` and `neuraltest.exe` linked; renderer requirement text and unsupported native-fallback note compiled | UI subset build pass; live capability, metrics, and debug-view controls remain open

#22 2026-09-02 working tree | incremental build first outside, then inside `VsDevCmd.bat -arch=x64 -host_arch=x64`; `neuraltest selftest` | outside-developer-shell compile failed at standard headers and the stale 25-check binary was disregarded; proper MSVC build linked both targets and current binary passed 26/26 | corrected invocation pass; failed launch retained and is not test evidence

#23 2026-09-02 working tree | fresh Ninja Release configure/build with `FLYCAST_NEURAL=OFF`, NGX/test off, DX9/tests off | 1090 steps; `flycast.exe` linked exit 0; no `flycast-neural` target in generated graph | feature-off build remains green after guarded production seam

#24 2026-09-02 working tree | Ninja Release configure/build with neural ON, NGX ON, SDK v310.7.0; run `neuraltest selftest` | `flycast.exe` and `neuraltest.exe` linked exit 0; 26/26 checks pass | NGX-linked configuration remains green; no live NGX lifecycle/evaluation exists yet

#25 2026-09-02 cfe2285 | build production three-slot R32 depth-export replay with neural/NGX ON; rebuild fresh feature-off tree; run selftest | both `flycast.exe` builds linked exit 0; 26/26 checks pass; export issues only OP/PT lists and unbinds OIT UAVs before replay by construction | depth resource/ownership build pass for DX11 and OIT; no runtime pixels captured, so Gate 3 remains open

#26 2026-09-02 665bfd4 | build last-successful draw-history ownership and FramebufferDirect atomic-package wiring with neural/NGX ON and feature OFF; run `neuraltest selftest` | both `flycast.exe` links exit 0; 30/30 checks pass; rejected synthetic frame does not replace reference, later matching returns to accepted frame, source transitions increment generation twice | FC-030 pass; FramebufferDirect/RTT runtime cadence evidence and remaining reset call sites stay open

#27 2026-09-02 780f5a0 | build CPU temporal/reference additions and run `neuraltest selftest` | `flycast.exe` and `neuraltest.exe` linked exit 0; 37/37 checks pass: changed-count strip retains >=90% overlap, N2 column-major projection exact, unmatched/oversize vectors zero and bias current, 4:3 content rect 1440x1080 at x=240 vs 1920x1080 widescreen, both depth inverses within 1e-3 | CPU/unit subset pass; real vertex correspondence and HLSL exports remain open

#28 2026-09-02 ed2baff | first source-extracted production HLSL contract test | export permutation failed at compile because the shared modifier-volume function referenced native-only `PSO.col`; corrected with an explicit export branch | failed attempt retained; no gate claim

#29 2026-09-02 ed2baff | build fixed three-slot DX11 export package with neural/NGX ON, compile native/export production HLSL from source, run `neuraltest selftest`, and rebuild feature OFF | enabled and disabled `flycast.exe` links exit 0; current selftest 39/39; package owns RGBA8 color, R32 depth, RG16F motion, R8 bias/confidence, and R16_UINT draw-ID resources | resource/atomic-contract and shader-compile pass only; no runtime game capture, OP/PT draw IDs only, and motion is intentionally zero with mask 1, so Gates 3-6 remain open

#30 2026-09-02 2bd9bf9 | first NGX backend compile | failed on an anonymous-namespace boundary, exact SDK engine enumerator spelling, and const access through Flycast's custom `ComPtr`; all three were corrected directly | failed attempt retained; no runtime or build claim

#31 2026-09-02 2bd9bf9 | build public NGX D3D11 backend in SDK-enabled tree; fresh configure/build instrumentation-only tree; run both selftests | NGX-enabled and NGX-disabled `flycast.exe`/`neuraltest.exe` linked; 41/41 checks pass in both binaries | lifecycle/resource/fallback structure build pass; live NGX create/evaluate remains unrun pending harness GPU texture wiring

#32 2026-09-02 cdefd7f | first live-harness compile | MSVC rejected `std::max(1u, frames)` after the Windows `max` macro expanded; replaced it with an explicit zero-frame normalization and rebuilt both targets | failed attempt retained; no runtime claim

#33 2026-09-02 working tree | first RTX 5090 live D3D11 DLAA probes | initial directory-level input failed because `neural` requires a frame package; corrected input reached NGX but reported availability 0 because the code used the wrong project GUID and `PATH` did not populate NGX's feature search list | failed attempts retained; exact specification GUID, `GIT_VERSION`, and documented external feature path then applied

#34 2026-09-02 working tree | live `neural --api d3d11 --backend dlaa --frames 240` on `camera-translate`, `particles`, and `textured-checker-edge` using SDK v310.7.0 release runtime | RTX 5090: each run submitted 240/240, result 1 (`Success`), exception 0, busy/fallback 0, invalid frames 0; final hashes `cbbbf5ea47d3eb30`, `bf648c0497c22f21`, `1b68d2f0356be50c`; per-frame readback is harness-only | public D3D11 DLAA lifecycle/cadence subset pass; inputs are deterministic harness frames, not production PVR captures

#35 2026-09-02 working tree | D3D11 unsupported matrix on the same harness | WARP returned init `0xBAD00001`; missing feature runtime returned availability 0 / feature-init `0xBAD00004`; `--no-ngx` returned explicit disabled reason; all exited cleanly without exception | missing-runtime, WARP/non-NVIDIA, and explicit-disable failure subset pass; injected failures remain pending

#36 2026-09-02 working tree | rebuild SDK-enabled `flycast`/`neuraltest`, instrumentation-only `flycast`/`neuraltest`, and feature-off `flycast`; run both enabled selftests | all targets linked; SDK and no-SDK harnesses each passed 41/41 | three-configuration build matrix remains green after live-runner integration

#37 2026-09-02 1b7120f | add public optimal-settings query and explicit SR output/mode arguments | NGX returned 1280x960 Quality input for 1920x1440 output and accepted it; deliberate 320x240 mismatch returned a precise unsupported reason | real-upscale dimension contract pass; no 1:1 SR claim

#38 2026-09-02 working tree | D3D11 SR Quality 1280x960 -> 1920x1440 and Performance 1280x960 -> 2560x1920, 240 frames each, on `camera-translate`, `particles`, `textured-checker-edge` | all six runs submitted 240/240 with result 1, zero exceptions, busy skips, fallbacks, and invalid frames; final hashes Quality `0820ac65892d2064`,`2c7143441a2eab4d`,`4c7a8579e94196ec`, Performance `8fab374f23153673`,`43939767f31a17c3`,`7b8e8287d018b3d1` | D3D11 public SR live matrix pass on static harness inputs; production temporal inputs remain pending

#39 2026-09-02 working tree | compare Performance output against direct 8x reference for the same frame | camera: 33718 differing pixels, max 193, PSNR 33.681036; particles: 11437 / 249 / 36.994271; checker: 379356 / 242 / 42.291854 | measurements recorded without an acceptance threshold; DLAA/reference downsample comparison remains pending

#40 2026-09-02 working tree | per-frame static-output metric on 240-frame camera Quality run | 239 hash changes; worst adjacent frame affected 1195/2764800 pixels, max delta 41, PSNR 69.0399 dB; zero black frames | static convergence/flicker measured, not declared threshold-green

#41 2026-09-02 working tree | correct create flags to the exact DLAA/SR contract and rerun 240-frame checker DLAA | DLAA flags 0 submitted 240/240 with zero invalid/busy/fallback/exception; 238 adjacent hash changes, worst 506/76800 pixels, delta 27, PSNR 60.0902 dB | DLAA no longer incorrectly advertises low-resolution motion; `MVLowRes` is SR-only

#42 2026-09-02 6388f36 | first D3D12 backend compile | SDK v310.7.0's D3D12 create helper required explicit creation/visibility node masks unlike D3D11; corrected both to node mask 1 and rebuilt | failed attempt retained; no runtime claim

#43 2026-09-02 working tree | first public D3D12 DLAA evaluation using dedicated allocator/list/output/fence slot | RTX 5090 submitted 1/1, result 1, exception 0, valid output hash `837c174685cf0994`; same one-frame D3D11 output matched exactly | native D3D12 backend live smoke pass

#44 2026-09-02 working tree | D3D12 DLAA and zero-jitter DLAA-hook on `camera-translate`, `particles`, `textured-checker-edge`, 240 frames each | all six runs submitted 240/240, zero invalid/busy/fallback/exception; final hashes matched the D3D11 results | public standard evaluate-shape hook subset pass; no third-party module was loaded or inspected

#45 2026-09-02 working tree | D3D12 Quality/Performance SR on the same three fixtures, then 12 cross-API final-image comparisons | six SR runs submitted 240/240 with zero invalid frames; DLAA, hook, Quality, and Performance D3D11/D3D12 pairs all had zero differing pixels and max delta 0 | FC-046 harness cross-API parity pass; static synthetic inputs only

#46 2026-09-02 working tree | WARP and `--no-ngx` texture/export probes on D3D11 and D3D12; no-SDK build D3D12 repeat | both APIs allocated the complete GPU input set; D3D12 additionally created/cleared/released a same-queue D3D11On12 wrapped target; all reported explicit unsupported without SDK invocation | FC-049 pass within harness boundary

#47 2026-09-02 working tree | rebuild SDK, instrumentation-only, and feature-off configurations; run both enabled selftests | all three `flycast` targets and both harnesses linked; each selftest passed 41/41 | configuration matrix green after D3D12 backend and harness additions

#48 2026-09-02 working tree | first compile of accepted-output presentation wiring | MSVC rejected a raw `ID3D11ShaderResourceView*` at Flycast's `Quad::draw`, which requires an owning `ComPtr` lvalue | failed attempt retained; output ownership was corrected instead of weakening the quad API

#49 2026-09-02 working tree | rebuild SDK, instrumentation-only, and feature-off `flycast`; run both enabled selftests | all three `flycast.exe` targets linked; SDK and no-SDK selftests each passed 41/41 | D3D11 submitted output is now retained through the final content-rect blit before OSD, framebuffer-direct submission follows the same accepted-output rule, and global render reset/save-state deserialize notify neural history; runtime game evidence remains unavailable

#50 2026-09-02 working tree | implement route-neutral experimental consumer mode; build SDK, instrumentation-only, and feature-off `flycast`; run SDK/no-SDK selftests plus component-absent D3D11/D3D12 and no-NGX controls | all three `flycast.exe` targets linked; SDK and no-SDK selftests passed 45/45; component-absent public NGX submitted 5/5 on each API while reporting `missing-components`, zero rebuilds, and no DLSS 5 confirmation; no-NGX returned clean unsupported with the selected D3D11 route retained | M0 readiness/rebuild policy pass: no forced D3D12, configurable 300-evaluation default, transition-triggered idempotent release, two-attempt default bound, and reason telemetry; public contract output only, so Gate 10 remains entirely open

#51 2026-09-02 2e6995c4b plus working tree | audit Feeder v0.10.0-beta.2, then run `Soulcalibur (USA).chd` through corrected `config:rend.NeuralMode=8` native D3D11 staging with ReShade 6.8.0.2155, supplied RenoDX DLSS 5 v4.7 add-on, public DLSS 310.8.0.0, and signed NR SHA-256 `6EB209E764F39872625DEBD6ABAF45E2BB6322F6F270F781F70C059AE30B3927` | Flycast evaluated the genuine public D3D11 contract and reported `d3d11-external-unclassified` / `contract-evaluated`; the add-on loaded but installed D3D12 NGX hooks only and recorded no intercepted D3D11 evaluation or feature-18 create/evaluate; two earlier staging runs used the wrong transient-key namespace and are explicitly invalid route evidence | M1A blocked because available Feeder constructs an image-derived ReShade contract and standalone NIGos is absent; M1B blocked for this exact supplied add-on, not for all possible D3D11 consumers

#52 2026-09-02 working tree | complete the conditional production D3D11On12 device/queue, wrapped neural input/output resources, and queue-created swapchain; run Soulcalibur no-host controls | the first single-wrapped-backbuffer draft lost the device on first Present with D3D12 reason `0x887A002B`; replacing it with a two-entry wrapped RTV ring selected by `GetCurrentBackBufferIndex` ran stably, shut down cleanly, and reported route `d3d11on12`; no-host readiness remained `missing-components` | production On12 ownership/synchronization subset pass; failed single-buffer attempt retained, and full resize/fullscreen/device-loss/OIT/OSD/ImGui/timing matrix remains open

#53 2026-09-02 working tree | run the corrected On12 route with the supplied Soulcalibur CHD, ReShade/add-on/runtime set, and frame-tagged public-output presentation telemetry; repeat add-on-off and no-NGX controls; rerun the final binary in fresh `m1c-final-positive-20260902`, `m1c-final-fallback-20260902`, and `m1c-final-no-ngx-20260902` stages | external log positively recorded first Flycast D3D12 NGX evaluation, lazy adoption of the public feature handle, signed DLSSNR 310.8 initialization, feature 18 create, and successful 640x480 feature-18 evaluations through count 60; Flycast frame 1 reported candidate public-output wrap, accepted-output blit selection, and successful Present; the final add-on-off run stayed stable for 12 seconds with `missing-components`, retained native presentation, emitted no candidate-output line, and exited cleanly, while the final no-NGX run stayed stable for 10 seconds with `disabled`/unsupported, no accepted output, and a clean exit | Gate 10 route identity/contract observation/feature create-evaluate/cadence subset is positive, but exact external-output resource identity, sentinel/pixel differentiation, F6 A/B, and latency are not proven; DLSS 5 is not yet ready

#54 2026-09-02 working tree | add an opt-in D3D12 input/returned-output readback and public-output marker, then iterate Soulcalibur evidence timing and bias controls | first-frame input and returned hashes were identical (`742A0703FA4DA325`) both before and after forcing the diagnostic bias mask to zero; moving the capture target to evaluation 60 did not produce a capture in that run and was reverted; ordinary Windows-capture images were rejected as A/B evidence because the save-data scene animates | failed/ambiguous attempts retained; exact-equality falsifies any visual-mutation claim for the captured frame and motivated a direct swapchain sentinel check

#55 2026-09-02 working tree | run `m3-sentinel-positive-20260902` with the supplied add-on/runtime, `m3-sentinel-no-addon-20260902` without the add-on, and `m3-sentinel-no-ngx-20260902` from the no-NGX build; attempt F6 controls including a 5000 ms evidence-arm delay | positive frame 1 logged input and pre-marker returned FNV-64 `742A0703FA4DA325`, marked hash `67B941B19BB5C325`, 24022 us synchronous diagnostic wait, 1024/1024 marker pixels in the 640x480 swapchain backbuffer, then successful Present; ReShade logged feature-18 create/evaluate through count 60; the no-add-on run produced the same internal hashes but no candidate-output, swapchain-evidence, or marker presentation and exited cleanly; no-NGX stayed `disabled` with zero candidate/present-evidence lines and exited cleanly; injected F6 attempts did not prevent feature-18 creation/evaluation and therefore are not valid OFF evidence | public-output GPU round trip and native-fallback isolation pass; external feature-18 output identity, neural pixel change, F6 A/B, and production latency remain open, so Gate 10 and DLSS 5 readiness remain blocked

#56 2026-09-02 working tree | build the evidence change in NGX, no-NGX, and feature-off configurations through `VsDevCmd.bat -arch=x64 -host_arch=x64`; run both current selftests | all three `flycast.exe` targets linked and NGX/no-NGX selftests passed 45/45; an initial build outside the developer environment failed only on missing MSVC standard headers and was superseded by the recorded developer-shell build | configuration matrix pass; evidence mode remains compiled out with the neural feature and remains dormant by default in neural builds

#57 2026-09-02 working tree | replace zero-as-empty presentation bookkeeping with an explicit pending flag, rebuild all three configurations, rerun both 45-check selftests, then run `m3-sentinel-frame0-delayed-proof-20260902`, `m3-sentinel-frame0-no-addon-final-20260902`, and `m3-sentinel-frame0-no-ngx-final-20260902` against the supplied Soulcalibur CHD | the delayed positive run preserved frame 0 through capture, accepted-output readiness, blit, 1024/1024 swapchain marker verification, and successful Present; hashes were input/returned `F318E0E2F9B92325` and marked `35EBAAE2FFBCA325`, with a 26337 us synchronous diagnostic wait; the supplied add-on created/evaluated feature 18, while the refreshed no-add-on and no-NGX controls produced zero candidate-output and zero present-evidence lines | frame-0 identity bug fixed and public-output routing proof remains positive; identical pre-marker input/returned pixels still block a neural-mutation claim, F6 injection remains invalid OFF evidence, and diagnostic wait time is not production latency
