#include "AppConfig.h"
#include "GSH_VulkanOffscreen.h"
#include <mutex>
#include <atomic>
#include <stdexcept>
// An isolated link experiment; neither this configuration nor factory is shipped in the APK.
fs::path CAppConfig::GetBasePath() const { return fs::path("/data/local/tmp/ares-play-gs-link-check"); }
namespace {
std::mutex creationMutex;
std::atomic<void*> selectedLibrary{nullptr};
}
// Borrowed process-lifetime handle. The caller must retain it while any GS object exists.
extern "C" void* ares_play_vulkan_library() { return selectedLibrary.load(); }
// The real application will pass aresAndroidVulkanLibrary(), after driver validation.
extern "C" CGSHandler* ares_play_gs_link_check_create(void* library) {
    std::lock_guard<std::mutex> lock(creationMutex);
    if(!library || (selectedLibrary.load() && selectedLibrary.load() != library))
        throw std::runtime_error("A process must retain one selected Vulkan loader");
    selectedLibrary.store(library);
    return new CGSH_VulkanOffscreen();
}
extern "C" void ares_play_gs_link_check_destroy(CGSHandler* renderer) { delete renderer; }
