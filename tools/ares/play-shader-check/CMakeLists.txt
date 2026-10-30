cmake_minimum_required(VERSION 3.22)
project(AresPlayShaderCheck LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(PLAY_ROOT "" CACHE PATH "Pinned Play! source")
set(VULKAN_HEADERS "" CACHE PATH "Vulkan include root")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/extract.py"
    "${PLAY_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/shaders.cpp" COMMAND_ERROR_IS_FATAL ANY)
add_subdirectory("${PLAY_ROOT}/deps/Nuanceur/build_cmake" nuanceur)
add_executable(ares_play_shader_check "${CMAKE_CURRENT_BINARY_DIR}/shaders.cpp"
    "${PLAY_ROOT}/Source/gs/GSH_Vulkan/GSH_VulkanDrawUtils.cpp"
    "${PLAY_ROOT}/Source/gs/GsPixelFormats.cpp"
    "${PLAY_ROOT}/Source/gs/GSH_Vulkan/GSH_VulkanMemoryUtils.cpp"
    "${PLAY_ROOT}/deps/Framework/src/MemStream.cpp"
    "${PLAY_ROOT}/deps/Framework/src/Stream.cpp")
target_include_directories(ares_play_shader_check PRIVATE "${VULKAN_HEADERS}"
    "${PLAY_ROOT}/Source/gs/GSH_Vulkan" "${PLAY_ROOT}/Source"
    "${PLAY_ROOT}/deps/Framework/include"
    "${PLAY_ROOT}/deps/Dependencies/ghc_filesystem/include")
target_link_libraries(ares_play_shader_check PRIVATE Nuanceur)
option(ARES_PLAY_SPIRV10 "Validate the guarded mobile SPIR-V 1.0 adaptation" OFF)
if(ARES_PLAY_SPIRV10)
    include("${CMAKE_CURRENT_SOURCE_DIR}/../play-vulkan-spirv10.cmake")
    ares_enable_play_spirv10()
endif()
if(ARES_PLAY_SPIRV10)
    add_executable(ares_play_shader_reject reject.cpp
        "${PLAY_ROOT}/deps/Framework/src/MemStream.cpp" "${PLAY_ROOT}/deps/Framework/src/Stream.cpp")
    target_link_libraries(ares_play_shader_reject PRIVATE Nuanceur)
endif()
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../prepare_play_loader.py"
    "${PLAY_ROOT}/deps/Framework/src/vulkan/Loader.cpp" "${CMAKE_CURRENT_BINARY_DIR}/AresPlayLoader.cpp"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(ares_play_loader_test loader_test.cpp "${CMAKE_CURRENT_BINARY_DIR}/AresPlayLoader.cpp")
target_include_directories(ares_play_loader_test PRIVATE "${VULKAN_HEADERS}" "${PLAY_ROOT}/deps/Framework/include")
target_link_libraries(ares_play_loader_test PRIVATE dl)
