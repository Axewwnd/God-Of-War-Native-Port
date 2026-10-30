#include "AppConfig.h"
#include "GSH_VulkanOffscreen.h"
// This is an application-specific configuration directory, not a GS stub.
fs::path CAppConfig::GetBasePath() const { return fs::path("/data/local/tmp/ares-play-gs-link-check"); }
// The exported factory forces the real renderer and its vtables into the link.
extern "C" CGSHandler *ares_play_gs_link_check_create() { return new CGSH_VulkanOffscreen(); }
extern "C" void ares_play_gs_link_check_destroy(CGSHandler *renderer) { delete renderer; }
