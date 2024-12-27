#pragma once

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#define NOSERVICE
#define NOCRYPT
#define NOMCX
#include <windows.h>
#include <windows.h>
#include <synchapi.h>

#include <vulkan/vulkan.h>

#define D3D11_NO_HELPERS
#define CINTERFACE
#define COBJMACROS
#define WIDL_C_INLINE_WRAPPERS
#include <d3d11_1.h>
#include <initguid.h>
#include <dxgi.h>
#include <dxgi1_4.h>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_VULKAN
#define XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_reflection.h>
#include <openxr/openxr_loader_negotiation.h>
#include <d3d11_3.h>
#include <d3d11_4.h>

#ifdef MID_IDE_ANALYSIS
#undef XRAPI_CALL
#define XRAPI_CALL
#endif

//
//// Mid Common Utility
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

//
//// Mid OpenXR
typedef uint32_t XrHash;
typedef uint32_t XrHandle;
#define XR_HANDLE_INVALID -1

typedef enum XrGraphicsApi {
	XR_GRAPHICS_API_OPENGL,
	XR_GRAPHICS_API_OPENGL_ES,
	XR_GRAPHICS_API_VULKAN,
	XR_GRAPHICS_API_D3D11_4,
	XR_GRAPHICS_API_COUNT,
} XrGraphicsApi;

void midXrInitialize();
void xrClaimSession(XrHandle* pSessionHandle);
void xrReleaseSession(XrHandle sessionHandle);
void midXrGetReferenceSpaceBounds(XrHandle sessionHandle, XrExtent2Df* pBounds);
void midXrClaimFramebufferImages(XrHandle sessionHandle, int imageCount, HANDLE* pHandle);

void xrGetSessionTimeline(uint32_t sessionHandle, HANDLE* pHandle);
void xrSetSessionTimelineValue(XrHandle sessionHandle, uint64_t timelineValue);

void xrClaimSwapPoolImage(XrHandle sessionHandle, uint8_t* pIndex);
void xrReleaseSwapPoolImage(XrHandle sessionHandle, uint8_t index);

void xrGetCompositorTimeline(XrHandle sessionHandle, HANDLE* pHandle);
void xrSetInitialCompositorTimelineValue(XrHandle sessionHandle, uint64_t timelineValue);
void xrGetCompositorTimelineValue(XrHandle sessionHandle, bool synchronized, uint64_t* pTimelineValue);
void xrProgressCompositorTimelineValue(XrHandle sessionHandle, uint64_t timelineValue, bool synchronized);

void xrGetHeadPose(XrHandle sessionHandle, XrPosef* pPose);
void xrGetEyeView(uint32_t sessionHandle, int viewIndex, XrView* pView);

XrTime xrGetFrameInterval(XrHandle sessionHandle, bool synchronized);

//
//// Mid OpenXR Constants
#define XR_FORCE_STEREO_TO_MONO

#define XR_DEFAULT_WIDTH   1024
#define XR_DEFAULT_HEIGHT  1024
#define XR_DEFAULT_SAMPLES 1

#define XR_SWAP_COUNT 2

#define XR_OPENGL_MAJOR_VERSION 4
#define XR_OPENGL_MINOR_VERSION 6

//
//// Mid OpenXR Types
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

typedef enum XrSwapType {
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


typedef struct Slot {
	bool occupied : 1;
} Slot;


#define XR_CHECK(_command)                     \
	({                                         \
		XrResult result = _command;            \
		if (__builtin_expect(!!(result), 0)) { \
			return result;                     \
		}                                      \
	})

#define POOL(_type, _capacity)                             \
	typedef struct __attribute((aligned(4))) _type##Pool { \
		Slot  slots[_capacity];                            \
		_type data[_capacity];                             \
	} _type##Pool;


