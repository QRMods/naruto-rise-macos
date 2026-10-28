// The module snapshot of the diagnostic journal, on macOS: the executable.
#pragma once
#include <mach-o/dyld.h>
#include "windows.h"

#define TH32CS_SNAPMODULE 0x00000008
#define TH32CS_SNAPMODULE32 0x00000010
struct MODULEENTRY32W {
  DWORD dwSize;
  wchar_t szModule[256];
  wchar_t szExePath[MAX_PATH];
};
inline HANDLE CreateToolhelp32Snapshot(DWORD, DWORD) {
  return new win32_compat::Object(win32_compat::Object::Kind::kFile);
}
inline BOOL Module32FirstW(HANDLE, MODULEENTRY32W* entry) {
  char path[MAX_PATH * 4] = {};
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) != 0) return FALSE;
  size_t i = 0;
  for (; path[i] && i + 1 < MAX_PATH; ++i) entry->szExePath[i] = wchar_t(static_cast<unsigned char>(path[i]));
  entry->szExePath[i] = 0;
  entry->szModule[0] = 0;
  return TRUE;
}
inline BOOL Module32NextW(HANDLE, MODULEENTRY32W*) { return FALSE; }
