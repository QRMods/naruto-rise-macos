// The native renderer's diagnostics built on Windows internals (the thread
// sampler, minidumps and the guest heap watch, which walk x64 stacks and use
// DbgHelp) are off on macOS: their switches read as disabled and their notes
// are dropped.
#include "gpu/diagnostics/crash_dump.h"
#include "gpu/diagnostics/heap_watch.h"
#include "gpu/diagnostics/sampler.h"

namespace gpu::sampler {
bool Enabled() { return false; }
void RegisterThread(const char*) {}
void NoteSwap(uint64_t) {}
}  // namespace gpu::sampler

namespace gpu::crash_dump {
void Install() {}
void NoteFree(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {}
}  // namespace gpu::crash_dump

namespace gpu::heap_watch {
void Install() {}
bool Enabled() { return false; }
void NoteResolvedLock(bool, uint32_t) {}
void WriteReport(const char*) {}
}  // namespace gpu::heap_watch