#define POOL_HASHED(_type, _capacity)                                                            \
	typedef struct _type##Pool {                                                                 \
		uint32_t count;                                                                          \
		_type    data[_capacity];                                                                \
		XrHash   hash[_capacity];                                                                \
	} _type##Pool;                                                                               \
	static XrResult Claim##_type(_type##Pool* p##_type##s, _type** pp##_type, XrHash hash)       \
	{                                                                                            \
		if (p##_type##s->count >= COUNT(p##_type##s->data)) return XR_ERROR_LIMIT_REACHED;       \
		const uint32_t handle = p##_type##s->count++;                                            \
		p##_type##s->hash[handle] = hash;                                                        \
		*pp##_type = &p##_type##s->data[handle];                                                 \
		return XR_SUCCESS;                                                                       \
	}                                                                                            \
	static inline XrHash Get##_type##Hash(const _type##Pool* p##_type##s, const _type* p##_type) \
	{                                                                                            \
		const int handle = p##_type - p##_type##s->data;                                         \
		return p##_type##s->hash[handle];                                                        \
	}                                                                                            \
	static inline _type* Get##_type##ByHash(_type##Pool* p##_type##s, const XrHash hash)         \
	{                                                                                            \
		for (int i = 0; i < p##_type##s->count; ++i) {                                           \
			if (p##_type##s->hash[i] == hash)                                                    \
				return &p##_type##s->data[i];                                                    \
		}                                                                                        \
		return NULL;                                                                             \
	}

// We are returning pointers, not handles, and treating XrHandles as pointers
// because when they come back from the application they immediately reference their given
// data, which can be used as an entry point into the rest of the data structure. Also, the
// XrHandles passed over OpenXR have to be 64-bit anyways, so we can't really save much with handles.


#define CACHE_SIZE 64
typedef struct __attribute((aligned(CACHE_SIZE))) {
	uint8_t data[CACHE_SIZE];
} MemoryBlock;

#define POOL_BLOCK_COUNT 1U << 16
typedef uint16_t BlockHandle;
typedef struct {
	Slot        slots[POOL_BLOCK_COUNT];
	MemoryBlock blocks[POOL_BLOCK_COUNT];
} PoolMemory;

typedef struct {
	// explore 24 and 20 bit hashes
	uint32_t value;
} Hash;

#define POOL_HANDLE_MAX_CAPACITY 1U << 8
typedef uint8_t PoolHandle;
typedef struct {
	Slot        slots[POOL_HANDLE_MAX_CAPACITY];
	BlockHandle blockHandles[POOL_HANDLE_MAX_CAPACITY];
	Hash        hashes[POOL_HANDLE_MAX_CAPACITY];
	size_t      blockSize;
} HashedPool;

#define POOL_STRUCT(_type, _capacity)                                                                                  \
	typedef struct {                                                                                                   \
		Slot        slots[_capacity];                                                                                  \
		BlockHandle blockHandles[_capacity];                                                                           \
		Hash        hashes[_capacity];                                                                                 \
	} _type##HashedPool;                                                                                               \
	_Static_assert(_capacity <= POOL_HANDLE_MAX_CAPACITY, #_type " " #_capacity " exceeds POOL_HANDLE_MAX_CAPACITY."); \
	static inline void ClaimSessionPoolHandle(_type##HashedPool* pPool, PoolHandle* pPoolHandle)                       \
	{                                                                                                                  \
		ClaimPoolHandle(sizeof(_type), _capacity, pPool->slots, pPool->blockHandles, pPool->hashes, pPoolHandle);      \
	}

extern PoolMemory poolMemory;

static XrResult ClaimBlockHandle(size_t size, BlockHandle* pBlockHandle)
{
	assert((size / CACHE_SIZE) < POOL_BLOCK_COUNT);
	uint32_t blockCount = (size / CACHE_SIZE) + 1;
	printf("Scanning blockCount size %d\n", blockCount);

	uint32_t  requiredBlockCount = blockCount;
	uint32_t  blockHandle = UINT32_MAX;
	for (int i = 0; i < POOL_BLOCK_COUNT; ++i) {
		if (poolMemory.slots[i].occupied) {
			requiredBlockCount = blockCount;
			continue;
		}

		if (--requiredBlockCount != 0)
			continue;

		blockHandle = i;
		break;
	}

	if (blockHandle == UINT32_MAX) {
		LOG_ERROR("Couldn't find required blockCount size.\n");
		return XR_ERROR_LIMIT_REACHED;
	}

	*pBlockHandle = blockHandle;

	printf("Found blockHandle %d\n", blockHandle);
	return XR_SUCCESS;
}
static void ReleaseBlockHandle(size_t size, BlockHandle blockHandle)
{
	assert((size / CACHE_SIZE) < POOL_BLOCK_COUNT);
	uint8_t blockCount = (size / CACHE_SIZE) + 1;
	printf("Releasing blockCount size %d\n", blockCount);

	for (int i = 0; i < blockCount; ++i) {
		assert((blockHandle + i) < POOL_BLOCK_COUNT);
		poolMemory.slots[blockHandle + i].occupied = false;
	}
}

static XrResult ClaimPoolHandle(int blockSize, int poolCapacity, Slot* pSlots, BlockHandle* pBlockHandles, Hash* pHashes, PoolHandle* pPoolHandle)
{
	uint32_t poolHandle = UINT32_MAX;
	for (int i = 0; i < poolCapacity; ++i) {
		if (pSlots[i].occupied) {
			continue;
		}

		pSlots[i].occupied = true;
		poolHandle = i;
		break;
	}

	if (poolHandle == UINT32_MAX) {
		LOG_ERROR("Couldn't find available handle.\n");
		return XR_ERROR_LIMIT_REACHED;
	}

	BlockHandle blockHandle;
	if (ClaimBlockHandle(blockSize, &blockHandle) != XR_SUCCESS) {
		return XR_ERROR_LIMIT_REACHED;
	}

	pBlockHandles[poolHandle] = blockHandle;

	*pPoolHandle = poolHandle;

	printf("Found PoolHandle %d\n", poolHandle);
	return XR_SUCCESS;
}
static XrResult ReleasePoolHandle(int blockSize, Slot* pSlots, BlockHandle* pBlockHandles, Hash* pHashes, PoolHandle poolHandle)
{
	assert(pSlots[poolHandle].occupied);

	ReleaseBlockHandle(blockSize, pBlockHandles[poolHandle]);
	pSlots[poolHandle].occupied = false;
	pHashes[poolHandle].value = 0;

	printf("Released PoolHandle %d\n", poolHandle);
	return XR_SUCCESS;
}


//#define ClaimHandleHashed(_pool, _pValue, _hash) XR_CHECK(_ClaimHandleHashed((Pool*)&_pool, sizeof(_pool.data[0]), COUNT(_pool.data), (void**)&_pValue, _hash))
//static XrResult _ClaimHandleHashed(Pool* pPool, int stride, int capacity, void** ppValue, XrHash _hash)
//{
//	if (pPool->currentIndex >= capacity) {
//		fprintf(stderr, "XR_ERROR_LIMIT_REACHED");
//		return XR_ERROR_LIMIT_REACHED;
//	}
//	const uint32_t handle = pPool->currentIndex++;
//	*ppValue = &pPool->data + (handle * stride);
//	XrHash* pHashStart = (XrHash*)(&pPool->data + (capacity * stride));
//	XrHash* pHash = pHashStart + (handle * sizeof(XrHash));
//	*pHash = _hash;
//	return XR_SUCCESS;
//}
#define ClaimHandle(_pool, _pValue) XR_CHECK(_ClaimHandle(_pool.slots, _pool.data, sizeof(_pool.data[0]), COUNT(_pool.slots), (void**)&_pValue))
static XrResult _ClaimHandle(Slot* pSlots, void* pData, int stride, int capacity, void** ppValue)
{
	uint32_t handle = UINT32_MAX;
	for (int i = 0; i < capacity; ++i) {
		if (pSlots[i].occupied) {
			continue;
		}

		pSlots[i].occupied = true;
		handle = i;
		break;
	}

	if (handle == UINT32_MAX) {
		LOG_ERROR("XR_ERROR_LIMIT_REACHED\n");
		return XR_ERROR_LIMIT_REACHED;
	}

	*ppValue = pData + (handle * stride);
	return XR_SUCCESS;
}
#define ReleaseHandle(_pool, _handle) XR_CHECK(_ReleaseHandle(_pool.slots, _handle))
static XrResult _ReleaseHandle(Slot* pSlots, XrHandle handle)
{
	assert(pSlots[handle].occupied == true);
	pSlots[handle].occupied = false;
	return XR_SUCCESS;
}
#define GetHandle(_pool, _pValue) _GetHandle(_pool.data, sizeof(_pool.data[0]), _pValue)
static XrHandle _GetHandle(void* pData, int stride, const void* pValue)
{
	return (XrHandle)((pValue - pData) / stride);
}


#define MIDXR_MAX_PATHS 128
typedef struct Path {
	char string[XR_MAX_PATH_LENGTH];
} Path;
POOL_HASHED(Path, MIDXR_MAX_PATHS)

typedef struct Binding {
	XrPath path;
	int (*func)(void*);
} Binding;
POOL_HASHED(Binding, 16)

typedef struct InteractionProfile {
	XrPath      path;
	BindingPool bindings;
} InteractionProfile;
POOL_HASHED(InteractionProfile, 16)

#define XR_MAX_ACTIONS 64
typedef Binding* XrBinding;
POOL(XrBinding, XR_MAX_ACTIONS)

typedef InteractionProfile* XrInteractionProfile;

#define XR_MAX_SUBACTION_PATHS 2
typedef struct Action {
	XrActionSet   actionSet;
	uint32_t      countSubactionPaths;
	XrBindingPool subactionBindings[XR_MAX_SUBACTION_PATHS];
	XrPath        subactionPaths[XR_MAX_SUBACTION_PATHS];
	XrActionType  actionType;
	char          actionName[XR_MAX_ACTION_NAME_SIZE];
	char          localizedActionName[XR_MAX_LOCALIZED_ACTION_NAME_SIZE];
} Action;
POOL(Action, XR_MAX_ACTIONS)

//#define XR_MAX_BOUND_ACTIONS 4
//CONTAINER(XrAction, XR_MAX_BOUND_ACTIONS)

typedef struct SubactionState {
	uint32_t lastSyncedPriority;

	// actual layout from OXR to memcpy
	float    currentState;
	XrBool32 changedSinceLastSync;
	XrTime   lastChangeTime;
	XrBool32 isActive;
} SubactionState;
typedef struct ActionState {
	SubactionState subactionStates[XR_MAX_SUBACTION_PATHS];
} ActionState;
POOL(ActionState, XR_MAX_ACTIONS)

typedef struct ActionSetState {
	//	ActionStatePool actionStates;
	ActionState states[XR_MAX_ACTIONS];
	XrActionSet actionSet;
} ActionSetState;
POOL_HASHED(ActionSetState, 4)

typedef struct ActionSet {
	char       actionSetName[XR_MAX_ACTION_SET_NAME_SIZE];
	uint32_t   priority;
	XrSession  attachedToSession;
	ActionPool Actions;
} ActionSet;
POOL_HASHED(ActionSet, 4)

#define XR_MAX_SPACE_COUNT 32
typedef struct Space {
	XrSession            session;
	XrReferenceSpaceType referenceSpaceType;
	XrPosef              poseInReferenceSpace;
} Space;
POOL(Space, XR_MAX_SPACE_COUNT)

typedef struct Swapchain {
	// we should replace XrHandle pointers with uint16_t handle offsets
	XrSession session;

	uint8_t swapIndex;
	union {
		struct {
			GLuint texture;
			GLuint memObject;
		} gl;
		struct {
			ID3D11Texture2D* texture;
			IDXGIKeyedMutex* keyedMutex;
			ID3D11RenderTargetView* rtView;
		} d3d11;
	} color[XR_SWAP_COUNT];

	XrSwapchainCreateFlags createFlags;
	XrSwapchainUsageFlags  usageFlags;
	int64_t                format;
	uint32_t               sampleCount;
	uint32_t               width;
	uint32_t               height;
	uint32_t               faceCount;
	uint32_t               arraySize;
	uint32_t               mipCount;
} Swapchain;
// I don't think I actually need a pool? Just one per session, and it has to switch
POOL(Swapchain, 2)

#define MIDXR_MAX_SESSIONS 4
// this layout is awful from a DOD perspective can I make it less awful?
typedef struct Session {
	XrInstance instance;
	XrSystemId systemId;
	XrSwapType swapType;

	bool running;
	bool exiting;

	XrSessionState activeSessionState;
	XrSessionState pendingSessionState;

	XrHandle activeReferenceSpaceHandle;
	XrHandle pendingReferenceSpaceHandle;

	XrInteractionProfile activeInteractionProfile;
	XrInteractionProfile pendingInteractionProfile;

	XrBool32 activeIsUserPresent;
	XrBool32 pendingIsUserPresent;

	SpacePool referenceSpaces;
	SpacePool actionSpaces;

	XrTime frameWaited;
	XrTime frameBegan;
	XrTime frameEnded;

	// CTS seems to need 3 retained displayTimes to pass.
	XrTime predictedDisplayTimes[3];
	XrTime lastBeginDisplayTime;
	XrTime lastDisplayTime;
	uint32_t synchronizedFrameCount;

	uint64_t sessionTimelineValue;

	SwapchainPool      swapchains;
	ActionSetStatePool actionSetStates;

	XrViewConfigurationType primaryViewConfigurationType;
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
POOL(Session, MIDXR_MAX_SESSIONS)
//POOL_STRUCT(Session, MIDXR_MAX_SESSIONS);

#define MIDXR_MAX_INSTANCES 2
typedef struct Instance {
	// probably need this
	//	uint8_t stateQueueStart;
	//	uint8_t stateQeueEnd;
	//	uint8_t stateQueue[256];

	XrApplicationInfo applicationInfo;

	XrSystemId   systemId;
	XrFormFactor systemFormFactor;
	SessionPool  sessions;

//	SessionHashedPool sessions2;

	ActionSetPool          actionSets;
	InteractionProfilePool interactionProfiles;
	PathPool               paths;

	// not sure if I shold support only one graphics api per instance?
	XrGraphicsApi graphicsApi;
	union {
		struct {
			LUID              adapterLuid;
			D3D_FEATURE_LEVEL minFeatureLevel;
		} d3d11;
	} graphics;

} Instance;
//POOL(Instance, MIDXR_MAX_INSTANCES)

// I think I want to put all of the above in an arena pool

//
//// MidOpenXR Implementation
#ifdef MID_OPENXR_IMPLEMENTATION

static struct {
	Instance instance;
} xr;

PoolMemory poolMemory;

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

#ifdef WIN32
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

#define XR_EXPORT XRAPI_ATTR XrResult XRAPI_CALL

#define PI 3.14159265358979323846

	//static InstancePool instances;

//
//// Mid OpenXR Utility
static uint32_t CalcDJB2(const char* str, int max)
{
	char     c;
	int      i = 0;
	uint32_t hash = 5381;
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

static XrTime HzToXrTime(double hz)
{
	return (XrTime)(hz / 1e9);
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

#define FORMAT_QUAT     "{x: %.3f, y: %.3f, z: %.3f, w: %.3f}"
#define EXPAND_QUAT(_q) _q.x, _q.y, _q.z, _q.w
#define FORMAT_VEC3     "{x: %.3f, y: %.3f, z: %.3f}"
#define EXPAND_VEC3(_q) _q.x, _q.y, _q.z

static void LogNextChain(const XrBaseInStructure* nextProperties)
{
	while (nextProperties != NULL) {
		printf("NextType: %s\n", string_XrStructureType(nextProperties->type));
		nextProperties = (XrBaseInStructure*)nextProperties->next;
	}
}

//
//// Mid Device Input Funcs
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

//
//// Mid OpenXR Implementation
static XrResult RegisterBinding(
	XrInstance          instance,
	InteractionProfile* pInteractionProfile,
	Path*               pBindingPath,
	int (*func)(void*))
{
	Instance* pInstance = (Instance*)instance;

	int bindingPathHash = GetPathHash(&pInstance->paths, pBindingPath);
	for (int i = 0; i < pInteractionProfile->bindings.count; ++i) {
		if (GetPathHash(&pInstance->paths, (Path*)pInteractionProfile->bindings.data[i].path) == bindingPathHash) {
			LOG_ERROR("Trying to register path hash twice! %s %d\n", pBindingPath->string, bindingPathHash);
			return XR_ERROR_PATH_INVALID;
		}
	}

	Binding* pBinding;
	ClaimBinding(&pInteractionProfile->bindings, &pBinding, bindingPathHash);
	//	ClaimHandleHashed(pInteractionProfile->bindings, pBinding, bindingPathHash);
	pBinding->path = (XrPath)pBindingPath;
	pBinding->func = func;

	return XR_SUCCESS;
}

typedef struct BindingDefinition {
	int (*func)(void*);
	const char path[XR_MAX_PATH_LENGTH];
} BindingDefinition;
static XrResult InitBinding(XrInstance instance, const char* interactionProfile, int bindingDefinitionCount, BindingDefinition* pBindingDefinitions)
{
	Instance* pInstance = (Instance*)instance;

	XrPath interactionProfilePath;
	xrStringToPath(instance, interactionProfile, &interactionProfilePath);
	XrHash interactionProfileHash = GetPathHash(&pInstance->paths, (const Path*)interactionProfilePath);

	InteractionProfile* pInteractionProfile;
	//	ClaimHandleHashed(pInstance->interactionProfiles, pInteractionProfile, interactionProfileHash);
	XR_CHECK(ClaimInteractionProfile(&pInstance->interactionProfiles, &pInteractionProfile, interactionProfileHash));

	pInteractionProfile->path = interactionProfilePath;

	for (int i = 0; i < bindingDefinitionCount; ++i) {
		XrPath bindingPath;
		xrStringToPath(instance, pBindingDefinitions[i].path, &bindingPath);
		RegisterBinding(instance, pInteractionProfile, (Path*)bindingPath, pBindingDefinitions[i].func);
	}

	return XR_SUCCESS;
}

#define XR_DEFAULT_INTERACTION_PROFILE "/interaction_profiles/oculus/touch_controller"

static XrResult InitStandardBindings(XrInstance instance)
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
		InitBinding(instance, "/interaction_profiles/oculus/touch_controller", COUNT(bindingDefinitions), bindingDefinitions);
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
		InitBinding(instance, "/interaction_profiles/microsoft/motion_controller", COUNT(bindingDefinitions), bindingDefinitions);
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
		InitBinding(instance, "/interaction_profiles/khr/simple_controller", COUNT(bindingDefinitions), bindingDefinitions);
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
		InitBinding(instance, "/interaction_profiles/valve/index_controller", COUNT(bindingDefinitions), bindingDefinitions);
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
		InitBinding(instance, "/interaction_profiles/htc/vive_controller", COUNT(bindingDefinitions), bindingDefinitions);
	}

	return XR_SUCCESS;
#undef BINDING_DEFINITION
}

XR_EXPORT xrEnumerateApiLayerProperties(
	uint32_t              propertyCapacityInput,
	uint32_t*             propertyCountOutput,
	XrApiLayerProperties* properties)
{
	LOG_METHOD(xrEnumerateApiLayerProperties);
	return XR_SUCCESS;
}

XR_EXPORT xrEnumerateInstanceExtensionProperties(
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

		// expected by unity
//		{
//			.type = XR_TYPE_EXTENSION_PROPERTIES,
//			.extensionName = XR_KHR_VISIBILITY_MASK_EXTENSION_NAME,
//			.extensionVersion = XR_KHR_visibility_mask_SPEC_VERSION,
//		},
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
//		{
//			.type = XR_TYPE_EXTENSION_PROPERTIES,
//			.extensionName = XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME,
//			.extensionVersion = XR_KHR_composition_layer_depth_SPEC_VERSION,
//		},
//		{
//			.type = XR_TYPE_EXTENSION_PROPERTIES,
//			.extensionName = XR_VARJO_QUAD_VIEWS_EXTENSION_NAME,
//			.extensionVersion = XR_VARJO_quad_views_SPEC_VERSION,
//		},
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

XR_EXPORT xrCreateInstance(
	const XrInstanceCreateInfo* createInfo,
	XrInstance*                 instance)
{
	LOG_METHOD(xrCreateInstance);

	if (xr.instance.applicationInfo.apiVersion != 0) {
		LOG_ERROR("XR_ERROR_LIMIT_REACHED\n");
		return XR_ERROR_LIMIT_REACHED;
	}

	Instance* pInstance = &xr.instance;
//	ClaimHandle(instances, pInstance);

	for (int i = 0; i < createInfo->enabledApiLayerCount; ++i) {
		printf("Enabled API Layer: %s\n", createInfo->enabledApiLayerNames[i]);
	}

	for (int i = 0; i < createInfo->enabledExtensionCount; ++i) {
		printf("Enabled Extension: %s\n", createInfo->enabledExtensionNames[i]);
		if (strncmp(createInfo->enabledExtensionNames[i], XR_KHR_OPENGL_ENABLE_EXTENSION_NAME, XR_MAX_EXTENSION_NAME_SIZE) == 0)
			pInstance->graphicsApi = XR_GRAPHICS_API_OPENGL;
		if (strncmp(createInfo->enabledExtensionNames[i], XR_KHR_D3D11_ENABLE_EXTENSION_NAME, XR_MAX_EXTENSION_NAME_SIZE) == 0)
			pInstance->graphicsApi = XR_GRAPHICS_API_D3D11_4;
	}

	memcpy(&pInstance->applicationInfo, &createInfo->applicationInfo, sizeof(XrApplicationInfo));


	printf("applicationName: %s\n", createInfo->applicationInfo.applicationName);
	printf("applicationVersion: %d\n", createInfo->applicationInfo.applicationVersion);
	printf("engineName: %s\n", createInfo->applicationInfo.engineName);
	printf("engineVersion: %d\n", createInfo->applicationInfo.engineVersion);
	{
		int maj = XR_VERSION_MAJOR(pInstance->applicationInfo.apiVersion);
		int min = XR_VERSION_MINOR(pInstance->applicationInfo.apiVersion);
		int patch = XR_VERSION_PATCH(pInstance->applicationInfo.apiVersion);
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
//#define PRINT_STRUCT(_field) printf(#_field ": "); printf(FIELD_FORMAT_SPECIFIER(pInstance->applicationInfo._field), pInstance->applicationInfo._field); printf("\n");
//	XR_LIST_STRUCT_XrApplicationInfo(PRINT_STRUCT);

	*instance = (XrInstance)pInstance;

	InitStandardBindings(*instance);
	midXrInitialize();

	switch (pInstance->graphicsApi) {
		case XR_GRAPHICS_API_OPENGL: {
			gl.CreateMemoryObjectsEXT = (PFNGLCREATEMEMORYOBJECTSEXTPROC)wglGetProcAddress("glCreateMemoryObjectsEXT");
			if (!gl.CreateMemoryObjectsEXT) {
				printf("Failed to load glCreateMemoryObjectsEXT\n");
			}
			gl.ImportMemoryWin32HandleEXT = (PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC)wglGetProcAddress("glImportMemoryWin32HandleEXT");
			if (!gl.ImportMemoryWin32HandleEXT) {
				printf("Failed to load glImportMemoryWin32HandleEXT\n");
			}
			gl.CreateTextures = (PFNGLCREATETEXTURESPROC)wglGetProcAddress("glCreateTextures");
			if (!gl.CreateTextures) {
				printf("Failed to load glCreateTextures\n");
			}
			gl.TextureStorageMem2DEXT = (PFNGLTEXTURESTORAGEMEM2DEXTPROC)wglGetProcAddress("glTextureStorageMem2DEXT");
			if (!gl.TextureStorageMem2DEXT) {
				printf("Failed to load glTextureStorageMem2DEXT\n");
			}
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

XR_EXPORT xrDestroyInstance(
	XrInstance instance)
{
	LOG_METHOD(xrDestroyInstance);
	Instance* pInstance = &xr.instance;
	assert(instance == (XrInstance)pInstance);
	*pInstance = (Instance){};
	return XR_SUCCESS;
}

XR_EXPORT xrGetInstanceProperties(
	XrInstance            instance,
	XrInstanceProperties* instanceProperties)
{
	LOG_METHOD(xrGetInstanceProperties);

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

XR_EXPORT xrPollEvent(
	XrInstance         instance,
	XrEventDataBuffer* eventData)
{
	LOG_METHOD_ONCE(xrPollEvent);

	Instance*    pInstance = (Instance*)instance;
	SessionPool* pSessions = (SessionPool*)&pInstance->sessions;

	for (int i = 0; i < COUNT(pSessions->slots); ++i) {
		if (!pSessions->slots[i].occupied)
			continue;

		Session* pSession = &pSessions->data[i];

		if (pSession->activeSessionState != pSession->pendingSessionState) {

			eventData->type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;

			XrEventDataSessionStateChanged* pEventData = (XrEventDataSessionStateChanged*)eventData;
			pEventData->session = (XrSession)pSession;
			pEventData->time = xrGetTime();

			// Must cycle through Focus, Visible, Synchronized if exiting.
			if (pSession->exiting) {
				// todo refactor some this in switches
//				switch (pSession->activeSessionState) {
//					case XR_SESSION_STATE_FOCUSED:
//					case XR_SESSION_STATE_VISIBLE:
//					case XR_SESSION_STATE_READY:
//				}

				if (pSession->activeSessionState == XR_SESSION_STATE_FOCUSED)
					pSession->activeSessionState = XR_SESSION_STATE_VISIBLE;
				else if (pSession->activeSessionState == XR_SESSION_STATE_VISIBLE)
					pSession->activeSessionState = XR_SESSION_STATE_SYNCHRONIZED;
				else if (pSession->activeSessionState == XR_SESSION_STATE_READY)
					pSession->activeSessionState = XR_SESSION_STATE_SYNCHRONIZED;
				else if (pSession->activeSessionState == XR_SESSION_STATE_SYNCHRONIZED)
					pSession->activeSessionState = XR_SESSION_STATE_STOPPING;
				else if (pSession->activeSessionState == XR_SESSION_STATE_STOPPING) {
					if (pSession->running)
						return XR_EVENT_UNAVAILABLE; // must wait for Idle
					else {
						pSession->activeSessionState = XR_SESSION_STATE_IDLE;
						pSession->pendingSessionState = XR_SESSION_STATE_EXITING;
					}
				} else if (pSession->activeSessionState == XR_SESSION_STATE_IDLE) {
					if (pSession->running) {
						pSession->activeSessionState = XR_SESSION_STATE_STOPPING;
					} else {
						pSession->activeSessionState = XR_SESSION_STATE_EXITING;
					}
				}

				pEventData->state = pSession->activeSessionState;
				printf("XrEventDataSessionStateChanged: %s\n", string_XrSessionState(pEventData->state));
				return XR_SUCCESS;
			}

			if (!pSession->running) {

				if (pSession->pendingSessionState == XR_SESSION_STATE_IDLE) {

					if (pSession->activeSessionState == XR_SESSION_STATE_UNKNOWN) {
						pSession->activeSessionState = XR_SESSION_STATE_IDLE;
						// Immediately progress Idle to Ready when starting
						pSession->pendingSessionState = XR_SESSION_STATE_READY;
					} else if (pSession->activeSessionState == XR_SESSION_STATE_STOPPING)
						pSession->activeSessionState = XR_SESSION_STATE_IDLE;

				} else {
					pSession->activeSessionState = pSession->pendingSessionState;
				}

				pEventData->state = pSession->activeSessionState;
				printf("XrEventDataSessionStateChanged: %s\n", string_XrSessionState(pEventData->state));
				return XR_SUCCESS;
			}

			// Must cycle through Focus, Visible, Synchronized if going to idle.
			if (pSession->pendingSessionState == XR_SESSION_STATE_IDLE) {

				if (pSession->activeSessionState == XR_SESSION_STATE_FOCUSED)
					pSession->activeSessionState = XR_SESSION_STATE_VISIBLE;
				else if (pSession->activeSessionState == XR_SESSION_STATE_VISIBLE)
					pSession->activeSessionState = XR_SESSION_STATE_SYNCHRONIZED;
				else if (pSession->activeSessionState == XR_SESSION_STATE_SYNCHRONIZED)
					pSession->activeSessionState = XR_SESSION_STATE_STOPPING;
				else if (pSession->activeSessionState == XR_SESSION_STATE_STOPPING)
					pSession->activeSessionState = XR_SESSION_STATE_IDLE;

				pEventData->state = pSession->activeSessionState;
				printf("XrEventDataSessionStateChanged: %s\n", string_XrSessionState(pEventData->state));
				return XR_SUCCESS;
			}

			// Must cycle through Focus, Visible, Synchronized if stopping.
			if (pSession->pendingSessionState == XR_SESSION_STATE_STOPPING) {

				if (pSession->activeSessionState == XR_SESSION_STATE_FOCUSED)
					pSession->activeSessionState = XR_SESSION_STATE_VISIBLE;
				else if (pSession->activeSessionState == XR_SESSION_STATE_READY)
					pSession->activeSessionState = XR_SESSION_STATE_SYNCHRONIZED;
				else if (pSession->activeSessionState == XR_SESSION_STATE_VISIBLE)
					pSession->activeSessionState = XR_SESSION_STATE_SYNCHRONIZED;
				else if (pSession->activeSessionState == XR_SESSION_STATE_SYNCHRONIZED)
					pSession->activeSessionState = XR_SESSION_STATE_STOPPING;

				pEventData->state = pSession->activeSessionState;
				printf("XrEventDataSessionStateChanged: %s\n", string_XrSessionState(pEventData->state));
				return XR_SUCCESS;
			}

			// Must ensure stopping does not switch back to Ready after going Idle
			if (pSession->activeSessionState == XR_SESSION_STATE_STOPPING) {
				pSession->activeSessionState = pSession->pendingSessionState;
				pEventData->state = pSession->activeSessionState ;
				printf("XrEventDataSessionStateChanged: %s\n", string_XrSessionState(pEventData->state));
				return XR_SUCCESS;
			}

			pSession->activeSessionState = pSession->pendingSessionState;
			pEventData->state = pSession->activeSessionState ;
			printf("XrEventDataSessionStateChanged: %s\n", string_XrSessionState(pEventData->state));

			// Forcing to focused for now, but should happen after some user input probably
			if (pSession->activeSessionState == XR_SESSION_STATE_VISIBLE) {
				pSession->pendingSessionState = XR_SESSION_STATE_FOCUSED;
			}

			return XR_SUCCESS;
		}

		if (pSession->activeReferenceSpaceHandle != pSession->pendingReferenceSpaceHandle) {

			eventData->type = XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING;

			XrEventDataReferenceSpaceChangePending* pEventData = (XrEventDataReferenceSpaceChangePending*)eventData;
			pEventData->session = (XrSession)pSession;
			pEventData->referenceSpaceType = pSession->referenceSpaces.data[pSession->pendingReferenceSpaceHandle].referenceSpaceType;
			pEventData->changeTime = xrGetTime();
			pEventData->poseValid = XR_TRUE;
			// this is not correct, supposed to be origin of new space in space of prior space
			pEventData->poseInPreviousSpace = pSession->referenceSpaces.data[pSession->pendingReferenceSpaceHandle].poseInReferenceSpace;

			printf("XrEventDataReferenceSpaceChangePending: %d\n", pEventData->referenceSpaceType);

			pSession->activeReferenceSpaceHandle = pSession->pendingReferenceSpaceHandle;

			return XR_SUCCESS;
		}

		if (pSession->activeInteractionProfile != pSession->pendingInteractionProfile) {

			eventData->type = XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED;

			XrEventDataInteractionProfileChanged* pEventData = (XrEventDataInteractionProfileChanged*)eventData;
			pEventData->session = (XrSession)pSession;

			pSession->activeInteractionProfile = pSession->pendingInteractionProfile;

			InteractionProfile* pActionInteractionProfile = (InteractionProfile*)pSession->activeInteractionProfile;
			Path*               pActionInteractionProfilePath = (Path*)pActionInteractionProfile->path;
			printf("XrEventDataInteractionProfileChanged %s\n", pActionInteractionProfilePath->string);

			return XR_SUCCESS;
		}

		if (pSession->activeIsUserPresent != pSession->pendingIsUserPresent) {

			eventData->type = XR_TYPE_EVENT_DATA_USER_PRESENCE_CHANGED_EXT;

			XrEventDataUserPresenceChangedEXT* pEventData = (XrEventDataUserPresenceChangedEXT*)eventData;
			pEventData->session = (XrSession)pSession;
			pEventData->isUserPresent = pSession->pendingIsUserPresent;

			pSession->activeIsUserPresent = pSession->pendingIsUserPresent;

			printf("XrEventDataUserPresenceChangedEXT %d\n", pSession->activeIsUserPresent);

			return XR_SUCCESS;
		}
	}

	return XR_EVENT_UNAVAILABLE;
}

#define TRANSFER_ENUM_NAME(_type, _) \
	case _type: strncpy(buffer, #_type, XR_MAX_RESULT_STRING_SIZE); break;

XRAPI_ATTR XrResult XRAPI_CALL xrResultToString(
	XrInstance instance,
	XrResult   value,
	char       buffer[XR_MAX_RESULT_STRING_SIZE])
{
	switch (value) {
		XR_LIST_ENUM_XrResult(TRANSFER_ENUM_NAME);
		default:
			snprintf(buffer, XR_MAX_RESULT_STRING_SIZE, "XR_UNKNOWN_STRUCTURE_TYPE_%d", value);
			break;
	}
	buffer[XR_MAX_RESULT_STRING_SIZE - 1] = '\0';

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrStructureTypeToString(
	XrInstance      instance,
	XrStructureType value,
	char            buffer[XR_MAX_STRUCTURE_NAME_SIZE])
{
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
XRAPI_ATTR XrResult XRAPI_CALL xrGetSystem(
	XrInstance             instance,
	const XrSystemGetInfo* getInfo,
	XrSystemId*            systemId)
{
	LOG_METHOD_ONCE(xrGetSystem);
	LogNextChain((XrBaseInStructure*)getInfo->next);

	Instance* pInstance = (Instance*)instance;

	switch (getInfo->formFactor) {
		case XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY:
			pInstance->systemFormFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
			pInstance->systemId = SYSTEM_ID_HMD_VR_STEREO;
			*systemId = SYSTEM_ID_HMD_VR_STEREO;
			return XR_SUCCESS;
		case XR_FORM_FACTOR_HANDHELD_DISPLAY:
			pInstance->systemFormFactor = XR_FORM_FACTOR_HANDHELD_DISPLAY;
			pInstance->systemId = SYSTEM_ID_HANDHELD_AR;
			*systemId = SYSTEM_ID_HANDHELD_AR;
			return XR_SUCCESS;
		default:
			printf("XR_ERROR_FORM_FACTOR_UNSUPPORTED\n");
			*systemId = XR_NULL_SYSTEM_ID;
			return XR_ERROR_FORM_FACTOR_UNSUPPORTED;
	}
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetSystemProperties(
	XrInstance          instance,
	XrSystemId          systemId,
	XrSystemProperties* properties)
{
	LOG_METHOD(xrGetSystemProperties);
	LogNextChain((XrBaseInStructure*)properties->next);

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

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateEnvironmentBlendModes(
	XrInstance              instance,
	XrSystemId              systemId,
	XrViewConfigurationType viewConfigurationType,
	uint32_t                environmentBlendModeCapacityInput,
	uint32_t*               environmentBlendModeCountOutput,
	XrEnvironmentBlendMode* environmentBlendModes)
{
	LOG_METHOD(xrEnumerateEnvironmentBlendModes);

	Instance* pInstance = (Instance*)instance;

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

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSession(
	XrInstance                 instance,
	const XrSessionCreateInfo* createInfo,
	XrSession*                 session)
{
	LOG_METHOD(xrCreateSession);

	Instance* pInstance = (Instance*)instance;

	if (pInstance->systemId == XR_NULL_SYSTEM_ID) {
		LOG_ERROR("XR_ERROR_SYSTEM_INVALID\n");
		return XR_ERROR_SYSTEM_INVALID;
	}

	printf("SystemId: %llu\n", createInfo->systemId);
	if (pInstance->systemId != createInfo->systemId) {
		LOG_ERROR("XR_ERROR_SYSTEM_INVALID\n");
		return XR_ERROR_SYSTEM_INVALID;
	}

	if (pInstance->graphics.d3d11.adapterLuid.LowPart == 0) {
		LOG_ERROR("XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING\n");
		return XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING;
	}

	XrHandle externalSessionHandle;
	xrClaimSession(&externalSessionHandle);

//	PoolHandle sessionHandle;
//	ClaimSessionPoolHandle(&pInstance->sessions2, &sessionHandle);
//	BlockHandle blockHandle = pInstance->sessions2.blockHandles[sessionHandle];
//	Session* pSession = (Session*)&poolMemory.blocks[blockHandle];


	Session* pClaimedSession;
	ClaimHandle(pInstance->sessions, pClaimedSession);
	*pClaimedSession = (Session){};

	XrHandle sessionHandle = GetHandle(pInstance->sessions, pClaimedSession);

	printf("CreatedHandle %d ClaimedHandle %d\n", externalSessionHandle, sessionHandle);
	// these should be the same but probably want to get these handles better...
	assert(externalSessionHandle == sessionHandle);

	HANDLE compositorFenceHandle;
	xrGetCompositorTimeline(sessionHandle, &compositorFenceHandle);

	HANDLE sessionFenceHandle;
	xrGetSessionTimeline(sessionHandle, &sessionFenceHandle);

	if (createInfo->next == NULL) {
		LOG_ERROR("XR_ERROR_GRAPHICS_DEVICE_INVALID\n");
		return XR_ERROR_GRAPHICS_DEVICE_INVALID;
	}

	switch (*(XrStructureType*)createInfo->next) {
		case XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR: {
			printf("OpenXR Graphics Binding: XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR\n");
			XrGraphicsBindingOpenGLWin32KHR* binding = (XrGraphicsBindingOpenGLWin32KHR*)createInfo->next;

			pClaimedSession->binding.gl.hDC = binding->hDC;
			pClaimedSession->binding.gl.hGLRC = binding->hGLRC;

			*session = (XrSession)pClaimedSession;
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

			pClaimedSession->binding.d3d11.device5 = device5;
			pClaimedSession->binding.d3d11.context4 = context4;
			pClaimedSession->binding.d3d11.compositorFence = compositorFence;
			pClaimedSession->binding.d3d11.sessionFence = sessionFence;

			*session = (XrSession)pClaimedSession;

			break;
		}
		case XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR: {
			printf("OpenXR Graphics Binding: XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR\n");
			XrGraphicsBindingVulkanKHR* binding = (XrGraphicsBindingVulkanKHR*)createInfo->next;

			pClaimedSession->binding.vk.instance = binding->instance;
			pClaimedSession->binding.vk.physicalDevice = binding->physicalDevice;
			pClaimedSession->binding.vk.device = binding->device;
			pClaimedSession->binding.vk.queueFamilyIndex = binding->queueFamilyIndex;
			pClaimedSession->binding.vk.queueIndex = binding->queueIndex;

			*session = (XrSession)pClaimedSession;
		}
		default: {
			LOG_ERROR("XR_ERROR_GRAPHICS_DEVICE_INVALID\n");
			return XR_ERROR_GRAPHICS_DEVICE_INVALID;
		}
	}


	pClaimedSession->instance = instance;
	pClaimedSession->systemId = createInfo->systemId;
	pClaimedSession->activeSessionState = XR_SESSION_STATE_UNKNOWN;
	pClaimedSession->pendingSessionState = XR_SESSION_STATE_IDLE;
	pClaimedSession->activeReferenceSpaceHandle = XR_HANDLE_INVALID;
	pClaimedSession->pendingReferenceSpaceHandle = XR_HANDLE_INVALID;
	pClaimedSession->activeIsUserPresent = XR_FALSE;
	pClaimedSession->pendingIsUserPresent = XR_TRUE;

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySession(
	XrSession session)
{
	LOG_METHOD(xrDestroySession);

	Session* pSession = (Session*)session;
	Instance* pInstance = (Instance*)pSession->instance;
	XrHandle  sessionHandle = GetHandle(pInstance->sessions, pSession);
	ReleaseHandle(pInstance->sessions, sessionHandle);

	xrReleaseSession(sessionHandle);

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateReferenceSpaces(
	XrSession             session,
	uint32_t              spaceCapacityInput,
	uint32_t*             spaceCountOutput,
	XrReferenceSpaceType* spaces)
{
	LOG_METHOD(xrEnumerateReferenceSpaces);

	Session* pSession = (Session*)session;

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

XRAPI_ATTR XrResult XRAPI_CALL xrCreateReferenceSpace(
	XrSession                         session,
	const XrReferenceSpaceCreateInfo* createInfo,
	XrSpace*                          space)
{
	LOG("xrCreateReferenceSpace %s pos: " FORMAT_VEC3 " rot: " FORMAT_QUAT "\n",
		string_XrReferenceSpaceType(createInfo->referenceSpaceType),
		EXPAND_VEC3(createInfo->poseInReferenceSpace.position),
		EXPAND_QUAT(createInfo->poseInReferenceSpace.orientation));

	Session* pSession = (Session*)session;

	Space* pSpace;
	ClaimHandle(pSession->referenceSpaces, pSpace);

	pSpace->session = session;
	pSpace->referenceSpaceType = createInfo->referenceSpaceType;
	pSpace->poseInReferenceSpace = createInfo->poseInReferenceSpace;

	// auto switch to first created space
	if (pSession->pendingReferenceSpaceHandle == XR_HANDLE_INVALID) {
		pSession->pendingReferenceSpaceHandle = GetHandle(pSession->referenceSpaces, pSpace);
	}

	*space = (XrSpace)pSpace;

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetReferenceSpaceBoundsRect(
	XrSession            session,
	XrReferenceSpaceType referenceSpaceType,
	XrExtent2Df*         bounds)
{
	LOG_METHOD(xrGetReferenceSpaceBoundsRect);

	Session* pSession = (Session*)session;
	Instance* pInstance = (Instance*)pSession->instance;
	XrHandle  sessionHandle = GetHandle(pInstance->sessions, pSession);

	midXrGetReferenceSpaceBounds(sessionHandle, bounds);

	printf("xrGetReferenceSpaceBoundsRect %s width: %f height: %f\n", string_XrReferenceSpaceType(referenceSpaceType), bounds->width, bounds->height);

	return XR_SUCCESS;
}

//XRAPI_ATTR XrResult XRAPI_CALL xrSetReferenceSpaceBoundsRect(
//	XrSession            session,
//	XrReferenceSpaceType referenceSpaceType,
//	XrExtent2Df          bounds)
//{
//	return XR_SUCCESS;
//}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateActionSpace(
	XrSession                      session,
	const XrActionSpaceCreateInfo* createInfo,
	XrSpace*                       space)
{
	LOG_METHOD_ONCE(xrCreateActionSpace);
	assert(createInfo->next == NULL);

	Session* pSession = (Session*)session;

	Space* actionSpace;
	ClaimHandle(pSession->actionSpaces, actionSpace);

	*space = (XrSpace)actionSpace;

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrLocateSpace(
	XrSpace          space,
	XrSpace          baseSpace,
	XrTime           time,
	XrSpaceLocation* location)
{
	LOG_METHOD_ONCE(xrLocateSpace);

	Space* pSpace = (Space*)space;
	Session* pSession = (Session*)pSpace->session;
	Instance* pInstance = (Instance*)pSession->instance;
	XrHandle  sessionHandle = GetHandle(pInstance->sessions, pSession);

	location->locationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT |
							  XR_SPACE_LOCATION_POSITION_VALID_BIT |
							  XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT |
							  XR_SPACE_LOCATION_POSITION_TRACKED_BIT;

	xrGetHeadPose(sessionHandle, &location->pose);

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

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySpace(
	XrSpace space)
{
	LOG_METHOD(xrDestroySpace);
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrDestroySpace\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateViewConfigurations(
	XrInstance               instance,
	XrSystemId               systemId,
	uint32_t                 viewConfigurationTypeCapacityInput,
	uint32_t*                viewConfigurationTypeCountOutput,
	XrViewConfigurationType* viewConfigurationTypes)
{
	LOG_METHOD(xrEnumerateViewConfigurations);

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

XRAPI_ATTR XrResult XRAPI_CALL xrGetViewConfigurationProperties(
	XrInstance                     instance,
	XrSystemId                     systemId,
	XrViewConfigurationType        viewConfigurationType,
	XrViewConfigurationProperties* configurationProperties)
{
	LOG_METHOD(xrGetViewConfigurationProperties);
	printf("%s\n", string_XrViewConfigurationType(viewConfigurationType));

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

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateViewConfigurationViews(
	XrInstance               instance,
	XrSystemId               systemId,
	XrViewConfigurationType  viewConfigurationType,
	uint32_t                 viewCapacityInput,
	uint32_t*                viewCountOutput,
	XrViewConfigurationView* views)
{
	LOG_METHOD(xrEnumerateViewConfigurationViews);
	LOG("%s\n", string_XrViewConfigurationType(viewConfigurationType));

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

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateSwapchainFormats(
	XrSession session,
	uint32_t  formatCapacityInput,
	uint32_t* formatCountOutput,
	int64_t*  formats)
{
	LOG_METHOD(xrEnumerateSwapchainFormats);

	Session*  pSession = (Session*)session;
	Instance* pInstance = (Instance*)pSession->instance;

#define TRANSFER_SWAP_FORMATS                               \
	*formatCountOutput = COUNT(swapFormats);                \
	if (formats == NULL)                                    \
		return XR_SUCCESS;                                  \
	if (formatCapacityInput < COUNT(swapFormats))           \
		return XR_ERROR_SIZE_INSUFFICIENT;                  \
	for (int i = 0; i < COUNT(swapFormats); ++i) {          \
		printf("Enumerating Format: %llu\n", swapFormats[i]); \
		formats[i] = swapFormats[i];                        \
	}

	switch (pInstance->graphicsApi) {
		case XR_GRAPHICS_API_OPENGL: {
			int64_t swapFormats[] = {
				GL_SRGB8_ALPHA8,
				GL_SRGB8,
			};
			TRANSFER_SWAP_FORMATS
			return XR_SUCCESS;
		}
		case XR_GRAPHICS_API_VULKAN: {
			int64_t swapFormats[] = {
				VK_FORMAT_R8G8B8A8_UNORM,
				VK_FORMAT_R8G8B8A8_SRGB,
			};
			TRANSFER_SWAP_FORMATS
			return XR_SUCCESS;
		}
		case XR_GRAPHICS_API_D3D11_4: {
			int64_t swapFormats[] = {
				DXGI_FORMAT_R8G8B8A8_UNORM,
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			};
			TRANSFER_SWAP_FORMATS
			return XR_SUCCESS;
		}
		default:
			LOG_ERROR("xrEnumerateSwapchainFormats Graphics API not supported.");
			return XR_ERROR_RUNTIME_FAILURE;
	}

#undef TRANSFER_SWAP_FORMATS
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSwapchain(
	XrSession                    session,
	const XrSwapchainCreateInfo* createInfo,
	XrSwapchain*                 swapchain)
{
	LOG_METHOD(xrCreateSwapchain);

	printf("Create swapchain format: %lld sampleCount %d width: %d height %d faceCount: %d arraySize: %d mipCount %d\n",
		   createInfo->format, createInfo->sampleCount, createInfo->width, createInfo->height, createInfo->faceCount, createInfo->arraySize, createInfo->mipCount);

#define PRINT_CREATE_FLAGS(_flag, _bit)  \
	if (createInfo->createFlags & _flag) \
		printf(#_flag "\n");
	XR_LIST_BITS_XrSwapchainCreateFlags(PRINT_CREATE_FLAGS);
#undef PRINT_CREATE_FLAGS

#define PRINT_USAGE_FLAGS(_flag, _bit)  \
	if (createInfo->usageFlags & _flag) \
		printf(#_flag "\n");
	XR_LIST_BITS_XrSwapchainUsageFlags(PRINT_USAGE_FLAGS);
#undef PRINT_USAGE_FLAGS

	if (createInfo->createFlags & XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT) {
		LOG_ERROR("XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT unsupported \n");
		return XR_ERROR_FEATURE_UNSUPPORTED;
	}

	Session*  pSession = (Session*)session;

	int expectedWidth = XR_DEFAULT_WIDTH;
	int expectedHeight = XR_DEFAULT_HEIGHT;

	if (createInfo->width * 2 == expectedWidth) {
		pSession->swapType = XR_SWAP_TYPE_STEREO_DOUBLE_WIDE;
		expectedWidth *= 2;
		printf("Setting XR_SWAP_TYPE_STEREO_DOUBLE_WIDE for session.\n");
	}
	if (createInfo->arraySize > 1) {
		pSession->swapType = XR_SWAP_TYPE_STEREO_TEXTURE_ARRAY;
		printf("Setting XR_SWAP_TYPE_STEREO_TEXTURE_ARRAY for session.\n");
	}

	if (createInfo->width != expectedWidth || createInfo->height != expectedHeight) {
		LOG_ERROR("XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED %d %d\n", expectedWidth, expectedHeight);
		return XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED;
	}

	Instance* pInstance = (Instance*)pSession->instance;
	XrHandle  sessionHandle = GetHandle(pInstance->sessions, pSession);

	Swapchain* pSwapchain;
	ClaimHandle(pSession->swapchains, pSwapchain);

	pSwapchain->session = session;
	pSwapchain->createFlags = createInfo->createFlags;
	pSwapchain->usageFlags = createInfo->usageFlags;
	pSwapchain->format = createInfo->format;
	pSwapchain->sampleCount = createInfo->sampleCount;
	pSwapchain->width = createInfo->width;
	pSwapchain->height = createInfo->height;
	pSwapchain->faceCount = createInfo->faceCount;
	pSwapchain->arraySize = createInfo->arraySize;
	pSwapchain->mipCount = createInfo->mipCount;

	HANDLE colorHandles[XR_SWAP_COUNT];
	midXrClaimFramebufferImages(sessionHandle, XR_SWAP_COUNT, colorHandles);

	switch (pInstance->graphicsApi) {
		case XR_GRAPHICS_API_OPENGL: {
			printf("Creating OpenGL Swap");

#define DEFAULT_IMAGE_CREATE_INFO(_width, _height, _format, _memObject, _texture, _handle)                    \
	gl.CreateMemoryObjectsEXT(1, &_memObject);                                                                \
	gl.ImportMemoryWin32HandleEXT(_memObject, _width* _height * 4, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, _handle); \
	gl.CreateTextures(GL_TEXTURE_2D, 1, &_texture);                                                           \
	gl.TextureStorageMem2DEXT(_texture, 1, _format, _width, _height, _memObject, 0);
			for (int i = 0; i < XR_SWAP_COUNT; ++i) {
				DEFAULT_IMAGE_CREATE_INFO(pSwapchain->width, pSwapchain->height, GL_RGBA8, pSwapchain->color[i].gl.memObject, pSwapchain->color[i].gl.texture, colorHandles[i]);
				printf("Imported gl swap texture. Texture: %d MemObject: %d\n", pSwapchain->color[i].gl.texture, pSwapchain->color[i].gl.memObject);
			}
#undef DEFAULT_IMAGE_CREATE_INFO

			break;
		}
		case XR_GRAPHICS_API_D3D11_4: {
			printf("Creating D3D11 Swap\n");

			ID3D11Device5* device5 = pSession->binding.d3d11.device5;
			for (int i = 0; i < XR_SWAP_COUNT; ++i) {
				DX_CHECK(ID3D11Device5_OpenSharedResource1(device5, colorHandles[i], &IID_ID3D11Texture2D, (void**)&pSwapchain->color[i].d3d11.texture));
				printf("Imported d3d11 swap texture. Device: %p Handle: %p Texture: %p\n", device5, colorHandles[i], pSwapchain->color[i].d3d11.texture);
//				D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {
//					.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
//					.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D,
//					.Texture2D.MipSlice = 0,
//				};
//				DX_CHECK(ID3D11Device5_CreateRenderTargetView(device5, (ID3D11Resource*)pSwapchain->color[i].d3d11.texture, &rtvDesc, &pSwapchain->color[i].d3d11.rtView));
//				DX_CHECK(ID3D11Texture2D_QueryInterface(pSwapchain->color[i].d3d11.texture, &IID_IDXGIKeyedMutex, (void**)&pSwapchain->color[i].d3d11.keyedMutex));
//				IDXGIKeyedMutex_AcquireSync(pSwapchain->color[i].d3d11.keyedMutex, 0, INFINITE);
			}

			break;
		}
		case XR_GRAPHICS_API_VULKAN:
		default:
			return XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED;
	}

	*swapchain = (XrSwapchain)pSwapchain;
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySwapchain(
	XrSwapchain swapchain)
{
	LOG_METHOD(xrDestroySwapchain);
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrDestroySwapchain\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateSwapchainImages(
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

XRAPI_ATTR XrResult XRAPI_CALL xrAcquireSwapchainImage(
	XrSwapchain                        swapchain,
	const XrSwapchainImageAcquireInfo* acquireInfo,
	uint32_t*                          index)
{
	LOG_METHOD_ONCE(xrAcquireSwapchainImage);
	assert(acquireInfo->next == NULL);

	Swapchain* pSwapchain = (Swapchain*)swapchain;
	Session*   pSession = (Session*)pSwapchain->session;
	Instance*  pInstance = (Instance*)pSession->instance;
	XrHandle   sessionHandle = GetHandle(pInstance->sessions, pSession);

	xrClaimSwapPoolImage(sessionHandle, &pSwapchain->swapIndex);

	*index = pSwapchain->swapIndex;

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrWaitSwapchainImage(
	XrSwapchain                     swapchain,
	const XrSwapchainImageWaitInfo* waitInfo)
{
	LOG_METHOD_ONCE(xrWaitSwapchainImage);
	assert(waitInfo->next == NULL);

	Swapchain* pSwapchain = (Swapchain*)swapchain;
	Session*   pSession = (Session*)pSwapchain->session;
	Instance*  pInstance = (Instance*)pSession->instance;
	XrHandle   sessionHandle = GetHandle(pInstance->sessions, pSession);

	ID3D11DeviceContext4*   context4 = pSession->binding.d3d11.context4;

//	IDXGIKeyedMutex* keyedMutex = pSwapchain->color[pSwapchain->swapIndex].d3d11.keyedMutex;
//
//	ID3D11DeviceContext4_Flush(context4);
//	IDXGIKeyedMutex_ReleaseSync(keyedMutex, 0);

//	ID3D11RenderTargetView* rtView = pSwapchain->color[pSwapchain->swapIndex].d3d11.rtView;
//	ID3D11DeviceContext4_OMSetRenderTargets(context4, 1, &rtView, NULL);
//	ID3D11DeviceContext4_ClearRenderTargetView(context4, rtView, (float[]){0.0f, 0.0f, 0.0f, 0.0f});

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrReleaseSwapchainImage(
	XrSwapchain                        swapchain,
	const XrSwapchainImageReleaseInfo* releaseInfo)
{
	LOG_METHOD_ONCE(xrReleaseSwapchainImage);
	assert(releaseInfo->next == NULL);

	Swapchain* pSwapchain = (Swapchain*)swapchain;
	Session*   pSession = (Session*)pSwapchain->session;
	Instance*  pInstance = (Instance*)pSession->instance;
	XrHandle   sessionHandle = GetHandle(pInstance->sessions, pSession);

	xrReleaseSwapPoolImage(sessionHandle, pSwapchain->swapIndex);

	ID3D11DeviceContext4*   context4 = pSession->binding.d3d11.context4;

//	IDXGIKeyedMutex* keyedMutex = pSwapchain->color[pSwapchain->swapIndex].d3d11.keyedMutex;
//
//	ID3D11DeviceContext4_Flush(context4);
//	IDXGIKeyedMutex_AcquireSync(keyedMutex, 0, INFINITE);

//	ID3D11RenderTargetView* nullRTView[] = {NULL};
//	ID3D11DeviceContext4_OMSetRenderTargets(context4, 1, nullRTView, NULL);


	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrBeginSession(
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

	Instance* pInstance = (Instance*)pSession->instance;
	XrHandle  sessionHandle = GetHandle(pInstance->sessions, pSession);

//	switch (pInstance->graphicsApi) {
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

XRAPI_ATTR XrResult XRAPI_CALL xrEndSession(
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

XRAPI_ATTR XrResult XRAPI_CALL xrRequestExitSession(
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

XRAPI_ATTR XrResult XRAPI_CALL xrWaitFrame(
	XrSession              session,
	const XrFrameWaitInfo* frameWaitInfo,
	XrFrameState*          frameState)
{
	LOG_METHOD_ONCE(xrWaitFrame);

	if (frameWaitInfo != NULL)
		assert(frameWaitInfo->next == NULL);

	Session* pSession = (Session*)session;
	if (!pSession->running) {
		LOG_ERROR("XR_ERROR_SESSION_NOT_RUNNING\n");
		return XR_ERROR_SESSION_NOT_RUNNING;
	}

	XrTimeWaitWin32(&pSession->frameBegan, pSession->frameWaited);

	Instance* pInstance = (Instance*)pSession->instance;
	XrHandle  sessionHandle = GetHandle(pInstance->sessions, pSession);

	if (pSession->lastDisplayTime == 0) {
		uint64_t initialTimelineValue;
		switch (pInstance->graphicsApi) {
			case XR_GRAPHICS_API_D3D11_4:
				initialTimelineValue = ID3D11Fence_GetCompletedValue(pSession->binding.d3d11.compositorFence);
				break;
			default:
				LOG_ERROR("Graphics API not supported.\n");
				return XR_ERROR_RUNTIME_FAILURE;
		}
		xrSetInitialCompositorTimelineValue(sessionHandle, initialTimelineValue);
	}

	bool synchronized = pSession->activeSessionState >= XR_SESSION_STATE_SYNCHRONIZED;
	uint64_t compositorTimelineValue;
	xrGetCompositorTimelineValue(sessionHandle, synchronized, &compositorTimelineValue);

	switch (pInstance->graphicsApi) {
		case XR_GRAPHICS_API_OPENGL:    break;
		case XR_GRAPHICS_API_OPENGL_ES: break;
		case XR_GRAPHICS_API_VULKAN:    break;
		case XR_GRAPHICS_API_D3D11_4:   {
			ID3D11DeviceContext4* context4 = pSession->binding.d3d11.context4;
			ID3D11Fence*          compositorFence = pSession->binding.d3d11.compositorFence;

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
	XrTime lastFrameTime = pSession->lastDisplayTime == 0 ? currentTime : pSession->lastDisplayTime;
	XrTime frameInterval = xrGetFrameInterval(sessionHandle, synchronized);

	frameState->predictedDisplayPeriod = frameInterval;
	frameState->predictedDisplayTime = lastFrameTime + frameInterval;
	frameState->shouldRender = pSession->activeSessionState == XR_SESSION_STATE_VISIBLE ||
							   pSession->activeSessionState == XR_SESSION_STATE_FOCUSED;

	memmove(&pSession->predictedDisplayTimes[1], &pSession->predictedDisplayTimes[0], sizeof(pSession->predictedDisplayTimes[0]) * (COUNT(pSession->predictedDisplayTimes) - 1));
	pSession->predictedDisplayTimes[0] = frameState->predictedDisplayTime;
	pSession->lastBeginDisplayTime = currentTime;


//	double frameIntervalMs = xrTimeToMilliseconds(frameInterval);
//	double fps = 1.0 / MillisecondsToSeconds(frameIntervalMs);
//	printf("xrWaitFrame frameIntervalMs: %f fps: %f\n", frameIntervalMs, fps);

	__atomic_add_fetch(&pSession->frameWaited, 1, __ATOMIC_RELEASE);

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrBeginFrame(
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

XRAPI_ATTR XrResult XRAPI_CALL xrEndFrame(
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
				XrCompositionLayerProjection* pProjectionLayer = (XrCompositionLayerProjection*)frameEndInfo->layers[layer];
				assert(pProjectionLayer->viewCount == 2);

				for (int view = 0; view < pProjectionLayer->viewCount; ++view) {
					switch (pProjectionLayer->views[layer].type) {
						case XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW: {
							break;
						}
						default: {
							LOG_ERROR("Unknown Composition Layer View %d", pProjectionLayer->views[layer].type);
							break;
						}
					}
				}

				break;
			}
			case XR_TYPE_COMPOSITION_LAYER_QUAD: {
				printf("XR_TYPE_COMPOSITION_LAYER_QUAD");
			}
			default: {
				LOG_ERROR("Unknown Composition layer %d", frameEndInfo->layers[layer]->type);
				break;
			}
		}
	}

	Instance* pInstance = (Instance*)pSession->instance;
	XrHandle  sessionHandle = GetHandle(pInstance->sessions, pSession);
	bool      synchronized = pSession->activeSessionState >= XR_SESSION_STATE_SYNCHRONIZED;

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

		xrSetSessionTimelineValue(sessionHandle, sessionTimelineValue);

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
		XrTime frameInterval = xrGetFrameInterval(sessionHandle, synchronized);

		if (discrepancy > 0) {
//			printf("Frame took %f ms longer than predicted.\n", discrepancyMs);
			pSession->synchronizedFrameCount = 0;

			uint64_t initialTimelineValue;
			switch (pInstance->graphicsApi) {
				case XR_GRAPHICS_API_D3D11_4:
					initialTimelineValue = ID3D11Fence_GetCompletedValue(pSession->binding.d3d11.compositorFence);
					break;
				default:
					LOG_ERROR("Graphics API not supported.\n");
					return XR_ERROR_RUNTIME_FAILURE;
			}
			xrSetInitialCompositorTimelineValue(sessionHandle, initialTimelineValue);

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

		xrProgressCompositorTimelineValue(sessionHandle, 0, synchronized);
		XrTimeSignalWin32(&pSession->frameEnded, pSession->frameBegan);
	}

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrLocateViews(
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
	Instance* pInstance = (Instance*)pSession->instance;
	XrHandle  sessionHandle = GetHandle(pInstance->sessions, pSession);

	for (int i = 0; i < viewCapacityInput; ++i) {
		xrGetEyeView(sessionHandle, i, &views[i]);

		// Some OXR implementations (Unity) cannot properly calculate the width and height of the projection matrices
		// unless all the angles are negative.
		// I probably don't wna to do this, I probably want to use symmetrical angles with a render mask
		views[i].fov.angleUp -= views[i].fov.angleUp > 0 ? PI * 2 : 0;
		views[i].fov.angleDown -= views[i].fov.angleDown > 0 ? PI * 2 : 0;
		views[i].fov.angleLeft -= views[i].fov.angleLeft > 0 ? PI * 2 : 0;
		views[i].fov.angleRight -= views[i].fov.angleLeft > 0 ? PI * 2 : 0;
	}

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrStringToPath(
	XrInstance  instance,
	const char* pathString,
	XrPath*     path)
{
	//	LOG_METHOD(xrStringToPath);

	Instance* pInstance = (Instance*)instance;

	int pathHash = CalcDJB2(pathString, XR_MAX_PATH_LENGTH);
	for (int i = 0; i < pInstance->paths.count; ++i) {
		if (pInstance->paths.hash[i] != pathHash)
			continue;
		if (strncmp(pInstance->paths.data[i].string, pathString, XR_MAX_PATH_LENGTH)) {
			LOG_ERROR("Path Hash Collision! %s | %s\n", pInstance->paths.data[i].string, pathString);
			return XR_ERROR_PATH_INVALID;
		}
		*path = (XrPath)&pInstance->paths.data[i];
		//		printf("Path Handle Found: %d\n    %s\n", i, pInstance->paths.data[i].string);
		return XR_SUCCESS;
	}

	Path* pPath;
	XR_CHECK(ClaimPath(&pInstance->paths, &pPath, pathHash));
	XrHandle pathHandle = GetHandle(pInstance->paths, pPath);
	//	printf("Path Handle Claimed: %d\n    %s\n", pathHandle, pathString);

	strncpy(pPath->string, pathString, XR_MAX_PATH_LENGTH);
	pPath->string[XR_MAX_PATH_LENGTH - 1] = '\0';
	*path = (XrPath)pPath;

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrPathToString(
	XrInstance instance,
	XrPath     path,
	uint32_t   bufferCapacityInput,
	uint32_t*  bufferCountOutput,
	char*      buffer)
{
	//	LOG_METHOD(xrPathToString);

	Instance* pInstance = (Instance*)instance;  // check its in instance ?
	Path*     pPath = (Path*)path;

	strncpy(buffer, pPath->string, bufferCapacityInput < XR_MAX_PATH_LENGTH ? bufferCapacityInput : XR_MAX_PATH_LENGTH);
	*bufferCountOutput = strlen(buffer);

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateActionSet(
	XrInstance                   instance,
	const XrActionSetCreateInfo* createInfo,
	XrActionSet*                 actionSet)
{
#ifdef ENABLE_DEBUG_LOG_METHOD
//	LOG_METHOD(xrCreateActionSet);
//	printf("%lu:%lu: xrCreateActionSet %s %s\n", GetCurrentProcessId(), GetCurrentThreadId(), createInfo->actionSetName, createInfo->localizedActionSetName);
#endif
	assert(createInfo->next == NULL);

	Instance* pInstance = (Instance*)instance;

	XrHash     actionSetNameHash = CalcDJB2(createInfo->actionSetName, XR_MAX_ACTION_SET_NAME_SIZE);
	ActionSet* pActionSet;
	XR_CHECK(ClaimActionSet(&pInstance->actionSets, &pActionSet, actionSetNameHash));

	pActionSet->priority = createInfo->priority;
	strncpy((char*)&pActionSet->actionSetName, (const char*)&createInfo->actionSetName, XR_MAX_ACTION_SET_NAME_SIZE);
	*actionSet = (XrActionSet)pActionSet;

	printf("Created ActionSet with %s\n", createInfo->actionSetName);

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroyActionSet(
	XrActionSet actionSet)
{
	LOG_METHOD(xrDestroyActionSet);
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrDestroyActionSet\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateAction(
	XrActionSet               actionSet,
	const XrActionCreateInfo* createInfo,
	XrAction*                 action)
{
#ifdef ENABLE_DEBUG_LOG_METHOD
//	LOG_METHOD(xrCreateAction);
//	printf("%lu:%lu: xrCreateAction %s %s\n", GetCurrentProcessId(), GetCurrentThreadId(), createInfo->actionName, createInfo->localizedActionName);
#endif
	assert(createInfo->next == NULL);

	ActionSet* pActionSet = (ActionSet*)actionSet;

	if (pActionSet->attachedToSession != NULL) {
		LOG_ERROR("XR_ERROR_ACTIONSETS_ALREADY_ATTACHED");
		return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
	}
	if (createInfo->countSubactionPaths > XR_MAX_SUBACTION_PATHS) {
		LOG_ERROR("XR_ERROR_PATH_COUNT_EXCEEDED");
		return XR_ERROR_PATH_COUNT_EXCEEDED;
	}

	Action* pAction;
	ClaimHandle(pActionSet->Actions, pAction);

	pAction->actionSet = actionSet;
	pAction->countSubactionPaths = createInfo->countSubactionPaths;

	memcpy(pAction->subactionPaths, createInfo->subactionPaths, pAction->countSubactionPaths * sizeof(XrPath));
	pAction->actionType = createInfo->actionType;
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

XRAPI_ATTR XrResult XRAPI_CALL xrDestroyAction(
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
XRAPI_ATTR XrResult XRAPI_CALL xrSuggestInteractionProfileBindings(
	XrInstance                                  instance,
	const XrInteractionProfileSuggestedBinding* suggestedBindings)
{
	LOG_METHOD(xrSuggestInteractionProfileBindings);
	LogNextChain(suggestedBindings->next);

	Instance*           pInstance = (Instance*)instance;
	Path*               pInteractionProfilePath = (Path*)suggestedBindings->interactionProfile;
	XrHash              interactionProfilePathHash = GetPathHash(&pInstance->paths, pInteractionProfilePath);
	InteractionProfile* pInteractionProfile = GetInteractionProfileByHash(&pInstance->interactionProfiles, interactionProfilePathHash);
	if (pInteractionProfile == NULL) {
		LOG_ERROR("Could not find interaction profile binding set! %s %d\n", pInteractionProfilePath->string);
		return XR_ERROR_PATH_UNSUPPORTED;
	}

	printf("Binding: %s\n", pInteractionProfilePath->string);

	const XrActionSuggestedBinding* pSuggestedBindings = suggestedBindings->suggestedBindings;
	for (int i = 0; i < suggestedBindings->countSuggestedBindings; ++i) {
		Action*    pAction = (Action*)pSuggestedBindings[i].action;
		ActionSet* pActionSet = (ActionSet*)pAction->actionSet;
		if (pActionSet->attachedToSession != NULL)
			return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
	}

	for (int i = 0; i < suggestedBindings->countSuggestedBindings; ++i) {
		Action* pBindingAction = (Action*)pSuggestedBindings[i].action;
		Path*   pBindingPath = (Path*)pSuggestedBindings[i].binding;
		// if the pointers became 32 bit int handle, I could just use that as a hash?
		XrHash   bindingPathHash = GetPathHash(&pInstance->paths, pBindingPath);
		Binding* pBinding = GetBindingByHash(&pInteractionProfile->bindings, bindingPathHash);

		ActionSet* pBindingActionSet = (ActionSet*)pBindingAction->actionSet;

		if (pBindingAction->countSubactionPaths == 0) {
			XrBinding* pActionBinding;
			ClaimHandle(pBindingAction->subactionBindings[0], pActionBinding);
			*pActionBinding = pBinding;

//			printf("Bound Action: %s BindingPath: %s No Subaction\n", pBindingAction->actionName, pBindingPath->string);
			continue;
		}

		for (int subactionIndex = 0; subactionIndex < pBindingAction->countSubactionPaths; ++subactionIndex) {
			Path* pActionSubPath = (Path*)pBindingAction->subactionPaths[subactionIndex];
			if (!CompareSubPath(pActionSubPath->string, pBindingPath->string))
				continue;

			XrBinding* pActionBinding;
			ClaimHandle(pBindingAction->subactionBindings[subactionIndex], pActionBinding);
			*pActionBinding = pBinding;

//			printf("Bound Action: %s BindingPath: %s Subaction Index: %d %s\n", pBindingAction->actionName, pBindingPath->string, subactionIndex, pActionSubPath->string);
		}
	}

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrAttachSessionActionSets(
	XrSession                            session,
	const XrSessionActionSetsAttachInfo* attachInfo)
{
	LOG_METHOD(xrAttachSessionActionSets);
	LogNextChain(attachInfo->next);

	Session* pSession = (Session*)session;

	if (pSession->actionSetStates.count != 0)
		return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;

	Instance* pInstance = (Instance*)pSession->instance;
	for (int i = 0; i < attachInfo->countActionSets; ++i) {
		ActionSet* pAttachingActionSet = (ActionSet*)attachInfo->actionSets[i];
		XrHash     attachingActionSetHash = GetActionSetHash(&pInstance->actionSets, pAttachingActionSet);

		ActionSetState* pClaimedActionSetState;
		XR_CHECK(ClaimActionSetState(&pSession->actionSetStates, &pClaimedActionSetState, attachingActionSetHash));

//		pClaimedActionSetState->actionStates.slots = pAttachingActionSet->Actions.slots;
		pClaimedActionSetState->actionSet = (XrActionSet)pAttachingActionSet;

		pAttachingActionSet->attachedToSession = session;

		printf("Attached ActionSet %s\n", pAttachingActionSet->actionSetName);
	}

	if (pSession->pendingInteractionProfile == NULL) {
		printf("Setting default interaction profile: %s\n", XR_DEFAULT_INTERACTION_PROFILE);
		XrPath interactionProfilePath;
		xrStringToPath(pSession->instance, XR_DEFAULT_INTERACTION_PROFILE, &interactionProfilePath);
		XrHash              interactionProfilePathHash = GetPathHash(&pInstance->paths, (Path*)interactionProfilePath);
		InteractionProfile* pInteractionProfile = GetInteractionProfileByHash(&pInstance->interactionProfiles, interactionProfilePathHash);
		pSession->pendingInteractionProfile = pInteractionProfile;
	}

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetCurrentInteractionProfile(
	XrSession                  session,
	XrPath                     topLevelUserPath,
	XrInteractionProfileState* interactionProfile)
{
	assert(interactionProfile->next == NULL);

	Path* pPath = (Path*)topLevelUserPath;
	printf("xrGetCurrentInteractionProfile %s\n", pPath->string);

	Session*            pSession = (Session*)session;
	// application might set interaction profile then immediately call thigs without letting xrPollEvent update activeInteractionProfile
	InteractionProfile* pInteractionProfile = pSession->activeInteractionProfile != NULL ?
												  pSession->activeInteractionProfile :
												  pSession->pendingInteractionProfile;
	interactionProfile->interactionProfile = pInteractionProfile->path;

	Path* pInteractionProfilePath = (Path*)pInteractionProfile->path;
	printf("Found InteractionProfile %s\n", pInteractionProfilePath->string);

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateBoolean(
	XrSession                   session,
	const XrActionStateGetInfo* getInfo,
	XrActionStateBoolean*       state)
{
	LOG_METHOD_ONCE(xrGetActionStateBoolean);
	LogNextChain(getInfo->next);

	Action*    pGetAction = (Action*)getInfo->action;
	Path*      pGetActionSubactionPath = (Path*)getInfo->subactionPath;
	ActionSet* pGetActionSet = (ActionSet*)pGetAction->actionSet;

//	LOG_METHOD_LOOP("Action: %s\n", pGetAction->actionName);

	//	state->lastChangeTime = GetXrTime();
	//	state->changedSinceLastSync = true;
	//	state->currentState = true;
	//	state->isActive = true;

//	LOG_ERROR("XR_ERROR_PATH_UNSUPPORTED\n");
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateFloat(
	XrSession                   session,
	const XrActionStateGetInfo* getInfo,
	XrActionStateFloat*         state)
{
	LOG_METHOD_ONCE(xrGetActionStateFloat);
	LogNextChain(getInfo->next);

	Action*    pGetAction = (Action*)getInfo->action;
	Path*      pGetActionSubactionPath = (Path*)getInfo->subactionPath;
	ActionSet* pGetActionSet = (ActionSet*)pGetAction->actionSet;

//	printf("Action: %s\n", pGetAction->actionName);

	Session*        pSession = (Session*)session;
	Instance*       pInstance = (Instance*)pSession->instance;
	XrHash          getActionSetHash = GetActionSetHash(&pInstance->actionSets, pGetActionSet);
	ActionSetState* pGetActionSetState = GetActionSetStateByHash(&pSession->actionSetStates, getActionSetHash);

	XrHandle     getActionHandle = GetHandle(pGetActionSet->Actions, pGetAction);
	ActionState* pGetActionState = &pGetActionSetState->states[getActionHandle];

	if (pGetActionSubactionPath == NULL) {
		memcpy(&state->currentState, &pGetActionState->subactionStates[0].currentState, sizeof(ActionState));
		pGetActionState->subactionStates[0].changedSinceLastSync = false;

		return XR_SUCCESS;
	}

	for (int i = 0; i < pGetAction->countSubactionPaths; ++i) {
		const Path* pSubPath = (Path*)pGetAction->subactionPaths[i];
		if (pGetActionSubactionPath == pSubPath) {
			memcpy(&state->currentState, &pGetActionState->subactionStates[i].currentState, sizeof(ActionState));
			pGetActionState->subactionStates[i].changedSinceLastSync = false;

			return XR_SUCCESS;
		}
	}

	LOG_ERROR("XR_ERROR_PATH_UNSUPPORTED\n");
	return XR_ERROR_PATH_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateVector2f(
	XrSession                   session,
	const XrActionStateGetInfo* getInfo,
	XrActionStateVector2f*      state)
{
	LOG_METHOD(xrGetActionStateVector2f);
	LOG_ERROR("XR_ERROR_PATH_UNSUPPORTED\n");
	return XR_ERROR_PATH_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStatePose(
	XrSession                   session,
	const XrActionStateGetInfo* getInfo,
	XrActionStatePose*          state)
{
	LOG_METHOD(xrGetActionStatePose);
	LOG_ERROR("XR_ERROR_PATH_UNSUPPORTED\n");
	return XR_ERROR_PATH_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrSyncActions(
	XrSession                session,
	const XrActionsSyncInfo* syncInfo)
{
	LOG_METHOD_ONCE(xrSyncActions);
	LogNextChain(syncInfo->next);

	Session* pSession = (Session*)session;

	if (pSession->activeSessionState != XR_SESSION_STATE_FOCUSED)
		return XR_SESSION_NOT_FOCUSED;

	// short ciruit for now
	return XR_SUCCESS;

	Instance* pInstance = (Instance*)pSession->instance;
	XrTime    currentTime = xrGetTime();

	for (int sessionIndex = 0; sessionIndex < pSession->actionSetStates.count; ++sessionIndex) {
		ActionSet*      pActionSet = (ActionSet*)syncInfo->activeActionSets[sessionIndex].actionSet;
		XrHash          actionSetHash = GetActionSetHash(&pInstance->actionSets, pActionSet);
		ActionSetState* pActionSetState = GetActionSetStateByHash(&pSession->actionSetStates, actionSetHash);

		if (pActionSetState == NULL)
			continue;

		for (int actionIndex = 0; actionIndex < COUNT(pActionSet->Actions.slots); ++actionIndex) {
			Action*      pAction = &pActionSet->Actions.data[actionIndex];
			ActionState* pActionState = &pActionSetState->states[actionIndex];

			for (int subActionIndex = 0; subActionIndex < pAction->countSubactionPaths; ++subActionIndex) {
				XrBindingPool*  pSubactionBindingPool = &pAction->subactionBindings[subActionIndex];
				SubactionState* pSubactionState = &pActionState->subactionStates[subActionIndex];

				if (pSubactionState->lastSyncedPriority > pActionSet->priority &&
					pSubactionState->lastChangeTime == currentTime)
					continue;

				const float lastState = pSubactionState->currentState;

				// We are going to want to keep a 'max count'
				for (int bindingIndex = 0; bindingIndex < COUNT(pSubactionBindingPool->slots); ++bindingIndex) {
					if (!pSubactionBindingPool->slots[bindingIndex].occupied)
						continue;

					Binding* pBinding = (Binding*)pSubactionBindingPool->data[bindingIndex];
					pBinding->func(pActionState);
					pSubactionState->lastSyncedPriority = pActionSet->priority;
					pSubactionState->lastChangeTime = currentTime;
					pSubactionState->isActive = XR_TRUE;
					pSubactionState->changedSinceLastSync = pSubactionState->currentState != lastState;
				}
			}
		}
	}

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateBoundSourcesForAction(
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

XRAPI_ATTR XrResult XRAPI_CALL xrGetInputSourceLocalizedName(
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

XRAPI_ATTR XrResult XRAPI_CALL xrApplyHapticFeedback(
	XrSession                 session,
	const XrHapticActionInfo* hapticActionInfo,
	const XrHapticBaseHeader* hapticFeedback)
{
	LOG_METHOD(xrApplyHapticFeedback);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrApplyHapticFeedback\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrStopHapticFeedback(
	XrSession                 session,
	const XrHapticActionInfo* hapticActionInfo)
{
	LOG_METHOD(xrStopHapticFeedback);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrStopHapticFeedback\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetVisibilityMaskKHR(
	XrSession               session,
	XrViewConfigurationType viewConfigurationType,
	uint32_t                viewIndex,
	XrVisibilityMaskTypeKHR visibilityMaskType,
	XrVisibilityMaskKHR*    visibilityMask)
{
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrGetVisibilityMaskKHR\n");
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrSetInputDeviceActiveEXT(
	XrSession session,
	XrPath    interactionProfile,
	XrPath    topLevelPath,
	XrBool32  isActive)
{
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSetInputDeviceActiveEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrSetInputDeviceStateBoolEXT(
	XrSession session,
	XrPath    topLevelPath,
	XrPath    inputSourcePath,
	XrBool32  state)
{
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSetInputDeviceActiveEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrSetInputDeviceStateFloatEXT(
	XrSession session,
	XrPath    topLevelPath,
	XrPath    inputSourcePath,
	float     state)
{
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSetInputDeviceStateFloatEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrSetInputDeviceStateVector2fEXT(
	XrSession  session,
	XrPath     topLevelPath,
	XrPath     inputSourcePath,
	XrVector2f state)
{
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSetInputDeviceStateVector2fEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrSetInputDeviceLocationEXT(
	XrSession session,
	XrPath    topLevelPath,
	XrPath    inputSourcePath,
	XrSpace   space,
	XrPosef   pose)
{
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSetInputDeviceLocationEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumeratePerformanceMetricsCounterPathsMETA(
	XrInstance instance,
	uint32_t   counterPathCapacityInput,
	uint32_t*  counterPathCountOutput,
	XrPath*    counterPaths)
{
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrEnumeratePerformanceMetricsCounterPathsMETA\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrSetPerformanceMetricsStateMETA(
	XrSession                            session,
	const XrPerformanceMetricsStateMETA* state)
{
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSetPerformanceMetricsStateMETA\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetPerformanceMetricsStateMETA(
	XrSession                      session,
	XrPerformanceMetricsStateMETA* state)
{
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrGetPerformanceMetricsStateMETA\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrQueryPerformanceMetricsCounterMETA(
	XrSession                        session,
	XrPath                           counterPath,
	XrPerformanceMetricsCounterMETA* counter)
{
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrQueryPerformanceMetricsCounterMETA\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrPerfSettingsSetPerformanceLevelEXT(
	XrSession               session,
	XrPerfSettingsDomainEXT domain,
	XrPerfSettingsLevelEXT  level)
{
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrPerfSettingsSetPerformanceLevelEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrSetDebugUtilsObjectNameEXT(
	XrInstance                           instance,
	const XrDebugUtilsObjectNameInfoEXT* nameInfo)
{
	LOG_METHOD(xrSetDebugUtilsObjectNameEXT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSetDebugUtilsObjectNameEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateDebugUtilsMessengerEXT(
	XrInstance                                instance,
	const XrDebugUtilsMessengerCreateInfoEXT* createInfo,
	XrDebugUtilsMessengerEXT*                 messenger)
{
	LOG_METHOD(xrCreateDebugUtilsMessengerEXT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrCreateDebugUtilsMessengerEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroyDebugUtilsMessengerEXT(
	XrDebugUtilsMessengerEXT messenger)
{
	LOG_METHOD(xrDestroyDebugUtilsMessengerEXT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrDestroyDebugUtilsMessengerEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrSubmitDebugUtilsMessageEXT(
	XrInstance                                  instance,
	XrDebugUtilsMessageSeverityFlagsEXT         messageSeverity,
	XrDebugUtilsMessageTypeFlagsEXT             messageTypes,
	const XrDebugUtilsMessengerCallbackDataEXT* callbackData)
{
	LOG_METHOD(xrSubmitDebugUtilsMessageEXT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSubmitDebugUtilsMessageEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrSessionBeginDebugUtilsLabelRegionEXT(
	XrSession                   session,
	const XrDebugUtilsLabelEXT* labelInfo)
{
	LOG_METHOD(xrSessionBeginDebugUtilsLabelRegionEXT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSessionBeginDebugUtilsLabelRegionEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrSessionEndDebugUtilsLabelRegionEXT(
	XrSession session)
{
	LOG_METHOD(xrSessionEndDebugUtilsLabelRegionEXT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSessionEndDebugUtilsLabelRegionEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrSessionInsertDebugUtilsLabelEXT(
	XrSession                   session,
	const XrDebugUtilsLabelEXT* labelInfo)
{
	LOG_METHOD(xrSessionInsertDebugUtilsLabelEXT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrSessionInsertDebugUtilsLabelEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetOpenGLGraphicsRequirementsKHR(
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

XRAPI_ATTR XrResult XRAPI_CALL xrGetD3D11GraphicsRequirementsKHR(
	XrInstance                      instance,
	XrSystemId                      systemId,
	XrGraphicsRequirementsD3D11KHR* graphicsRequirements)
{
	LOG_METHOD(xrGetD3D11GraphicsRequirementsKHR);
	LogNextChain(graphicsRequirements->next);

	Instance* pInstance = (Instance*)instance;

	if (pInstance->systemId != systemId) {
		LOG_ERROR("Invalid System ID.");
		return XR_ERROR_SYSTEM_INVALID;
	}

	if (pInstance->graphics.d3d11.adapterLuid.LowPart != 0) {
		graphicsRequirements->adapterLuid = pInstance->graphics.d3d11.adapterLuid;
		graphicsRequirements->minFeatureLevel = pInstance->graphics.d3d11.minFeatureLevel;
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

	pInstance->graphics.d3d11.adapterLuid = graphicsRequirements->adapterLuid;
	pInstance->graphics.d3d11.minFeatureLevel = graphicsRequirements->minFeatureLevel;

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateHandTrackerEXT(
	XrSession                         session,
	const XrHandTrackerCreateInfoEXT* createInfo,
	XrHandTrackerEXT*                 handTracker)
{
	LOG_METHOD(xrCreateHandTrackerEXT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrCreateHandTrackerEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroyHandTrackerEXT(
	XrHandTrackerEXT handTracker)
{
	LOG_METHOD(xrDestroyHandTrackerEXT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrDestroyHandTrackerEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrLocateHandJointsEXT(
	XrHandTrackerEXT                 handTracker,
	const XrHandJointsLocateInfoEXT* locateInfo,
	XrHandJointLocationsEXT*         locations)
{
	LOG_METHOD(xrLocateHandJointsEXT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrLocateHandJointsEXT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSpatialAnchorMSFT(
	XrSession                            session,
	const XrSpatialAnchorCreateInfoMSFT* createInfo,
	XrSpatialAnchorMSFT*                 anchor)
{
	LOG_METHOD(xrCreateSpatialAnchorMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrCreateSpatialAnchorMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSpatialAnchorSpaceMSFT(
	XrSession                                 session,
	const XrSpatialAnchorSpaceCreateInfoMSFT* createInfo,
	XrSpace*                                  space)
{
	LOG_METHOD(xrCreateSpatialAnchorSpaceMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrCreateSpatialAnchorSpaceMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySpatialAnchorMSFT(
	XrSpatialAnchorMSFT anchor)
{
	LOG_METHOD(xrDestroySpatialAnchorMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrDestroySpatialAnchorMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateSceneComputeFeaturesMSFT(
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

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSceneObserverMSFT(
	XrSession                            session,
	const XrSceneObserverCreateInfoMSFT* createInfo,
	XrSceneObserverMSFT*                 sceneObserver)
{
	LOG_METHOD(xrCreateSceneObserverMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrCreateSceneObserverMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySceneObserverMSFT(
	XrSceneObserverMSFT sceneObserver)
{
	LOG_METHOD(xrDestroySceneObserverMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrDestroySceneObserverMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSceneMSFT(
	XrSceneObserverMSFT          sceneObserver,
	const XrSceneCreateInfoMSFT* createInfo,
	XrSceneMSFT*                 scene)
{
	LOG_METHOD(xrCreateSceneMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrCreateSceneMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySceneMSFT(
	XrSceneMSFT scene)
{
	LOG_METHOD(xrDestroySceneMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrDestroySceneMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrComputeNewSceneMSFT(
	XrSceneObserverMSFT              sceneObserver,
	const XrNewSceneComputeInfoMSFT* computeInfo)
{
	LOG_METHOD(xrComputeNewSceneMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrComputeNewSceneMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetSceneComputeStateMSFT(
	XrSceneObserverMSFT      sceneObserver,
	XrSceneComputeStateMSFT* state)
{
	LOG_METHOD(xrGetSceneComputeStateMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrGetSceneComputeStateMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetSceneComponentsMSFT(
	XrSceneMSFT                         scene,
	const XrSceneComponentsGetInfoMSFT* getInfo,
	XrSceneComponentsMSFT*              components)
{
	LOG_METHOD(xrGetSceneComponentsMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrGetSceneComponentsMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrLocateSceneComponentsMSFT(
	XrSceneMSFT                            scene,
	const XrSceneComponentsLocateInfoMSFT* locateInfo,
	XrSceneComponentLocationsMSFT*         locations)
{
	LOG_METHOD(xrLocateSceneComponentsMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrLocateSceneComponentsMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetSceneMeshBuffersMSFT(
	XrSceneMSFT                          scene,
	const XrSceneMeshBuffersGetInfoMSFT* getInfo,
	XrSceneMeshBuffersMSFT*              buffers)
{
	LOG_METHOD(xrGetSceneMeshBuffersMSFT);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrGetSceneMeshBuffersMSFT\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrConvertWin32PerformanceCounterToTimeKHR(
	XrInstance           instance,
	const LARGE_INTEGER* performanceCounter,
	XrTime*              time)
{
	LOG_METHOD(xrConvertWin32PerformanceCounterToTimeKHR);

	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrConvertWin32PerformanceCounterToTimeKHR\n");
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(
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

//	LOG_ERROR("xrGetInstanceProcAddr XR_ERROR_FUNCTION_UNSUPPORTED %s\n", name);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult EXPORT XRAPI_CALL xrNegotiateLoaderRuntimeInterface(
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