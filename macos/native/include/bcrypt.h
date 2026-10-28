// The Windows CNG hash calls the native renderer makes (SHA-256 of files and
// copies for its diagnostics), on CommonCrypto.
#pragma once
#include <CommonCrypto/CommonDigest.h>
#include "windows.h"

using PUCHAR = unsigned char*;
using BCRYPT_ALG_HANDLE = void*;
using BCRYPT_HASH_HANDLE = CC_SHA256_CTX*;
#define BCRYPT_SUCCESS(status) ((status) >= 0)
#define BCRYPT_SHA256_ALGORITHM L"SHA256"

inline NTSTATUS BCryptOpenAlgorithmProvider(BCRYPT_ALG_HANDLE* algorithm, const wchar_t*, const wchar_t*, ULONG) {
  *algorithm = reinterpret_cast<void*>(uintptr_t(1));
  return 0;
}
inline NTSTATUS BCryptCloseAlgorithmProvider(BCRYPT_ALG_HANDLE, ULONG) { return 0; }
inline NTSTATUS BCryptCreateHash(BCRYPT_ALG_HANDLE, BCRYPT_HASH_HANDLE* hash, PUCHAR, ULONG, PUCHAR, ULONG, ULONG) {
  *hash = new CC_SHA256_CTX;
  CC_SHA256_Init(*hash);
  return 0;
}
inline NTSTATUS BCryptHashData(BCRYPT_HASH_HANDLE hash, PUCHAR data, ULONG size, ULONG) {
  CC_SHA256_Update(hash, data, size);
  return 0;
}
inline NTSTATUS BCryptFinishHash(BCRYPT_HASH_HANDLE hash, PUCHAR digest, ULONG size, ULONG) {
  if (size < CC_SHA256_DIGEST_LENGTH) return -1;
  CC_SHA256_Final(digest, hash);
  return 0;
}
inline NTSTATUS BCryptDestroyHash(BCRYPT_HASH_HANDLE hash) {
  delete hash;
  return 0;
}
