// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently

#pragma once

#ifdef _WIN32
#include <tchar.h>
#include <windows.h>
#else
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <thread>
#include <unistd.h>

// Windows basic types
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint64_t QWORD;
typedef uint32_t UINT;
typedef unsigned long ULONG;
typedef long LONG;
typedef int BOOL;
typedef void VOID;
typedef unsigned char UCHAR;
typedef unsigned short USHORT;

// Pointer types
typedef void *HANDLE;
typedef void *PVOID;
typedef void *LPVOID;
typedef const void *LPCVOID;
typedef BYTE *PBYTE;
typedef BYTE *LPBYTE;
typedef WORD *PWORD;
typedef WORD *LPWORD;
typedef DWORD *PDWORD;
typedef DWORD *LPDWORD;
typedef char *LPSTR;
typedef const char *LPCSTR;
typedef char TCHAR;
typedef char *LPTSTR;
typedef const char *LPCTSTR;
typedef UCHAR *PUCHAR;
typedef char *PCHAR;
typedef ULONG *PULONG;
typedef LONG *PLONG;
typedef LONG *LPLONG;
typedef void *LPOVERLAPPED;
typedef void *LPSECURITY_ATTRIBUTES;

// MSVC CRT safe string shims
template <size_t N>
inline void strcpy_s(char (&dst)[N], const char *src) {
  strncpy(dst, src, N - 1);
  dst[N - 1] = '\0';
}
inline void strcpy_s(char *dst, size_t size, const char *src) {
  strncpy(dst, src, size - 1);
  dst[size - 1] = '\0';
}

template <size_t N>
inline void strcat_s(char (&dst)[N], const char *src) {
  size_t len = strlen(dst);
  if (len < N) {
    strncpy(dst + len, src, N - len - 1);
    dst[N - 1] = '\0';
  }
}
inline void strcat_s(char *dst, size_t size, const char *src) {
  size_t len = strlen(dst);
  if (len < size) {
    strncpy(dst + len, src, size - len - 1);
    dst[size - 1] = '\0';
  }
}

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#define WINAPI
#define __stdcall
#define __cdecl
#ifndef __declspec
#define __declspec(x)
#endif

// Sleep shim
inline void Sleep(DWORD dwMilliseconds) {
  std::this_thread::sleep_for(std::chrono::milliseconds(dwMilliseconds));
}

// TCHAR string shims
#ifndef _T
#define _T(x) x
#endif
#ifndef TEXT
#define TEXT(x) x
#endif

#define _tprintf printf
#define _ftprintf fprintf
#define _stprintf sprintf
#define _snprintf_s(buf, size, count, fmt, ...)                                \
  snprintf(buf, size, fmt, ##__VA_ARGS__)
#define _stprintf_s snprintf
#define _stscanf sscanf

#define _tcslen strlen
#define _tcscmp strcmp
#define _tcsncmp strncmp
#define _tcsicmp strcasecmp
#define _tcsnicmp strncasecmp
#define _tcscpy strcpy
#define _tcsncpy strncpy
#define _tcscat strcat
#define _tcsncat strncat
#define _tcschr strchr
#define _tcsrchr strrchr
#define _tcsstr strstr
#define _tcstok strtok
#define _tcstok_s strtok_r

#define _tcscpy_s(dst, size, src) strncpy(dst, src, size)
#define _tcsncpy_s(dst, dstsize, src, count)                                   \
  strncpy(dst, src, (count) < (dstsize) ? (count) : (dstsize))
#define _tcsncat_s(dst, dstsize, src, count)                                   \
  strncat(dst, src, (count) < (dstsize) ? (count) : (dstsize))

#define _tfopen fopen
#define _tmain main

#endif

