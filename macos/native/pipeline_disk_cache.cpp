// A Vulkan pipeline cache for the native renderer kept across sessions.
//
// On MoltenVK a new pipeline translates its SPIR-V to Metal Shading Language
// and compiles it: tens to hundreds of milliseconds, during which the draw
// waits (pipeline_set.h: a draw never skips). The cache holds the translated
// shaders, so a pipeline seen in an earlier session is created again in a
// few milliseconds (Metal keeps its own compiled code by source). It is read
// at the first pipeline, written after a quiet spell without new pipelines
// and when the game closes.
#include <plume_vulkan.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <rex/logging.h>

namespace {
std::mutex cache_mutex;
std::filesystem::path cache_path;
VkDevice cache_device = VK_NULL_HANDLE;
VkPipelineCache cache = VK_NULL_HANDLE;
std::atomic<uint64_t> created{0};
uint64_t saved_created = 0;
std::once_flag saver_started;

void SaveLocked() {
  if (cache == VK_NULL_HANDLE || cache_path.empty() || saved_created == created.load()) return;
  size_t size = 0;
  if (vkGetPipelineCacheData(cache_device, cache, &size, nullptr) != VK_SUCCESS || !size) return;
  std::vector<char> data(size);
  if (vkGetPipelineCacheData(cache_device, cache, &size, data.data()) != VK_SUCCESS) return;
  std::error_code ec;
  std::filesystem::create_directories(cache_path.parent_path(), ec);
  const auto temporary = cache_path.string() + ".tmp";
  {
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    out.write(data.data(), std::streamsize(size));
    if (!out) return;
  }
  std::filesystem::rename(temporary, cache_path, ec);
  if (ec) return;
  saved_created = created.load();
  REXLOG_INFO("[macOS] Native pipeline cache saved: {} KB, {} pipelines this session", size / 1024,
              saved_created);
}

// Writes the cache once no pipeline has been created for a few seconds.
void Saver() {
  uint64_t seen = 0;
  auto quiet_since = std::chrono::steady_clock::now();
  for (;;) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    const uint64_t now = created.load();
    if (now != seen) {
      seen = now;
      quiet_since = std::chrono::steady_clock::now();
      continue;
    }
    if (std::chrono::steady_clock::now() - quiet_since < std::chrono::seconds(5)) continue;
    std::lock_guard lock(cache_mutex);
    SaveLocked();
  }
}
}  // namespace

void NarutoriseSetPipelineCachePath(const std::filesystem::path& path) {
  std::lock_guard lock(cache_mutex);
  cache_path = path;
}

VkPipelineCache NarutoriseVulkanPipelineCache(VkDevice device) {
  std::lock_guard lock(cache_mutex);
  if (cache != VK_NULL_HANDLE && cache_device == device) return cache;
  if (cache != VK_NULL_HANDLE) return VK_NULL_HANDLE;  // Another device: no cache.
  std::vector<char> data;
  if (!cache_path.empty()) {
    std::ifstream in(cache_path, std::ios::binary);
    data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  }
  VkPipelineCacheCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
  info.initialDataSize = data.size();
  info.pInitialData = data.empty() ? nullptr : data.data();
  VkResult result = vkCreatePipelineCache(device, &info, nullptr, &cache);
  if (result != VK_SUCCESS && !data.empty()) {
    // A cache of another driver or version: start over.
    info.initialDataSize = 0;
    info.pInitialData = nullptr;
    result = vkCreatePipelineCache(device, &info, nullptr, &cache);
  }
  if (result != VK_SUCCESS) {
    cache = VK_NULL_HANDLE;
    return VK_NULL_HANDLE;
  }
  cache_device = device;
  REXLOG_INFO("[macOS] Native pipeline cache: {} ({} KB loaded)", cache_path.string(), data.size() / 1024);
  std::call_once(saver_started, [] { std::thread(Saver).detach(); });
  return cache;
}

void NarutoriseVulkanPipelineCreated() { created.fetch_add(1, std::memory_order_relaxed); }

void NarutoriseVulkanPipelineCacheSave() {
  std::lock_guard lock(cache_mutex);
  SaveLocked();
}
