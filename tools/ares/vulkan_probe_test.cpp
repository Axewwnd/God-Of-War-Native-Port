#include "ares_vulkan_probe.h"
#include <dlfcn.h>
#include <iostream>
#include <stdexcept>

int main()
{
    auto log = [](const std::string &message) { std::cerr << message << '\n'; };
    void *unrelated = dlopen("libc.so.6", RTLD_NOW | RTLD_LOCAL);
    if (!unrelated) throw std::runtime_error("Cannot open negative-test library");
    bool rejected = false;
    try { (void)AresVulkanProbe::probe(unrelated, log, false); }
    catch (const std::exception &error)
    {
        rejected = std::string(error.what()).find("vkGetInstanceProcAddr") != std::string::npos;
    }
    dlclose(unrelated);
    if (!rejected) throw std::runtime_error("Non-Vulkan library was not rejected");
    void *library = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!library) throw std::runtime_error("Vulkan loader is required for this integration test");
    const auto result = AresVulkanProbe::probe(library, log, false);
    if (!result.usable) throw std::runtime_error("No usable logical Vulkan device");
    // This test uses the host ICD, never the uploaded Android Turnip module.
    if (result.turnip) throw std::runtime_error("Unexpected Turnip identity in host software-ICD test");
    std::cout << result.json << '\n';
    dlclose(library);
}
