#include "vulkan/Loader.h"
#include <dlfcn.h>
#include <stdexcept>
#include <iostream>
namespace { void* selected = nullptr; }
extern "C" void* ares_play_vulkan_library() { return selected; }
int main() {
    unsigned rejected = 0;
    try { Framework::Vulkan::CLoader loader; } catch(const std::runtime_error&) { ++rejected; }
    selected = dlopen("libc.so.6", RTLD_NOW | RTLD_LOCAL);
    if (!selected) throw std::runtime_error("Missing test library");
    try { Framework::Vulkan::CLoader loader; } catch(const std::runtime_error&) { ++rejected; }
    dlclose(selected);
    if (rejected != 2) throw std::runtime_error("Loader silently accepted missing or unrelated handle");
    selected = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!selected) throw std::runtime_error("Missing host Vulkan loader");
    {
        Framework::Vulkan::CLoader loader;
        if (reinterpret_cast<void*>(loader.vkGetInstanceProcAddr) != dlsym(selected, "vkGetInstanceProcAddr"))
            throw std::runtime_error("Framework used a different loader");
        uint32_t count = 0;
        if (loader.vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS || count == 0)
            throw std::runtime_error("Selected loader enumeration failed");
    }
    dlclose(selected);
    std::cout << "PASS: absent/unrelated handles rejected; real selected Vulkan loader used\n";
}
