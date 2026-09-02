add_executable(neuraltest main.cpp)
target_compile_features(neuraltest PRIVATE cxx_std_17)
target_link_libraries(neuraltest PRIVATE flycast-neural)
set_target_properties(neuraltest PROPERTIES FOLDER "Tools")
