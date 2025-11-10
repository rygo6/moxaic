/*
 * Mid Common
 */

/*
	Mid-Level C Style

 	C11 + GCC/Clang Extensions. Is not portable to MSVC.

	variable names can signify the type with a character prefix
	iValue = Data structure index. Handle but without data structure metadata.
	hValue = Data structure handle. Index but with data structure metadata.
	pValue = Data structure pointer.
 	valueId = Categorical identifier.
 	XrType = Opaque Handle. Data structure handle in u64 with additional metadata.

 	primitive types are snake_case

	extern struct types are PrefixPascalCase
 	static struct types are PascalCase

 	macros and constants are CAPITAL_SNAKE_CASE

 	static methods are PascalCase
 	extern methods are prefixCamelCase with a prefix less than 3 chars

 	global fields should be in anonymous struct to namespace

 	auto_t can be used if the name of the type is obvious from somewhere else in the assignment line

 	typedef of abbreviated type names should be used if possible
 	abbreviations should be 8 or less chars

	All APIs are assert by default with no error handling and no branching. Or should they check? No I think assert so then by default you are implementing something expected to be correct by default. Additional error handling is extra.
 	Any error handling should be supplemental and optional to allow implementing the branch higher up.

    REQUIRE = Panic
    ASSERT = release compile out
    CHECK = error log return 0
    TRY = error log goto Error

*/

#ifndef MID_COMMON_H
#define MID_COMMON_H

#include <stdio.h>
#include <stdatomic.h>
#include <assert.h>
#include <string.h>

/*
 * Attributes
 */

#define UNUSED            __attribute__((unused))
#define FALLTHROUGH       __attribute__((fallthrough))
#define HOT               __attribute__((hot))
#define CACHE_ALIGN       __attribute__((aligned(64)))
#define INLINE            __attribute__((always_inline)) static inline
#define TRANSPARENT_UNION __attribute__((transparent_union))
#define PACKED            __attribute__((packed))
#define ALIGN(size)       __attribute__((aligned(size)))
#define NO_RETURN         __attribute__((noreturn))

/*
 * Types
 */
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

typedef _Atomic uint64_t a_u64;

#define auto_t __auto_type

/*
 * Debug Log
 */
typedef enum PACKED MidResult {
	// These align with VkResult
	MID_SUCCESS          = 0,
	MID_EMPTY            = 20001,
	MID_LIMIT_REACHED    = 20002,
	MID_RESULT_FINALIZED = 20003,
	MID_ERROR_UNKNOW     = -13,
	MID_RESULT_MAX_ENUM  = 0x7FFFFFFF
} MidResult;

// TODO
// REQUIRE = Panic
// ASSERT = release compile out
// CHECK = error log return 0
// TRY = error log goto ExitError

extern void Panic(const char* file, int line, const char* message);
#define PANIC(_message) ({ Panic(__FILE__, __LINE__, _message); __builtin_unreachable(); })

// Check if the condition is 1=True or 0=False TODO this needs to be REQUIRE_TRUE
#define REQUIRE(_state, _message) if (UNLIKELY(!(_state))) PANIC(_message);

#define ASSERT(_condition, ...) ((_condition) || (_assert((#_condition " " __VA_ARGS__), __FILE__, __LINE__), 0))
#define STATIC_ASSERT(_condition, ...) _Static_assert(_condition, #_condition " " #__VA_ARGS__)

// Check if the condition is 0=Success or 1=Fail
#define CHECK(_err, _message)                \
	if (UNLIKELY(_err)) {                    \
		LOG_ERROR("Error Code: %d\n", _err); \
		PANIC(_message);                     \
	}

// Check if the condition is 1=True or 0=False
#define CHECK_TRUE(_err, _message)           \
	if (UNLIKELY(!(_err))) {                 \
		LOG_ERROR("Error Code: %d\n", _err); \
		PANIC(_message);                     \
	}

#define REQUIRE_FALSE(_a, _b, ...)  REQUIRE_EQUAL(_a, false, __VA_ARGS__)

