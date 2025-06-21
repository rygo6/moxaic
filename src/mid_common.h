//////////////////////
//// Mid Common Header
//////////////////////
#pragma once

#include <stdio.h>

//////////
//// Types
typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;
typedef int8_t      i8;
typedef int16_t     i16;
typedef int32_t     i32;
typedef int64_t     i64;
typedef _Float16    f16;
typedef float       f32;
typedef double      f64;
typedef const char* str;

//////////////////
//// Debug Logging
////
extern void Panic(const char* file, int line, const char* message);
#define PANIC(_message) ({ Panic(__FILE__, __LINE__, _message); __builtin_unreachable(); })
// Check if the condition is 0=Success or 0=False
#define CHECK(_err, _message)                      \
	if (UNLIKELY(_err)) {                          \
		fprintf(stderr, "Error Code: %d\n", _err); \
		PANIC(_message);                           \
	}
#define LOG(...) printf(__VA_ARGS__)
#define LOG_ERROR(...) fprintf(stderr, "Error!!! " __VA_ARGS__)
//#define LOG_ERROR(...) printf("Error!!! " __VA_ARGS__)

////////////
//// Utility
////
#define HOT    __attribute__((hot))
#define CONCAT(_a, _b) #_a #_b
#define CACHE_ALIGN __attribute((aligned(64)))
#define INLINE __attribute__((always_inline)) static inline
#define EXTRACT_FIELD(_p, _field) auto _field = (_p)->_field
#define PACKED __attribute__((packed))
#define ALIGN(size) __attribute((aligned(size)))
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define COUNT(_array) (sizeof(_array) / sizeof(_array[0]))
#define FLAG(b) (1 << (b))
#define CONTAINS(_array, _count, _)        \
	({                                     \
		bool found = false;                \
		for (int i = 0; i < _count; ++i) { \
			if (_array[i] == _) {          \
				found = true;              \
				break;                     \
			}                              \
		}                                  \
		found;                             \
	})

//////////
//// Win32
////
#ifdef _WIN32

#include <windows.h>

static void LogWin32Error(HRESULT err)
{
	fprintf(stderr, "Win32 Error Code: 0x%08lX\n", err);
	char* errStr;
	if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
					  NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), (LPSTR)&errStr, 0, NULL)) {
		fprintf(stderr, "%s\n", errStr);
		LocalFree(errStr);
	}
}

#define CHECK_WIN32(condition, err) \
	if (UNLIKELY(!(condition))) {   \
		LogWin32Error(err);         \
		PANIC("Win32 Error!");      \
	}
// this seems like it should be more generic?
#define DX_CHECK(command)           \
	({                              \
		HRESULT hr = command;       \
		if (UNLIKELY(FAILED(hr))) { \
			LogWin32Error(hr);      \
			PANIC("DX Error!");     \
		}                           \
	})

#endif

//////////////////////////////
//// Mid Common Implementation
//////////////////////////////
#if defined(MID_COMMON_IMPLEMENTATION) || defined(MID_IDE_ANALYSIS)

//// Override for your own panic behaviour
#ifndef MID_PANIC_METHOD
#define MID_PANIC_METHOD
[[noreturn]] void Panic(const char* file, int line, const char* message)
{
	fprintf(stderr, "\n%s:%d Error! %s\n", file, line, message);
	__builtin_trap();
}
#endif

#endif