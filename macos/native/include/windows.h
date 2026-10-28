// The Win32 names the PC port's native renderer uses, on macOS: POSIX and Mach
// implementations of the same behavior, or a refusal where macOS has no
// equivalent (thread suspension for the diagnostic sampler, minidumps).
#pragma once

#include <dlfcn.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/mach_vm.h>
#include <mach-o/dyld.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <mutex>
#include <string>

#define WINAPI
#define CALLBACK
#define APIENTRY
#ifndef __declspec
#define __declspec(x) __declspec_##x
#define __declspec_dllexport __attribute__((visibility("default")))
#define __declspec_dllimport
#define __declspec_noinline __attribute__((noinline))
#define __declspec_noreturn [[noreturn]]
#define __declspec_thread thread_local
#endif
#ifndef __forceinline
#define __forceinline inline __attribute__((always_inline))
#endif

using BOOL = int;
using BYTE = uint8_t;
using WORD = uint16_t;
using DWORD = uint32_t;
using DWORD64 = uint64_t;
using UINT = unsigned int;
using INT = int;
using LONG = int32_t;
using ULONG = uint32_t;
using LONG64 = int64_t;
using ULONG64 = uint64_t;
using SIZE_T = size_t;
using ULONG_PTR = uintptr_t;
using DWORD_PTR = uintptr_t;
using LONG_PTR = intptr_t;
using PVOID = void*;
using LPVOID = void*;
using LPCVOID = const void*;
using HANDLE = void*;
using HMODULE = void*;
using HINSTANCE = void*;
using HWND = void*;
using WCHAR = wchar_t;
using LPCWSTR = const wchar_t*;
using LPCSTR = const char*;
using NTSTATUS = int32_t;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#define INFINITE 0xFFFFFFFFu
#define WAIT_OBJECT_0 0u
#define WAIT_TIMEOUT 258u
#define WAIT_FAILED 0xFFFFFFFFu
#define INVALID_HANDLE_VALUE (reinterpret_cast<HANDLE>(intptr_t(-1)))

union LARGE_INTEGER {
  struct {
    uint32_t LowPart;
    int32_t HighPart;
  };
  int64_t QuadPart;
};
union ULARGE_INTEGER {
  struct {
    uint32_t LowPart;
    uint32_t HighPart;
  };
  uint64_t QuadPart;
};
struct FILETIME {
  DWORD dwLowDateTime;
  DWORD dwHighDateTime;
};
struct RECT {
  LONG left, top, right, bottom;
};
struct POINT {
  LONG x, y;
};

inline uint16_t _byteswap_ushort(uint16_t v) { return __builtin_bswap16(v); }
inline uint32_t _byteswap_ulong(uint32_t v) { return __builtin_bswap32(v); }
inline uint64_t _byteswap_uint64(uint64_t v) { return __builtin_bswap64(v); }
inline void YieldProcessor() { __builtin_arm_yield(); }
inline DWORD GetLastError() { return DWORD(errno); }

