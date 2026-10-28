// Native macOS entry point, using the upstream game application and hooks.
#include <dlfcn.h>
#include "generated/default/narutorise_init.h"
#include "narutorise_app.h"

REXCVAR_DECLARE(std::string, perf_log_csv);

// The native renderer's pipeline cache kept across sessions (native/pipeline_disk_cache.cpp).
void NarutoriseSetPipelineCachePath(const std::filesystem::path& path);
void NarutoriseVulkanPipelineCacheSave();

// The SDK's search for the Vulkan loader and MoltenVK bundled with the app
// (src/ui/vulkan/vulkan_moltenvk.h, exported by librexruntime).
namespace rex::ui::vulkan {
struct MacOSVulkanRuntimePaths {
  std::filesystem::path sdk_root;
  std::filesystem::path spirv_tools_library;
  std::filesystem::path moltenvk_icd;
  std::vector<std::filesystem::path> loader_candidates;
};
MacOSVulkanRuntimePaths DetectMacOSVulkanRuntimePaths();
void ConfigureMacOSVulkanEnvironment(const MacOSVulkanRuntimePaths& paths);
}  // namespace rex::ui::vulkan

// vkGetInstanceProcAddr of that loader, for the native renderer's Plume
// (macos/CMakeLists.txt routes its volk initialization here).
extern "C" void* NarutoriseVulkanGetInstanceProcAddr() {
  static void* const get_instance_proc_addr = []() -> void* {
    const auto paths = rex::ui::vulkan::DetectMacOSVulkanRuntimePaths();
    rex::ui::vulkan::ConfigureMacOSVulkanEnvironment(paths);
    // Metal compiles a new pipeline's shaders on several threads.
    setenv("MVK_CONFIG_SHOULD_MAXIMIZE_CONCURRENT_COMPILATION", "1", 0);
    for (const auto& candidate : paths.loader_candidates) {
      if (void* library = dlopen(candidate.c_str(), RTLD_NOW | RTLD_LOCAL)) {
        if (void* function = dlsym(library, "vkGetInstanceProcAddr")) {
          REXLOG_INFO("[macOS] Native renderer Vulkan loader: {}", candidate.string());
          return function;
        }
        dlclose(library);
      }
    }
    REXLOG_ERROR("[macOS] No Vulkan loader found for the native renderer");
    return nullptr;
  }();
  return get_instance_proc_addr;
}

class MacApp final : public NarutoriseApp {
 public:
  using NarutoriseApp::NarutoriseApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<MacApp>(new MacApp(ctx, "narutorise", PPCImageConfig));
  }

  void OnConfigurePaths(rex::PathConfig& paths) override {
    NarutoriseApp::OnConfigurePaths(paths);
    // The SDK defines perf_log_csv and writes a row per guest frame, but never
    // opens the file itself.
    if (!REXCVAR_GET(perf_log_csv).empty()) rex::perf::SetCsvLogPath(REXCVAR_GET(perf_log_csv));
    // Finder supplies no command-line flags. Resolve the packaged paths before
    // ReXGlue loads the normal settings and constructs the runtime.
    const auto executable_dir = rex::filesystem::GetExecutableFolder();
    auto file = executable_dir / "narutorise.toml";
    const auto packaged_file = executable_dir.parent_path() / "Resources/narutorise.toml";
    if (std::filesystem::exists(packaged_file)) file = packaged_file;
    if (!std::filesystem::exists(file)) return;
    try {
      auto config = toml::parse_file(file.string());
      if (auto user = config["user_data_root"].value<std::string>(); user && !user->empty()) {
        const auto settings = std::filesystem::path(*user) / "narutorise.toml";
        if (std::filesystem::exists(settings)) {
          file = settings;
          config = toml::parse_file(file.string());
        }
      }
      paths.config_path = file;
      auto read = [&](const char* name, std::filesystem::path& target) {
        if (auto value = config[name].value<std::string>(); value && !value->empty())
          target = *value;
      };
      if (REXCVAR_GET(game_data_root).empty()) read("game_data_root", paths.game_data_root);
      if (REXCVAR_GET(user_data_root).empty()) read("user_data_root", paths.user_data_root);
      if (REXCVAR_GET(cache_root).empty()) read("cache_root", paths.cache_root);
    } catch (const toml::parse_error& error) {
      REXLOG_ERROR("[macOS] Invalid path configuration: {}", error.description());
    }
    if (!paths.cache_root.empty()) NarutoriseSetPipelineCachePath(paths.cache_root / "native-vulkan-pipelines.bin");
  }

  bool OnWindowCloseRequested() override {
    // The process exits hard once the game closes: write the pipelines first.
    NarutoriseVulkanPipelineCacheSave();
    return NarutoriseApp::OnWindowCloseRequested();
  }
};

REX_DEFINE_APP(narutorise, MacApp::Create)
