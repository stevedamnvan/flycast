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
