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
