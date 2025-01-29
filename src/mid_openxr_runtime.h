//////////////////////
//// Mid OpenXR Header
//////////////////////
#pragma once

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef _WIN32
	#define XR_USE_PLATFORM_WIN32
	#define XR_USE_GRAPHICS_API_D3D11

	#define WIN32_LEAN_AND_MEAN
	#define NOCOMM
	#define NOSERVICE
	#define NOCRYPT
	#define NOMCX
	#include <windows.h>
	#include <synchapi.h>

	#define D3D11_NO_HELPERS
	#define CINTERFACE
	#define COBJMACROS
	#define WIDL_C_INLINE_WRAPPERS
	#include <initguid.h>
	#include <d3d11_1.h>
	#include <d3d11_3.h>
	#include <d3d11_4.h>
	#include <dxgi.h>
	#include <dxgi1_4.h>
#endif

#define XR_USE_GRAPHICS_API_VULKAN
#include <vulkan/vulkan.h>

#define XR_USE_GRAPHICS_API_OPENGL
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_reflection.h>
#include <openxr/openxr_loader_negotiation.h>

#include "mid_bit.h"

#ifdef MID_IDE_ANALYSIS
	#undef XRAPI_CALL
	#define XRAPI_CALL
#endif

///////////////////////
//// Common Utility
////
#ifndef COUNT
#define COUNT(_array) (sizeof(_array) / sizeof(_array[0]))
#endif

#if defined(__GNUC__)
#define EXPORT __attribute__((visibility("default")))
#elif defined(_MSC_VER)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

#define LOG(...) printf(__VA_ARGS__)
//#define LOG_ERROR(_format, ...) fprintf(stderr, "Error!!! " _format, __VA_ARGS__);
#define LOG_ERROR(...) printf("Error!!! " __VA_ARGS__)

#ifdef _WIN32
#ifndef MID_WIN32_DEBUG
#define MID_WIN32_DEBUG
static void LogWin32Error(HRESULT err)
{
	LOG_ERROR("Win32 Code: 0x%08lX\n", err);
	char* errStr;
	if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
					  NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), (LPSTR)&errStr, 0, NULL)) {
		LOG_ERROR("%s\n", errStr);
		LocalFree(errStr);
	}
}
#define CHECK_WIN32(condition, err)          \
	if (__builtin_expect(!(condition), 0)) { \
		LogWin32Error(err);                  \
		PANIC("Win32 Error!");               \
	}
#define DX_CHECK(command)               \
	({                                  \
		HRESULT hr = command;           \
		CHECK_WIN32(SUCCEEDED(hr), hr); \
	})
#endif
#endif

// this need to go in mid common
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef _Float16 f16;
typedef float    f32;
typedef double   f64;

////////////////////////
//// Method Declarations
////
typedef enum XrGraphicsApi {
	XR_GRAPHICS_API_OPENGL,
	XR_GRAPHICS_API_OPENGL_ES,
	XR_GRAPHICS_API_VULKAN,
	XR_GRAPHICS_API_D3D11_4,
	XR_GRAPHICS_API_COUNT,
} XrGraphicsApi;

void midXrInitialize();

// Session may claim an index from implementation or reference state in the implementation
typedef uint16_t XrSessionIndex;
void xrClaimSessionIndex(XrSessionIndex* pSessionIndex);

void xrReleaseSessionIndex(XrSessionIndex sessionIndex);
void xrGetReferenceSpaceBounds(XrSessionIndex sessionIndex, XrExtent2Df* pBounds);
void xrClaimFramebufferImages(XrSessionIndex sessionIndex, int imageCount, HANDLE* pHandle);

void xrGetSessionTimeline(XrSessionIndex sessionIndex, HANDLE* pHandle);
void xrSetSessionTimelineValue(XrSessionIndex sessionIndex, uint64_t timelineValue);

void xrClaimSwapPoolImage(XrSessionIndex sessionIndex, uint8_t* pIndex);
void xrReleaseSwapPoolImage(XrSessionIndex sessionIndex, uint8_t index);

void xrGetCompositorTimeline(XrSessionIndex sessionIndex, HANDLE* pHandle);
void xrSetInitialCompositorTimelineValue(XrSessionIndex sessionIndex, uint64_t timelineValue);
void xrGetCompositorTimelineValue(XrSessionIndex sessionIndex, bool synchronized, uint64_t* pTimelineValue);
void xrProgressCompositorTimelineValue(XrSessionIndex sessionIndex, uint64_t timelineValue, bool synchronized);

void xrGetHeadPose(XrSessionIndex sessionIndex, XrVector3f* pEuler, XrVector3f* pPos);

typedef struct XrEyeView {
	XrVector3f euler;
	XrVector3f position;
	XrVector2f fovRad;
	XrVector2f upperLeftClip;
	XrVector2f lowerRightClip;
} XrEyeView;
void xrGetEyeView(XrSessionIndex sessionIndex, uint8_t viewIndex, XrEyeView *pEyeView);

XrTime xrGetFrameInterval(XrSessionIndex sessionIndex, bool synchronized);

static inline XrTime xrHzToXrTime(double hz)
{
	return (XrTime)(hz / 1e9);
}

/////////////////////
//// OpenXR Constants
////
#define XR_FORCE_STEREO_TO_MONO

#define XR_DEFAULT_WIDTH   1024
#define XR_DEFAULT_HEIGHT  1024
#define XR_DEFAULT_SAMPLES 1

#define XR_SWAP_COUNT 2

#define XR_OPENGL_MAJOR_VERSION 4
#define XR_OPENGL_MINOR_VERSION 6


/////////////////
//// OpenXR Types
////
typedef float XrMat4 __attribute__((vector_size(sizeof(float) * 16)));

typedef enum XrStructureTypeExt {
	XR_TYPE_FRAME_BEGIN_SWAP_POOL_EXT = 2000470000,
	XR_TYPE_SUB_VIEW = 2000480000,
	XR_TYPE_SPACE_BOUNDS = 2000490000,
} XrStructureTypeExt;

typedef enum XrReferenceSpaceTypeExt {
	XR_REFERENCE_SPACE_TYPE_LOCAL_BOUNDED = 2000115000,
} XrReferenceSpaceTypeExt;

typedef struct XrSubView {
	XrStructureType    type;
	void* XR_MAY_ALIAS next;
	XrRect2Di          imageRect;
	uint32_t           imageArrayIndex;
} XrSubView;

typedef struct XrSpaceBounds {
	XrStructureType    type;
	void* XR_MAY_ALIAS next;
	XrRect2Di          imageRect;
} XrSpaceBounds;

typedef enum XrSwapType : uint8_t {
	XR_SWAP_TYPE_MONO,
	XR_SWAP_TYPE_STEREO,
	XR_SWAP_TYPE_STEREO_TEXTURE_ARRAY,
	XR_SWAP_TYPE_STEREO_DOUBLE_WIDE,
	XR_SWAP_TYPE_COUNT,
} XrSwapType;

//typedef struct XrFrameEndInfo {
//	XrStructureType    type;
//	void* XR_MAY_ALIAS next;
//	XrRect2Di          imageRect;
//	uint32_t       imageArrayIndex;
//} XrFrameEndInfo;

// maybe?
//typedef struct XrFrameBeginSwapPoolInfo {
//	XrStructureType             type;
//	const void* XR_MAY_ALIAS    next;
//} XrFrameBeginSwapPoolInfo;

#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#define XR_UNQUALIFIED_FAILED(result) ((result) != 0)
#define XR_CHECK(_command)                             \
	({                                                 \
		XrResult result = _command;                    \
		if (UNLIKELY(XR_UNQUALIFIED_FAILED(result))) { \
			return result;                             \
		}                                              \
	})

///////////
//// Handle
////
typedef u16 map_handle;
typedef u16 map_key;

typedef u16 block_handle;
typedef u32 block_key;

#define HANDLE_INDEX_BIT_COUNT 12
#define HANDLE_INDEX_MASK      0x0FFF
#define HANDLE_INDEX_MAX       ((1u << HANDLE_INDEX_BIT_COUNT) - 1)

#define HANDLE_INDEX(handle)            (handle & HANDLE_INDEX_MASK)
#define HANDLE_INDEX_SET(handle, value) ((handle & HANDLE_GENERATION_MASK) | (value & HANDLE_INDEX_MASK))

#define HANDLE_GENERATION_BIT_COUNT 4
#define HANDLE_GENERATION_MASK      0xF000
#define HANDLE_GENERATION_MAX       ((1u << HANDLE_GENERATION_BIT_COUNT) - 1)

#define HANDLE_GENERATION(handle)            ((handle & HANDLE_GENERATION_MASK) >> HANDLE_INDEX_BIT_COUNT)
#define HANDLE_GENERATION_SET(handle, value) (value << HANDLE_INDEX_BIT_COUNT) | HANDLE_INDEX(handle)

// 0 generation to signify it as invalid. Max index to make it assert errors if still used.
#define HANDLE_DEFAULT ((0 & HANDLE_GENERATION_MASK) | (UINT16_MAX & HANDLE_INDEX_MASK))

#define HANDLE_VALID(handle) (HANDLE_GENERATION(handle) != 0)

#define HANDLE_GENERATION_INCREMENT(handle) ({          \
	u8 g = HANDLE_GENERATION(handle);                   \
	g = g == HANDLE_GENERATION_MAX ? 1 : (g + 1) & 0xF; \
	HANDLE_GENERATION_SET(handle, g);                   \
})