// Threads and the process. A thread handle names the calling thread only.
inline DWORD GetCurrentThreadId() {
  return DWORD(pthread_mach_thread_np(pthread_self()));
}
inline DWORD GetCurrentProcessId() { return DWORD(getpid()); }
inline HANDLE GetCurrentThread() { return reinterpret_cast<HANDLE>(intptr_t(-2)); }
inline HANDLE GetCurrentProcess() { return reinterpret_cast<HANDLE>(intptr_t(-1)); }
#define THREAD_PRIORITY_LOWEST -2
#define THREAD_PRIORITY_BELOW_NORMAL -1
#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_ABOVE_NORMAL 1
#define THREAD_PRIORITY_HIGHEST 2
#define THREAD_PRIORITY_TIME_CRITICAL 15
#define THREAD_SUSPEND_RESUME 0x0002
#define THREAD_GET_CONTEXT 0x0008
#define THREAD_QUERY_LIMITED_INFORMATION 0x0800
inline BOOL SetThreadPriority(HANDLE, int priority) {
  // Quality of service classes are the macOS scheduler's priorities.
  qos_class_t qos = QOS_CLASS_DEFAULT;
  if (priority >= THREAD_PRIORITY_HIGHEST) qos = QOS_CLASS_USER_INTERACTIVE;
  else if (priority > THREAD_PRIORITY_NORMAL) qos = QOS_CLASS_USER_INITIATED;
  else if (priority < THREAD_PRIORITY_NORMAL) qos = QOS_CLASS_UTILITY;
  return pthread_set_qos_class_self_np(qos, 0) == 0;
}
inline long SetThreadDescription(HANDLE, const wchar_t* name) {
  char text[64];
  size_t i = 0;
  for (; name && name[i] && i + 1 < sizeof(text); ++i) text[i] = char(name[i] < 128 ? name[i] : '?');
  text[i] = 0;
  pthread_setname_np(text);
  return 0;
}
inline BOOL GetThreadTimes(HANDLE, FILETIME* creation, FILETIME* exit, FILETIME* kernel, FILETIME* user) {
  // 100 ns units, as Windows reports them; the calling thread's times.
  thread_basic_info_data_t info;
  mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
  mach_port_t port = mach_thread_self();
  const kern_return_t result = thread_info(port, THREAD_BASIC_INFO, thread_info_t(&info), &count);
  mach_port_deallocate(mach_task_self(), port);
  if (result != KERN_SUCCESS) return FALSE;
  auto put = [](FILETIME* t, uint64_t v) {
    if (t) { t->dwLowDateTime = DWORD(v); t->dwHighDateTime = DWORD(v >> 32); }
  };
  put(creation, 0);
  put(exit, 0);
  put(kernel, (uint64_t(info.system_time.seconds) * 1000000 + info.system_time.microseconds) * 10);
  put(user, (uint64_t(info.user_time.seconds) * 1000000 + info.user_time.microseconds) * 10);
  return TRUE;
}
inline DWORD SuspendThread(HANDLE) { return DWORD(-1); }
inline DWORD ResumeThread(HANDLE) { return DWORD(-1); }
inline BOOL DuplicateHandle(HANDLE, HANDLE source, HANDLE, HANDLE* target, DWORD, BOOL, DWORD) {
  if (target) *target = source;
  return FALSE;
}
inline HANDLE OpenThread(DWORD, BOOL, DWORD) { return nullptr; }
#define DUPLICATE_SAME_ACCESS 0x2

// Time: the counter is mach_absolute_time in nanoseconds.
inline BOOL QueryPerformanceFrequency(LARGE_INTEGER* frequency) {
  frequency->QuadPart = 1000000000ll;
  return TRUE;
}
inline BOOL QueryPerformanceCounter(LARGE_INTEGER* counter) {
  static const mach_timebase_info_data_t timebase = [] {
    mach_timebase_info_data_t t;
    mach_timebase_info(&t);
    return t;
  }();
  counter->QuadPart = int64_t(__uint128_t(mach_absolute_time()) * timebase.numer / timebase.denom);
  return TRUE;
}
inline uint64_t GetTickCount64() {
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return uint64_t(now.QuadPart) / 1000000ull;
}
inline void Sleep(DWORD ms) { usleep(useconds_t(ms) * 1000); }
inline void GetSystemTimeAsFileTime(FILETIME* time) {
  // 100 ns since 1601-01-01.
  timeval tv;
  gettimeofday(&tv, nullptr);
  const uint64_t v = (uint64_t(tv.tv_sec) + 11644473600ull) * 10000000ull + uint64_t(tv.tv_usec) * 10;
  time->dwLowDateTime = DWORD(v);
  time->dwHighDateTime = DWORD(v >> 32);
}

