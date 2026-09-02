add_library(flycast-neural STATIC
    instrumentation.cpp
    instrumentation.h
    motion_reference.cpp
    motion_reference.h
    neural_frame.h
    neural_stage.cpp
    neural_stage.h
    recovery_controller.cpp
    recovery_controller.h)

target_include_directories(flycast-neural PUBLIC
    "${CMAKE_SOURCE_DIR}/core"
    "${CMAKE_SOURCE_DIR}/core/deps")
target_compile_features(flycast-neural PUBLIC cxx_std_17)
target_compile_definitions(flycast-neural PUBLIC FLYCAST_ENABLE_NEURAL=1)
target_link_libraries(flycast-neural PUBLIC d3d11 dxgi glm::glm nowide::nowide)

if(FLYCAST_NEURAL_NGX)
    target_compile_definitions(flycast-neural PUBLIC FLYCAST_ENABLE_NGX=1)
    target_link_libraries(flycast-neural PRIVATE NVIDIA::NGX)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE flycast-neural)

if(FLYCAST_NEURALTEST)
    add_subdirectory("${CMAKE_SOURCE_DIR}/neuraltest" "${CMAKE_BINARY_DIR}/neuraltest")
endif()
