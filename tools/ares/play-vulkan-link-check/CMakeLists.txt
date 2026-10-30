cmake_minimum_required(VERSION 3.22)
project(AresPlayVulkanLinkCheck LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(PLAY_ROOT "" CACHE PATH "Pinned Play! checkout with Dependencies, Framework, Nuanceur and CodeGen")
if(NOT ANDROID OR NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "This link experiment targets Android ARM64")
endif()
execute_process(COMMAND git -C "${PLAY_ROOT}" rev-parse HEAD
    OUTPUT_VARIABLE PLAY_REV OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
if(NOT PLAY_REV STREQUAL "1f27232772e484bfb79364b751b84f41f4c87481")
    message(FATAL_ERROR "Use the validated Play! revision 1f27232772e484bfb79364b751b84f41f4c87481")
endif()
list(PREPEND CMAKE_MODULE_PATH "${PLAY_ROOT}/deps/Dependencies/cmake-modules")
include(Header)
add_subdirectory("${PLAY_ROOT}/deps/Framework/build_cmake/Framework" framework)
add_subdirectory("${PLAY_ROOT}/Source/gs/GSH_Vulkan" gsh)
add_library(ares_play_vulkan_link_check SHARED
 check.cpp
 "${PLAY_ROOT}/Source/gs/GSHandler.cpp"
 "${PLAY_ROOT}/Source/gs/GsPixelFormats.cpp"
 "${PLAY_ROOT}/Source/gs/GsCachedArea.cpp"
 "${PLAY_ROOT}/Source/MailBox.cpp"
 "${PLAY_ROOT}/Source/FrameDump.cpp"
 "${PLAY_ROOT}/Source/states/MemoryStateFile.cpp"
 "${PLAY_ROOT}/Source/states/RegisterStateFile.cpp"
 "${PLAY_ROOT}/Source/states/RegisterState.cpp"
 "${PLAY_ROOT}/Source/ee/INTC.cpp")
target_include_directories(ares_play_vulkan_link_check PRIVATE "${PLAY_ROOT}/deps/CodeGen/include" "${PLAY_ROOT}/Source" "${PLAY_ROOT}/Source/gs/GSH_Vulkan")
target_link_libraries(ares_play_vulkan_link_check PRIVATE gsh_vulkan)
target_link_options(ares_play_vulkan_link_check PRIVATE -Wl,--no-undefined)
option(ARES_PLAY_SPIRV10 "Use the guarded mobile SPIR-V 1.0 adaptation" OFF)
if(ARES_PLAY_SPIRV10)
    include("${CMAKE_CURRENT_SOURCE_DIR}/../play-vulkan-spirv10.cmake")
    ares_enable_play_spirv10()
endif()
# Route Framework through the caller's already selected loader, with no hidden fallback.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../prepare_play_loader.py"
    "${PLAY_ROOT}/deps/Framework/src/vulkan/Loader.cpp" "${CMAKE_CURRENT_BINARY_DIR}/AresPlayLoader.cpp"
    COMMAND_ERROR_IS_FATAL ANY)
get_target_property(framework_sources Framework_Vulkan SOURCES)
list(FILTER framework_sources EXCLUDE REGEX "(^|/)Loader\\.cpp$")
list(APPEND framework_sources "${CMAKE_CURRENT_BINARY_DIR}/AresPlayLoader.cpp")
set_property(TARGET Framework_Vulkan PROPERTY SOURCES "${framework_sources}")
