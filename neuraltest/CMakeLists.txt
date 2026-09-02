add_executable(neuraltest
    artifacts.cpp
    fixtures.cpp
    harness.h
    main.cpp
    renderer.cpp)
target_compile_features(neuraltest PRIVATE cxx_std_17)
target_include_directories(neuraltest PRIVATE "${CMAKE_SOURCE_DIR}/core/deps/stb")
target_link_libraries(neuraltest PRIVATE flycast-neural d3dcompiler)
set_target_properties(neuraltest PROPERTIES FOLDER "Tools")
