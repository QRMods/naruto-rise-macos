// The upstream adapter selector and extra crash reporter use Win32 APIs.
// ReXGlue selects the Vulkan device and handles guest faults on macOS.
#include <rex/cvar.h>
#include <rex/logging.h>

#include "graphics_device.h"

// Read by the native renderer's settings; macOS has one GPU, chosen by Vulkan.
REXCVAR_DEFINE_STRING(graphics_device, "", "narutorise/Graphics",
                      "Graphics card by its name (empty = automatic)")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
#include "crash_report.h"
#include "wine.h"

void SelectXenosAdapter() {}
void RestoreXenosAdapter() {}

namespace narutorise {
const char* WineVersion() { return nullptr; }
namespace crash_report {
void Install() {
  REXLOG_INFO("[macOS] Native ARM64 runtime; guest fault diagnostics provided by ReXGlue");
}
}
}
