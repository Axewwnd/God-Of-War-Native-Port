# Real GS mobile shader compatibility check

This builds the four shader generators from pinned Play! mobile draw code, using its real
Nuanceur, pixel format and memory helpers. Extraction changes the method into a free function
and replaces the final Vulkan module upload with a file sink; shader expressions stay unchanged.
No Play! EE/IOP CPU is used. This does not implement the ARES raster backend adapter.

Dependencies: CMake, Ninja, C++17, Python 3, Vulkan headers, libxcb development headers on Linux
(the upstream Vulkan headers select XCB), and spirv-val. Tested with SPIRV-Tools 2023.1.
Initialize the pinned Play! submodules listed in `android/ARES-TURNIP-PORT.md` first.

```sh
cmake -S tools/ares/play-shader-check -B build/gs-shaders -G Ninja \
  -DPLAY_ROOT=/path/to/Play- -DVULKAN_HEADERS=/path/to/include
cmake --build build/gs-shaders -j2
mkdir -p build/shaders-original
build/gs-shaders/ares_play_shader_check build/shaders-original
python tools/ares/play-shader-check/validate.py /path/to/spirv-val \
  build/shaders-original build/original-validation.json

cmake -S tools/ares/play-shader-check -B build/gs-shaders-10 -G Ninja \
  -DPLAY_ROOT=/path/to/Play- -DVULKAN_HEADERS=/path/to/include -DARES_PLAY_SPIRV10=ON
cmake --build build/gs-shaders-10 -j2
mkdir -p build/shaders-10
build/gs-shaders-10/ares_play_shader_check build/shaders-10
python tools/ares/play-shader-check/validate.py /path/to/spirv-val \
  build/shaders-10 build/mobile-validation.json
build/gs-shaders-10/ares_play_shader_reject
build/gs-shaders-10/ares_play_loader_test
```

The matrix contains 136 modules: one load/store vertex shader, plus draw/load/store shaders
for 45 combinations of framebuffer/depth formats, texture formats, and selected effects.
It covers CT32/24/16, textures CT32/24/16/T8/T4, and selected texture, fog, alpha and depth paths.
It is a bounded matrix, not every GS state, CLUT variant, transfer or presentation shader.
`validate.py` preserves errors for both environments; read `passed` in the resulting JSON.
It returns a report even for the intentionally failing original Vulkan 1.0 baseline.

`ARES_PLAY_SPIRV10` creates a source copy of the exact hash-checked Nuanceur generator.
It emits SPIR-V 1.0 for this mobile subset, which uses Uniform/BufferBlock and 32-bit operations,
and explicitly rejects int8/int16 storage and fragment interlock. Validation covers instructions,
capabilities and layout, beyond checking the version word. The original reference tree stays unchanged.
The option is available in `play-vulkan-link-check` too; its default remains OFF.

The Framework loader adaptation also rejects missing/unrelated handles, uses the caller's selected
loader, and has no implicit system-driver fallback. The Android experiment accepts a borrowed
process-lifetime loader handle at its factory. It remains separate from the APK and from ARES rendering.
Neither shader validation nor an Android link demonstrates shader execution or a correct game image.