// Kernel objects: events and waitable timers. A handle is a Win32Object.
namespace win32_compat {
struct Object {
  enum class Kind { kEvent, kTimer, kFile } kind;
  std::mutex mutex;
  std::condition_variable condition;
  bool manual_reset = false;
  bool signaled = false;
  std::chrono::steady_clock::time_point due{};
  int fd = -1;
  explicit Object(Kind k) : kind(k) {}
};
// Every wait observes this condition: a wait on several objects needs one
// notification source.
inline std::mutex& WaitMutex() {
  static std::mutex m;
  return m;
}
inline std::condition_variable& WaitCondition() {
  static std::condition_variable c;
  return c;
}
inline Object* AsObject(HANDLE handle) {
  if (!handle || handle == INVALID_HANDLE_VALUE || handle == GetCurrentThread()) return nullptr;
  return static_cast<Object*>(handle);
}
// Checks and consumes the signal of an object; WaitMutex is held.
inline bool TryAcquire(Object* object) {
  if (object->kind == Object::Kind::kTimer && !object->signaled &&
      object->due != std::chrono::steady_clock::time_point{} &&
      std::chrono::steady_clock::now() >= object->due) {
    object->signaled = true;
  }
  if (!object->signaled) return false;
  if (!object->manual_reset) object->signaled = false;
  return true;
}
}  // namespace win32_compat

inline HANDLE CreateEventW(void*, BOOL manual_reset, BOOL initial_state, const wchar_t*) {
  auto* event = new win32_compat::Object(win32_compat::Object::Kind::kEvent);
  event->manual_reset = manual_reset;
  event->signaled = initial_state;
  return event;
}
inline HANDLE CreateEventA(void* a, BOOL m, BOOL i, const char*) { return CreateEventW(a, m, i, nullptr); }
inline BOOL SetEvent(HANDLE handle) {
  auto* object = win32_compat::AsObject(handle);
  if (!object) return FALSE;
  {
    std::lock_guard lock(win32_compat::WaitMutex());
    object->signaled = true;
  }
  win32_compat::WaitCondition().notify_all();
  return TRUE;
}
inline BOOL ResetEvent(HANDLE handle) {
  auto* object = win32_compat::AsObject(handle);
  if (!object) return FALSE;
  std::lock_guard lock(win32_compat::WaitMutex());
  object->signaled = false;
  return TRUE;
}
#define CREATE_WAITABLE_TIMER_MANUAL_RESET 0x1
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x2
#define TIMER_ALL_ACCESS 0x1F0003
inline HANDLE CreateWaitableTimerExW(void*, const wchar_t*, DWORD flags, DWORD) {
  auto* timer = new win32_compat::Object(win32_compat::Object::Kind::kTimer);
  timer->manual_reset = (flags & CREATE_WAITABLE_TIMER_MANUAL_RESET) != 0;
  return timer;
}
// due: negative, relative in 100 ns units; positive, absolute (not used here).
inline BOOL SetWaitableTimerEx(HANDLE handle, const LARGE_INTEGER* due, LONG, void*, void*, void*, ULONG) {
  auto* timer = win32_compat::AsObject(handle);
  if (!timer || !due) return FALSE;
  const int64_t relative_ns = due->QuadPart < 0 ? -due->QuadPart * 100 : 0;
  {
    std::lock_guard lock(win32_compat::WaitMutex());
    timer->signaled = false;
    timer->due = std::chrono::steady_clock::now() + std::chrono::nanoseconds(relative_ns);
  }
  win32_compat::WaitCondition().notify_all();
  return TRUE;
}
inline DWORD WaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL wait_all, DWORD ms) {
  using clock = std::chrono::steady_clock;
  const auto deadline = ms == INFINITE ? clock::time_point::max() : clock::now() + std::chrono::milliseconds(ms);
  std::unique_lock lock(win32_compat::WaitMutex());
  for (;;) {
    if (wait_all) {
      bool all = true;
      for (DWORD i = 0; i < count; ++i) {
        auto* o = win32_compat::AsObject(handles[i]);
        all = all && o && (o->signaled || (o->kind == win32_compat::Object::Kind::kTimer &&
                                           o->due != clock::time_point{} && clock::now() >= o->due));
      }
      if (all) {
        for (DWORD i = 0; i < count; ++i) win32_compat::TryAcquire(win32_compat::AsObject(handles[i]));
        return WAIT_OBJECT_0;
      }
    } else {
      for (DWORD i = 0; i < count; ++i) {
        auto* o = win32_compat::AsObject(handles[i]);
        if (o && win32_compat::TryAcquire(o)) return WAIT_OBJECT_0 + i;
      }
    }
    auto wake = deadline;
    for (DWORD i = 0; i < count; ++i) {
      auto* o = win32_compat::AsObject(handles[i]);
      if (o && o->kind == win32_compat::Object::Kind::kTimer && o->due != clock::time_point{})
        wake = std::min(wake, o->due);
    }
    if (clock::now() >= deadline) return WAIT_TIMEOUT;
    if (wake == clock::time_point::max()) win32_compat::WaitCondition().wait(lock);
    else win32_compat::WaitCondition().wait_until(lock, wake);
  }
}
inline DWORD WaitForSingleObject(HANDLE handle, DWORD ms) {
  if (!win32_compat::AsObject(handle)) return WAIT_FAILED;
  return WaitForMultipleObjects(1, &handle, FALSE, ms);
}

