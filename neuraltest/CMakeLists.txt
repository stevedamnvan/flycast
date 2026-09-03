add_executable(neuraltest
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
