// Copies from guest memory for the native renderer without a system call.
//
// The PC port copies under __try, so an address without memory fails instead
// of faulting (gpu/common/guest_memory.h). macOS has no __try, and
// ReadProcessMemory (mach_vm_read_overwrite) is a Mach call at every read: the
// renderer reads the title's vertices, indices, constants and fence words
// thousands of times per frame, so the cost grew with what a scene draws.
// Here the copy is a plain memcpy; a fault inside it jumps back to the copy,
// which fails. The handler sits in front of the SDK's
// (exception_handler_posix.cpp, installed with the guest memory, before the
// renderer's first copy), which still gets every other fault: the write
// watches, the MMIO range.
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>

#include <cstddef>
#include <cstring>
#include <mutex>

namespace {
pthread_key_t guard_key;  // The copy of the thread in progress (its jmp_buf), else null.
struct sigaction previous_segv, previous_bus;
std::once_flag installed;

void Forward(const struct sigaction& previous, int signal, siginfo_t* info, void* context) {
  if (previous.sa_flags & SA_SIGINFO) {
    if (previous.sa_sigaction) previous.sa_sigaction(signal, info, context);
    return;
  }
  if (previous.sa_handler != SIG_DFL && previous.sa_handler != SIG_IGN) {
    previous.sa_handler(signal);
    return;
  }
  // No handler before this one: the access faults again, with the default action.
  struct sigaction fallback {};
  fallback.sa_handler = SIG_DFL;
  sigemptyset(&fallback.sa_mask);
  sigaction(signal, &fallback, nullptr);
}

void OnFault(int signal, siginfo_t* info, void* context) {
  if (auto* copy = static_cast<jmp_buf*>(pthread_getspecific(guard_key))) {
    // The kernel blocked the signal for this handler, which never returns.
    sigset_t faults;
    sigemptyset(&faults);
    sigaddset(&faults, SIGSEGV);
    sigaddset(&faults, SIGBUS);
    pthread_sigmask(SIG_UNBLOCK, &faults, nullptr);
    _longjmp(*copy, 1);
  }
  Forward(signal == SIGBUS ? previous_bus : previous_segv, signal, info, context);
}

void Install() {
  pthread_key_create(&guard_key, nullptr);
  struct sigaction action {};
  action.sa_sigaction = OnFault;
  action.sa_flags = SA_SIGINFO;
  sigemptyset(&action.sa_mask);
  sigaction(SIGSEGV, &action, &previous_segv);
  sigaction(SIGBUS, &action, &previous_bus);
}
}  // namespace

namespace gpu {
// gpu/common/guest_memory.h
bool GuardedCopy(void* out, const void* in, size_t size) {
  if (!size) return true;
  std::call_once(installed, Install);
  jmp_buf copy;
  if (_setjmp(copy)) {
    pthread_setspecific(guard_key, nullptr);
    return false;
  }
  pthread_setspecific(guard_key, &copy);
  std::memcpy(out, in, size);
  pthread_setspecific(guard_key, nullptr);
  return true;
}
}  // namespace gpu