#define HANDLE_CHECK(handle, error) \
	if (!HANDLE_VALID(handle)) {    \
		LOG_ERROR(#error "\n");     \
		return error;               \
	}

//////////
//// Block
////
#define BLOCK_DECL(type, n)       \
	struct {                      \
		BITSET_DECL(occupied, n); \
		block_key keys[n];        \
		type      blocks[n];      \
		uint8_t   generations[n]; \
	}

static block_handle ClaimBlock(int occupiedCount, bitset_t* pOccupiedSet, block_key* pKeys, uint8_t* pGenerations, uint32_t key)
{
	int i = BitScanFirstZero(occupiedCount, pOccupiedSet);
	if (i == -1) return HANDLE_DEFAULT;
	BITSET(pOccupiedSet, i);
	pKeys[i] = key;
	pGenerations[i] = pGenerations[i] == HANDLE_GENERATION_MAX ? 1 : (pGenerations[i] + 1) & 0xF;
	return HANDLE_GENERATION_SET(i, pGenerations[i]);
}
static block_handle FindBlockByHash(int hashCount, block_key* pHashes, uint8_t* pGenerations, block_key hash)
{
	for (int i = 0; i < hashCount; ++i) {
		if (pHashes[i] == hash)
			return HANDLE_GENERATION_SET(i, pGenerations[i]);
	}
	return HANDLE_DEFAULT;
}

#define BLOCK_CLAIM(block, key)                                                                                           \
	({                                                                                                                    \
		int _handle = ClaimBlock(sizeof(block.occupied), (bitset_t*)&block.occupied, block.keys, block.generations, key); \
		assert(_handle >= 0 && #block ": Claiming handle. Out of capacity.");                                             \
		(block_handle) _handle;                                                                                           \
	})
#define BLOCK_RELEASE(block, handle)                                                                                                                       \
	({                                                                                                                                                     \
		assert(HANDLE_INDEX(handle) >= 0 && HANDLE_INDEX(handle) < block.blocks + COUNT(block.blocks) && #block ": Releasing SLAB Handle. Out of range."); \
		assert(BITSET(block.occupied, HANDLE_INDEX(handle)) && #block ": Releasing SLAB handle. Should be occupied.");                                     \
		BITCLEAR(block.occupied, (int)HANDLE_INDEX(handle));                                                                                               \
	})
#define BLOCK_FIND(block, hash)                                                   \
	({                                                                            \
		assert(hash != 0);                                                        \
		FindBlockByHash(sizeof(block.keys), block.keys, block.generations, hash); \
	})
#define BLOCK_HANDLE(block, p)                                                                                                \
	({                                                                                                                        \
		assert(p >= block.blocks && p < block.blocks + COUNT(block.blocks) && #block ": Getting SLAB handle. Out of range."); \
		HANDLE_GENERATION_SET((p - block.blocks), block.generations[(p - block.blocks)]);                                     \
	})
#define BLOCK_KEY(block, p)                                                                                                \
	({                                                                                                                     \
		assert(p >= block.blocks && p < block.blocks + COUNT(block.blocks) && #block ": Getting SLAB key. Out of range."); \
		block.keys[p - block.blocks];                                                                                      \
	})
#define BLOCK_PTR(block, handle)                                                                                                       \
	({                                                                                                                                 \
		assert(HANDLE_INDEX(handle) >= 0 && HANDLE_INDEX(handle) < COUNT(block.blocks) && #block ": Getting SLAB ptr. Out of range."); \
		&block.blocks[HANDLE_INDEX(handle)];                                                                                           \
	})


////////
//// Map
////
typedef struct MapBase {
	uint32_t  count;
	block_key keys[];
	// keys could be any size
	//	block_handle handles[];
} MapBase;

#define MAP_DECL(n)              \
	struct {                     \
		uint32_t     count;      \
		block_key    keys[n];    \
		block_handle handles[n]; \
	}

static inline map_handle MapAdd(int capacity, MapBase* pMap, block_handle handle, map_key key)
{
	int i = pMap->count;
	if (i == capacity)
		return HANDLE_DEFAULT;
	block_handle* pHandles = (block_handle*)(pMap->keys + capacity);
	pHandles[i] = handle;
	pMap->keys[i] = key;
	pMap->count++;
	return HANDLE_GENERATION_INCREMENT(i);
}

static inline map_handle MapFind(int capacity, MapBase* pMap, map_key key)
{
	for (int i = 0; i < pMap->count; ++i) {
		if (pMap->keys[i] == key) {
			block_handle* pHandles = (block_handle*)(pMap->keys + capacity);
			return pHandles[i];
		}
	}
	return HANDLE_DEFAULT;
}

#define MAP_ADD(map, handle, key) MapAdd(COUNT(map.keys), (MapBase*)&map, handle, key)
#define MAP_FIND(map, key) MapFind(COUNT(map.keys), (MapBase*)&map, key)

////////
//// Set
////
typedef struct SetBase {
	u32           count;
	block_handle* handles;
} SetBase;
#define SET_DECL(n)              \
	struct CACHE_ALIGN {         \
		uint32_t     count;      \
		block_handle handles[n]; \
	}

/////////////////
//// OpenXR Types
////
#define XR_PATH_CAPACITY 256
typedef struct Path {
	char string[XR_MAX_PATH_LENGTH];
} Path;

#define XR_BINDINGS_CAPACITY 256
typedef struct Binding {
	block_handle hPath;
	int (*func)(void*);
} Binding;

#define XR_MAX_BINDINGS      16
#define XR_INTERACTION_PROFILE_CAPACITY 16
typedef struct InteractionProfile {
	XrPath path;
	MAP_DECL(XR_MAX_BINDINGS) bindings;
} InteractionProfile;
typedef InteractionProfile* XrInteractionProfile;

#define XR_SUBACTION_CAPACITY 256
typedef struct SubactionState {
	// need to understand this priority again
	//	uint32_t lastSyncedPriority;

	// actual layout from OXR to memcpy
	f32      currentState;
	XrBool32 changedSinceLastSync;
	XrTime   lastChangeTime;
	XrBool32 isActive;
} SubactionState;

#define XR_MAX_SUBACTION_PATHS 2
#define XR_ACTION_CAPACITY     128
typedef struct Action {
	block_handle hActionSet;

	u8           countSubactions;
	block_handle hSubactionStates[XR_MAX_SUBACTION_PATHS];
	block_handle hSubactionBindings[XR_MAX_SUBACTION_PATHS];
	block_handle hSubactionPaths[XR_MAX_SUBACTION_PATHS];

	XrActionType actionType;
	char         actionName[XR_MAX_ACTION_NAME_SIZE];
	char         localizedActionName[XR_MAX_LOCALIZED_ACTION_NAME_SIZE];
} Action;

#define XR_ACTION_SET_CAPACITY 64
#define XR_MAX_ACTION_SET_STATES 64
typedef struct ActionSet {
	MAP_DECL(XR_MAX_ACTION_SET_STATES) actions;
	MAP_DECL(XR_MAX_ACTION_SET_STATES) states;

	block_handle attachedToSession;
	char         actionSetName[XR_MAX_ACTION_SET_NAME_SIZE];
	char         localizedActionSetName[XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE];
	u32          priority;
} ActionSet;

#define XR_SPACE_CAPACITY 128
typedef struct Space {
	block_handle         hSession;
	XrReferenceSpaceType referenceSpaceType;
	XrPosef              poseInReferenceSpace;
} Space;

#define XR_SWAPCHAIN_CAPACITY 8
typedef struct Swapchain {
	block_handle hSession;

	uint8_t swapIndex;
	union {
		struct {
			GLuint texture;
			GLuint memObject;
		} gl;
		struct {
			ID3D11Texture2D* texture;
//			IDXGIKeyedMutex* keyedMutex;
//			ID3D11RenderTargetView* rtView;
		} d3d11;
	} color[XR_SWAP_COUNT];

	XrSwapchainCreateFlags createFlags;
	XrSwapchainUsageFlags  usageFlags;
	i64                    format;
	u32                    sampleCount;
	u32                    width;
	u32                    height;
	u32                    faceCount;
	u32                    arraySize;
	u32                    mipCount;
} Swapchain;

#define XR_SESSIONS_CAPACITY 8
#define XR_MAX_SPACES 8
typedef struct Session {
	//// Info
	XrSessionIndex          index;
	XrSystemId              systemId;
	XrViewConfigurationType primaryViewConfigurationType;

	//// Swap
	CACHE_ALIGN
	block_handle hSwap;
	XrSwapType   swapType;

	//// Timing
	CACHE_ALIGN
	XrTime frameWaited;
	XrTime frameBegan;
	XrTime frameEnded;
	XrTime predictedDisplayTimes[3];
	XrTime lastBeginDisplayTime;
	XrTime lastDisplayTime;
	u32    synchronizedFrameCount;
	u64    sessionTimelineValue;

	//// Maps
	CACHE_ALIGN
	MAP_DECL(XR_MAX_ACTION_SET_STATES) actionSets;
	MAP_DECL(XR_MAX_SPACES) referenceSpaces;
	MAP_DECL(XR_MAX_SPACES) actionSpaces;

	//// State
	CACHE_ALIGN
	bool running: 1;
	bool exiting: 1;

	//// Events
	CACHE_ALIGN
	XrSessionState activeSessionState: 4;
	XrSessionState pendingSessionState: 4;

	block_handle hActiveReferenceSpace;
	block_handle hPendingReferenceSpace;

	block_handle hActiveInteractionProfile;
	block_handle hPendingInteractionProfile;

	XrBool32 activeIsUserPresent: 1;
	XrBool32 pendingIsUserPresent: 1;

	//// Graphics
	CACHE_ALIGN
	union {
		struct {
			HDC   hDC;
			HGLRC hGLRC;
		} gl;
		struct {
			ID3D11Device5*        device5;
			ID3D11DeviceContext4* context4;
			ID3D11Fence*          compositorFence;
			ID3D11Fence*          sessionFence;
		} d3d11;
		struct {
			VkInstance       instance;
			VkPhysicalDevice physicalDevice;
			VkDevice         device;
			uint32_t         queueFamilyIndex;
			uint32_t         queueIndex;
		} vk;
	} binding;

} Session;

typedef struct Instance {
	//// System
	XrApplicationInfo applicationInfo;
	XrSystemId        systemId;
	XrFormFactor      systemFormFactor;
	XrGraphicsApi     graphicsApi;

	//// Interaction
	MAP_DECL(XR_INTERACTION_PROFILE_CAPACITY) interactionProfiles;

	/// Graphics
	union {
		struct {
			LUID              adapterLuid;
			D3D_FEATURE_LEVEL minFeatureLevel;
		} d3d11;
	} graphics;

} Instance;

//////////////////////////////
//// Mid OpenXR Implementation
//////////////////////////////
#if defined(MID_OPENXR_IMPLEMENTATION) || defined(MID_IDE_ANALYSIS)

///////////////////
//// Global Context
////
static struct {
	Instance instance;
} xr;

/// maybe want to call this block pool since its technically not a SLAB
static struct {
	BLOCK_DECL(Session, XR_SESSIONS_CAPACITY) session;
	BLOCK_DECL(Path, XR_PATH_CAPACITY) path;
	BLOCK_DECL(Binding, XR_BINDINGS_CAPACITY) binding;
	BLOCK_DECL(InteractionProfile, XR_INTERACTION_PROFILE_CAPACITY) profile;
	BLOCK_DECL(ActionSet, XR_ACTION_SET_CAPACITY) actionSet;
	BLOCK_DECL(Action, XR_ACTION_CAPACITY) action;
	BLOCK_DECL(SubactionState, XR_SUBACTION_CAPACITY) state;
	BLOCK_DECL(Space, XR_SPACE_CAPACITY) space;
	BLOCK_DECL(Swapchain, XR_SWAPCHAIN_CAPACITY) swap;
} block;

////////////
//// Utility
////
#define XR_PROC XRAPI_ATTR XrResult XRAPI_CALL

static uint32_t CalcDJB2(const char* str, int max)
{
	char c;
	int  i = 0;
	u32  hash = 5381;
	while ((c = *str++) && i++ < max)
		hash = ((hash << 5) + hash) + c;
	return hash;
}

static double MillisecondsToSeconds(double milliseconds )
{
	return milliseconds / 1000.0;
}

static double xrTimeToMilliseconds(XrTime nanoseconds)
{
	return (double)nanoseconds / 1000000.0;
}

static XrTime xrGetTime()
{
	LARGE_INTEGER qpc;
	QueryPerformanceCounter(&qpc);

	static double secondsToNanos = 1e9;
	static LARGE_INTEGER frequency = {0};
	if (frequency.QuadPart == 0) {
		QueryPerformanceFrequency(&frequency);
	}

	double seconds = (double)qpc.QuadPart / (double)frequency.QuadPart;
	XrTime xrTime = (XrTime)(seconds * secondsToNanos);

	return xrTime;
}

static inline XrResult XrTimeWaitWin32(XrTime* pSharedTime, XrTime waitTime)
{
	// this should be pthreads since it doesnt happen that much and cross platform is probably fine
	while (1) {
		XrTime currentTime = __atomic_load_n(pSharedTime, __ATOMIC_ACQUIRE);
		if (currentTime >= waitTime) {
			return XR_SUCCESS;
		}

		printf("XrTime needs waiting!\n");
		if (!WaitOnAddress(pSharedTime, &currentTime, sizeof(XrTime), INFINITE)) {
			return XR_TIMEOUT_EXPIRED;
		}
	}
}

static inline void XrTimeSignalWin32(XrTime* pSharedTime, XrTime signalTime)
{
	__atomic_store_n(pSharedTime, signalTime, __ATOMIC_RELEASE);
	WakeByAddressAll(pSharedTime);
}


//////////////////
//// OpenXR Debug Logging
////
#define CHECK_INSTANCE(instance)                \
	if (&xr.instance != (Instance*)instance) {  \
		LOG_ERROR("XR_ERROR_HANDLE_INVALID\n"); \
		return XR_ERROR_HANDLE_INVALID;         \
	}

#define ENABLE_DEBUG_LOG_METHOD
#ifdef ENABLE_DEBUG_LOG_METHOD
#define LOG_METHOD(_method) printf("%lu:%lu: " #_method "\n", GetCurrentProcessId(), GetCurrentThreadId())
#define LOG_METHOD_ONCE(_method)   \
	{                              \
		static int logged = false; \
		if (!logged) {             \
			logged = true;         \
			LOG_METHOD(_method);   \
		}                          \
	}
#else
#define LOG_METHOD(_name)
#endif

#define ENUM_NAME_CASE(_enum, _value) \
	case _value:                      \
		return #_enum;
#define STRING_ENUM_TYPE(_type)                    \
	static const char* string_##_type(_type value) \
	{                                              \
		switch (value) {                           \
			XR_LIST_ENUM_##_type(ENUM_NAME_CASE);  \
			default:                               \
				return "N/A";                      \
		}                                          \
	}

STRING_ENUM_TYPE(XrReferenceSpaceType)
STRING_ENUM_TYPE(XrViewConfigurationType)
STRING_ENUM_TYPE(XrVisibilityMaskTypeKHR)

#undef ENUM_NAME_CASE
#undef STRING_ENUM_TYPE

#define STRUCTURE_TYPE_NAME_CASE(_name, _type) \
	case _type:                                \
		return #_name;
static const char* string_XrStructureType(XrStructureType type)
{
	switch (type) {
		XR_LIST_STRUCTURE_TYPES(STRUCTURE_TYPE_NAME_CASE);
		default:
			return "N/A";
	}
}
#undef PRINT_STRUCT_TYPE_NAME

#define STR(s)         #s
#define XSTR(s)        STR(s)
#define DOT(_)         ._
#define COMMA          ,
#define BRACES(f, _)   {f(_)}

#define FORMAT_F(_)          _: %.3f
#define FORMAT_STRUCT_F(_)  "%s: " XSTR(BRACES(XR_LIST_STRUCT_##_, FORMAT_F))
#define EXPAND_STRUCT(t, _) STR(_) XR_LIST_STRUCT_##t(COMMA _ DOT)

static void LogNextChain(const XrBaseInStructure* nextProperties)
{
	while (nextProperties != NULL) {
		printf("NextType: %s\n", string_XrStructureType(nextProperties->type));
		nextProperties = (XrBaseInStructure*)nextProperties->next;
	}
}

/////////
//// Math
////
#define PI 3.14159265358979323846
// I prolly just want to make a depenedency on mid math
static inline XrQuaternionf xrRotFromMat4(XrMat4 m)
{
	XrQuaternionf q;
	float trace = m[0] + m[5] + m[10];  // m00 + m11 + m22
	if (trace > 0) {
		float s = 0.5f / sqrtf(trace + 1.0f);
		q.w = 0.25f / s;
		q.x = (m[6] - m[9]) * s;    // m12 - m21
		q.y = (m[8] - m[2]) * s;    // m20 - m02
		q.z = (m[1] - m[4]) * s;    // m01 - m10
	}
	else if (m[0] > m[5] && m[0] > m[10]) {    // m00 largest
		float s = 2.0f * sqrtf(1.0f + m[0] - m[5] - m[10]);
		q.w = (m[6] - m[9]) / s;    // m12 - m21
		q.x = 0.25f * s;
		q.y = (m[4] + m[1]) / s;    // m10 + m01
		q.z = (m[8] + m[2]) / s;    // m20 + m02
	}
	else if (m[5] > m[10]) {    // m11 largest
		float s = 2.0f * sqrtf(1.0f + m[5] - m[0] - m[10]);
		q.w = (m[8] - m[2]) / s;    // m20 - m02
		q.x = (m[4] + m[1]) / s;    // m10 + m01
		q.y = 0.25f * s;
		q.z = (m[9] + m[6]) / s;    // m21 + m12
	}
	else {    // m22 largest
		float s = 2.0f * sqrtf(1.0f + m[10] - m[0] - m[5]);
		q.w = (m[1] - m[4]) / s;    // m01 - m10
		q.x = (m[8] + m[2]) / s;    // m20 + m02
		q.y = (m[9] + m[6]) / s;    // m21 + m12
		q.z = 0.25f * s;
	}
	return q;
}
static inline XrVector3f xrPosFromMat4(XrMat4 m)
{
	float w = m[15];    // m33 (c3r3)
	return (XrVector3f){
		m[12] / w,      // m30 (c3r0)
		m[13] / w,      // m31 (c3r1)
		m[14] / w       // m32 (c3r2)
	};
}
static inline XrQuaternionf xrQuaternionFromEuler(XrVector3f euler)
{
	XrVector3f c, s;
	c.x = cos(euler.x * 0.5f);
	c.y = cos(euler.y * 0.5f);
	c.z = cos(euler.z * 0.5f);
	s.x = sin(euler.x * 0.5f);
	s.y = sin(euler.y * 0.5f);
	s.z = sin(euler.z * 0.5f);

	XrQuaternionf out;
	out.w = c.x * c.y * c.z + s.x * s.y * s.z;
	out.x = s.x * c.y * c.z - c.x * s.y * s.z;
	out.y = c.x * s.y * c.z + s.x * c.y * s.z;
	out.z = c.x * c.y * s.z - s.x * s.y * c.z;
	return out;
}
static inline XrMat4 xrMat4XInvertPos(XrMat4 m)
{
	m[12] = -m[12];  // c3r0
	return m;
}
static inline XrMat4 xrMat4YInvertPos(XrMat4 m)
{
	m[13] = -m[13];  // c3r1
	return m;
}
static inline XrMat4 xrMat4ZInvertPos(XrMat4 m)
{
	m[14] = -m[14];  // c3r2
	return m;
}
static inline XrMat4 xrMat4MirrorXRot(XrMat4 m) {
	// Get local X axis (first column of rotation part)
	float local_x[3] = {
		m[0],  // c0r0
		m[1],  // c0r1
		m[2]   // c0r2
	};

	// Create reflection matrix that mirrors across plane perpendicular to local X
	// Householder reflection formula: I - 2(v⊗v)
	// Negate the rotational part except for components along local X

	m[4] -= 2.0f * local_x[0] * local_x[1];  // c1r0
	m[5] -= 2.0f * local_x[1] * local_x[1];  // c1r1
	m[6] -= 2.0f * local_x[2] * local_x[1];  // c1r2

	m[8] -= 2.0f * local_x[0] * local_x[2];  // c2r0
	m[9] -= 2.0f * local_x[1] * local_x[2];  // c2r1
	m[10] -= 2.0f * local_x[2] * local_x[2]; // c2r2
	return m;
}
static inline XrMat4 xrMat4InvertXRot(XrMat4 m) {
	m[1] = -m[1];    // c0r1 (Y component of first column)
	m[2] = -m[2];    // c0r2 (Z component of first column)

	m[5] = -m[5];    // c1r1 (Y component of second column)
	m[6] = -m[6];    // c1r2 (Z component of second column)

	m[9] = -m[9];    // c2r1 (Y component of third column)
	m[10] = -m[10];  // c2r2 (Z component of third column)
	return m;
}
static inline XrMat4 xrMat4InvertYRot(XrMat4 m) {
	m[0] = -m[0];  // c0r0
	m[8] = -m[8];  // c2r0
}
static inline XrMat4 xrMat4InvertZRot(XrMat4 m) {
	m[0] = -m[0];  // c0r0
	m[4] = -m[4];  // c1r0
}

#define XR_CONVERT_DD11_POSITION(_) _.y = -_.y
#define XR_CONVERT_D3D11_EULER(_) _.x = -_.x

static inline float xrFloatLerp(float a, float b, float t)
{
	return a + t * (b - a);
}

///////////////////////
//// Device Input Funcs
////
static int OculusLeftClick(float* pValue)
{
	*pValue = 100;
	return 0;
}
static int OculusRightClick(float* pValue)
{
	*pValue = 200;
	return 0;
}

////////////
//// Binding
////
static XrResult RegisterBinding(
	XrInstance          instance,
	InteractionProfile* pInteractionProfile,
	Path*               pBindingPath,
	int (*func)(void*))
{
	auto bindPathHash = BLOCK_KEY(block.path, pBindingPath);
	for (int i = 0; i < pInteractionProfile->bindings.count; ++i) {
		if (pInteractionProfile->bindings.keys[i] == bindPathHash) {
			LOG_ERROR("Trying to register path hash twice! %s %d\n", pBindingPath->string, bindPathHash);
			return XR_ERROR_PATH_INVALID;
		}
	}

	auto hBind = BLOCK_CLAIM(block.binding, bindPathHash);
	auto pBind = BLOCK_PTR(block.binding, hBind);
	pBind->hPath = BLOCK_HANDLE(block.path, pBindingPath);
	pBind->func = func;

	MAP_ADD(pInteractionProfile->bindings, hBind, bindPathHash);

	return XR_SUCCESS;
}

typedef struct BindingDefinition {
	int (*func)(void*);
	const char path[XR_MAX_PATH_LENGTH];
} BindingDefinition;
static XrResult InitBinding(const char* interactionProfile, int bindingDefinitionCount, BindingDefinition* pBindingDefinitions)
{
	XrPath interactionProfilePath;
	xrStringToPath((XrInstance)&xr.instance, interactionProfile, &interactionProfilePath);
	auto interactionProfilePathHash = BLOCK_KEY(block.path, (Path*)interactionProfilePath);

	auto interactionProfileHandle  = BLOCK_CLAIM(block.profile, interactionProfilePathHash);
	auto pInteractionProfile = BLOCK_PTR(block.profile, interactionProfileHandle);

	pInteractionProfile->path = interactionProfilePath;

	for (int i = 0; i < bindingDefinitionCount; ++i) {
		XrPath bindingPath;
		xrStringToPath((XrInstance)&xr.instance, pBindingDefinitions[i].path, &bindingPath);
		XR_CHECK(RegisterBinding((XrInstance)&xr.instance, pInteractionProfile, (Path*)bindingPath, pBindingDefinitions[i].func));
	}

	return XR_SUCCESS;
}

#define XR_DEFAULT_INTERACTION_PROFILE "/interaction_profiles/oculus/touch_controller"

static XrResult InitStandardBindings()
{

#define BINDING_DEFINITION(_func, _path) (int (*)(void*)) _func, _path

	{
		BindingDefinition bindingDefinitions[] = {
			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/select/click"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/select/click"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/squeeze/value"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/squeeze/value"),
			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/squeeze/click"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/squeeze/click"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/trigger/value"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/trigger/value"),
			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/trigger/click"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/trigger/click"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/grip/pose"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/grip/pose"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/output/haptic"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/output/haptic"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/menu/click"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/menu/click"),
		};
		InitBinding("/interaction_profiles/oculus/touch_controller", COUNT(bindingDefinitions), bindingDefinitions);
	}

	{
		BindingDefinition bindingDefinitions[] = {
			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/select/click"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/select/click"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/squeeze/value"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/squeeze/value"),
			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/squeeze/click"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/squeeze/click"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/trigger/value"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/trigger/value"),
			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/trigger/click"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/trigger/click"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/grip/pose"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/grip/pose"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/output/haptic"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/output/haptic"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/menu/click"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/menu/click"),
		};
		InitBinding("/interaction_profiles/microsoft/motion_controller", COUNT(bindingDefinitions), bindingDefinitions);
	}

	{
		BindingDefinition bindingDefinitions[] = {
			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/select/click"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/select/click"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/squeeze/value"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/squeeze/value"),
			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/squeeze/click"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/squeeze/click"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/trigger/value"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/trigger/value"),
			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/trigger/click"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/trigger/click"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/grip/pose"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/grip/pose"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/output/haptic"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/output/haptic"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/menu/click"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/menu/click"),
		};
		InitBinding("/interaction_profiles/khr/simple_controller", COUNT(bindingDefinitions), bindingDefinitions);
	}

	{
		BindingDefinition bindingDefinitions[] = {
			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/select/click"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/select/click"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/squeeze/value"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/squeeze/value"),
			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/squeeze/click"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/squeeze/click"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/trigger/value"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/trigger/value"),
			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/trigger/click"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/trigger/click"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/grip/pose"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/grip/pose"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/output/haptic"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/output/haptic"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/menu/click"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/menu/click"),
		};
		InitBinding("/interaction_profiles/valve/index_controller", COUNT(bindingDefinitions), bindingDefinitions);
	}

	{
		BindingDefinition bindingDefinitions[] = {
			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/select/click"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/select/click"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/squeeze/value"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/squeeze/value"),
			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/squeeze/click"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/squeeze/click"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/trigger/value"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/trigger/value"),
			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/trigger/click"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/trigger/click"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/grip/pose"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/grip/pose"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/output/haptic"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/output/haptic"),

			BINDING_DEFINITION(OculusLeftClick, "/user/hand/left/input/menu/click"),
			BINDING_DEFINITION(OculusRightClick, "/user/hand/right/input/menu/click"),
		};
		InitBinding("/interaction_profiles/htc/vive_controller", COUNT(bindingDefinitions), bindingDefinitions);
	}

#undef BINDING_DEFINITION

	return XR_SUCCESS;
}

//////////////////////////////////
//// OpenXR Method Implementations
////
XR_PROC xrEnumerateApiLayerProperties(
	uint32_t              propertyCapacityInput,
	uint32_t*             propertyCountOutput,
	XrApiLayerProperties* properties)
{
	LOG_METHOD(xrEnumerateApiLayerProperties);
	return XR_SUCCESS;
}

XR_PROC xrEnumerateInstanceExtensionProperties(
	const char*            layerName,
	uint32_t               propertyCapacityInput,
	uint32_t*              propertyCountOutput,
	XrExtensionProperties* properties)
{
	LOG_METHOD(xrEnumerateInstanceExtensionProperties);

	XrExtensionProperties supportedProperties[] = {
		// graphics API
		{
			.type = XR_TYPE_EXTENSION_PROPERTIES,
			.extensionName = XR_KHR_OPENGL_ENABLE_EXTENSION_NAME,
			.extensionVersion = XR_KHR_opengl_enable_SPEC_VERSION,
		},
		{
			.type = XR_TYPE_EXTENSION_PROPERTIES,
			.extensionName = XR_KHR_D3D11_ENABLE_EXTENSION_NAME,
			.extensionVersion = XR_KHR_D3D11_enable_SPEC_VERSION,
		},

		{
			.type = XR_TYPE_EXTENSION_PROPERTIES,
			.extensionName = XR_META_RECOMMENDED_LAYER_RESOLUTION_EXTENSION_NAME,
			.extensionVersion = XR_META_recommended_layer_resolution_SPEC_VERSION,
		},

		// expected by unity
		{
			.type = XR_TYPE_EXTENSION_PROPERTIES,
			.extensionName = XR_KHR_VISIBILITY_MASK_EXTENSION_NAME,
			.extensionVersion = XR_KHR_visibility_mask_SPEC_VERSION,
		},
//		{
//			.type = XR_TYPE_EXTENSION_PROPERTIES,
//			.extensionName = XR_EXT_USER_PRESENCE_EXTENSION_NAME,
//			.extensionVersion = XR_EXT_user_presence_SPEC_VERSION,
//		},
//		{
//			.type = XR_TYPE_EXTENSION_PROPERTIES,
//			.extensionName = XR_EXT_CONFORMANCE_AUTOMATION_EXTENSION_NAME,
//			.extensionVersion = XR_EXT_conformance_automation_SPEC_VERSION,
//		},
		{
			.type = XR_TYPE_EXTENSION_PROPERTIES,
			.extensionName = XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME,
			.extensionVersion = XR_KHR_composition_layer_depth_SPEC_VERSION,
		},
		{
			.type = XR_TYPE_EXTENSION_PROPERTIES,
			.extensionName = XR_VARJO_QUAD_VIEWS_EXTENSION_NAME,
			.extensionVersion = XR_VARJO_quad_views_SPEC_VERSION,
		},
		{
			.type = XR_TYPE_EXTENSION_PROPERTIES,
			.extensionName = XR_MSFT_SECONDARY_VIEW_CONFIGURATION_EXTENSION_NAME,
			.extensionVersion = XR_MSFT_secondary_view_configuration_SPEC_VERSION,
		},
//		{
//			.type = XR_TYPE_EXTENSION_PROPERTIES,
//			.extensionName = XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME,
//			.extensionVersion = XR_EXT_eye_gaze_interaction_SPEC_VERSION,
//		},
//		{
//			.type = XR_TYPE_EXTENSION_PROPERTIES,
//			.extensionName = XR_MSFT_HAND_INTERACTION_EXTENSION_NAME,
//			.extensionVersion = XR_MSFT_hand_interaction_SPEC_VERSION,
//		},
		{
			.type = XR_TYPE_EXTENSION_PROPERTIES,
			.extensionName = XR_MSFT_FIRST_PERSON_OBSERVER_EXTENSION_NAME,
			.extensionVersion = XR_MSFT_first_person_observer_SPEC_VERSION,
		},
//		{
//			.type = XR_TYPE_EXTENSION_PROPERTIES,
//			.extensionName = XR_META_PERFORMANCE_METRICS_EXTENSION_NAME,
//			.extensionVersion = XR_META_performance_metrics_SPEC_VERSION,
//		},
//		{
//			.type = XR_TYPE_EXTENSION_PROPERTIES,
//			.extensionName = XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME,
//			.extensionVersion = XR_EXT_performance_settings_SPEC_VERSION,
//		},

		// chrome wants these? or I think its just some steamvr had but im not sure of the necessity
//		{
//			.type = XR_TYPE_EXTENSION_PROPERTIES,
//			.extensionName = XR_KHR_WIN32_CONVERT_PERFORMANCE_COUNTER_TIME_EXTENSION_NAME,
//			.extensionVersion = XR_KHR_win32_convert_performance_counter_time_SPEC_VERSION,
//		},
//		{
//			.type = XR_TYPE_EXTENSION_PROPERTIES,
//			.extensionName = XR_EXT_LOCAL_FLOOR_EXTENSION_NAME,
//			.extensionVersion = XR_EXT_local_floor_SPEC_VERSION,
//		},
//		{
//			.type = XR_TYPE_EXTENSION_PROPERTIES,
//			.extensionName = XR_EXT_WIN32_APPCONTAINER_COMPATIBLE_EXTENSION_NAME,
//			.extensionVersion = XR_EXT_win32_appcontainer_compatible_SPEC_VERSION,
//		},
//		{
//			.type = XR_TYPE_EXTENSION_PROPERTIES,
//			.extensionName = XR_MSFT_UNBOUNDED_REFERENCE_SPACE_EXTENSION_NAME,
//			.extensionVersion = XR_MSFT_unbounded_reference_space_SPEC_VERSION,
//		},
	};

	*propertyCountOutput = COUNT(supportedProperties);

	if (!properties)
		return XR_SUCCESS;

	if (propertyCapacityInput != COUNT(supportedProperties))
		return XR_ERROR_SIZE_INSUFFICIENT;

	int supportedSize = sizeof(supportedProperties);
	int testSize = sizeof(XrExtensionProperties) * propertyCapacityInput;
	memcpy(properties, supportedProperties,  sizeof(XrExtensionProperties) * propertyCapacityInput);

	return XR_SUCCESS;
}