#define REQUIRE_EQUAL(_a, _b, ...)                               \
	if (UNLIKELY(((_a) != (_b)))) {                              \
		PANIC("Expected: " #_a " == " #_b " " __VA_ARGS__ "\n"); \
	}

#define REQUIRE_NOT_EQUAL(_a, _b, ...)                           \
	if (UNLIKELY(((_a) == (_b)))) {                              \
		PANIC("Expected: " #_a " != " #_b " " __VA_ARGS__ "\n"); \
	}

#define LOG(...) printf(__VA_ARGS__)
#define LOG_ERROR(...) fprintf(stdout, "Error!!! " __VA_ARGS__)

#define LOG_ONCE(...)               \
	({                              \
		static int _logged = false; \
		if (!_logged) {             \
			_logged = true;         \
			LOG(__VA_ARGS__);       \
		}                                 \
	})

#define STRING_CASE(_) case _: return #_

/*
 * Utility
 */

#define IS_POWER_OF_2(_v)          (((_v) != 0) && (((_v) & ((_v) - 1)) == 0))
#define SHIFT_DIVIDE(_v, _divisor) ({ static_assert(IS_POWER_OF_2(_divisor)); ((_v) >> __builtin_ctz(_divisor)); })
#define TYPES_EQUAL(a, b)          __builtin_types_compatible_p(__typeof__(a), __typeof__(b))
#define CONCAT(_a, _b)             #_a #_b
#define EXTRACT_FIELD(_p, _field)  __typeof__((_p)->_field) _field = (_p)->_field
#define LIKELY(x)                  __builtin_expect(!!(x), 1)
#define UNLIKELY(x)                __builtin_expect(!!(x), 0)
#define COUNT(_array)              (sizeof(_array) / sizeof(_array[0]))
#define FLAG(b)                    (1 << (b))
#define ZERO(_p)                   memset((_p), 0, sizeof(*_p))
#define IS_STRUCT_P_ZEROED(_p)     (memcmp(_p, &(typeof(*_p)){0}, sizeof(*_p)) == 0)

#define XMALLOC_P(_p) \
	_p = malloc(sizeof(*_p)); \
	REQUIRE(_p, #_p " XMALLOC Fail!");

#define XMALLOC_ZERO_P(_p) \
	_p = malloc(sizeof(*_p)); \
	REQUIRE(_p, #_p " XMALLOC Fail!"); \
	ZERO(_p);

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

#define ATOMIC_FENCE_SCOPE                                                \
	for (bool _done = (atomic_thread_fence(memory_order_acquire), false); \
		 !_done;                                                          \
		 _done = true, atomic_thread_fence(memory_order_release))

#define MALLOC_SCOPE(_ptr)             \
	for (_ptr = malloc(sizeof(*_ptr)); \
		_ptr != NULL;                  \
		free(_ptr), _ptr = NULL)

#define MALLOC_SCOPE_ZEROED(_ptr)                                      \
	for (_ptr = malloc(sizeof(*_ptr)), memset(_ptr, 0, sizeof(*_ptr)); \
		_ptr != NULL;                                                  \
		free(_ptr), _ptr = NULL)


/*
 * Win32
 */
#ifdef _WIN32

#include <windows.h>

static void LogWin32Error(HRESULT err)
{
	LOG_ERROR("Win32 Error Code: 0x%08lX\n", err);
	char* errStr;
	if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
					  NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), (LPSTR)&errStr, 0, NULL)) {
		LOG("%s\n", errStr);
		LocalFree(errStr);
	}
}

#define CHECK_WIN32(command)               \
	({                                     \
		bool error = command;              \
		if (UNLIKELY(!error)) {            \
			LogWin32Error(GetLastError()); \
			PANIC("Win32 Error!");         \
		}                                  \
	})

#define DX_CHECK(command)           \
	({                              \
		HRESULT hr = command;       \
		if (UNLIKELY(FAILED(hr))) { \
			LogWin32Error(hr);      \
			PANIC("DX Error!");     \
		}                           \
	})

#endif

#endif // MID_COMMON_H

/*
 * Mid Common Implementation
 */
#if defined(MID_COMMON_IMPLEMENTATION) || defined(MID_IDE_ANALYSIS)

/* Panic Trap */
#ifndef MID_PANIC_METHOD
#define MID_PANIC_METHOD
void NO_RETURN Panic(const char* file, int line, const char* message)
{
	LOG("\n%s:%d PANIC! %s\n", file, line, message);
	__builtin_trap();
}
#endif // MID_PANIC_METHOD

#undef MID_COMMON_IMPLEMENTATION
#endif // MID_COMMON_IMPLEMENTATION