// Files (the diagnostic journal and the hashes of files).
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 0x1
#define FILE_SHARE_WRITE 0x2
#define FILE_SHARE_DELETE 0x4
#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define FILE_ATTRIBUTE_NORMAL 0x80
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000
#define FILE_FLAG_WRITE_THROUGH 0x80000000u
namespace win32_compat {
inline std::string Utf8(const wchar_t* path) {
  std::string out;
  for (; path && *path; ++path) {
    const uint32_t c = uint32_t(*path);
    if (c < 0x80) out += char(c);
    else if (c < 0x800) { out += char(0xC0 | (c >> 6)); out += char(0x80 | (c & 0x3F)); }
    else if (c < 0x10000) {
      out += char(0xE0 | (c >> 12)); out += char(0x80 | ((c >> 6) & 0x3F)); out += char(0x80 | (c & 0x3F));
    } else {
      out += char(0xF0 | (c >> 18)); out += char(0x80 | ((c >> 12) & 0x3F));
      out += char(0x80 | ((c >> 6) & 0x3F)); out += char(0x80 | (c & 0x3F));
    }
  }
  return out;
}
}  // namespace win32_compat
inline HANDLE CreateFileW(const wchar_t* path, DWORD access, DWORD, void*, DWORD disposition, DWORD, HANDLE) {
  int flags = (access & GENERIC_WRITE) ? ((access & GENERIC_READ) ? O_RDWR : O_WRONLY) : O_RDONLY;
  if (disposition == CREATE_NEW) flags |= O_CREAT | O_EXCL;
  else if (disposition == CREATE_ALWAYS) flags |= O_CREAT | O_TRUNC;
  else if (disposition == OPEN_ALWAYS) flags |= O_CREAT;
  const int fd = open(win32_compat::Utf8(path).c_str(), flags | O_CLOEXEC, 0644);
  if (fd < 0) return INVALID_HANDLE_VALUE;
  auto* file = new win32_compat::Object(win32_compat::Object::Kind::kFile);
  file->fd = fd;
  return file;
}
inline BOOL WriteFile(HANDLE handle, const void* data, DWORD size, DWORD* written, void*) {
  auto* file = win32_compat::AsObject(handle);
  if (!file || file->fd < 0) return FALSE;
  const ssize_t n = write(file->fd, data, size);
  if (written) *written = n > 0 ? DWORD(n) : 0;
  return n == ssize_t(size);
}
inline BOOL ReadFile(HANDLE handle, void* data, DWORD size, DWORD* read_bytes, void*) {
  auto* file = win32_compat::AsObject(handle);
  if (!file || file->fd < 0) return FALSE;
  const ssize_t n = read(file->fd, data, size);
  if (read_bytes) *read_bytes = n > 0 ? DWORD(n) : 0;
  return n >= 0;
}
inline BOOL FlushFileBuffers(HANDLE handle) {
  auto* file = win32_compat::AsObject(handle);
  return file && file->fd >= 0 && fsync(file->fd) == 0;
}
inline BOOL GetFileSizeEx(HANDLE handle, LARGE_INTEGER* size) {
  auto* file = win32_compat::AsObject(handle);
  struct stat st;
  if (!file || file->fd < 0 || fstat(file->fd, &st) != 0) return FALSE;
  size->QuadPart = st.st_size;
  return TRUE;
}
inline BOOL CloseHandle(HANDLE handle) {
  auto* object = win32_compat::AsObject(handle);
  if (!object) return FALSE;
  if (object->fd >= 0) close(object->fd);
  delete object;
  return TRUE;
}

