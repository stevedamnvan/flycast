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
