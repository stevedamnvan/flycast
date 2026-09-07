add_executable(neuraltest
    remake_preview.cpp
    remake_scene.cpp
    remake_scene_tests.cpp
    artifacts.cpp
	capture_compare.cpp
	color_contract.cpp
	disocclusion_contract.cpp
	guidance_contract.cpp
    fixtures.cpp
    harness.h
	main.cpp
	motion_contract.cpp
	neural_runner.cpp
	neural_runner_d3d12.cpp
	overlay_contract.cpp
    renderer.cpp
	shader_contract.cpp
	transparency_contract.cpp
    unit_tests.cpp)
target_compile_features(neuraltest PRIVATE cxx_std_17)
target_include_directories(neuraltest PRIVATE "${CMAKE_SOURCE_DIR}/core/deps/stb")
target_compile_definitions(neuraltest PRIVATE NEURAL_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(neuraltest PRIVATE flycast-neural d3dcompiler)
set_target_properties(neuraltest PROPERTIES FOLDER "Tools")

# Header-only ABI/call-contract check. No runtime loading or dependency fetching.
option(NEURALTEST_REMAKE_SDK "Build developer-only Remix API mock contract test" OFF)
set(NEURALTEST_REMIX_INCLUDE_DIR "" CACHE PATH "Directory containing reviewed remix_c.h")
if(NEURALTEST_REMAKE_SDK)
    if(NOT EXISTS "${NEURALTEST_REMIX_INCLUDE_DIR}/remix_c.h")
        message(FATAL_ERROR "NEURALTEST_REMAKE_SDK requires a user-supplied reviewed remix_c.h")
    endif()
    # Pin the reviewed API, not merely its version number. Normalize line endings
    # so an ordinary Windows checkout of the same upstream file is accepted.
    file(READ "${NEURALTEST_REMIX_INCLUDE_DIR}/remix_c.h" remake_header)
    string(REPLACE "\r\n" "\n" remake_header "${remake_header}")
    string(SHA256 remake_header_sha "${remake_header}")
    if(NOT remake_header_sha STREQUAL "793bec004819ba2bd3d4026032b845c0b2fa7804513a2c6a3895c842f4bfd1e1")
        message(FATAL_ERROR "Remix header differs from the reviewed FC-067 revision; audit before updating the pin")
    endif()
    add_executable(remake-sdk-contract remake_scene.cpp remake_scene_tests.cpp
        remake_remix_adapter.cpp remake_remix_tests.cpp)
    target_compile_features(remake-sdk-contract PRIVATE cxx_std_17)
    target_include_directories(remake-sdk-contract PRIVATE "${NEURALTEST_REMIX_INCLUDE_DIR}")
    target_compile_definitions(remake-sdk-contract PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    set_target_properties(remake-sdk-contract PROPERTIES FOLDER "Tools")
endif()