// Memory: reads and writes that fail on unmapped addresses instead of faulting.
inline BOOL ReadProcessMemory(HANDLE, const void* address, void* buffer, SIZE_T size, SIZE_T* read_bytes) {
  mach_vm_size_t copied = 0;
  const kern_return_t result = mach_vm_read_overwrite(mach_task_self(), mach_vm_address_t(address), size,
                                                      mach_vm_address_t(buffer), &copied);
  if (read_bytes) *read_bytes = SIZE_T(copied);
  return result == KERN_SUCCESS && copied == size;
}
#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define MEM_RELEASE 0x8000
#define PAGE_NOACCESS 0x01
#define PAGE_READONLY 0x02
#define PAGE_READWRITE 0x04
inline int win32_compat_protection(DWORD protect) {
  return protect == PAGE_READWRITE ? (PROT_READ | PROT_WRITE)
         : protect == PAGE_READONLY ? PROT_READ
                                    : PROT_NONE;
}
inline void* VirtualAlloc(void*, SIZE_T size, DWORD, DWORD protect) {
  void* p = mmap(nullptr, size, win32_compat_protection(protect), MAP_PRIVATE | MAP_ANON, -1, 0);
  return p == MAP_FAILED ? nullptr : p;
}
inline BOOL VirtualFree(void* address, SIZE_T size, DWORD) { return munmap(address, size ? size : 1) == 0; }
inline BOOL VirtualProtect(void* address, SIZE_T size, DWORD protect, DWORD* old_protect) {
  if (old_protect) *old_protect = PAGE_READWRITE;
  return mprotect(address, size, win32_compat_protection(protect)) == 0;
}

// Modules.
inline HMODULE GetModuleHandleW(const wchar_t* name) { return name ? nullptr : dlopen(nullptr, RTLD_NOW); }
inline HMODULE LoadLibraryW(const wchar_t*) { return nullptr; }
inline void* GetProcAddress(HMODULE module, const char* name) { return module ? dlsym(module, name) : nullptr; }
inline void OutputDebugStringA(const char* text) { std::fputs(text, stderr); }

// Structured exception handling: the renderer's units take another path on
// macOS (guest_memory.h, sampler.cpp); these keep their declarations compiling.
#define EXCEPTION_EXECUTE_HANDLER 1
#define EXCEPTION_CONTINUE_SEARCH 0
#define EXCEPTION_CONTINUE_EXECUTION (-1)
#define EXCEPTION_ACCESS_VIOLATION 0xC0000005u
struct EXCEPTION_RECORD {
  DWORD ExceptionCode;
  DWORD ExceptionFlags;
  void* ExceptionAddress;
  DWORD NumberParameters;
  ULONG_PTR ExceptionInformation[15];
};
struct CONTEXT {
  DWORD64 Pc, Sp, Fp;
};
struct EXCEPTION_POINTERS {
  EXCEPTION_RECORD* ExceptionRecord;
  CONTEXT* ContextRecord;
};
using PVECTORED_EXCEPTION_HANDLER = LONG (*)(EXCEPTION_POINTERS*);
inline void* AddVectoredExceptionHandler(ULONG, PVECTORED_EXCEPTION_HANDLER) { return nullptr; }
inline uint16_t RtlCaptureStackBackTrace(DWORD, DWORD, void**, DWORD*) { return 0; }
inline BOOL GetThreadContext(HANDLE, CONTEXT*) { return FALSE; }

