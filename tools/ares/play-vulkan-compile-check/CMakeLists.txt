cmake_minimum_required(VERSION 3.22)
project(AresPlayVulkanCompileCheck LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(PLAY_ROOT "" CACHE PATH "Play! source checkout with Framework, Nuanceur and Dependencies submodules")
if(NOT ANDROID OR NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "This check targets Android ARM64")
endif()
if(NOT EXISTS "${PLAY_ROOT}/Source/gs/GSH_Vulkan/GSH_Vulkan.cpp")
    message(FATAL_ERROR "Set PLAY_ROOT to the pinned Play! checkout")
endif()
execute_process(COMMAND git -C "${PLAY_ROOT}" rev-parse HEAD
    OUTPUT_VARIABLE PLAY_REV OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
if(NOT PLAY_REV STREQUAL "1f27232772e484bfb79364b751b84f41f4c87481")
    message(FATAL_ERROR "This experiment was validated only against Play! 1f27232772e484bfb79364b751b84f41f4c87481")
endif()
message(STATUS "Compile-only experiment: no executable link, GS bridge or device rendering test")
file(GLOB RENDERER_SOURCES CONFIGURE_DEPENDS "${PLAY_ROOT}/Source/gs/GSH_Vulkan/*.cpp")
add_library(ares_play_vulkan_compile_check STATIC ${RENDERER_SOURCES} "${PLAY_ROOT}/Source/ui_android/GSH_VulkanAndroid.cpp")
target_include_directories(ares_play_vulkan_compile_check PRIVATE
 "${PLAY_ROOT}/Source" "${PLAY_ROOT}/Source/gs" "${PLAY_ROOT}/Source/gs/GSH_Vulkan"
 "${PLAY_ROOT}/Source/app_shared" "${PLAY_ROOT}/deps/Framework/include"
 "${PLAY_ROOT}/deps/Nuanceur/include" "${PLAY_ROOT}/deps/Dependencies/ghc_filesystem/include")