static struct {
	PFNGLCREATEMEMORYOBJECTSEXTPROC     CreateMemoryObjectsEXT;
	PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC ImportMemoryWin32HandleEXT;
	PFNGLCREATETEXTURESPROC             CreateTextures;
	PFNGLTEXTURESTORAGEMEM2DEXTPROC     TextureStorageMem2DEXT;
} gl;

XR_PROC xrCreateInstance(
	const XrInstanceCreateInfo* createInfo,
	XrInstance*                 instance)
{
	LOG_METHOD(xrCreateInstance);

	if (xr.instance.applicationInfo.apiVersion != 0) {
		LOG_ERROR("XR_ERROR_LIMIT_REACHED\n");
		return XR_ERROR_LIMIT_REACHED;
	}

	for (int i = 0; i < createInfo->enabledApiLayerCount; ++i) {
		printf("Enabled API Layer: %s\n", createInfo->enabledApiLayerNames[i]);
	}

	for (int i = 0; i < createInfo->enabledExtensionCount; ++i) {
		printf("Enabled Extension: %s\n", createInfo->enabledExtensionNames[i]);
		if (strncmp(createInfo->enabledExtensionNames[i], XR_KHR_OPENGL_ENABLE_EXTENSION_NAME, XR_MAX_EXTENSION_NAME_SIZE) == 0)
			xr.instance.graphicsApi = XR_GRAPHICS_API_OPENGL;
		if (strncmp(createInfo->enabledExtensionNames[i], XR_KHR_D3D11_ENABLE_EXTENSION_NAME, XR_MAX_EXTENSION_NAME_SIZE) == 0)
			xr.instance.graphicsApi = XR_GRAPHICS_API_D3D11_4;
	}

	memcpy(&xr.instance.applicationInfo, &createInfo->applicationInfo, sizeof(XrApplicationInfo));

	printf("applicationName: %s\n", createInfo->applicationInfo.applicationName);
	printf("applicationVersion: %d\n", createInfo->applicationInfo.applicationVersion);
	printf("engineName: %s\n", createInfo->applicationInfo.engineName);
	printf("engineVersion: %d\n", createInfo->applicationInfo.engineVersion);
	{
		int maj = XR_VERSION_MAJOR(xr.instance.applicationInfo.apiVersion);
		int min = XR_VERSION_MINOR(xr.instance.applicationInfo.apiVersion);
		int patch = XR_VERSION_PATCH(xr.instance.applicationInfo.apiVersion);
		printf("apiVersion: %d.%d.%d\n", maj, min, patch);
	}
	{
		int maj = XR_VERSION_MAJOR(XR_CURRENT_API_VERSION);
		int min = XR_VERSION_MINOR(XR_CURRENT_API_VERSION);
		int patch = XR_VERSION_PATCH(XR_CURRENT_API_VERSION);
		printf("min apiVersion: 1.0.0\nmax apiVersion: %d.%d.%d\n", maj, min, patch);
	}
	if (createInfo->applicationInfo.apiVersion < XR_MAKE_VERSION(1, 0, 0) ||
		createInfo->applicationInfo.apiVersion > XR_CURRENT_API_VERSION) {
		LOG_ERROR("XR_ERROR_API_VERSION_UNSUPPORTED\n");
		return XR_ERROR_API_VERSION_UNSUPPORTED;
	}

//#define FIELD_FORMAT_SPECIFIER(_field) _Generic(_field, uint64_t: "%llu", uint32_t: "%d", float: "%f", double: "%lf", char*: "%s", const char*: "%s")
//#define PRINT_STRUCT(_field) printf(#_field ": "); printf(FIELD_FORMAT_SPECIFIER(xr.instance.applicationInfo._field), xr.instance.applicationInfo._field); printf("\n");
//	XR_LIST_STRUCT_XrApplicationInfo(PRINT_STRUCT);

	*instance = (XrInstance)&xr.instance;

	InitStandardBindings();
	midXrInitialize();

	switch (xr.instance.graphicsApi) {
		case XR_GRAPHICS_API_OPENGL: {
			gl.CreateMemoryObjectsEXT = (PFNGLCREATEMEMORYOBJECTSEXTPROC)wglGetProcAddress("glCreateMemoryObjectsEXT");
			if (!gl.CreateMemoryObjectsEXT)
				LOG_ERROR("Failed to load glCreateMemoryObjectsEXT\n");
			gl.ImportMemoryWin32HandleEXT = (PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC)wglGetProcAddress("glImportMemoryWin32HandleEXT");
			if (!gl.ImportMemoryWin32HandleEXT)
				LOG_ERROR("Failed to load glImportMemoryWin32HandleEXT\n");
			gl.CreateTextures = (PFNGLCREATETEXTURESPROC)wglGetProcAddress("glCreateTextures");
			if (!gl.CreateTextures)
				LOG_ERROR("Failed to load glCreateTextures\n");
			gl.TextureStorageMem2DEXT = (PFNGLTEXTURESTORAGEMEM2DEXTPROC)wglGetProcAddress("glTextureStorageMem2DEXT");
			if (!gl.TextureStorageMem2DEXT)
				LOG_ERROR("Failed to load glTextureStorageMem2DEXT\n");
			return XR_SUCCESS;
		}
		case XR_GRAPHICS_API_D3D11_4: {
			return XR_SUCCESS;
		}
		case XR_GRAPHICS_API_OPENGL_ES:
		case XR_GRAPHICS_API_VULKAN:
		default:
			LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED\n");
			return XR_ERROR_GRAPHICS_DEVICE_INVALID;
	}
}

XR_PROC xrDestroyInstance(
	XrInstance instance)
{
	LOG_METHOD(xrDestroyInstance);
	CHECK_INSTANCE(instance);

	xr.instance = (Instance){};

	return XR_SUCCESS;
}

XR_PROC xrGetInstanceProperties(
	XrInstance            instance,
	XrInstanceProperties* instanceProperties)
{
	LOG_METHOD(xrGetInstanceProperties);
	CHECK_INSTANCE(instance);

	instanceProperties->runtimeVersion = XR_MAKE_VERSION(0, 1, 0);
	strncpy(instanceProperties->runtimeName, "Moxaic OpenXR", XR_MAX_RUNTIME_NAME_SIZE);

	return XR_SUCCESS;
}