// Windows: an HWND is the SDL window (rex::ui::WindowSDL::GetNativeWindowHandle
// on macOS). Sizes are in pixels, as the swapchain's.
#include <SDL3/SDL.h>
using HMONITOR = void*;
using HRESULT = int32_t;
#define S_OK 0
#define SUCCEEDED(hr) (HRESULT(hr) >= 0)
#define FAILED(hr) (HRESULT(hr) < 0)
#define WM_CLOSE 0x0010
#define SWP_NOSIZE 0x0001
#define SWP_NOMOVE 0x0002
#define SWP_NOZORDER 0x0004
#define SWP_NOACTIVATE 0x0010
#define MONITOR_DEFAULTTONULL 0
#define MONITOR_DEFAULTTOPRIMARY 1
#define MONITOR_DEFAULTTONEAREST 2
#define ENUM_CURRENT_SETTINGS (DWORD(-1))
struct MONITORINFOEXW {
  DWORD cbSize;
  RECT rcMonitor;
  RECT rcWork;
  DWORD dwFlags;
  wchar_t szDevice[32];
};
struct DEVMODEW {
  WORD dmSize;
  DWORD dmDisplayFrequency;
};
using DPI_AWARENESS_CONTEXT = void*;
inline SDL_Window* win32_compat_window(HWND hwnd) { return static_cast<SDL_Window*>(hwnd); }
inline BOOL IsWindow(HWND hwnd) { return hwnd != nullptr; }
inline BOOL IsIconic(HWND hwnd) {
  return hwnd && (SDL_GetWindowFlags(win32_compat_window(hwnd)) & SDL_WINDOW_MINIMIZED) != 0;
}
inline BOOL IsWindowVisible(HWND hwnd) {
  return hwnd && (SDL_GetWindowFlags(win32_compat_window(hwnd)) & SDL_WINDOW_HIDDEN) == 0;
}
inline BOOL GetClientRect(HWND hwnd, RECT* rect) {
  int w = 0, h = 0;
  if (!hwnd || !SDL_GetWindowSizeInPixels(win32_compat_window(hwnd), &w, &h)) return FALSE;
  *rect = RECT{0, 0, w, h};
  return TRUE;
}
inline BOOL GetWindowRect(HWND hwnd, RECT* rect) {
  int x = 0, y = 0;
  if (!GetClientRect(hwnd, rect)) return FALSE;
  SDL_GetWindowPosition(win32_compat_window(hwnd), &x, &y);
  rect->left += x; rect->right += x; rect->top += y; rect->bottom += y;
  return TRUE;
}
// Pixel sizes, converted to the window's points.
inline BOOL SetWindowPos(HWND hwnd, HWND, int, int, int w, int h, UINT flags) {
  if (!hwnd) return FALSE;
  if (flags & SWP_NOSIZE) return TRUE;
  const float scale = std::max(SDL_GetWindowPixelDensity(win32_compat_window(hwnd)), 1.0f);
  return SDL_SetWindowSize(win32_compat_window(hwnd), int(w / scale + 0.5f), int(h / scale + 0.5f));
}
inline BOOL PostMessageW(HWND hwnd, UINT message, uintptr_t, intptr_t) {
  if (!hwnd || message != WM_CLOSE) return FALSE;
  SDL_Event event{};
  event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
  event.window.windowID = SDL_GetWindowID(win32_compat_window(hwnd));
  return SDL_PushEvent(&event);
}
inline HMONITOR MonitorFromWindow(HWND, DWORD) { return nullptr; }
inline BOOL GetMonitorInfoW(HMONITOR, MONITORINFOEXW*) { return FALSE; }
inline BOOL EnumDisplaySettingsW(const wchar_t*, DWORD, DEVMODEW*) { return FALSE; }
inline UINT GetDpiForWindow(HWND hwnd) {
  return hwnd ? UINT(96 * SDL_GetWindowDisplayScale(win32_compat_window(hwnd))) : 96;
}
inline DPI_AWARENESS_CONTEXT GetWindowDpiAwarenessContext(HWND) { return nullptr; }
inline int GetAwarenessFromDpiAwarenessContext(DPI_AWARENESS_CONTEXT) { return 2; }

