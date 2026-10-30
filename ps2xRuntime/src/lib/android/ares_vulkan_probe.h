#pragma once
#include <functional>
#include <string>
namespace AresVulkanProbe {
struct Result { bool usable = false; bool turnip = false; std::string json; };
// Creates/destroys a real instance and logical test device, without presenting.
// The caller owns the library and must retain it while using its entry points.
Result probe(void *library, const std::function<void(const std::string &)> &log, bool androidSurface);
}
