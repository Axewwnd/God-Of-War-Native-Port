#pragma once

#if defined(__ANDROID__)
// Retained for the process lifetime. A future GS backend must obtain its Vulkan
// entry points from this handle, rather than independently opening libvulkan.so.
// This initializes/probes a loader; it does not implement a PS2 renderer.
void *aresAndroidVulkanLibrary() noexcept;
bool aresAndroidConsumeDiagnosticRequest() noexcept;
// Standalone diagnostic only: pumps NativeActivity lifecycle while probing on
// a worker. Do not use this event loop after raylib takes ownership of events.
void aresAndroidRunDriverDiagnostic();
#endif