#define STRING_ENUM_CASE(_method, _value) case _value: { return #_method; }
#define STRING_ENUM_METHOD(_method)                             \
	const char* string_##_method(_method val)                     \
	{                                                         \
		switch (val) {                                        \
			XR_LIST_ENUM_##_method(STRING_ENUM_CASE) default:   \
			{                                                 \
				assert(false && #_method " string not found."); \
				return "N/A";                                 \
			}                                                 \
		}                                                     \
	}

STRING_ENUM_METHOD(XrSessionState)

XR_PROC xrPollEvent(
	XrInstance         instance,
	XrEventDataBuffer* eventData)
{
	LOG_METHOD_ONCE(xrPollEvent);
	CHECK_INSTANCE(instance);

	// technicall could be worth iterating map in instance?
	for (int i = 0; i < XR_SESSIONS_CAPACITY; ++i) {

		if (!BITSET(block.session.occupied, i))
			continue;

		auto pSess = &block.session.blocks[i];

		if (pSess->activeSessionState != pSess->pendingSessionState) {

			eventData->type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;

			XrEventDataSessionStateChanged* pEventData = (XrEventDataSessionStateChanged*)eventData;
			pEventData->session = (XrSession)pSess;
			pEventData->time = xrGetTime();

			// Must cycle through Focus, Visible, Synchronized if exiting.
			if (pSess->exiting) {
				// todo refactor some this in switches
//				switch (pSession->activeSessionState) {
//					case XR_SESSION_STATE_FOCUSED:
//					case XR_SESSION_STATE_VISIBLE:
//					case XR_SESSION_STATE_READY:
//				}

				if (pSess->activeSessionState == XR_SESSION_STATE_FOCUSED)
					pSess->activeSessionState = XR_SESSION_STATE_VISIBLE;
				else if (pSess->activeSessionState == XR_SESSION_STATE_VISIBLE)
					pSess->activeSessionState = XR_SESSION_STATE_SYNCHRONIZED;
				else if (pSess->activeSessionState == XR_SESSION_STATE_READY)
					pSess->activeSessionState = XR_SESSION_STATE_SYNCHRONIZED;
				else if (pSess->activeSessionState == XR_SESSION_STATE_SYNCHRONIZED)
					pSess->activeSessionState = XR_SESSION_STATE_STOPPING;
				else if (pSess->activeSessionState == XR_SESSION_STATE_STOPPING) {
					if (pSess->running)
						return XR_EVENT_UNAVAILABLE; // must wait for Idle
					else {
						pSess->activeSessionState = XR_SESSION_STATE_IDLE;
						pSess->pendingSessionState = XR_SESSION_STATE_EXITING;
					}
				} else if (pSess->activeSessionState == XR_SESSION_STATE_IDLE) {
					if (pSess->running) {
						pSess->activeSessionState = XR_SESSION_STATE_STOPPING;
					} else {
						pSess->activeSessionState = XR_SESSION_STATE_EXITING;
					}
				}

				pEventData->state = pSess->activeSessionState;
				LOG("XrEventDataSessionStateChanged: %s\n", string_XrSessionState(pEventData->state));
				return XR_SUCCESS;
			}

			if (!pSess->running) {

				if (pSess->pendingSessionState == XR_SESSION_STATE_IDLE) {

					if (pSess->activeSessionState == XR_SESSION_STATE_UNKNOWN) {
						pSess->activeSessionState = XR_SESSION_STATE_IDLE;
						// Immediately progress Idle to Ready when starting
						pSess->pendingSessionState = XR_SESSION_STATE_READY;
					} else if (pSess->activeSessionState == XR_SESSION_STATE_STOPPING)
						pSess->activeSessionState = XR_SESSION_STATE_IDLE;

				} else {
					pSess->activeSessionState = pSess->pendingSessionState;
				}

				pEventData->state = pSess->activeSessionState;
				LOG("XrEventDataSessionStateChanged: %s\n", string_XrSessionState(pEventData->state));
				return XR_SUCCESS;
			}

			// Must cycle through Focus, Visible, Synchronized if going to idle.
			if (pSess->pendingSessionState == XR_SESSION_STATE_IDLE) {

				if (pSess->activeSessionState == XR_SESSION_STATE_FOCUSED)
					pSess->activeSessionState = XR_SESSION_STATE_VISIBLE;
				else if (pSess->activeSessionState == XR_SESSION_STATE_VISIBLE)
					pSess->activeSessionState = XR_SESSION_STATE_SYNCHRONIZED;
				else if (pSess->activeSessionState == XR_SESSION_STATE_SYNCHRONIZED)
					pSess->activeSessionState = XR_SESSION_STATE_STOPPING;
				else if (pSess->activeSessionState == XR_SESSION_STATE_STOPPING)
					pSess->activeSessionState = XR_SESSION_STATE_IDLE;

				pEventData->state = pSess->activeSessionState;
				LOG("XrEventDataSessionStateChanged: %s\n", string_XrSessionState(pEventData->state));
				return XR_SUCCESS;
			}

			// Must cycle through Focus, Visible, Synchronized if stopping.
			if (pSess->pendingSessionState == XR_SESSION_STATE_STOPPING) {

				if (pSess->activeSessionState == XR_SESSION_STATE_FOCUSED)
					pSess->activeSessionState = XR_SESSION_STATE_VISIBLE;
				else if (pSess->activeSessionState == XR_SESSION_STATE_READY)
					pSess->activeSessionState = XR_SESSION_STATE_SYNCHRONIZED;
				else if (pSess->activeSessionState == XR_SESSION_STATE_VISIBLE)
					pSess->activeSessionState = XR_SESSION_STATE_SYNCHRONIZED;
				else if (pSess->activeSessionState == XR_SESSION_STATE_SYNCHRONIZED)
					pSess->activeSessionState = XR_SESSION_STATE_STOPPING;

				pEventData->state = pSess->activeSessionState;
				LOG("XrEventDataSessionStateChanged: %s\n", string_XrSessionState(pEventData->state));
				return XR_SUCCESS;
			}

			// Must ensure stopping does not switch back to Ready after going Idle
			if (pSess->activeSessionState == XR_SESSION_STATE_STOPPING) {
				pSess->activeSessionState = pSess->pendingSessionState;
				pEventData->state = pSess->activeSessionState ;
				LOG("XrEventDataSessionStateChanged: %s\n", string_XrSessionState(pEventData->state));
				return XR_SUCCESS;
			}

			pSess->activeSessionState = pSess->pendingSessionState;
			pEventData->state = pSess->activeSessionState ;
			LOG("XrEventDataSessionStateChanged: %s\n", string_XrSessionState(pEventData->state));

			// Forcing to focused for now, but should happen after some user input probably
			if (pSess->activeSessionState == XR_SESSION_STATE_VISIBLE) {
				pSess->pendingSessionState = XR_SESSION_STATE_FOCUSED;
			}

			return XR_SUCCESS;
		}

		if (pSess->hActiveReferenceSpace != pSess->hPendingReferenceSpace) {

			eventData->type = XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING;

			auto pPendingSpace = BLOCK_PTR(block.space, pSess->hPendingReferenceSpace);

			auto pEventData = (XrEventDataReferenceSpaceChangePending*)eventData;
			pEventData->session = (XrSession)pSess;
			pEventData->referenceSpaceType = pPendingSpace->referenceSpaceType;
			pEventData->changeTime = xrGetTime();
			pEventData->poseValid = XR_TRUE;
			// this is not correct, supposed to be origin of new space in space of prior space
			pEventData->poseInPreviousSpace = pPendingSpace->poseInReferenceSpace;

			printf("XrEventDataReferenceSpaceChangePending: %d\n", pEventData->referenceSpaceType);

			pSess->hActiveReferenceSpace = pSess->hPendingReferenceSpace;

			return XR_SUCCESS;
		}

		if (pSess->hActiveInteractionProfile != pSess->hPendingInteractionProfile) {

			eventData->type = XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED;
			auto pEventData = (XrEventDataInteractionProfileChanged*)eventData;
			pEventData->session = (XrSession)pSess;

			pSess->hActiveInteractionProfile = pSess->hPendingInteractionProfile;

			auto pActionInteractionProfile = BLOCK_PTR(block.profile, pSess->hActiveInteractionProfile);
			auto pActionInteractionProfilePath = (Path*)pActionInteractionProfile->path;
			printf("XrEventDataInteractionProfileChanged %s\n", pActionInteractionProfilePath->string);

			return XR_SUCCESS;
		}

		if (pSess->activeIsUserPresent != pSess->pendingIsUserPresent) {

			eventData->type = XR_TYPE_EVENT_DATA_USER_PRESENCE_CHANGED_EXT;

			auto pEventData = (XrEventDataUserPresenceChangedEXT*)eventData;
			pEventData->session = (XrSession)pSess;
			pEventData->isUserPresent = pSess->pendingIsUserPresent;

			pSess->activeIsUserPresent = pSess->pendingIsUserPresent;

			printf("XrEventDataUserPresenceChangedEXT %d\n", pSess->activeIsUserPresent);

			return XR_SUCCESS;
		}
	}

	return XR_EVENT_UNAVAILABLE;
}

#define TRANSFER_ENUM_NAME(_type, _) \
	case _type: strncpy(buffer, #_type, XR_MAX_RESULT_STRING_SIZE); break;

XR_PROC xrResultToString(
	XrInstance instance,
	XrResult   value,
	char       buffer[XR_MAX_RESULT_STRING_SIZE])
{
	CHECK_INSTANCE(instance);

	switch (value) {
		XR_LIST_ENUM_XrResult(TRANSFER_ENUM_NAME);
		default:
			snprintf(buffer, XR_MAX_RESULT_STRING_SIZE, "XR_UNKNOWN_STRUCTURE_TYPE_%d", value);
			break;
	}
	buffer[XR_MAX_RESULT_STRING_SIZE - 1] = '\0';

	return XR_SUCCESS;
}

XR_PROC xrStructureTypeToString(
	XrInstance      instance,
	XrStructureType value,
	char            buffer[XR_MAX_STRUCTURE_NAME_SIZE])
{
	CHECK_INSTANCE(instance);

	switch (value) {
		XR_LIST_ENUM_XrStructureType(TRANSFER_ENUM_NAME);
		default:
			snprintf(buffer, XR_MAX_RESULT_STRING_SIZE, "XR_UNKNOWN_STRUCTURE_TYPE_%d", value);
			break;
	}
	buffer[XR_MAX_RESULT_STRING_SIZE - 1] = '\0';

	return XR_SUCCESS;
}

typedef enum SystemId {
	SYSTEM_ID_HANDHELD_AR = 1,
	SYSTEM_ID_HMD_VR_STEREO = 2,
	SYSTEM_ID_COUNT,
} SystemId;
XR_PROC xrGetSystem(
	XrInstance             instance,
	const XrSystemGetInfo* getInfo,
	XrSystemId*            systemId)
{
	LOG_METHOD_ONCE(xrGetSystem);
	LogNextChain((XrBaseInStructure*)getInfo->next);
	CHECK_INSTANCE(instance);

	Instance* pInstance = (Instance*)instance;

	switch (getInfo->formFactor) {
		case XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY:
			xr.instance.systemFormFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
			xr.instance.systemId = SYSTEM_ID_HMD_VR_STEREO;
			*systemId = SYSTEM_ID_HMD_VR_STEREO;
			return XR_SUCCESS;
		case XR_FORM_FACTOR_HANDHELD_DISPLAY:
			xr.instance.systemFormFactor = XR_FORM_FACTOR_HANDHELD_DISPLAY;
			pInstance->systemId = SYSTEM_ID_HANDHELD_AR;
			*systemId = SYSTEM_ID_HANDHELD_AR;
			return XR_SUCCESS;
		default:
			printf("XR_ERROR_FORM_FACTOR_UNSUPPORTED\n");
			*systemId = XR_NULL_SYSTEM_ID;
			return XR_ERROR_FORM_FACTOR_UNSUPPORTED;
	}
}

XR_PROC xrGetSystemProperties(
	XrInstance          instance,
	XrSystemId          systemId,
	XrSystemProperties* properties)
{
	LOG_METHOD(xrGetSystemProperties);
	LogNextChain((XrBaseInStructure*)properties->next);
	CHECK_INSTANCE(instance);

	switch ((SystemId)systemId) {
		default:
			return XR_ERROR_HANDLE_INVALID;
		case SYSTEM_ID_HANDHELD_AR:
			properties->systemId = SYSTEM_ID_HANDHELD_AR;
			properties->vendorId = 0;
			strncpy(properties->systemName, "MoxaicDesktop", XR_MAX_SYSTEM_NAME_SIZE);
			properties->graphicsProperties.maxLayerCount = XR_MIN_COMPOSITION_LAYERS_SUPPORTED;
			properties->graphicsProperties.maxSwapchainImageWidth = XR_DEFAULT_WIDTH;
			properties->graphicsProperties.maxSwapchainImageHeight = XR_DEFAULT_HEIGHT;
			properties->trackingProperties.orientationTracking = XR_TRUE;
			properties->trackingProperties.positionTracking = XR_TRUE;
			return XR_SUCCESS;
		case SYSTEM_ID_HMD_VR_STEREO:
			properties->systemId = SYSTEM_ID_HMD_VR_STEREO;
			properties->vendorId = 0;
			strncpy(properties->systemName, "MoxaicHMD", XR_MAX_SYSTEM_NAME_SIZE);
			properties->graphicsProperties.maxLayerCount = XR_MIN_COMPOSITION_LAYERS_SUPPORTED;
			properties->graphicsProperties.maxSwapchainImageWidth = XR_DEFAULT_WIDTH;
			properties->graphicsProperties.maxSwapchainImageHeight = XR_DEFAULT_HEIGHT;
			properties->trackingProperties.orientationTracking = XR_TRUE;
			properties->trackingProperties.positionTracking = XR_TRUE;
			return XR_SUCCESS;
	}
}

XR_PROC xrEnumerateEnvironmentBlendModes(
	XrInstance              instance,
	XrSystemId              systemId,
	XrViewConfigurationType viewConfigurationType,
	uint32_t                environmentBlendModeCapacityInput,
	uint32_t*               environmentBlendModeCountOutput,
	XrEnvironmentBlendMode* environmentBlendModes)
{
	LOG_METHOD(xrEnumerateEnvironmentBlendModes);
	CHECK_INSTANCE(instance);

	const XrEnvironmentBlendMode modes[] = {
		XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
		XR_ENVIRONMENT_BLEND_MODE_ADDITIVE,
		XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND,
	};

	*environmentBlendModeCountOutput = COUNT(modes);

	if (environmentBlendModes == NULL)
		return XR_SUCCESS;

	if (environmentBlendModeCapacityInput < COUNT(modes))
		return XR_ERROR_SIZE_INSUFFICIENT;

	switch (viewConfigurationType) {
		case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:
		case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
		case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET:
		case XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT:
		default:
			for (int i = 0; i < COUNT(modes); ++i) {
				environmentBlendModes[i] = modes[i];
			}
			break;
	}

	return XR_SUCCESS;
}

XR_PROC xrCreateSession(
	XrInstance                 instance,
	const XrSessionCreateInfo* createInfo,
	XrSession*                 session)
{
	LOG_METHOD(xrCreateSession);
	CHECK_INSTANCE(instance);

	if (xr.instance.systemId == XR_NULL_SYSTEM_ID) {
		LOG_ERROR("XR_ERROR_SYSTEM_INVALID\n");
		return XR_ERROR_SYSTEM_INVALID;
	}

	printf("SystemId: %llu\n", createInfo->systemId);
	if (xr.instance.systemId != createInfo->systemId) {
		LOG_ERROR("XR_ERROR_SYSTEM_INVALID\n");
		return XR_ERROR_SYSTEM_INVALID;
	}

	if (xr.instance.graphics.d3d11.adapterLuid.LowPart == 0) {
		LOG_ERROR("XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING\n");
		return XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING;
	}

	XrSessionIndex sessionIndex;
	xrClaimSessionIndex(&sessionIndex);
	printf("Claimed SessionIndex %d\n", sessionIndex);

	auto hSess = BLOCK_CLAIM(block.session, sessionIndex);
	auto pSess = BLOCK_PTR(block.session, hSess);
	*pSess = (Session){};

	pSess->index = sessionIndex;

	HANDLE compositorFenceHandle;
	xrGetCompositorTimeline(sessionIndex, &compositorFenceHandle);

	HANDLE sessionFenceHandle;
	xrGetSessionTimeline(sessionIndex, &sessionFenceHandle);

	if (createInfo->next == NULL) {
		LOG_ERROR("XR_ERROR_GRAPHICS_DEVICE_INVALID\n");
		return XR_ERROR_GRAPHICS_DEVICE_INVALID;
	}

	switch (*(XrStructureType*)createInfo->next) {
		case XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR: {
			printf("OpenXR Graphics Binding: XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR\n");
			XrGraphicsBindingOpenGLWin32KHR* binding = (XrGraphicsBindingOpenGLWin32KHR*)createInfo->next;

			pSess->binding.gl.hDC = binding->hDC;
			pSess->binding.gl.hGLRC = binding->hGLRC;

			*session = (XrSession)pSess;
			break;
		}
		case XR_TYPE_GRAPHICS_BINDING_D3D11_KHR: {
			printf("OpenXR Graphics Binding: XR_TYPE_GRAPHICS_BINDING_D3D11_KHR\n");
			XrGraphicsBindingD3D11KHR* binding = (XrGraphicsBindingD3D11KHR*)createInfo->next;

			printf("XR D3D11 Device: %p\n", binding->device);
			// do I need cleanup goto? maybe. Probably
			if (binding->device == NULL) {
				LOG_ERROR("XR D3D11 Device Invalid.\n");
				return XR_ERROR_GRAPHICS_DEVICE_INVALID;
			}

			ID3D11Device5* device5;
			DX_CHECK(ID3D11Device_QueryInterface(binding->device, &IID_ID3D11Device5, (void**)&device5));
			printf("XR D3D11 Device5: %p\n", device5);
			if (device5 == NULL) {
				LOG_ERROR("XR D3D11 Device5 Invalid.\n");
				return XR_ERROR_GRAPHICS_DEVICE_INVALID;
			}

			ID3D11DeviceContext* context;
			ID3D11Device5_GetImmediateContext(device5, &context);
			ID3D11DeviceContext4* context4;
			DX_CHECK(ID3D11DeviceContext_QueryInterface(context, &IID_ID3D11DeviceContext4, (void**)&context4));
			printf("XR D3D11 Context4: %p\n", context4);
			if (context4 == NULL) {
				LOG_ERROR("XR D3D11 Context4 Invalid.\n");
				return XR_ERROR_GRAPHICS_DEVICE_INVALID;
			}
			ID3D11DeviceContext_Release(context);

			ID3D11Fence* compositorFence;
			printf("XR D3D11 Compositor Fence Handle: %p\n", compositorFenceHandle);
			ID3D11Device5_OpenSharedFence(device5, compositorFenceHandle, &IID_ID3D11Fence, (void**)&compositorFence);
			printf("XR D3D11 Compositor Fence: %p\n", compositorFence);
			if (compositorFence == NULL) {
				LOG_ERROR("XR D3D11 Compositor Fence Invalid.\n");
				return XR_ERROR_GRAPHICS_DEVICE_INVALID;
			}

			ID3D11Fence* sessionFence;
			printf("XR D3D11 Session Fence Handle: %p\n", sessionFenceHandle);
			ID3D11Device5_OpenSharedFence(device5, sessionFenceHandle, &IID_ID3D11Fence, (void**)&sessionFence);
			printf("XR D3D11 Session Fence: %p\n", sessionFence);
			if (sessionFence == NULL) {
				LOG_ERROR("XR D3D11 Session Fence Invalid.\n");
				return XR_ERROR_GRAPHICS_DEVICE_INVALID;
			}

			pSess->binding.d3d11.device5 = device5;
			pSess->binding.d3d11.context4 = context4;
			pSess->binding.d3d11.compositorFence = compositorFence;
			pSess->binding.d3d11.sessionFence = sessionFence;

			*session = (XrSession)pSess;

			break;
		}
		case XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR: {
			printf("OpenXR Graphics Binding: XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR\n");
			XrGraphicsBindingVulkanKHR* binding = (XrGraphicsBindingVulkanKHR*)createInfo->next;

			pSess->binding.vk.instance = binding->instance;
			pSess->binding.vk.physicalDevice = binding->physicalDevice;
			pSess->binding.vk.device = binding->device;
			pSess->binding.vk.queueFamilyIndex = binding->queueFamilyIndex;
			pSess->binding.vk.queueIndex = binding->queueIndex;

			*session = (XrSession)pSess;
		}
		default: {
			LOG_ERROR("XR_ERROR_GRAPHICS_DEVICE_INVALID\n");
			return XR_ERROR_GRAPHICS_DEVICE_INVALID;
		}
	}


	pSess->systemId = createInfo->systemId;
	pSess->activeSessionState = XR_SESSION_STATE_UNKNOWN;
	pSess->pendingSessionState = XR_SESSION_STATE_IDLE;
	pSess->hActiveReferenceSpace = HANDLE_DEFAULT;
	pSess->hPendingReferenceSpace = HANDLE_DEFAULT;
	pSess->hActiveInteractionProfile = HANDLE_DEFAULT;
	pSess->hPendingInteractionProfile = HANDLE_DEFAULT;
	pSess->activeIsUserPresent = XR_FALSE;
	pSess->pendingIsUserPresent = XR_TRUE;

	return XR_SUCCESS;
}

XR_PROC xrDestroySession(
	XrSession session)
{
	LOG_METHOD(xrDestroySession);

	auto pSession = (Session*)session;
	auto hSession = BLOCK_HANDLE(block.session, pSession);
	BLOCK_RELEASE(block.session, hSession);

	xrReleaseSessionIndex(pSession->index);

	return XR_SUCCESS;
}

XR_PROC xrEnumerateReferenceSpaces(
	XrSession             session,
	uint32_t              spaceCapacityInput,
	uint32_t*             spaceCountOutput,
	XrReferenceSpaceType* spaces)
{
	LOG_METHOD(xrEnumerateReferenceSpaces);

	auto pSess = (Session*)session;

	const XrReferenceSpaceType supportedSpaces[] = {
		XR_REFERENCE_SPACE_TYPE_VIEW,
		XR_REFERENCE_SPACE_TYPE_LOCAL,
//		XR_REFERENCE_SPACE_TYPE_STAGE,
//		XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR,
//		XR_REFERENCE_SPACE_TYPE_UNBOUNDED_MSFT,
	};

	*spaceCountOutput = COUNT(supportedSpaces);

	if (spaces == NULL)
		return XR_SUCCESS;

	if (spaceCapacityInput < COUNT(supportedSpaces))
		return XR_ERROR_SIZE_INSUFFICIENT;

	for (int i = 0; i < spaceCapacityInput && i < COUNT(supportedSpaces); ++i) {
		spaces[i] = supportedSpaces[i];
	}

	return XR_SUCCESS;
}

XR_PROC xrCreateReferenceSpace(
	XrSession                         session,
	const XrReferenceSpaceCreateInfo* createInfo,
	XrSpace*                          space)
{
	LOG("xrCreateReferenceSpace %s pos: " FORMAT_STRUCT_F(XrVector3f) FORMAT_STRUCT_F(XrQuaternionf) "\n",
		string_XrReferenceSpaceType(createInfo->referenceSpaceType),
		EXPAND_STRUCT(XrVector3f, createInfo->poseInReferenceSpace.position),
		EXPAND_STRUCT(XrQuaternionf, createInfo->poseInReferenceSpace.orientation));
	assert(createInfo->next == NULL);

	auto pSess = (Session*)session;

	auto hSpace = BLOCK_CLAIM(block.path, 0);
	HANDLE_CHECK(hSpace, XR_ERROR_LIMIT_REACHED);

	auto pSpace = BLOCK_PTR(block.space, hSpace);
	pSpace->hSession = BLOCK_HANDLE(block.session, pSess);
	pSpace->referenceSpaceType = createInfo->referenceSpaceType;
	pSpace->poseInReferenceSpace = createInfo->poseInReferenceSpace;

	// auto switch to first created space
	if (!HANDLE_VALID(pSess->hPendingReferenceSpace))
		pSess->hPendingReferenceSpace = hSpace;

	*space = (XrSpace)pSpace;

	return XR_SUCCESS;
}

XR_PROC xrGetReferenceSpaceBoundsRect(
	XrSession            session,
	XrReferenceSpaceType referenceSpaceType,
	XrExtent2Df*         bounds)
{
	LOG_METHOD(xrGetReferenceSpaceBoundsRect);

	Session* pSession = (Session*)session;

	xrGetReferenceSpaceBounds(pSession->index, bounds);

	printf("xrGetReferenceSpaceBoundsRect %s width: %f height: %f\n", string_XrReferenceSpaceType(referenceSpaceType), bounds->width, bounds->height);

	return XR_SUCCESS;
}

//XR_PROC xrSetReferenceSpaceBoundsRect(
//	XrSession            session,
//	XrReferenceSpaceType referenceSpaceType,
//	XrExtent2Df          bounds)
//{
//	return XR_SUCCESS;
//}

XR_PROC xrCreateActionSpace(
	XrSession                      session,
	const XrActionSpaceCreateInfo* createInfo,
	XrSpace*                       space)
{
	LOG_METHOD_ONCE(xrCreateActionSpace);
	assert(createInfo->next == NULL);

	auto hSpace = BLOCK_CLAIM(block.space, 0);
	HANDLE_CHECK(hSpace, XR_ERROR_LIMIT_REACHED);
	auto pSpace = BLOCK_PTR(block.space, hSpace);

	auto pSess = (Session*)session;
	MAP_ADD(pSess->actionSpaces, hSpace, 0);

	*space = (XrSpace)pSpace;

	return XR_SUCCESS;
}

XR_PROC xrLocateSpace(
	XrSpace          space,
	XrSpace          baseSpace,
	XrTime           time,
	XrSpaceLocation* location)
{
	LOG_METHOD_ONCE(xrLocateSpace);

	auto pSpace = (Space*)space;
	auto pSess = BLOCK_PTR(block.session, pSpace->hSession);

	location->locationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT |
							  XR_SPACE_LOCATION_POSITION_VALID_BIT |
							  XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT |
							  XR_SPACE_LOCATION_POSITION_TRACKED_BIT;

	XrVector3f euler;
	XrVector3f pos;
	xrGetHeadPose(pSess->index, &euler, &pos);

	switch (xr.instance.graphicsApi) {
		case XR_GRAPHICS_API_OPENGL:    break;
		case XR_GRAPHICS_API_OPENGL_ES: break;
		case XR_GRAPHICS_API_VULKAN:    break;
		case XR_GRAPHICS_API_D3D11_4:
			XR_CONVERT_D3D11_EULER(euler);
			XR_CONVERT_DD11_POSITION(pos);
			break;
		default: break;
	}

	location->pose.orientation = xrQuaternionFromEuler(euler);
	location->pose.position = pos;

	if (location->next != NULL) {
		switch (*(XrStructureType*)location->next) {
			case XR_TYPE_SPACE_VELOCITY: {
				XrSpaceVelocity* pVelocity = (XrSpaceVelocity*)location->next;
				break;
			}
			default:
				LogNextChain(location->next);
				break;
		}
	};

	return XR_SUCCESS;
}

XR_PROC xrDestroySpace(
	XrSpace space)
{
	LOG_METHOD(xrDestroySpace);
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrDestroySpace\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrEnumerateViewConfigurations(
	XrInstance               instance,
	XrSystemId               systemId,
	uint32_t                 viewConfigurationTypeCapacityInput,
	uint32_t*                viewConfigurationTypeCountOutput,
	XrViewConfigurationType* viewConfigurationTypes)
{
	LOG_METHOD(xrEnumerateViewConfigurations);
	CHECK_INSTANCE(instance);

#define TRANSFER_MODES                                     \
	*viewConfigurationTypeCountOutput = COUNT(modes);      \
	if (viewConfigurationTypes == NULL)                    \
		return XR_SUCCESS;                                 \
	if (viewConfigurationTypeCapacityInput < COUNT(modes)) \
		return XR_ERROR_SIZE_INSUFFICIENT;                 \
	for (int i = 0; i < COUNT(modes); ++i) {               \
		viewConfigurationTypes[i] = modes[i];              \
	}

	switch ((SystemId)systemId) {
		case SYSTEM_ID_HANDHELD_AR: {
			XrViewConfigurationType modes[] = {
				XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO,
			};
			TRANSFER_MODES
			return XR_SUCCESS;
		}
		case SYSTEM_ID_HMD_VR_STEREO: {
			XrViewConfigurationType modes[] = {
				XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO,
				XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
//				XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET,
//				XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT,
//				XR_VIEW_CONFIGURATION_TYPE_PRIMARY_QUAD_VARJO,
			};
			TRANSFER_MODES
			return XR_SUCCESS;
		}
		default:
			LOG_ERROR("XR_ERROR_SYSTEM_INVALID\n");
			return XR_ERROR_SYSTEM_INVALID;
	}

#undef TRANSFER_MODES
}

XR_PROC xrGetViewConfigurationProperties(
	XrInstance                     instance,
	XrSystemId                     systemId,
	XrViewConfigurationType        viewConfigurationType,
	XrViewConfigurationProperties* configurationProperties)
{
	LOG_METHOD(xrGetViewConfigurationProperties);
	printf("%s\n", string_XrViewConfigurationType(viewConfigurationType));
	CHECK_INSTANCE(instance);

	if (!configurationProperties) {
		return XR_ERROR_VALIDATION_FAILURE;
	}

	Instance* pInstance = (Instance*)instance;

	switch (viewConfigurationType) {
		case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:
			configurationProperties->viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO;
			configurationProperties->fovMutable = XR_TRUE;
			return XR_SUCCESS;
		case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
			configurationProperties->viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
			configurationProperties->fovMutable = XR_TRUE;
			return XR_SUCCESS;
		case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET:
			configurationProperties->viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET;
			configurationProperties->fovMutable = XR_TRUE;
			return XR_SUCCESS;
		case XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT:
			configurationProperties->viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT;
			configurationProperties->fovMutable = XR_TRUE;
			return XR_SUCCESS;
		default:
			return XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
	}
}

XR_PROC xrEnumerateViewConfigurationViews(
	XrInstance               instance,
	XrSystemId               systemId,
	XrViewConfigurationType  viewConfigurationType,
	uint32_t                 viewCapacityInput,
	uint32_t*                viewCountOutput,
	XrViewConfigurationView* views)
{
	LOG_METHOD(xrEnumerateViewConfigurationViews);
	LOG("%s\n", string_XrViewConfigurationType(viewConfigurationType));
	CHECK_INSTANCE(instance);

	switch ((SystemId)systemId) {
		case SYSTEM_ID_HANDHELD_AR:

			switch (viewConfigurationType) {
				case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
				case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET:
				case XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT:
					return XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
				default:
					break;
			}

		case SYSTEM_ID_HMD_VR_STEREO:
			break;
		default:
			return XR_ERROR_SYSTEM_INVALID;
	};

	switch (viewConfigurationType) {
		case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:
			*viewCountOutput = 1;
			break;
		case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
			*viewCountOutput = 2;
			break;
		case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET:
			*viewCountOutput = 4;
			break;
		case XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT:
			*viewCountOutput = 1;
			break;
		default:
			return XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
	}

	for (int i = 0; i < viewCapacityInput && i < *viewCountOutput; ++i) {
		views[i] = (XrViewConfigurationView){
			.recommendedImageRectWidth = XR_DEFAULT_WIDTH,
			.maxImageRectWidth = XR_DEFAULT_WIDTH,
			.recommendedImageRectHeight = XR_DEFAULT_HEIGHT,
			.maxImageRectHeight = XR_DEFAULT_HEIGHT,
			.recommendedSwapchainSampleCount = XR_DEFAULT_SAMPLES,
			.maxSwapchainSampleCount = XR_DEFAULT_SAMPLES,
		};
	};

	return XR_SUCCESS;
}

XR_PROC xrEnumerateSwapchainFormats(
	XrSession session,
	uint32_t  formatCapacityInput,
	uint32_t* formatCountOutput,
	int64_t*  formats)
{
	LOG_METHOD(xrEnumerateSwapchainFormats);

	auto pSess = (Session*)session;
	auto hSess = BLOCK_HANDLE(block.session, pSess);
	HANDLE_CHECK(hSess, XR_ERROR_HANDLE_INVALID);

#define TRANSFER_SWAP_FORMATS(_)                    \
	*formatCountOutput = COUNT(_);                  \
	if (formats == NULL)                            \
		return XR_SUCCESS;                          \
	if (formatCapacityInput < COUNT(_))             \
		return XR_ERROR_SIZE_INSUFFICIENT;          \
	for (int i = 0; i < COUNT(_); ++i) {            \
		printf("Enumerating Format: %llu\n", _[i]); \
		formats[i] = _[i];                          \
	}

	switch (xr.instance.graphicsApi) {
		case XR_GRAPHICS_API_OPENGL: {
			int64_t swapFormats[] = {
				GL_SRGB8_ALPHA8,
				GL_SRGB8,
				// unity can't output depth on opengl
//				GL_DEPTH_COMPONENT16,
//				GL_DEPTH_COMPONENT32F,
//				GL_DEPTH24_STENCIL8
			};
			TRANSFER_SWAP_FORMATS(swapFormats);
			return XR_SUCCESS;
		}
		case XR_GRAPHICS_API_VULKAN: {
			int64_t swapFormats[] = {
				VK_FORMAT_R8G8B8A8_UNORM,
				VK_FORMAT_R8G8B8A8_SRGB,
				// unity supported
				VK_FORMAT_D16_UNORM,
				VK_FORMAT_D24_UNORM_S8_UINT,

//				VK_FORMAT_D32_SFLOAT,
			};
			TRANSFER_SWAP_FORMATS(swapFormats);
			return XR_SUCCESS;
		}
		case XR_GRAPHICS_API_D3D11_4: {
			int64_t swapFormats[] = {
				DXGI_FORMAT_R8G8B8A8_UNORM,
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,

				// unity supported
				DXGI_FORMAT_D16_UNORM,
				DXGI_FORMAT_D32_FLOAT_S8X24_UINT

//				DXGI_FORMAT_D32_FLOAT,
//				DXGI_FORMAT_D24_UNORM_S8_UINT,
			};
			TRANSFER_SWAP_FORMATS(swapFormats);
			return XR_SUCCESS;
		}
		default:
			LOG_ERROR("xrEnumerateSwapchainFormats Graphics API not supported.");
			return XR_ERROR_RUNTIME_FAILURE;
	}

#undef TRANSFER_SWAP_FORMATS
}

XR_PROC xrCreateSwapchain(
	XrSession                    session,
	const XrSwapchainCreateInfo* createInfo,
	XrSwapchain*                 swapchain)
{
	LOG_METHOD(xrCreateSwapchain);

	LOG("format: %lld\n", createInfo->format);
	LOG("sampleCount: %u\n", createInfo->sampleCount);
	LOG("width: %u\n", createInfo->width);
	LOG("height: %u\n", createInfo->height);
	LOG("faceCount: %u\n", createInfo->faceCount);
	LOG("arraySize: %u\n", createInfo->arraySize);
	LOG("mipCount: %u\n", createInfo->mipCount);

	// move up to log section
#define PRINT_CREATE_FLAGS(_flag, _bit)  \
	if (createInfo->createFlags & _flag) \
		printf("flag: " #_flag "\n");

	XR_LIST_BITS_XrSwapchainCreateFlags(PRINT_CREATE_FLAGS);

#undef PRINT_CREATE_FLAGS

#define PRINT_USAGE_FLAGS(_flag, _bit)  \
	if (createInfo->usageFlags & _flag) \
		printf("usage: " #_flag "\n");

	XR_LIST_BITS_XrSwapchainUsageFlags(PRINT_USAGE_FLAGS);

#undef PRINT_USAGE_FLAGS

	if (createInfo->createFlags & XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT) {
		LOG_ERROR("XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT unsupported \n");
		return XR_ERROR_FEATURE_UNSUPPORTED;
	}

	auto pSess = (Session*)session;
	auto hSess = BLOCK_HANDLE(block.session, pSess);

	int expectedWidth = XR_DEFAULT_WIDTH;
	int expectedHeight = XR_DEFAULT_HEIGHT;

	if (createInfo->width * 2 == expectedWidth) {
		pSess->swapType = XR_SWAP_TYPE_STEREO_DOUBLE_WIDE;
		expectedWidth *= 2;
		printf("Setting XR_SWAP_TYPE_STEREO_DOUBLE_WIDE for session.\n");
	}
	if (createInfo->arraySize > 1) {
		pSess->swapType = XR_SWAP_TYPE_STEREO_TEXTURE_ARRAY;
		printf("Setting XR_SWAP_TYPE_STEREO_TEXTURE_ARRAY for session.\n");
	}

	if (createInfo->width != expectedWidth || createInfo->height != expectedHeight) {
		LOG_ERROR("XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED %d %d\n", expectedWidth, expectedHeight);
		return XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED;
	}

	auto hSwap = BLOCK_CLAIM(block.swap, 0);
	HANDLE_CHECK(hSwap, XR_ERROR_LIMIT_REACHED);
	auto pSwap = BLOCK_PTR(block.swap, hSwap);

	pSwap->hSession = hSess;
	pSwap->createFlags = createInfo->createFlags;
	pSwap->usageFlags = createInfo->usageFlags;
	pSwap->format = createInfo->format;
	pSwap->sampleCount = createInfo->sampleCount;
	pSwap->width = createInfo->width;
	pSwap->height = createInfo->height;
	pSwap->faceCount = createInfo->faceCount;
	pSwap->arraySize = createInfo->arraySize;
	pSwap->mipCount = createInfo->mipCount;

	HANDLE colorHandles[XR_SWAP_COUNT];
	xrClaimFramebufferImages(pSess->index, XR_SWAP_COUNT, colorHandles);

	switch (xr.instance.graphicsApi) {
		case XR_GRAPHICS_API_OPENGL: {
			printf("Creating OpenGL Swap");

#define DEFAULT_IMAGE_CREATE_INFO(_width, _height, _format, _memObject, _texture, _handle)                    \
	gl.CreateMemoryObjectsEXT(1, &_memObject);                                                                \
	gl.ImportMemoryWin32HandleEXT(_memObject, _width* _height * 4, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, _handle); \
	gl.CreateTextures(GL_TEXTURE_2D, 1, &_texture);                                                           \
	gl.TextureStorageMem2DEXT(_texture, 1, _format, _width, _height, _memObject, 0);
			for (int i = 0; i < XR_SWAP_COUNT; ++i) {
				DEFAULT_IMAGE_CREATE_INFO(pSwap->width, pSwap->height, GL_RGBA8, pSwap->color[i].gl.memObject, pSwap->color[i].gl.texture, colorHandles[i]);
				printf("Imported gl swap texture. Texture: %d MemObject: %d\n", pSwap->color[i].gl.texture, pSwap->color[i].gl.memObject);
			}
#undef DEFAULT_IMAGE_CREATE_INFO

			break;
		}
		case XR_GRAPHICS_API_D3D11_4: {
			printf("Creating D3D11 Swap\n");

			ID3D11Device5* device5 = pSess->binding.d3d11.device5;
			for (int i = 0; i < XR_SWAP_COUNT; ++i) {
				DX_CHECK(ID3D11Device5_OpenSharedResource1(device5, colorHandles[i], &IID_ID3D11Texture2D, (void**)&pSwap->color[i].d3d11.texture));
				printf("Imported d3d11 swap texture. Device: %p Handle: %p Texture: %p\n", device5, colorHandles[i], pSwap->color[i].d3d11.texture);
//				D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {
//					.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
//					.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
//					.Texture2D.MipSlice = 0,
//				};
//				DX_CHECK(ID3D11Device5_CreateRenderTargetView(device5, (ID3D11Resource*)pSwap->color[i].d3d11.texture, &rtvDesc, &pSwap->color[i].d3d11.rtView));
//				DX_CHECK(ID3D11Texture2D_QueryInterface(pSwap->color[i].d3d11.texture, &IID_IDXGIKeyedMutex, (void**)&pSwap->color[i].d3d11.keyedMutex));
//				IDXGIKeyedMutex_AcquireSync(pSwap->color[i].d3d11.keyedMutex, 0, INFINITE);
			}

			break;
		}
		case XR_GRAPHICS_API_VULKAN:
		default:
			return XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED;
	}

	*swapchain = (XrSwapchain)pSwap;
	return XR_SUCCESS;
}

XR_PROC xrDestroySwapchain(
	XrSwapchain swapchain)
{
	LOG_METHOD(xrDestroySwapchain);
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrDestroySwapchain\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrEnumerateSwapchainImages(
	XrSwapchain                 swapchain,
	uint32_t                    imageCapacityInput,
	uint32_t*                   imageCountOutput,
	XrSwapchainImageBaseHeader* images)
{
	LOG_METHOD_ONCE(xrEnumerateSwapchainImages);

	*imageCountOutput = XR_SWAP_COUNT;

	if (images == NULL)
		return XR_SUCCESS;

	if (imageCapacityInput != XR_SWAP_COUNT)
		return XR_ERROR_SIZE_INSUFFICIENT;

	Swapchain* pSwapchain = (Swapchain*)swapchain;

	switch (images[0].type) {
		case XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR: {
			printf("Enumerating gl Swapchain Images\n");
			XrSwapchainImageOpenGLKHR* pImage = (XrSwapchainImageOpenGLKHR*)images;
			for (int i = 0; i < imageCapacityInput && i < XR_SWAP_COUNT; ++i) {
				assert(pImage[i].type == XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR);
				assert(pImage[i].next == NULL);
				assert(pImage[i].image == 0);
				pImage[i].image = pSwapchain->color[i].gl.texture;
			}
			break;
		}
		case XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR: {
			printf("Enumerating d3d11 Swapchain Images\n");
			XrSwapchainImageD3D11KHR* pImage = (XrSwapchainImageD3D11KHR*)images;
			for (int i = 0; i < imageCapacityInput && i < XR_SWAP_COUNT; ++i) {
				assert(pImage[i].type == XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR);
				assert(pImage[i].next == NULL);
				assert(pImage[i].texture == NULL);
				pImage[i].texture = pSwapchain->color[i].d3d11.texture;
			}
			break;
		}
		default:
			LOG_ERROR("Swap type not currently supported\n");
			return XR_ERROR_HANDLE_INVALID;
	}

	return XR_SUCCESS;
}

XR_PROC xrAcquireSwapchainImage(
	XrSwapchain                        swapchain,
	const XrSwapchainImageAcquireInfo* acquireInfo,
	uint32_t*                          index)
{
	LOG_METHOD_ONCE(xrAcquireSwapchainImage);
	assert(acquireInfo->next == NULL);

	auto pSwap = (Swapchain*)swapchain;
	auto pSess = BLOCK_PTR(block.session, pSwap->hSession);

	xrClaimSwapPoolImage(pSess->index, &pSwap->swapIndex);

	*index = pSwap->swapIndex;

	return XR_SUCCESS;
}

XR_PROC xrWaitSwapchainImage(
	XrSwapchain                     swapchain,
	const XrSwapchainImageWaitInfo* waitInfo)
{
	LOG_METHOD_ONCE(xrWaitSwapchainImage);
	assert(waitInfo->next == NULL);

	auto pSwap = (Swapchain*)swapchain;
	auto pSess = BLOCK_PTR(block.session, pSwap->hSession);

	ID3D11DeviceContext4*   context4 = pSess->binding.d3d11.context4;

//	IDXGIKeyedMutex* keyedMutex = pSwapchain->color[pSwapchain->swapIndex].d3d11.keyedMutex;
//
//	ID3D11DeviceContext4_Flush(context4);
//	IDXGIKeyedMutex_ReleaseSync(keyedMutex, 0);

//	ID3D11RenderTargetView* rtView = pSwapchain->color[pSwapchain->swapIndex].d3d11.rtView;
//	ID3D11DeviceContext4_OMSetRenderTargets(context4, 1, &rtView, NULL);
//	ID3D11DeviceContext4_ClearRenderTargetView(context4, rtView, (float[]){0.0f, 0.0f, 0.0f, 0.0f});

	return XR_SUCCESS;
}

XR_PROC xrReleaseSwapchainImage(
	XrSwapchain                        swapchain,
	const XrSwapchainImageReleaseInfo* releaseInfo)
{
	LOG_METHOD_ONCE(xrReleaseSwapchainImage);
	assert(releaseInfo->next == NULL);

	auto pSwap = (Swapchain*)swapchain;
	auto pSess = BLOCK_PTR(block.session, pSwap->hSession);

	xrReleaseSwapPoolImage(pSess->index, pSwap->swapIndex);

	ID3D11DeviceContext4*   context4 = pSess->binding.d3d11.context4;

//	IDXGIKeyedMutex* keyedMutex = pSwapchain->color[pSwapchain->swapIndex].d3d11.keyedMutex;
//
//	ID3D11DeviceContext4_Flush(context4);
//	IDXGIKeyedMutex_AcquireSync(keyedMutex, 0, INFINITE);

//	ID3D11RenderTargetView* nullRTView[] = {NULL};
//	ID3D11DeviceContext4_OMSetRenderTargets(context4, 1, nullRTView, NULL);

	return XR_SUCCESS;
}

XR_PROC xrBeginSession(
	XrSession                 session,
	const XrSessionBeginInfo* beginInfo)
{
	LOG_METHOD_ONCE(xrBeginSession);
	LogNextChain(beginInfo->next);

	Session* pSession = (Session*)session;

	if (pSession->running) {
		LOG_ERROR("XR_ERROR_SESSION_RUNNING\n");
		return XR_ERROR_SESSION_RUNNING;
	}

	if (pSession->activeSessionState != XR_SESSION_STATE_READY) {
		LOG_ERROR("XR_ERROR_SESSION_NOT_READY\n");
		return XR_ERROR_SESSION_NOT_READY;
	}

	pSession->running = true;
	pSession->primaryViewConfigurationType = beginInfo->primaryViewConfigurationType;

//	switch (xr.instance.graphicsApi) {
//		case XR_GRAPHICS_API_OPENGL:    break;
//		case XR_GRAPHICS_API_OPENGL_ES: break;
//		case XR_GRAPHICS_API_VULKAN:    break;
//		case XR_GRAPHICS_API_D3D11_4:   break;
//		default:
//			LOG_ERROR("Graphics API not supported.\n");
//			return XR_ERROR_RUNTIME_FAILURE;
//	}

	if (beginInfo->next != NULL) {
		XrSecondaryViewConfigurationSessionBeginInfoMSFT* secondBeginInfo = (XrSecondaryViewConfigurationSessionBeginInfoMSFT*)beginInfo->next;
		assert(secondBeginInfo->type == XR_TYPE_SECONDARY_VIEW_CONFIGURATION_SESSION_BEGIN_INFO_MSFT);
		assert(secondBeginInfo->next == NULL);
		for (int i = 0; i < secondBeginInfo->viewConfigurationCount; ++i) {
			printf("Secondary ViewConfiguration: %d", secondBeginInfo->enabledViewConfigurationTypes[i]);
		}
	}

	return XR_SUCCESS;
}

XR_PROC xrEndSession(
	XrSession session)
{
	LOG_METHOD(xrEndSession);

	Session* pSession = (Session*)session;

	if (!pSession->running) {
		LOG_ERROR("XR_ERROR_SESSION_NOT_RUNNING\n");
		return XR_ERROR_SESSION_NOT_RUNNING;
	}

	if (pSession->activeSessionState != XR_SESSION_STATE_STOPPING) {
		LOG_ERROR("XR_ERROR_SESSION_NOT_STOPPING\n");
		return XR_ERROR_SESSION_NOT_STOPPING;
	}

	pSession->pendingSessionState = XR_SESSION_STATE_IDLE;
	pSession->running = false;

	return XR_SUCCESS;
}

XR_PROC xrRequestExitSession(
	XrSession session)
{
	LOG_METHOD(xrRequestExitSession);

	Session* pSession = (Session*)session;

	if (!pSession->running) {
		LOG_ERROR("XR_ERROR_SESSION_NOT_RUNNING\n");
		return XR_ERROR_SESSION_NOT_RUNNING;
	}

	pSession->pendingSessionState = XR_SESSION_STATE_EXITING;
	pSession->exiting = true;

	return XR_SUCCESS;
}

XR_PROC xrWaitFrame(
	XrSession              session,
	const XrFrameWaitInfo* frameWaitInfo,
	XrFrameState*          frameState)
{
	LOG_METHOD_ONCE(xrWaitFrame);

	if (frameWaitInfo != NULL)
		assert(frameWaitInfo->next == NULL);

	auto pSess = (Session*)session;
	if (!pSess->running) {
		LOG_ERROR("XR_ERROR_SESSION_NOT_RUNNING\n");
		return XR_ERROR_SESSION_NOT_RUNNING;
	}

	XrTimeWaitWin32(&pSess->frameBegan, pSess->frameWaited);

	if (pSess->lastDisplayTime == 0) {
		uint64_t initialTimelineValue;
		switch (xr.instance.graphicsApi) {
			case XR_GRAPHICS_API_D3D11_4:
				initialTimelineValue = ID3D11Fence_GetCompletedValue(pSess->binding.d3d11.compositorFence);
				break;
			default:
				LOG_ERROR("Graphics API not supported.\n");
				return XR_ERROR_RUNTIME_FAILURE;
		}
		xrSetInitialCompositorTimelineValue(pSess->index, initialTimelineValue);
	}

	bool synchronized = pSess->activeSessionState >= XR_SESSION_STATE_SYNCHRONIZED;
	uint64_t compositorTimelineValue;
	xrGetCompositorTimelineValue(pSess->index, synchronized, &compositorTimelineValue);

	switch (xr.instance.graphicsApi) {
		case XR_GRAPHICS_API_OPENGL:    break;
		case XR_GRAPHICS_API_OPENGL_ES: break;
		case XR_GRAPHICS_API_VULKAN:    break;
		case XR_GRAPHICS_API_D3D11_4:   {
			ID3D11DeviceContext4* context4 = pSess->binding.d3d11.context4;
			ID3D11Fence*          compositorFence = pSess->binding.d3d11.compositorFence;

			ID3D11DeviceContext4_Flush(context4);
			ID3D11DeviceContext4_Wait(context4, compositorFence, compositorTimelineValue);

			HANDLE eventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
			if (!eventHandle) {
				LOG_ERROR("Failed to create event handle.\n");
				return XR_ERROR_RUNTIME_FAILURE;
			}
			DX_CHECK(ID3D11Fence_SetEventOnCompletion(compositorFence, compositorTimelineValue, eventHandle));
			WaitForSingleObject(eventHandle, INFINITE);
			CloseHandle(eventHandle);

			break;
		}
		default:
			LOG_ERROR("Graphics API not supported.\n");
			return XR_ERROR_RUNTIME_FAILURE;
	}

	XrTime currentTime = xrGetTime();
	XrTime lastFrameTime = pSess->lastDisplayTime == 0 ? currentTime : pSess->lastDisplayTime;
	XrTime frameInterval = xrGetFrameInterval(pSess->index, synchronized);

	frameState->predictedDisplayPeriod = frameInterval;
	frameState->predictedDisplayTime = lastFrameTime + frameInterval;
	frameState->shouldRender = pSess->activeSessionState == XR_SESSION_STATE_VISIBLE ||
							   pSess->activeSessionState == XR_SESSION_STATE_FOCUSED;

	memmove(&pSess->predictedDisplayTimes[1], &pSess->predictedDisplayTimes[0], sizeof(pSess->predictedDisplayTimes[0]) * (COUNT(pSess->predictedDisplayTimes) - 1));
	pSess->predictedDisplayTimes[0] = frameState->predictedDisplayTime;
	pSess->lastBeginDisplayTime = currentTime;

//	double frameIntervalMs = xrTimeToMilliseconds(frameInterval);
//	double fps = 1.0 / MillisecondsToSeconds(frameIntervalMs);
//	printf("xrWaitFrame frameIntervalMs: %f fps: %f\n", frameIntervalMs, fps);

	__atomic_add_fetch(&pSess->frameWaited, 1, __ATOMIC_RELEASE);

	return XR_SUCCESS;
}

XR_PROC xrBeginFrame(
	XrSession               session,
	const XrFrameBeginInfo* frameBeginInfo)
{
	LOG_METHOD_ONCE(xrBeginFrame);

	if (frameBeginInfo != NULL)
		assert(frameBeginInfo->next == NULL);

	Session* pSession = (Session*)session;
	if (!pSession->running) {
		LOG_ERROR("XR_ERROR_SESSION_NOT_RUNNING\n");
		return XR_ERROR_SESSION_NOT_RUNNING;
	}
	if (pSession->frameWaited == pSession->frameEnded || pSession->frameWaited == pSession->frameBegan) {
		LOG_ERROR("XR_ERROR_CALL_ORDER_INVALID \n");
		return XR_ERROR_CALL_ORDER_INVALID;
	}

	bool discardFrame = false;
	bool frameWaitCalled = pSession->frameBegan < pSession->frameWaited;
	bool frameEndCalled = pSession->frameEnded == pSession->frameWaited - 1;
	if (frameWaitCalled && frameEndCalled) {

		XrTimeWaitWin32(&pSession->frameEnded, pSession->frameBegan);

	} else {

		LOG_ERROR("XR_FRAME_DISCARDED\n");
		discardFrame = true;

	}

	XrTimeSignalWin32(&pSession->frameBegan, pSession->frameWaited);

	return discardFrame ? XR_FRAME_DISCARDED : XR_SUCCESS;
}

XR_PROC xrEndFrame(
	XrSession             session,
	const XrFrameEndInfo* frameEndInfo)
{
	LOG_METHOD_ONCE(xrEndFrame);
	assert(frameEndInfo->next == NULL);

	Session* pSession = (Session*)session;

	if (!pSession->running || pSession->activeSessionState == XR_SESSION_STATE_IDLE ||  pSession->activeSessionState == XR_SESSION_STATE_EXITING) {
		LOG_ERROR("XR_ERROR_SESSION_NOT_RUNNING\n");
		return XR_ERROR_SESSION_NOT_RUNNING;
	}
	if (frameEndInfo->displayTime <= 0 ) {
		LOG_ERROR("XR_ERROR_TIME_INVALID \n");
		return XR_ERROR_TIME_INVALID ;
	}
	if (pSession->frameBegan == pSession->frameEnded) {
		LOG_ERROR("XR_ERROR_CALL_ORDER_INVALID \n");
		return XR_ERROR_CALL_ORDER_INVALID;
	}

	bool displayTimeFound = false;
	for (int i = 0; i < COUNT(pSession->predictedDisplayTimes); ++i) {
		if (i > 0)
			printf("MISSED FRAME END TIME\n");
		if (pSession->predictedDisplayTimes[i] == frameEndInfo->displayTime) {
			displayTimeFound = true;
			break;
		}
	}
	if (!displayTimeFound) {
		LOG_ERROR("XR_ERROR_TIME_INVALID\n");
		return XR_ERROR_TIME_INVALID;
	}

	for (int layer = 0; layer < frameEndInfo->layerCount; ++layer) {

		if (frameEndInfo->layers[layer] == NULL) {
			LOG_ERROR("XR_ERROR_LAYER_INVALID\n");
			return XR_ERROR_LAYER_INVALID;
		}

		switch (frameEndInfo->layers[layer]->type) {
			case XR_TYPE_COMPOSITION_LAYER_PROJECTION: {
				auto pProjectionLayer = (XrCompositionLayerProjection*)frameEndInfo->layers[layer];
				assert(pProjectionLayer->viewCount == 2);

				for (int view = 0; view < pProjectionLayer->viewCount; ++view) {
					auto pView = &pProjectionLayer->views[view];
					assert(pView->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW);
//					printf("subImage %d " FORMAT_RECT_I "\n", pView->subImage.imageArrayIndex, EXPAND_RECT(pView->subImage.imageRect));
				}

				break;
			}
			case XR_TYPE_COMPOSITION_LAYER_QUAD: {
				LOG("XR_TYPE_COMPOSITION_LAYER_QUAD");
			}
			default: {
				LOG_ERROR("Unknown Composition layer %d", frameEndInfo->layers[layer]->type);
				break;
			}
		}
	}

	bool synchronized = pSession->activeSessionState >= XR_SESSION_STATE_SYNCHRONIZED;

	{
		XrTime timeBeforeWait = xrGetTime();

		pSession->sessionTimelineValue++;
		uint64_t sessionTimelineValue = pSession->sessionTimelineValue;

		ID3D11DeviceContext4* context4 = pSession->binding.d3d11.context4;
		ID3D11Fence*          sessionFence = pSession->binding.d3d11.sessionFence;

		ID3D11DeviceContext4_Flush(context4);
		ID3D11DeviceContext4_Signal(context4, sessionFence, sessionTimelineValue);
		ID3D11DeviceContext4_Wait(context4, sessionFence, sessionTimelineValue);

		// causes CPU to wait
		HANDLE eventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (!eventHandle) {
			LOG_ERROR("Failed to create event handle.\n");
			return XR_ERROR_RUNTIME_FAILURE;
		}
		DX_CHECK(ID3D11Fence_SetEventOnCompletion(sessionFence, sessionTimelineValue, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);

		xrSetSessionTimelineValue(pSession->index, sessionTimelineValue);

//		XrTime timeAfterWait = xrGetTime();
//		XrTime waitTime = timeAfterWait - timeBeforeWait;
//		double waitTimeMs = xrTimeToMilliseconds(waitTime);
//		printf("sessionTimelineValue: %llu waitTimeMs: %f\n", sessionTimelineValue, waitTimeMs);
	}

	{
		XrTime currenTime = xrGetTime();

		XrTime delta = currenTime - pSession->lastBeginDisplayTime;
		double deltaMs = xrTimeToMilliseconds(delta);
		double fps = 1.0 / MillisecondsToSeconds(deltaMs);

		XrTime discrepancy = currenTime - frameEndInfo->displayTime;
		double discrepancyMs = xrTimeToMilliseconds(discrepancy);
		XrTime frameInterval = xrGetFrameInterval(pSession->index, synchronized);

		if (discrepancy > 0) {
//			printf("Frame took %f ms longer than predicted.\n", discrepancyMs);
			pSession->synchronizedFrameCount = 0;

			uint64_t initialTimelineValue;
			switch (xr.instance.graphicsApi) {
				case XR_GRAPHICS_API_D3D11_4:
					initialTimelineValue = ID3D11Fence_GetCompletedValue(pSession->binding.d3d11.compositorFence);
					break;
				default:
					LOG_ERROR("Graphics API not supported.\n");
					return XR_ERROR_RUNTIME_FAILURE;
			}
			xrSetInitialCompositorTimelineValue(pSession->index, initialTimelineValue);

		} else {
			pSession->synchronizedFrameCount++;
		}

		pSession->lastDisplayTime = currenTime;

//		printf("xrEndFrame discrepancy: %f deltaMs: %f fps: %f sync: %d\n", discrepancyMs, deltaMs, fps, pSession->synchronizedFrameCount);

		// You wait till first end frame to say synchronized. I don't know if this is really needed... maybe just a chrome quirk
//		if (pSession->activeSessionState == XR_SESSION_STATE_READY && pSession->synchronizedFrameCount > 4)
		if (pSession->activeSessionState == XR_SESSION_STATE_READY && pSession->sessionTimelineValue > 2)
			pSession->pendingSessionState = XR_SESSION_STATE_SYNCHRONIZED;

		if (pSession->activeSessionState == XR_SESSION_STATE_SYNCHRONIZED)
			pSession->pendingSessionState = XR_SESSION_STATE_VISIBLE;

		//	if (pSession->activeSessionState == XR_SESSION_STATE_VISIBLE)
		//		pSession->pendingSessionState = XR_SESSION_STATE_FOCUSED;

		xrProgressCompositorTimelineValue(pSession->index, 0, synchronized);
		XrTimeSignalWin32(&pSession->frameEnded, pSession->frameBegan);
	}

	return XR_SUCCESS;
}

XR_PROC xrLocateViews(
	XrSession               session,
	const XrViewLocateInfo* viewLocateInfo,
	XrViewState*            viewState,
	uint32_t                viewCapacityInput,
	uint32_t*               viewCountOutput,
	XrView*                 views)
{
	LOG_METHOD_ONCE(xrLocateViews);
//	printf("Locating View %s\n", string_XrViewConfigurationType(viewLocateInfo->viewConfigurationType));
	assert(views->next == NULL);

	viewState->viewStateFlags = XR_VIEW_STATE_ORIENTATION_VALID_BIT |
								XR_VIEW_STATE_POSITION_VALID_BIT |
								XR_VIEW_STATE_ORIENTATION_TRACKED_BIT |
								XR_VIEW_STATE_POSITION_TRACKED_BIT;

	switch (viewLocateInfo->viewConfigurationType) {
		default:
		case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:
			*viewCountOutput = 1;
			break;
		case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
			*viewCountOutput = 2;
			break;
		case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET:
			*viewCountOutput = 4;
			break;
		case XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT:
			*viewCountOutput = 1;  // not sure
			break;
	}

	if (views == NULL)
		return XR_SUCCESS;

	Session*  pSession = (Session*)session;

	for (int i = 0; i < viewCapacityInput; ++i) {
		XrEyeView eyeView;
		xrGetEyeView(pSession->index, i, &eyeView);

		switch (xr.instance.graphicsApi) {
			case XR_GRAPHICS_API_OPENGL:    break;
			case XR_GRAPHICS_API_OPENGL_ES: break;
			case XR_GRAPHICS_API_VULKAN:    break;
			case XR_GRAPHICS_API_D3D11_4:
				XR_CONVERT_D3D11_EULER(eyeView.euler);
				XR_CONVERT_DD11_POSITION(eyeView.position);
				break;
			default: break;
		}

		float fovHalfXRad = eyeView.fovRad.x / 2.0f;
		float fovHalfYRad = eyeView.fovRad.y / 2.0f;
		float angleLeft = xrFloatLerp(-fovHalfXRad, fovHalfXRad, eyeView.upperLeftClip.x);
		float angleRight = xrFloatLerp(-fovHalfXRad, fovHalfXRad, eyeView.lowerRightClip.x);
		float angleUp;
		float angleDown;

		switch (xr.instance.graphicsApi) {
			default:
			case XR_GRAPHICS_API_OPENGL:
			case XR_GRAPHICS_API_OPENGL_ES:
			case XR_GRAPHICS_API_VULKAN:
				angleUp = xrFloatLerp(-fovHalfYRad, fovHalfYRad, eyeView.upperLeftClip.y);
				angleDown = xrFloatLerp(-fovHalfYRad, fovHalfYRad, eyeView.lowerRightClip.y);
				break;
			case XR_GRAPHICS_API_D3D11_4:
				// DX11 Y needs to invert
				angleUp = xrFloatLerp(-fovHalfYRad, fovHalfYRad, 1.0f - eyeView.upperLeftClip.y);
				angleDown = xrFloatLerp(-fovHalfYRad, fovHalfYRad, 1.0f - eyeView.lowerRightClip.y);

				// Some OXR implementations (Unity) cannot properly calculate the width and height of the projection matrices unless all the angles are negative.
				angleUp -= PI * 2;
				angleDown -= PI * 2;
				angleLeft -= PI * 2;
				angleRight -= PI * 2;

				break;
		}

		views[i].pose.orientation = xrQuaternionFromEuler(eyeView.euler);
		views[i].pose.position = eyeView.position;
		views[i].fov.angleLeft = angleLeft;
		views[i].fov.angleRight = angleRight;
		views[i].fov.angleUp = angleUp;
		views[i].fov.angleDown = angleDown;

		// do this? the app could calculate the subimage and pass it back with endframe info
		// but xrEnumerateViewConfigurationViews instance based, not session based, so it can't be relied on to get expected frame size
		// visibility mask is not checked every render, so that cannot be used
		// only way to control rendered area every frame by existing standard is changing the swap size
		// We could use foveation? Not all foveation APIs offer explicit width height.
		// We might actually need swaps of different sizes, and different heights/widths
		// We do need an in-spec solution if this is to work with everything initially
		if (views[i].next != NULL) {
			XrStructureTypeExt* type = (XrStructureTypeExt*)views[i].next;
			switch (*type) {
				case XR_TYPE_SUB_VIEW: {
					XrSubView* pSubView = (XrSubView*)views[i].next;
					pSubView->imageRect.extent.width = eyeView.upperLeftClip.x - eyeView.lowerRightClip.x;
					pSubView->imageRect.extent.height = eyeView.upperLeftClip.y - eyeView.lowerRightClip.y;
					break;
				}
				default:
					printf("EyeView->Next Unknown: %d\n", *type);
			}
		}
	}

	return XR_SUCCESS;
}

XR_PROC xrStringToPath(
	XrInstance  instance,
	const char* pathString,
	XrPath*     path)
{
	LOG_METHOD_ONCE(xrStringToPath);
	CHECK_INSTANCE(instance);

	auto pathHash = CalcDJB2(pathString, XR_MAX_PATH_LENGTH);
	for (int i = 0; i < XR_PATH_CAPACITY; ++i) {
		if (block.path.keys[i] != pathHash)
			continue;
		if (strncmp(block.path.blocks[i].string, pathString, XR_MAX_PATH_LENGTH)) {
			LOG_ERROR("Path Hash Collision! %s | %s\n", block.path.blocks[i].string, pathString);
			return XR_ERROR_PATH_INVALID;
		}
		printf("Path Handle Found: %d\n    %s\n", i, block.path.blocks[i].string);
		*path = (XrPath)&block.path.blocks[i];
		return XR_SUCCESS;
	}

	auto hPath = BLOCK_CLAIM(block.path, pathHash);
	HANDLE_CHECK(hPath, XR_ERROR_PATH_COUNT_EXCEEDED);

	auto i = HANDLE_INDEX(hPath);

	auto pPath = BLOCK_PTR(block.path, hPath);
	strncpy(pPath->string, pathString, XR_MAX_PATH_LENGTH);
	pPath->string[XR_MAX_PATH_LENGTH - 1] = '\0';

	*path = (XrPath)pPath;

	return XR_SUCCESS;
}

XR_PROC xrPathToString(
	XrInstance instance,
	XrPath     path,
	uint32_t   bufferCapacityInput,
	uint32_t*  bufferCountOutput,
	char*      buffer)
{
	LOG_METHOD_ONCE(xrPathToString);
	CHECK_INSTANCE(instance);

	auto pPath = (Path*)path;

	*bufferCountOutput = strlen(pPath->string) + 1;

	if (buffer == NULL)
		return XR_SUCCESS;

	if (bufferCapacityInput > XR_MAX_PATH_LENGTH) {
		LOG_ERROR("XR_ERROR_VALIDATION_FAILURE\n");
		return XR_ERROR_VALIDATION_FAILURE;
	}

	if (bufferCapacityInput < *bufferCountOutput) {
		LOG_ERROR("XR_ERROR_SIZE_INSUFFICIENT\n");
		return XR_ERROR_SIZE_INSUFFICIENT;
	}

	strncpy(buffer, pPath->string, *bufferCountOutput);

	return XR_SUCCESS;
}

XR_PROC xrCreateActionSet(
	XrInstance                   instance,
	const XrActionSetCreateInfo* createInfo,
	XrActionSet*                 actionSet)
{
#ifdef ENABLE_DEBUG_LOG_METHOD
	LOG_METHOD(xrCreateActionSet);
	printf("%lu:%lu: xrCreateActionSet %s %s\n", GetCurrentProcessId(), GetCurrentThreadId(), createInfo->actionSetName, createInfo->localizedActionSetName);
#endif
	assert(createInfo->next == NULL);

	block_key actionSetNameHash = CalcDJB2(createInfo->actionSetName, XR_MAX_ACTION_SET_NAME_SIZE);

	auto hFoundActionSet = BLOCK_FIND(block.actionSet, actionSetNameHash);
	if (HANDLE_VALID(hFoundActionSet)) {
		LOG_ERROR("XR_ERROR_LOCALIZED_NAME_DUPLICATED %s\n", createInfo->actionSetName);
		return XR_ERROR_LOCALIZED_NAME_DUPLICATED;
	}

	auto hActionSet = BLOCK_CLAIM(block.actionSet, actionSetNameHash);
	HANDLE_CHECK(hActionSet, XR_ERROR_LIMIT_REACHED);

	auto pActionSet = BLOCK_PTR(block.actionSet, hActionSet);
	*pActionSet = (ActionSet){};

	pActionSet->attachedToSession = HANDLE_DEFAULT;

	strncpy((char*)&pActionSet->actionSetName, (const char*)&createInfo->actionSetName, XR_MAX_ACTION_SET_NAME_SIZE);
	strncpy((char*)&pActionSet->localizedActionSetName, (const char*)&createInfo->localizedActionSetName, XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE);
	pActionSet->priority = createInfo->priority;

	*actionSet = (XrActionSet)pActionSet;

	printf("Created ActionSet with %s\n", createInfo->actionSetName);
	printf("Created ActionSet with %s\n", createInfo->localizedActionSetName);
	return XR_SUCCESS;
}

XR_PROC xrDestroyActionSet(
	XrActionSet actionSet)
{
	LOG_METHOD(xrDestroyActionSet);
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrDestroyActionSet\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrCreateAction(
	XrActionSet               actionSet,
	const XrActionCreateInfo* createInfo,
	XrAction*                 action)
{
#ifdef ENABLE_DEBUG_LOG_METHOD
//	LOG_METHOD(xrCreateAction);
//	printf("%lu:%lu: xrCreateAction %s %s\n", GetCurrentProcessId(), GetCurrentThreadId(), createInfo->actionName, createInfo->localizedActionName);
#endif
	assert(createInfo->next == NULL);

	auto pActionSet = (ActionSet*)actionSet;
	auto hActionSet = BLOCK_HANDLE(block.actionSet, pActionSet);
	HANDLE_CHECK(hActionSet, XR_ERROR_VALIDATION_FAILURE);

	HANDLE_CHECK(pActionSet->attachedToSession, XR_ERROR_ACTIONSETS_ALREADY_ATTACHED);

	if (createInfo->countSubactionPaths > XR_MAX_SUBACTION_PATHS) {
		LOG_ERROR("XR_ERROR_PATH_COUNT_EXCEEDED");
		return XR_ERROR_PATH_COUNT_EXCEEDED;
	}

	block_handle actionHandle = BLOCK_CLAIM(block.action, 0);
	HANDLE_CHECK(actionHandle, XR_ERROR_LIMIT_REACHED);
	Action* pAction = BLOCK_PTR(block.action, actionHandle);
	*pAction = (Action){};

	pAction->hActionSet = hActionSet;
	pAction->actionType = createInfo->actionType;
	pAction->countSubactions = createInfo->countSubactionPaths;
	for (int i = 0; i < createInfo->countSubactionPaths; ++i)
		pAction->hSubactionPaths[i] = BLOCK_HANDLE(block.path, (Path*)createInfo->subactionPaths[i]);
	strncpy((char*)&pAction->actionName, (const char*)&createInfo->actionName, XR_MAX_ACTION_SET_NAME_SIZE);
	strncpy((char*)&pAction->localizedActionName, (const char*)&createInfo->localizedActionName, XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE);

	*action = (XrAction)pAction;

	//	printf("	actionName: %s\n", pAction->actionName);
	//	printf("	localizedActionName %s\n", pAction->localizedActionName);
	//	printf("	actionType: %d\n", pAction->actionType);
	//	printf("	countSubactionPaths: %d\n", pAction->countSubactionPaths);
	//	for (int i = 0; i < createInfo->countSubactionPaths; ++i) {
	//		Path* pPath = (Path*)pAction->subactionPaths[i];
	//		printf("		subactionPath: %s\n", pPath->string);
	//	}

	return XR_SUCCESS;
}

XR_PROC xrDestroyAction(
	XrAction action)
{
	LOG_METHOD(xrDestroyAction);
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrDestroyAction\n");

	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

static int CompareSubPath(const char* pSubPath, const char* pPath)
{
	while (*pSubPath != '\0') {
		if (*pSubPath != *pPath)
			return 1;

		pSubPath++;
		pPath++;
	}
	return 0;
}

XR_PROC xrSuggestInteractionProfileBindings(
	XrInstance                                  instance,
	const XrInteractionProfileSuggestedBinding* suggestedBindings)
{
	LOG_METHOD(xrSuggestInteractionProfileBindings);
	LogNextChain(suggestedBindings->next);
	assert(suggestedBindings->next == NULL);
	CHECK_INSTANCE(instance);

	auto pProfilePath = (Path*)suggestedBindings->interactionProfile;
	auto profilePathHash = BLOCK_KEY(block.path, pProfilePath);

	auto hProfile = BLOCK_FIND(block.profile, profilePathHash);
	HANDLE_CHECK(hProfile, XR_ERROR_PATH_UNSUPPORTED);

	auto pProfile = BLOCK_PTR(block.profile, hProfile);
	printf("Binding: %s\n", pProfilePath->string);

	const auto pSuggestions = suggestedBindings->suggestedBindings;

	// Pre-emptively check everything so you don't end up with a half finished binding suggestion that breaks things.
	// Since this method is rarely run the overhead should be fine.
	for (int i = 0; i < suggestedBindings->countSuggestedBindings; ++i) {
		auto pAct = (Action*)pSuggestions[i].action;
		auto pActSet = BLOCK_PTR(block.actionSet, pAct->hActionSet);
		HANDLE_CHECK(pActSet->attachedToSession, XR_ERROR_ACTIONSETS_ALREADY_ATTACHED);
		// Might want to check if there is space too?
	}

	int actsBound = 0;
	for (int i = 0; i < suggestedBindings->countSuggestedBindings; ++i) {
		auto        pAct = (Action*)pSuggestions[i].action;
		auto        pBindPath = (Path*)pSuggestions[i].binding;

		auto bindPathHash = BLOCK_KEY(block.path, pBindPath);
		auto hMapBind = MAP_FIND(pProfile->bindings, bindPathHash);
		if (!HANDLE_VALID(hMapBind)) {
			printf("Could not find suggested Binding: %s on InteractionProfile: %s\n", pBindPath->string, pProfilePath->string);
			continue;
		}

		auto hBind = pProfile->bindings.handles[hMapBind];
		auto pBind = BLOCK_PTR(block.binding, hBind);

		if (pAct->countSubactions == 0) {

			pAct->hSubactionBindings[0] = hBind;

			printf("Bound Action: %s BindingPath: %s No Subaction\n", pAct->actionName, pBindPath->string);
			actsBound++;

			continue;
		}

		for (int subactionIndex = 0; subactionIndex < pAct->countSubactions; ++subactionIndex) {

			auto pSubactPath = BLOCK_PTR(block.path, pAct->hSubactionPaths[subactionIndex]);
			if (!CompareSubPath(pSubactPath->string, pBindPath->string))
				continue;

			pAct->hSubactionBindings[subactionIndex] = hBind;

			printf("Bound Action: %s BindingPath: %s Subaction Index: %d %s\n", pAct->actionName, pBindPath->string, subactionIndex, pSubactPath->string);
			actsBound++;
		}
	}

	if (actsBound == 0) {
		LOG_ERROR("XR_ERROR_PATH_UNSUPPORTED\n");
		return XR_ERROR_PATH_UNSUPPORTED;
	}

	return XR_SUCCESS;
}

XR_PROC xrAttachSessionActionSets(
	XrSession                            session,
	const XrSessionActionSetsAttachInfo* attachInfo)
{
	LOG_METHOD(xrAttachSessionActionSets);
	LogNextChain(attachInfo->next);
	assert(attachInfo->next == NULL);

	auto pSess = (Session*)session;
	auto hSess = BLOCK_HANDLE(block.session, pSess);

	for (int i = 0; i < attachInfo->countActionSets; ++i) {
		auto pActSet = (ActionSet*)attachInfo->actionSets[i];
		if (HANDLE_VALID(pActSet->attachedToSession)) {
			LOG_ERROR("XR_ERROR_ACTIONSETS_ALREADY_ATTACHED %s\n", pActSet->actionSetName);
			return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
		}
	}

	for (int i = 0; i < attachInfo->countActionSets; ++i) {
		auto pActSet = (ActionSet*)attachInfo->actionSets[i];
		auto hActSet = BLOCK_HANDLE(block.actionSet, pActSet);
		HANDLE_CHECK(hActSet, XR_ERROR_HANDLE_INVALID);

		auto actSetHash = BLOCK_KEY(block.actionSet, pActSet);
		MAP_ADD(pSess->actionSets, hActSet, actSetHash);

		memset(&pActSet->states, 0, sizeof(pActSet->states));

		pActSet->attachedToSession = hSess;
		printf("Attached ActionSet %s\n", pActSet->actionSetName);
	}

	if (!HANDLE_VALID(pSess->hPendingInteractionProfile)) {
		printf("Setting default interaction profile: %s\n", XR_DEFAULT_INTERACTION_PROFILE);

		XrPath profilePath;
		xrStringToPath((XrInstance)&xr.instance, XR_DEFAULT_INTERACTION_PROFILE, &profilePath);
		auto profileHash = BLOCK_KEY(block.path, (Path*)profilePath);
		auto hProfile = BLOCK_FIND(block.profile, profileHash);
		HANDLE_CHECK(hProfile, XR_ERROR_HANDLE_INVALID);

		pSess->hPendingInteractionProfile = hProfile;
	}

	return XR_SUCCESS;
}

XR_PROC xrGetCurrentInteractionProfile(
	XrSession                  session,
	XrPath                     topLevelUserPath,
	XrInteractionProfileState* interactionProfile)
{
	auto pPath = (Path*)topLevelUserPath;
	LOG("xrGetCurrentInteractionProfile %s\n", pPath->string);
	assert(interactionProfile->next == NULL);

	// application might set interaction profile then immediately call things without letting xrPollEvent update activeInteractionProfile
	auto pSess = (Session*)session;
	if (HANDLE_VALID(pSess->hActiveInteractionProfile))
		interactionProfile->interactionProfile = (XrPath)BLOCK_PTR(block.profile, pSess->hActiveInteractionProfile);
	else if (HANDLE_VALID(pSess->hPendingInteractionProfile))
		interactionProfile->interactionProfile = (XrPath)BLOCK_PTR(block.profile, pSess->hPendingInteractionProfile);
	else
		interactionProfile->interactionProfile = XR_NULL_HANDLE;

	return XR_SUCCESS;
}

XR_PROC xrGetActionStateBoolean(
	XrSession                   session,
	const XrActionStateGetInfo* getInfo,
	XrActionStateBoolean*       state)
{
	LOG_METHOD_ONCE(xrGetActionStateBoolean);
	LogNextChain(getInfo->next);

	auto pGetAct = (Action*)getInfo->action;
	auto pGetActSubPath = (Path*)getInfo->subactionPath;
	auto hGetActSet = pGetAct->hActionSet;
	auto pGetActSet = BLOCK_PTR(block.actionSet, hGetActSet);

	//	LOG_METHOD_LOOP("Action: %s\n", pGetAction->actionName);

	//	state->lastChangeTime = GetXrTime();
	//	state->changedSinceLastSync = true;
	//	state->currentState = true;
	//	state->isActive = true;

//	LOG_ERROR("XR_ERROR_PATH_UNSUPPORTED\n");
	return XR_SUCCESS;
}

XR_PROC xrGetActionStateFloat(
	XrSession                   session,
	const XrActionStateGetInfo* getInfo,
	XrActionStateFloat*         state)
{
	LOG_METHOD_ONCE(xrGetActionStateFloat);
	LogNextChain(getInfo->next);

	auto pSess = (Session*)session;
	auto pAct = (Action*)getInfo->action;
	auto pSubactPath = (Path*)getInfo->subactionPath;
	auto hSubactPath = BLOCK_HANDLE(block.path, pSubactPath);

	auto pActSet = BLOCK_PTR(block.actionSet, pAct->hActionSet);
	auto actSetHash = BLOCK_KEY(block.actionSet, pActSet);

	if (pSubactPath == NULL) {
		auto hState = pAct->hSubactionPaths[0];
		auto pState = BLOCK_PTR(block.state, hState);

		memcpy(&state->currentState, &pState->currentState, sizeof(SubactionState));
		pState->changedSinceLastSync = false;

		return XR_SUCCESS;
	}

	for (int i = 0; i < pAct->countSubactions; ++i) {
		if (hSubactPath == pAct->hSubactionPaths[i]) {
			auto hState = pAct->hSubactionPaths[i];
			auto pState = BLOCK_PTR(block.state, hState);

			memcpy(&state->currentState, &pState->currentState, sizeof(SubactionState));
			pState->changedSinceLastSync = false;

			return XR_SUCCESS;
		}
	}

	LOG_ERROR("XR_ERROR_PATH_UNSUPPORTED\n");
	return XR_ERROR_PATH_UNSUPPORTED;
}

XR_PROC xrGetActionStateVector2f(
	XrSession                   session,
	const XrActionStateGetInfo* getInfo,
	XrActionStateVector2f*      state)
{
	LOG_METHOD(xrGetActionStateVector2f);
	LOG_ERROR("XR_ERROR_PATH_UNSUPPORTED\n");
	return XR_ERROR_PATH_UNSUPPORTED;
}

XR_PROC xrGetActionStatePose(
	XrSession                   session,
	const XrActionStateGetInfo* getInfo,
	XrActionStatePose*          state)
{
	LOG_METHOD(xrGetActionStatePose);
	LOG_ERROR("XR_ERROR_PATH_UNSUPPORTED\n");
	return XR_ERROR_PATH_UNSUPPORTED;
}

XR_PROC xrSyncActions(
	XrSession                session,
	const XrActionsSyncInfo* syncInfo)
{
	LOG_METHOD_ONCE(xrSyncActions);
	LogNextChain(syncInfo->next);

	auto pSess = (Session*)session;
	if (pSess->activeSessionState != XR_SESSION_STATE_FOCUSED) {
		LOG("XR_SESSION_NOT_FOCUSED\n");
		return XR_SESSION_NOT_FOCUSED;
	}

	auto time = xrGetTime();
	for (int si = 0; si < syncInfo->countActiveActionSets; ++si) {

		auto pActSet = (ActionSet*)syncInfo->activeActionSets[si].actionSet;
		auto actSetHash = BLOCK_KEY(block.actionSet, pActSet);

		for (int ai = 0; ai < pActSet->actions.count; ++ai) {
			auto pAct = BLOCK_PTR(block.action, pActSet->actions.handles[ai]);

			for (int sai = 0; sai < pAct->countSubactions; ++sai) {
				auto pState = BLOCK_PTR(block.state, pAct->hSubactionStates[sai]);
				auto pBind = BLOCK_PTR(block.binding, pAct->hSubactionBindings[sai]);

				// need to understand this priority again
//				if (pState->lastSyncedPriority > pActSet->priority &&
//					pState->lastChangeTime == currentTime)
//					continue;

				float priorState = pState->currentState;
				pBind->func(&pState->currentState);
//				pState->lastSyncedPriority = pActSet->priority;
				pState->lastChangeTime = time;
				pState->isActive = XR_TRUE;
				pState->changedSinceLastSync = pState->currentState != priorState;
			}
		}
	}

	return XR_SUCCESS;
}

XR_PROC xrEnumerateBoundSourcesForAction(
	XrSession                                   session,
	const XrBoundSourcesForActionEnumerateInfo* enumerateInfo,
	uint32_t                                    sourceCapacityInput,
	uint32_t*                                   sourceCountOutput,
	XrPath*                                     sources)
{
	LOG_METHOD(xrEnumerateBoundSourcesForAction);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrEnumerateBoundSourcesForAction\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrGetInputSourceLocalizedName(
	XrSession                                session,
	const XrInputSourceLocalizedNameGetInfo* getInfo,
	uint32_t                                 bufferCapacityInput,
	uint32_t*                                bufferCountOutput,
	char*                                    buffer)
{
	LOG_METHOD(xrGetInputSourceLocalizedName);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrGetInputSourceLocalizedName\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrApplyHapticFeedback(
	XrSession                 session,
	const XrHapticActionInfo* hapticActionInfo,
	const XrHapticBaseHeader* hapticFeedback)
{
	LOG_METHOD(xrApplyHapticFeedback);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrApplyHapticFeedback\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrStopHapticFeedback(
	XrSession                 session,
	const XrHapticActionInfo* hapticActionInfo)
{
	LOG_METHOD(xrStopHapticFeedback);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrStopHapticFeedback\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrGetVisibilityMaskKHR(
	XrSession               session,
	XrViewConfigurationType viewConfigurationType,
	uint32_t                viewIndex,
	XrVisibilityMaskTypeKHR visibilityMaskType,
	XrVisibilityMaskKHR*    visibilityMask)
{
	LOG_METHOD(xrGetVisibilityMaskKHR);
	printf("%s %d %s\n",
		   string_XrViewConfigurationType(viewConfigurationType),
		   viewIndex,
		   string_XrVisibilityMaskTypeKHR(visibilityMaskType));

	// this is such an easy way to look stuff up, should I really change it to handles?
	Session* pSession = (Session*)session;

	switch (viewConfigurationType) {
		case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:
		case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
		case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET:
		case XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT:
		default:
			break;
	}

	float      extent = 1.0f;
	XrVector2f innerVertices[] = {
		{-extent, -extent},  // 0
		{extent, -extent},   // 1
		{extent, extent},    // 2
		{-extent, extent},   // 3
	};
	constexpr XrVector2f outerVertices[] = {
		{-1, -1},  // 4
		{1, -1},   // 5
		{1, 1},    // 6
		{-1, 1},   // 7
	};
	//	7			6
	//		3	2
	//
	//		0	1
	//	4			5

#define CHECK_CAPACITY                                                              \
	if (visibilityMask->vertexCapacityInput == 0 ||                                 \
		visibilityMask->vertices == nullptr ||                                      \
		visibilityMask->indexCapacityInput == 0 ||                                  \
		visibilityMask->indices == nullptr) {                                       \
		return XR_SUCCESS;                                                          \
	}                                                                               \
	if (visibilityMask->vertexCapacityInput != visibilityMask->vertexCountOutput || \
		visibilityMask->indexCapacityInput != visibilityMask->indexCountOutput) {   \
		LOG_ERROR("XR_ERROR_SIZE_INSUFFICIENT\n");                                  \
		return XR_ERROR_SIZE_INSUFFICIENT;                                          \
	}

	switch (visibilityMaskType) {
		case XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR:
		case XR_VISIBILITY_MASK_TYPE_VISIBLE_TRIANGLE_MESH_KHR: {

			uint32_t indices[] = {
				0, 7, 4,
				0, 3, 7,
				1, 4, 5,
				1, 0, 4,
				2, 5, 6,
				2, 1, 5,
				3, 6, 7,
				3, 2, 6,
			};
			static_assert(COUNT(indices) == (COUNT(innerVertices) + COUNT(outerVertices))* 3);
			visibilityMask->vertexCountOutput = COUNT(innerVertices) + COUNT(outerVertices);
			visibilityMask->indexCountOutput = COUNT(indices);

			CHECK_CAPACITY

			memcpy(visibilityMask->vertices, innerVertices, sizeof(innerVertices));
			memcpy(visibilityMask->vertices + COUNT(innerVertices), outerVertices, sizeof(innerVertices));
			memcpy(visibilityMask->indices, indices, sizeof(indices));

			return XR_SUCCESS;
		}
		case XR_VISIBILITY_MASK_TYPE_LINE_LOOP_KHR: {

			uint32_t indices[] = {0, 1, 2, 3};
			static_assert(COUNT(indices) == COUNT(innerVertices));
			visibilityMask->vertexCountOutput = COUNT(innerVertices);
			visibilityMask->indexCountOutput = COUNT(indices);

			CHECK_CAPACITY

			memcpy(visibilityMask->vertices, innerVertices, sizeof(innerVertices));
			memcpy(visibilityMask->indices, indices, sizeof(indices));

			return XR_SUCCESS;
		}
		default:
			LOG_ERROR("XR_ERROR_VALIDATION_FAILURE\n");
			return XR_ERROR_VALIDATION_FAILURE;
	}

#undef CHECK_CAPACITY
}

XR_PROC xrSetInputDeviceActiveEXT(
	XrSession session,
	XrPath    interactionProfile,
	XrPath    topLevelPath,
	XrBool32  isActive)
{
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSetInputDeviceActiveEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrSetInputDeviceStateBoolEXT(
	XrSession session,
	XrPath    topLevelPath,
	XrPath    inputSourcePath,
	XrBool32  state)
{
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSetInputDeviceActiveEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrSetInputDeviceStateFloatEXT(
	XrSession session,
	XrPath    topLevelPath,
	XrPath    inputSourcePath,
	float     state)
{
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSetInputDeviceStateFloatEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrSetInputDeviceStateVector2fEXT(
	XrSession  session,
	XrPath     topLevelPath,
	XrPath     inputSourcePath,
	XrVector2f state)
{
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSetInputDeviceStateVector2fEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrSetInputDeviceLocationEXT(
	XrSession session,
	XrPath    topLevelPath,
	XrPath    inputSourcePath,
	XrSpace   space,
	XrPosef   pose)
{
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSetInputDeviceLocationEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrEnumeratePerformanceMetricsCounterPathsMETA(
	XrInstance instance,
	uint32_t   counterPathCapacityInput,
	uint32_t*  counterPathCountOutput,
	XrPath*    counterPaths)
{
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrEnumeratePerformanceMetricsCounterPathsMETA\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrSetPerformanceMetricsStateMETA(
	XrSession                            session,
	const XrPerformanceMetricsStateMETA* state)
{
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSetPerformanceMetricsStateMETA\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrGetPerformanceMetricsStateMETA(
	XrSession                      session,
	XrPerformanceMetricsStateMETA* state)
{
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrGetPerformanceMetricsStateMETA\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrQueryPerformanceMetricsCounterMETA(
	XrSession                        session,
	XrPath                           counterPath,
	XrPerformanceMetricsCounterMETA* counter)
{
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrQueryPerformanceMetricsCounterMETA\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrGetRecommendedLayerResolutionMETA(
	XrSession                                   session,
	const XrRecommendedLayerResolutionGetInfoMETA* info,
	XrRecommendedLayerResolutionMETA*           resolution) {
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrGetRecommendedLayerResolutionMETA\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrPerfSettingsSetPerformanceLevelEXT(
	XrSession               session,
	XrPerfSettingsDomainEXT domain,
	XrPerfSettingsLevelEXT  level)
{
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrPerfSettingsSetPerformanceLevelEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrSetDebugUtilsObjectNameEXT(
	XrInstance                           instance,
	const XrDebugUtilsObjectNameInfoEXT* nameInfo)
{
	LOG_METHOD(xrSetDebugUtilsObjectNameEXT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSetDebugUtilsObjectNameEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrCreateDebugUtilsMessengerEXT(
	XrInstance                                instance,
	const XrDebugUtilsMessengerCreateInfoEXT* createInfo,
	XrDebugUtilsMessengerEXT*                 messenger)
{
	LOG_METHOD(xrCreateDebugUtilsMessengerEXT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrCreateDebugUtilsMessengerEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrDestroyDebugUtilsMessengerEXT(
	XrDebugUtilsMessengerEXT messenger)
{
	LOG_METHOD(xrDestroyDebugUtilsMessengerEXT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrDestroyDebugUtilsMessengerEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrSubmitDebugUtilsMessageEXT(
	XrInstance                                  instance,
	XrDebugUtilsMessageSeverityFlagsEXT         messageSeverity,
	XrDebugUtilsMessageTypeFlagsEXT             messageTypes,
	const XrDebugUtilsMessengerCallbackDataEXT* callbackData)
{
	LOG_METHOD(xrSubmitDebugUtilsMessageEXT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSubmitDebugUtilsMessageEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrSessionBeginDebugUtilsLabelRegionEXT(
	XrSession                   session,
	const XrDebugUtilsLabelEXT* labelInfo)
{
	LOG_METHOD(xrSessionBeginDebugUtilsLabelRegionEXT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSessionBeginDebugUtilsLabelRegionEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrSessionEndDebugUtilsLabelRegionEXT(
	XrSession session)
{
	LOG_METHOD(xrSessionEndDebugUtilsLabelRegionEXT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSessionEndDebugUtilsLabelRegionEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrSessionInsertDebugUtilsLabelEXT(
	XrSession                   session,
	const XrDebugUtilsLabelEXT* labelInfo)
{
	LOG_METHOD(xrSessionInsertDebugUtilsLabelEXT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSessionInsertDebugUtilsLabelEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrGetOpenGLGraphicsRequirementsKHR(
	XrInstance                       instance,
	XrSystemId                       systemId,
	XrGraphicsRequirementsOpenGLKHR* graphicsRequirements)
{
	LOG_METHOD(xrGetOpenGLGraphicsRequirementsKHR);
	LogNextChain(graphicsRequirements->next);

	const XrVersion openglApiVersion = XR_MAKE_VERSION(XR_OPENGL_MAJOR_VERSION, XR_OPENGL_MINOR_VERSION, 0);
	*graphicsRequirements = (XrGraphicsRequirementsOpenGLKHR){
		.minApiVersionSupported = openglApiVersion,
		.maxApiVersionSupported = openglApiVersion,
	};
	return XR_SUCCESS;
}

XR_PROC xrGetD3D11GraphicsRequirementsKHR(
	XrInstance                      instance,
	XrSystemId                      systemId,
	XrGraphicsRequirementsD3D11KHR* graphicsRequirements)
{
	LOG_METHOD(xrGetD3D11GraphicsRequirementsKHR);
	LogNextChain(graphicsRequirements->next);
	CHECK_INSTANCE(instance);

	if (xr.instance.systemId != systemId) {
		LOG_ERROR("Invalid System ID.");
		return XR_ERROR_SYSTEM_INVALID;
	}

	if (xr.instance.graphics.d3d11.adapterLuid.LowPart != 0) {
		graphicsRequirements->adapterLuid = xr.instance.graphics.d3d11.adapterLuid;
		graphicsRequirements->minFeatureLevel = xr.instance.graphics.d3d11.minFeatureLevel;
		printf("Already found D3D11GraphicsRequirements LUID: %ld:%lu FeatureLevel: %d\n",
			   graphicsRequirements->adapterLuid.HighPart,
			   graphicsRequirements->adapterLuid.LowPart,
			   graphicsRequirements->minFeatureLevel);
		return XR_SUCCESS;
	}

	IDXGIFactory1* factory1 = NULL;
	DX_CHECK(CreateDXGIFactory1(&IID_IDXGIFactory, (void**)&factory1));

	// We aren't iterating, and just going with the first, but we might want to iterate somehow at some point
	// but the proper adapter seems to always be the first
	IDXGIAdapter* adapter = NULL;
	for (UINT i = 0; IDXGIFactory1_EnumAdapters(factory1, i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC desc;
		DX_CHECK(IDXGIAdapter_GetDesc(adapter, &desc));
		wprintf(L"DX11 Adapter %d Name: %ls LUID: %ld:%lu\n",
				i, desc.Description, desc.AdapterLuid.HighPart, desc.AdapterLuid.LowPart);

		// Check device
		ID3D11Device*        device;
		ID3D11DeviceContext* context;
		D3D_FEATURE_LEVEL    featureLevel;
		DX_CHECK(D3D11CreateDevice(
			adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, D3D11_CREATE_DEVICE_DEBUG,
			(D3D_FEATURE_LEVEL[]){D3D_FEATURE_LEVEL_11_1}, 1, D3D11_SDK_VERSION,
			&device, &featureLevel, &context));

		printf("XR D3D11 Device: %p %d\n", device, featureLevel);
		if (device == NULL || featureLevel < D3D_FEATURE_LEVEL_11_1) {
			LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED XR D3D11 Device Invalid\n");
			ID3D11Device_Release(device);
			ID3D11DeviceContext_Release(context);
			return XR_ERROR_GRAPHICS_DEVICE_INVALID;
		}

		graphicsRequirements->adapterLuid = desc.AdapterLuid;
		graphicsRequirements->minFeatureLevel = featureLevel;

		ID3D11Device_Release(device);
		ID3D11DeviceContext_Release(context);
		break;
	}

	IDXGIAdapter_Release(adapter);
	IDXGIFactory1_Release(factory1);

	printf("Found D3D11GraphicsRequirements LUID: %ld:%lu FeatureLevel: %d\n",
		   graphicsRequirements->adapterLuid.HighPart,
		   graphicsRequirements->adapterLuid.LowPart,
		   graphicsRequirements->minFeatureLevel);

	xr.instance.graphics.d3d11.adapterLuid = graphicsRequirements->adapterLuid;
	xr.instance.graphics.d3d11.minFeatureLevel = graphicsRequirements->minFeatureLevel;

	return XR_SUCCESS;
}

XR_PROC xrCreateHandTrackerEXT(
	XrSession                         session,
	const XrHandTrackerCreateInfoEXT* createInfo,
	XrHandTrackerEXT*                 handTracker)
{
	LOG_METHOD(xrCreateHandTrackerEXT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrCreateHandTrackerEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrDestroyHandTrackerEXT(
	XrHandTrackerEXT handTracker)
{
	LOG_METHOD(xrDestroyHandTrackerEXT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrDestroyHandTrackerEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrLocateHandJointsEXT(
	XrHandTrackerEXT                 handTracker,
	const XrHandJointsLocateInfoEXT* locateInfo,
	XrHandJointLocationsEXT*         locations)
{
	LOG_METHOD(xrLocateHandJointsEXT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrLocateHandJointsEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrCreateSpatialAnchorMSFT(
	XrSession                            session,
	const XrSpatialAnchorCreateInfoMSFT* createInfo,
	XrSpatialAnchorMSFT*                 anchor)
{
	LOG_METHOD(xrCreateSpatialAnchorMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrCreateSpatialAnchorMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrCreateSpatialAnchorSpaceMSFT(
	XrSession                                 session,
	const XrSpatialAnchorSpaceCreateInfoMSFT* createInfo,
	XrSpace*                                  space)
{
	LOG_METHOD(xrCreateSpatialAnchorSpaceMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrCreateSpatialAnchorSpaceMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrDestroySpatialAnchorMSFT(
	XrSpatialAnchorMSFT anchor)
{
	LOG_METHOD(xrDestroySpatialAnchorMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrDestroySpatialAnchorMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrEnumerateSceneComputeFeaturesMSFT(
	XrInstance                 instance,
	XrSystemId                 systemId,
	uint32_t                   featureCapacityInput,
	uint32_t*                  featureCountOutput,
	XrSceneComputeFeatureMSFT* features)
{
	LOG_METHOD(xrEnumerateSceneComputeFeaturesMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrEnumerateSceneComputeFeaturesMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrCreateSceneObserverMSFT(
	XrSession                            session,
	const XrSceneObserverCreateInfoMSFT* createInfo,
	XrSceneObserverMSFT*                 sceneObserver)
{
	LOG_METHOD(xrCreateSceneObserverMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrCreateSceneObserverMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrDestroySceneObserverMSFT(
	XrSceneObserverMSFT sceneObserver)
{
	LOG_METHOD(xrDestroySceneObserverMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrDestroySceneObserverMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrCreateSceneMSFT(
	XrSceneObserverMSFT          sceneObserver,
	const XrSceneCreateInfoMSFT* createInfo,
	XrSceneMSFT*                 scene)
{
	LOG_METHOD(xrCreateSceneMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrCreateSceneMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrDestroySceneMSFT(
	XrSceneMSFT scene)
{
	LOG_METHOD(xrDestroySceneMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrDestroySceneMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrComputeNewSceneMSFT(
	XrSceneObserverMSFT              sceneObserver,
	const XrNewSceneComputeInfoMSFT* computeInfo)
{
	LOG_METHOD(xrComputeNewSceneMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrComputeNewSceneMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrGetSceneComputeStateMSFT(
	XrSceneObserverMSFT      sceneObserver,
	XrSceneComputeStateMSFT* state)
{
	LOG_METHOD(xrGetSceneComputeStateMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrGetSceneComputeStateMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrGetSceneComponentsMSFT(
	XrSceneMSFT                         scene,
	const XrSceneComponentsGetInfoMSFT* getInfo,
	XrSceneComponentsMSFT*              components)
{
	LOG_METHOD(xrGetSceneComponentsMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrGetSceneComponentsMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrLocateSceneComponentsMSFT(
	XrSceneMSFT                            scene,
	const XrSceneComponentsLocateInfoMSFT* locateInfo,
	XrSceneComponentLocationsMSFT*         locations)
{
	LOG_METHOD(xrLocateSceneComponentsMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrLocateSceneComponentsMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrGetSceneMeshBuffersMSFT(
	XrSceneMSFT                          scene,
	const XrSceneMeshBuffersGetInfoMSFT* getInfo,
	XrSceneMeshBuffersMSFT*              buffers)
{
	LOG_METHOD(xrGetSceneMeshBuffersMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrGetSceneMeshBuffersMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrConvertWin32PerformanceCounterToTimeKHR(
	XrInstance           instance,
	const LARGE_INTEGER* performanceCounter,
	XrTime*              time)
{
	LOG_METHOD(xrConvertWin32PerformanceCounterToTimeKHR);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrConvertWin32PerformanceCounterToTimeKHR\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC xrGetInstanceProcAddr(
	XrInstance          instance,
	const char*         name,
	PFN_xrVoidFunction* function)
{
#define CHECK_PROC_ADDR(_method)                                    \
	if (strncmp(name, #_method, XR_MAX_STRUCTURE_NAME_SIZE) == 0) { \
		*function = (PFN_xrVoidFunction)_method;                    \
		return XR_SUCCESS;                                        \
	}

	// openxr standard procs
	CHECK_PROC_ADDR(xrGetInstanceProcAddr)
	CHECK_PROC_ADDR(xrEnumerateApiLayerProperties)
	CHECK_PROC_ADDR(xrEnumerateInstanceExtensionProperties)
	CHECK_PROC_ADDR(xrCreateInstance)
	CHECK_PROC_ADDR(xrDestroyInstance)
	CHECK_PROC_ADDR(xrGetInstanceProperties)
	CHECK_PROC_ADDR(xrPollEvent)
	CHECK_PROC_ADDR(xrResultToString)
	CHECK_PROC_ADDR(xrStructureTypeToString)
	CHECK_PROC_ADDR(xrGetSystem)
	CHECK_PROC_ADDR(xrGetSystemProperties)
	CHECK_PROC_ADDR(xrEnumerateEnvironmentBlendModes)
	CHECK_PROC_ADDR(xrCreateSession)
	CHECK_PROC_ADDR(xrDestroySession)
	CHECK_PROC_ADDR(xrEnumerateReferenceSpaces)
	CHECK_PROC_ADDR(xrCreateReferenceSpace)
	CHECK_PROC_ADDR(xrGetReferenceSpaceBoundsRect)
	CHECK_PROC_ADDR(xrCreateActionSpace)
	CHECK_PROC_ADDR(xrLocateSpace)
	CHECK_PROC_ADDR(xrDestroySpace)
	CHECK_PROC_ADDR(xrEnumerateViewConfigurations)
	CHECK_PROC_ADDR(xrGetViewConfigurationProperties)
	CHECK_PROC_ADDR(xrEnumerateViewConfigurationViews)
	CHECK_PROC_ADDR(xrEnumerateSwapchainFormats)
	CHECK_PROC_ADDR(xrCreateSwapchain)
	CHECK_PROC_ADDR(xrDestroySwapchain)
	CHECK_PROC_ADDR(xrEnumerateSwapchainImages)
	CHECK_PROC_ADDR(xrAcquireSwapchainImage)
	CHECK_PROC_ADDR(xrWaitSwapchainImage)
	CHECK_PROC_ADDR(xrReleaseSwapchainImage)
	CHECK_PROC_ADDR(xrBeginSession)
	CHECK_PROC_ADDR(xrEndSession)
	CHECK_PROC_ADDR(xrRequestExitSession)
	CHECK_PROC_ADDR(xrWaitFrame)
	CHECK_PROC_ADDR(xrBeginFrame)
	CHECK_PROC_ADDR(xrEndFrame)
	CHECK_PROC_ADDR(xrLocateViews)
	CHECK_PROC_ADDR(xrStringToPath)
	CHECK_PROC_ADDR(xrPathToString)
	CHECK_PROC_ADDR(xrCreateActionSet)
	CHECK_PROC_ADDR(xrDestroyActionSet)
	CHECK_PROC_ADDR(xrCreateAction)
	CHECK_PROC_ADDR(xrDestroyAction)
	CHECK_PROC_ADDR(xrSuggestInteractionProfileBindings)
	CHECK_PROC_ADDR(xrAttachSessionActionSets)
	CHECK_PROC_ADDR(xrGetCurrentInteractionProfile)
	CHECK_PROC_ADDR(xrGetActionStateBoolean)
	CHECK_PROC_ADDR(xrGetActionStateFloat)
	CHECK_PROC_ADDR(xrGetActionStateVector2f)
	CHECK_PROC_ADDR(xrGetActionStatePose)
	CHECK_PROC_ADDR(xrSyncActions)
	CHECK_PROC_ADDR(xrEnumerateBoundSourcesForAction)
	CHECK_PROC_ADDR(xrGetInputSourceLocalizedName)
	CHECK_PROC_ADDR(xrApplyHapticFeedback)
	CHECK_PROC_ADDR(xrStopHapticFeedback)

	// openxr extension procs
	CHECK_PROC_ADDR(xrGetVisibilityMaskKHR)

	CHECK_PROC_ADDR(xrSetInputDeviceActiveEXT)
	CHECK_PROC_ADDR(xrSetInputDeviceStateBoolEXT)
	CHECK_PROC_ADDR(xrSetInputDeviceStateFloatEXT)
	CHECK_PROC_ADDR(xrSetInputDeviceStateVector2fEXT)
	CHECK_PROC_ADDR(xrSetInputDeviceLocationEXT)

	CHECK_PROC_ADDR(xrEnumeratePerformanceMetricsCounterPathsMETA)
	CHECK_PROC_ADDR(xrSetPerformanceMetricsStateMETA)
	CHECK_PROC_ADDR(xrGetPerformanceMetricsStateMETA)
	CHECK_PROC_ADDR(xrQueryPerformanceMetricsCounterMETA)

	CHECK_PROC_ADDR(xrGetRecommendedLayerResolutionMETA)

	CHECK_PROC_ADDR(xrPerfSettingsSetPerformanceLevelEXT)


//	CHECK_PROC_ADDR(xrSetDebugUtilsObjectNameEXT)
//	CHECK_PROC_ADDR(xrCreateDebugUtilsMessengerEXT)
//	CHECK_PROC_ADDR(xrDestroyDebugUtilsMessengerEXT)
//	CHECK_PROC_ADDR(xrSubmitDebugUtilsMessageEXT)
//	CHECK_PROC_ADDR(xrSessionBeginDebugUtilsLabelRegionEXT)
//	CHECK_PROC_ADDR(xrSessionEndDebugUtilsLabelRegionEXT)
//	CHECK_PROC_ADDR(xrSessionInsertDebugUtilsLabelEXT)

	CHECK_PROC_ADDR(xrCreateHandTrackerEXT)
	CHECK_PROC_ADDR(xrDestroyHandTrackerEXT)
	CHECK_PROC_ADDR(xrLocateHandJointsEXT)
	CHECK_PROC_ADDR(xrCreateSpatialAnchorMSFT)
	CHECK_PROC_ADDR(xrCreateSpatialAnchorSpaceMSFT)
	CHECK_PROC_ADDR(xrDestroySpatialAnchorMSFT)
	CHECK_PROC_ADDR(xrEnumerateSceneComputeFeaturesMSFT)
	CHECK_PROC_ADDR(xrCreateSceneObserverMSFT)
	CHECK_PROC_ADDR(xrDestroySceneObserverMSFT)
	CHECK_PROC_ADDR(xrCreateSceneMSFT)
	CHECK_PROC_ADDR(xrDestroySceneMSFT)
	CHECK_PROC_ADDR(xrComputeNewSceneMSFT)
	CHECK_PROC_ADDR(xrGetSceneComputeStateMSFT)
	CHECK_PROC_ADDR(xrGetSceneComponentsMSFT)
	CHECK_PROC_ADDR(xrLocateSceneComponentsMSFT)
	CHECK_PROC_ADDR(xrGetSceneMeshBuffersMSFT)

	CHECK_PROC_ADDR(xrGetOpenGLGraphicsRequirementsKHR)
	CHECK_PROC_ADDR(xrGetD3D11GraphicsRequirementsKHR)
	CHECK_PROC_ADDR(xrConvertWin32PerformanceCounterToTimeKHR)

#undef CHECK_PROC_ADDR

	LOG_ERROR("xrGetInstanceProcAddr XR_ERROR_FUNCTION_UNSUPPORTED %s\n", name);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XR_PROC EXPORT xrNegotiateLoaderRuntimeInterface(
	const XrNegotiateLoaderInfo* loaderInfo,
	XrNegotiateRuntimeRequest*   runtimeRequest)
{
	LOG_METHOD(xrNegotiateLoaderRuntimeInterface);

	if (!loaderInfo ||
		!runtimeRequest ||
		loaderInfo->structType != XR_LOADER_INTERFACE_STRUCT_LOADER_INFO ||
		loaderInfo->structVersion != XR_LOADER_INFO_STRUCT_VERSION ||
		loaderInfo->structSize != sizeof(XrNegotiateLoaderInfo) ||
		loaderInfo->minInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION ||
		loaderInfo->maxInterfaceVersion < XR_CURRENT_LOADER_API_LAYER_VERSION ||
		loaderInfo->maxInterfaceVersion > XR_CURRENT_LOADER_API_LAYER_VERSION ||
		loaderInfo->maxApiVersion < XR_CURRENT_API_VERSION ||
		loaderInfo->minApiVersion > XR_API_VERSION_1_0 ||
		runtimeRequest->structType != XR_LOADER_INTERFACE_STRUCT_RUNTIME_REQUEST ||
		runtimeRequest->structVersion != XR_RUNTIME_INFO_STRUCT_VERSION ||
		runtimeRequest->structSize != sizeof(XrNegotiateRuntimeRequest)) {
		LOG_ERROR("xrNegotiateLoaderRuntimeInterface fail\n");
		return XR_ERROR_INITIALIZATION_FAILED;
	}

	runtimeRequest->getInstanceProcAddr = xrGetInstanceProcAddr;
	runtimeRequest->runtimeInterfaceVersion = XR_CURRENT_LOADER_API_LAYER_VERSION;
	runtimeRequest->runtimeApiVersion = XR_CURRENT_API_VERSION;

	return XR_SUCCESS;
}

#endif  //MID_OPENXR_IMPLEMENTATION