// Message boxes of fatal stops: the log and stderr carry the text on macOS.
#define MB_OK 0x0
#define MB_ICONERROR 0x10
#define MB_TOPMOST 0x40000
#define MB_SETFOREGROUND 0x10000
inline int MessageBoxW(HWND, const wchar_t* text, const wchar_t* caption, UINT) {
  const std::string body = win32_compat::Utf8(text), title = win32_compat::Utf8(caption);
  std::fprintf(stderr, "%s: %s\n", title.c_str(), body.c_str());
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title.c_str(), body.c_str(), nullptr);
  return 1;
}
inline BOOL TerminateProcess(HANDLE process, UINT code) {
  if (process == GetCurrentProcess()) _exit(int(code));
  return FALSE;
}
// Debug hotkeys of the interpolation (F7, F8): not read on macOS.
#define VK_F7 0x76
#define VK_F8 0x77
inline int16_t GetAsyncKeyState(int) { return 0; }

#define VK_F9 0x78
#define VK_F10 0x79
using MONITORINFO = MONITORINFOEXW;

// Window titles (REX_NATIVE_FPS_TITLE), on the UI thread as the callers run.
inline int GetWindowTextW(HWND hwnd, wchar_t* text, int size) {
  if (!hwnd || size <= 0) return 0;
  const char* title = SDL_GetWindowTitle(win32_compat_window(hwnd));
  int i = 0;
  for (; title && title[i] && i + 1 < size; ++i) text[i] = wchar_t(static_cast<unsigned char>(title[i]));
  text[i] = 0;
  return i;
}
inline BOOL SetWindowTextW(HWND hwnd, const wchar_t* text) {
  return hwnd && SDL_SetWindowTitle(win32_compat_window(hwnd), win32_compat::Utf8(text).c_str());
}
// Windows' %s of a wide format is a wide string: %ls on POSIX.
template <size_t N, typename... Args>
int swprintf_s(wchar_t (&buffer)[N], const wchar_t* format, Args... args) {
  std::wstring posix;
  for (const wchar_t* f = format; *f; ++f) {
    posix += *f;
    if (*f == L'%' && f[1] == L's') posix += L'l';
    else if (*f == L'%' && f[1] == L'%') posix += *++f;
  }
  return swprintf(buffer, N, posix.c_str(), args...);
}
template <size_t N>
int wcscat_s(wchar_t (&buffer)[N], const wchar_t* text) {
  const size_t used = wcslen(buffer);
  if (used + wcslen(text) + 1 > N) return 34;  // ERANGE
  wcscat(buffer, text);
  return 0;
}

// std::filesystem::path::c_str() is UTF-8 on POSIX.
inline HANDLE CreateFileW(const char* path, DWORD access, DWORD share, void* security, DWORD disposition,
                          DWORD flags, HANDLE templ) {
  std::wstring wide;
  for (const char* p = path; p && *p; ++p) wide += wchar_t(static_cast<unsigned char>(*p));
  // Non-ASCII bytes: open the UTF-8 path directly.
  for (const char* p = path; p && *p; ++p) {
    if (static_cast<unsigned char>(*p) >= 0x80) {
      int open_flags = (access & GENERIC_WRITE) ? ((access & GENERIC_READ) ? O_RDWR : O_WRONLY) : O_RDONLY;
      if (disposition == CREATE_NEW) open_flags |= O_CREAT | O_EXCL;
      else if (disposition == CREATE_ALWAYS) open_flags |= O_CREAT | O_TRUNC;
      else if (disposition == OPEN_ALWAYS) open_flags |= O_CREAT;
      const int fd = open(path, open_flags | O_CLOEXEC, 0644);
      if (fd < 0) return INVALID_HANDLE_VALUE;
      auto* file = new win32_compat::Object(win32_compat::Object::Kind::kFile);
      file->fd = fd;
      return file;
    }
  }
  return CreateFileW(wide.c_str(), access, share, security, disposition, flags, templ);
}

inline DWORD GetModuleFileNameW(HMODULE, wchar_t* path, DWORD size) {
  char utf8[4096] = {};
  uint32_t length = sizeof(utf8);
  if (_NSGetExecutablePath(utf8, &length) != 0 || size == 0) return 0;
  DWORD i = 0;
  for (; utf8[i] && i + 1 < size; ++i) path[i] = wchar_t(static_cast<unsigned char>(utf8[i]));
  path[i] = 0;
  return i;
}
