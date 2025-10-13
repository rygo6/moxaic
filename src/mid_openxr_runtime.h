#ifndef MID_OPENXR_RUNTIME_H
#define MID_OPENXR_RUNTIME_H

/*
 * Mid OpenXR Runtime Header
 */

/*
	Style

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

 	global fields should be an anonymous struct to namespace

 	auto can be used if the name of the type is obvious from somewhere else in the assignment line
 	auto should be used if the exact name of the type is elsewhere in the assignment line, such as compound literals
 	if the type name is 8 or less chars

 	typedef of abbreviated type names should be used if possible
 	abbreviations should be 8 or less chars
*/

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

#include "mid_common.h"
#include "mid_bit.h"
#include "mid_block.h"
#include "mid_qring.h"

#ifdef MID_IDE_ANALYSIS
#undef XRAPI_CALL
#define XRAPI_CALL
#endif

#define EXPORT __attribute__((visibility("default")))

/*
 * Mid OpenXR Types
 */

typedef enum SystemId {
	SYSTEM_ID_HANDHELD_AR = 1,
	SYSTEM_ID_HMD_VR_STEREO = 2,
	SYSTEM_ID_COUNT,
} SystemId;

typedef enum XrViewId {
	// XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT
	XR_VIEW_ID_CENTER_FIRST_PERSON = 0,
	XR_VIEW_ID_FIRST_PERSON_COUNT  = 0,

	// XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO
	XR_VIEW_ID_CENTER_MONO         = 0,
	XR_VIEW_ID_MONO_COUNT          = 1,

	// XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO
	XR_VIEW_ID_LEFT_STEREO         = 0,
	XR_VIEW_ID_RIGHT_STEREO        = 1,
	XR_VIEW_ID_STEREO_COUNT        = 2,

	// XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET
	XR_VIEW_ID_LEFT_OUTER_FOVEATED = 0,
	XR_VIEW_ID_RIGHT_OUTER_FOVEATED= 1,
	XR_VIEW_ID_LEFT_INSET_FOVEATED = 2,
	XR_VIEW_ID_RIGHT_INSET_FOVEATED= 3,
	XR_VIEW_ID_FOVEATED_COUNT      = 4,

	// XR_VIEW_CONFIGURATION_TYPE_PRIMARY_QUAD_VARJO
	XR_VIEW_ID_LEFT_CONTEXT_QUAD   = 0,
	XR_VIEW_ID_LEFT_FOCUS_QUAD     = 1,
	XR_VIEW_ID_RIGHT_CONTEXT_QUAD  = 2,
	XR_VIEW_ID_RIGHT_FOCUS_QUAD    = 3,
	XR_VIEW_ID_QUAD_COUNT          = 4,
} XrViewId;

#define XR_MAX_VIEW_COUNT XR_VIEW_ID_STEREO_COUNT // we don't support QUAD currently

typedef enum XrGraphicsApi {
	XR_GRAPHICS_API_OPENGL,
	XR_GRAPHICS_API_VULKAN,
	XR_GRAPHICS_API_D3D11_4,
	XR_GRAPHICS_API_COUNT,
} XrGraphicsApi;

typedef enum XrSwapOutput {
	XR_SWAP_OUTPUT_UNKNOWN,
	XR_SWAP_OUTPUT_COLOR,
	XR_SWAP_OUTPUT_DEPTH,
	XR_SWAP_OUTPUT_COUNT,
} XrSwapOutput;

static inline const char* string_XrSwapOutput(XrSwapOutput output) {
	switch (output) {
		case XR_SWAP_OUTPUT_UNKNOWN: return "XR_SWAP_OUTPUT_UNKNOWN";
		case XR_SWAP_OUTPUT_COLOR:   return "XR_SWAP_OUTPUT_COLOR";
		case XR_SWAP_OUTPUT_DEPTH:   return "XR_SWAP_OUTPUT_DEPTH";
		default:                     return "N/A";
	}
}

//typedef enum XrSwapType : u8 {
//	XR_SWAP_TYPE_UNKNOWN,
//	XR_SWAP_TYPE_MONO_SINGLE,
//	XR_SWAP_TYPE_STEREO_SINGLE,
//	XR_SWAP_TYPE_STEREO_TEXTURE_ARRAY,
//	XR_SWAP_TYPE_STEREO_DOUBLE_WIDE,
//	XR_SWAP_TYPE_ERROR,
//	XR_SWAP_TYPE_COUNT,
//} XrSwapType;

typedef enum XrSwapClip {
	XR_SWAP_CLIP_NONE,
	XR_SWAP_CLIP_STRETCH,
	XR_SWAP_CLIP_OCCLUSION_CROP,
	XR_SWAP_CLIP_COUNT,
} XrSwapClip;

typedef enum XrSwapState {
	XR_SWAP_STATE_UNITIALIZED,
	XR_SWAP_STATE_REQUESTED,
	XR_SWAP_STATE_CREATED,
	XR_SWAP_STATE_ERROR,
	XR_SWAP_STATE_COUNT,
} XrSwapState;

typedef struct XrSwapchainInfo {
	XrSwapchainCreateFlags createFlags;
	XrSwapchainUsageFlags  usageFlags;
	VkFormat               format;
	u16                    windowWidth;
	u16                    windowHeight;
	u8                     sampleCount;
	u8                     faceCount;
	u8                     arraySize;
	u8                     mipCount;
} XrSwapchainInfo;

typedef float XrMat4 __attribute__((vector_size(sizeof(float) * 16)));

typedef enum XrStructureTypeExt {
	XR_TYPE_FRAME_BEGIN_SWAP_POOL_EXT = 2000470000,
	XR_TYPE_SUB_VIEW                  = 2000480000,
	XR_TYPE_SPACE_BOUNDS              = 2000490000,
} XrStructureTypeExt;

typedef enum XrReferenceSpaceTypeExt {
	XR_REFERENCE_SPACE_TYPE_LOCAL_BOUNDED = 2000115000,
} XrReferenceSpaceTypeExt;

typedef struct XrOffset3Di {
	int32_t    x;
	int32_t    y;
	int32_t    z;
} XrOffset3Di;

typedef struct XrExtent3Di {
	int32_t    width;
	int32_t    height;
	int32_t    depth;
} XrExtent3Di;

typedef struct XrRect3Di {
	XrOffset3Di    offset;
	XrExtent3Di    extent;
} XrRect3Di;

typedef struct XrEulerPosef {
	XrVector3f euler;
	XrVector3f position;
} XrEulerPosef;

typedef struct XrSubView {
	XrStructureType    type;
	void* XR_MAY_ALIAS next;
	XrRect2Di          imageRect;
	uint32_t           imageArrayIndex;
} XrSubView;

typedef struct XrSpaceBounds {
	XrStructureType    type;
	void* XR_MAY_ALIAS next;
	XrRect3Di          bounds;
} XrSpaceBounds;

typedef struct XrShellSpaces {
	XrStructureType    type;
	void* XR_MAY_ALIAS next;
	int boundsCount;

} XrShellAppBounds;

#define XR_SPACE_LOCATION_ALL_TRACKED           \
	XR_SPACE_LOCATION_ORIENTATION_VALID_BIT   | \
	XR_SPACE_LOCATION_POSITION_VALID_BIT      | \
	XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT | \
	XR_SPACE_LOCATION_POSITION_TRACKED_BIT

typedef u16 session_i;
typedef u16 swap_i;
typedef u8 view_i;

/*
 * External Method Declarations
 */
void xrInitialize();
void xrGetViewConfigurationView(XrSystemId systemId, XrViewConfigurationView *pView);

// this should be sessionId and swapId. Id should be the external identifier
void xrClaimSessionId(session_i* pSessionIndex);

void xrReleaseSessionId(session_i iSession);
void xrGetReferenceSpaceBounds(session_i iSession, XrExtent2Df* pBounds);
void xrCreateSwapchainImages(session_i sessionId, const XrSwapchainInfo* pSwapInfo, swap_i iSwap);
void xrGetSwapchainImportedImage(session_i iSession, swap_i iSwap, int imageId, HANDLE* pHandle);
void xrSetColorSwapId(session_i iSession, XrViewId viewId, swap_i iSwap);
void xrSetDepthSwapId(session_i iSession, XrViewId viewId, swap_i iSwap);
void xrSetDepthInfo(session_i iSession, float minDepth, float maxDepth, float nearZ, float farZ);

void xrGetSessionTimeline(session_i iSession, HANDLE* pHandle);
void xrSetSessionTimelineValue(session_i iSession, u64 timelineValue);

void xrClaimSwapIndex(session_i iSession, uint8_t* pIndex);
void xrReleaseSwapIndex(session_i iSession, uint8_t index);

void xrGetCompositorTimeline(session_i iSession, HANDLE* pHandle);
void xrSetInitialCompositorTimelineValue(session_i iSession, u64 timelineValue);
void xrGetCompositorTimelineValue(session_i iSession, u64* pTimelineValue);
void xrProgressCompositorTimelineValue(session_i iSession, u64 timelineValue);

void xrGetHeadPose(session_i iSession, XrEulerPosef* pPose);

typedef struct XrEyeView {
	XrVector3f euler;
	XrVector3f position;
	XrVector2f fovRad;
	XrVector2f upperLeftClip;
	XrVector2f lowerRightClip;
} XrEyeView;
void xrGetEyeView(session_i iSession, view_i iView, XrEyeView *pEyeView);

XrTime xrGetFrameInterval(session_i sessionIndex);

static inline XrTime xrHzToXrTime(double hz)
{
	static const double oneSecondInNanoSeconds = 1e9;
	return (XrTime)(oneSecondInNanoSeconds / hz);
}

/*
 * Device Input Funcs
 */
typedef struct SubactionState SubactionState;

int xrInputSelectClick_Left(session_i sessionIndex, SubactionState* pState);
int xrInputSelectClick_Right(session_i sessionIndex, SubactionState* pState);
int xrInputSqueezeValue_Left(session_i sessionIndex, SubactionState* pState);
int xrInputSqueezeValue_Right(session_i sessionIndex, SubactionState* pState);
int xrInputSqueezeClick_Left(session_i sessionIndex, SubactionState* pState);
int xrInputSqueezeClick_Right(session_i sessionIndex, SubactionState* pState);
int xrInputTriggerValue_Left(session_i sessionIndex, SubactionState* pState);
int xrInputTriggerValue_Right(session_i sessionIndex, SubactionState* pState);
int xrInputTriggerClick_Left(session_i sessionIndex, SubactionState* pState);
int xrInputTriggerClick_Right(session_i sessionIndex, SubactionState* pState);
int xrInputMenuClick_Left(session_i sessionIndex, SubactionState* pState);
int xrInputMenuClick_Right(session_i sessionIndex, SubactionState* pState);
int xrInputGripPose_Left(session_i sessionIndex, SubactionState* pState);
int xrInputGripPose_Right(session_i sessionIndex, SubactionState* pState);
int xrInputAimPose_Left(session_i sessionIndex, SubactionState* pState);
int xrInputAimPose_Right(session_i sessionIndex, SubactionState* pState);
int xrOutputHaptic_Left(session_i sessionIndex, SubactionState* pState);
int xrOutputHaptic_Right(session_i sessionIndex, SubactionState* pState);

/*
 * OpenXR Constants
 */

#define XR_SWAPCHAIN_IMAGE_COUNT 2

#define XR_OPENGL_MAJOR_VERSION 4
#define XR_OPENGL_MINOR_VERSION 6

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

/*
 * Set
 */
typedef struct SetBase {
	u32           count;
	block_handle* handles;
} SetBase;
#define SET_DECL(n)              \
	struct CACHE_ALIGN {         \
		uint32_t     count;      \
		block_handle handles[n]; \
	}

/*
 * OpenXR Types
 */

typedef block_handle swap_h;
typedef block_handle space_h;
typedef block_handle session_h;

#define XR_PATH_CAPACITY 256
typedef struct Path {
	char string[XR_MAX_PATH_LENGTH];
} Path;

#define XR_MAX_BINDINGS      16
#define XR_INTERACTION_PROFILE_CAPACITY 16
typedef struct InteractionProfile {
	XrPath path;
	MAP_DECL(XR_MAX_BINDINGS) bindings;
} InteractionProfile;
typedef InteractionProfile* XrInteractionProfile;

#define XR_SUBACTION_CAPACITY 256
typedef struct SubactionState {
	bHnd hAction;

	// need to understand this priority again
	//	uint32_t lastSyncedPriority;

	union {
		XrBool32      boolValue;
		f32           floatValue;
		XrVector2f    vec2Value;
		XrVector3f    vec3Value;
		XrQuaternionf quatValue;
		XrPosef       poseValue;
		XrEulerPosef  eulerPoseValue;
	};

	XrBool32 changedSinceLastSync;
	XrTime   lastChangeTime;
	XrBool32 isActive;
} SubactionState;

#define XR_BINDINGS_CAPACITY 256
typedef struct Binding {
	bHnd hPath;
	int (*func)(session_i, SubactionState*);
} Binding;

#define XR_MAX_SUBACTION_PATHS 2
#define XR_ACTION_CAPACITY     128
typedef struct Action {
	bHnd hActionSet;

	u8   countSubactions;
	bHnd hSubactionStates[XR_MAX_SUBACTION_PATHS];
	bHnd hSubactionBindings[XR_MAX_SUBACTION_PATHS];
	bHnd hSubactionPaths[XR_MAX_SUBACTION_PATHS];

	XrActionType actionType;
	char         actionName[XR_MAX_ACTION_NAME_SIZE];
	char         localizedActionName[XR_MAX_LOCALIZED_ACTION_NAME_SIZE];

	bool errorLogged[XR_MAX_SUBACTION_PATHS];
} Action;

#define XR_ACTION_SET_CAPACITY 64
#define XR_MAX_ACTION_SET_STATES 64
typedef struct ActionSet {
	bHnd hAttachedToSession;

	MAP_DECL(XR_MAX_ACTION_SET_STATES) actions;
	MAP_DECL(XR_MAX_ACTION_SET_STATES) states;

	char actionSetName[XR_MAX_ACTION_SET_NAME_SIZE];
	char localizedActionSetName[XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE];
	u32  priority;
} ActionSet;

#define XR_SPACE_CAPACITY 128
typedef struct Space {
	XrStructureType type;
	bHnd            hSession;
	XrPosef         poseInSpace;

	union {
		struct {
			XrReferenceSpaceType spaceType;
		} reference;

		struct {
			bHnd hAction;
			bHnd hSubactionPath;
		} action;
	};

} Space;


#define XR_SWAPCHAIN_CAPACITY 8
typedef struct Swapchain {
	block_handle    hSession;
	XrSwapOutput    output;
	XrSwapchainInfo info;

	u8 acquiredSwapIndex;
	union {
		struct {
			GLuint texture;
			GLuint memObject;
		} gl;
		struct {
			ID3D11Texture2D* localTexture;
			ID3D11Texture2D* transferTexture;
			ID3D11Resource*  localResource;
			ID3D11Resource*  transferResource;
		} d3d11;
	} texture[XR_SWAPCHAIN_IMAGE_COUNT];

} Swapchain;

#define XR_SESSIONS_CAPACITY 8
#define XR_MAX_SPACES 8
typedef struct Session {
	/* Info */
	session_i               index;
	XrSystemId              systemId;
	XrViewConfigurationType primaryViewConfigurationType;

	/* Swap */
	swap_h      hSwap;
	XrSwapClip  swapClip;

	/* Timing */
	XrTime frameWaited;
	XrTime frameBegan;
	XrTime frameEnded;

	u64    sessionTimelineValue;

	/* Events */
	XrSessionState activeSessionState;
	block_handle hActiveReferenceSpace;
	block_handle hActiveInteractionProfile;
	block_handle hPendingInteractionProfile;

	XrBool32 activeIsUserPresent;
	XrBool32 pendingIsUserPresent;

	/* State */
	bool running;
	bool exiting;

	/* Maps */
	MAP_DECL(XR_MAX_ACTION_SET_STATES) actionSets;
	MAP_DECL(XR_MAX_SPACES) referenceSpaces;
	MAP_DECL(XR_MAX_SPACES) actionSpaces;

	/* Graphics */
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

typedef union XrEventDataUnion {
	XrEventDataBuffer                      dataBuffer;
	XrEventDataSessionStateChanged         sessionStateChanged;
	XrEventDataReferenceSpaceChangePending referenceSpaceChangePending;
} XrEventDataUnion;

typedef struct Instance {
	/* System */
	XrApplicationInfo applicationInfo;
	XrSystemId        systemId;
	XrFormFactor      systemFormFactor;
	XrGraphicsApi     graphicsApi;
	bool userPresenceEnabled;

	/* Interaction */
	MAP_DECL(XR_INTERACTION_PROFILE_CAPACITY) interactionProfiles;

	/* Graphics */
	union {
		struct {
			LUID              adapterLuid;
			D3D_FEATURE_LEVEL minFeatureLevel;
		} d3d11;
	} graphics;

	/* Events */
	MidQRing         eventDataQueue;
	XrEventDataUnion queuedEventDataBuffers[MID_QRING_CAPACITY];

} Instance;

#endif // MID_OPENXR_RUNTIME_H

/*
 *
 * Mid OpenXR Runtime Implementation
 *
 */
#if defined(MID_OPENXR_IMPLEMENTATION) || defined(MID_IDE_ANALYSIS)

static struct {
	Instance instance;

	struct {
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

} xr;

#define B xr.block // get rid of this

// Opaque Handle to Ptr
#define XR_OPAQUE_BLOCK_P(_opaqueHandle) _Generic(_opaqueHandle,                       \
	    XrSession: BLOCK_PTR(xr.block.session, (block_handle)(u16)(u64)_opaqueHandle), \
	    XrSpace:   BLOCK_PTR(xr.block.space,   (block_handle)(u16)(u64)_opaqueHandle), \
	    default: NULL)

// Opaque Handle to Handle
#define XR_OPAQUE_BLOCK_H(_opaqueHandle) (block_handle)(u64) _opaqueHandle

// Handle to Opaque Handle
#define XR_TO_OPAQUE_H(_type, _handle) _Generic((_type*)0,                 \
    XrSession*: (_type)((u64)XR_OBJECT_TYPE_SESSION << 32 | (u64)_handle), \
    default: (_type)0                                                      \
)

#define XR_OPAQUE_H_TYPE(_handle) (XrObjectType)(_handle >> 32);

/*
 * Utility
 */
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
		XrTime currentTime = atomic_load_explicit(pSharedTime, memory_order_acquire);
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
	atomic_store_explicit(pSharedTime, signalTime, memory_order_release);
	WakeByAddressAll(pSharedTime);
}

/*
 * OpenXR Debug Logging
 */
#define CHECK_INSTANCE(instance)                \
	if (&xr.instance != (Instance*)instance) {  \
		LOG_ERROR("XR_ERROR_HANDLE_INVALID\n"); \
		return XR_ERROR_HANDLE_INVALID;         \
	}

#define ENABLE_DEBUG_LOG_METHOD
#ifdef ENABLE_DEBUG_LOG_METHOD

#define LOG_METHOD(_method) printf("%lu:%lu: " #_method "\n", GetCurrentProcessId(), GetCurrentThreadId())

static const char* pLastLogMethod = NULL;
#define LOG_METHOD_ONCE(_method)                 \
	{                                            \
		static const char* methodPtr = #_method; \
		if (pLastLogMethod != methodPtr) {       \
			pLastLogMethod = methodPtr;          \
			LOG_METHOD(_method);                 \
		}                                        \
	}


#else
#define LOG_METHOD(_name)
#define LOG_METHOD_ONCE(_name)
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
STRING_ENUM_TYPE(XrSessionState)

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

#define STR(s)       #s
#define XSTR(s)      STR(s)
#define DOT(_)       ._
#define COMMA        ,
#define BRACES(f, _) {f(_)}

#define FORMAT_F(_)          _: %.3f
#define FORMAT_I(_)          _: %d
#define FORMAT_STRUCT_F(_)  "%s: " XSTR(BRACES(XR_LIST_STRUCT_##_, FORMAT_F))
#define FORMAT_STRUCT_I(_)  "%s: " XSTR(BRACES(XR_LIST_STRUCT_##_, FORMAT_I))
#define EXPAND_STRUCT(t, _) STR(_) XR_LIST_STRUCT_##t(COMMA _ DOT)

static void LogNextChain(const XrBaseInStructure* nextProperties)
{
	while (nextProperties != NULL) {
		printf("NextType: %s\n", string_XrStructureType(nextProperties->type));
		nextProperties = (XrBaseInStructure*)nextProperties->next;
	}
}

/*
 * Events
 */
#define XR_EVENT_ENQUEUE_SCOPE(_pData) \
	MID_QRING_ENQUEUE_SCOPE(&xr.instance.eventDataQueue, xr.instance.queuedEventDataBuffers, _pData)

static void EnqueueEventDataSessionStateChanged(session_h hSession, XrSessionState sessionState)
{
	Session* pSession = BLOCK_PTR(xr.block.session, hSession);
	pSession->activeSessionState = sessionState;

	XrEventDataUnion* pEventData;
	XR_EVENT_ENQUEUE_SCOPE(pEventData) {
		pEventData->sessionStateChanged = (XrEventDataSessionStateChanged){
			.type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED,
			.session = XR_TO_OPAQUE_H(XrSession, hSession),
			.state = sessionState,
			.time = xrGetTime(),
		};
	}

	LOG("Enqueue XrEventDataSessionStateChanged: %s\n",
	    string_XrSessionState(sessionState));
}

static void
EnqueueEventDataReferenceSpaceChangePending(session_h            hSession,
                                            space_h              hSpace,
                                            XrReferenceSpaceType referenceSpaceType,
                                            XrPosef              poseInPreviousSpace)
{
	Session* pSession = BLOCK_PTR(xr.block.session, hSession);
	pSession->hActiveReferenceSpace = hSpace;

	XrEventDataUnion* pSpaceEventData;
	XR_EVENT_ENQUEUE_SCOPE(pSpaceEventData)	{
		pSpaceEventData->referenceSpaceChangePending = (XrEventDataReferenceSpaceChangePending){
			.type = XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING,
			.session = XR_TO_OPAQUE_H(XrSession, hSession),
			.referenceSpaceType = referenceSpaceType,
			.changeTime = xrGetTime(),
			.poseValid = XR_TRUE,
			.poseInPreviousSpace = poseInPreviousSpace,
		};
	}

	LOG("Enqueue XrEventDataReferenceSpaceChangePending: %s\n",
	    string_XrReferenceSpaceType(referenceSpaceType));
}

/*
 * Math
 */
#ifndef PI
#define PI 3.14159265358979323846
#endif
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


#define XR_CONVERT_DD11_POSITION(_) _.y = -_.y
#define XR_CONVERT_D3D11_EULER(_) _.x = -_.x

static inline float xrFloatLerp(float a, float b, float t)
{
	return a + t * (b - a);
}

/*
 * Binding
 */
static XrResult RegisterBinding(
	XrInstance          instance,
	InteractionProfile* pInteractionProfile,
	Path*               pBindingPath,
	int (*func)(session_i, SubactionState*))
{
	auto bindPathHash = BLOCK_KEY(B.path, pBindingPath);
	for (u32 i = 0; i < pInteractionProfile->bindings.count; ++i) {
		if (pInteractionProfile->bindings.keys[i] == bindPathHash) {
			LOG_ERROR("Trying to register path hash twice! %s %d\n", pBindingPath->string, bindPathHash);
			return XR_ERROR_PATH_INVALID;
		}
	}

	auto hBind = BLOCK_CLAIM(B.binding, bindPathHash);
	auto pBind = BLOCK_PTR(B.binding, hBind);
	pBind->hPath = BLOCK_HANDLE(B.path, pBindingPath);
	pBind->func = func;

	MAP_ADD(pInteractionProfile->bindings, hBind, bindPathHash);

	return XR_SUCCESS;
}

typedef struct BindingDefinition {
	int (*func)(session_i, SubactionState*);
	const char path[XR_MAX_PATH_LENGTH];
} BindingDefinition;
static XrResult InitBinding(const char* interactionProfile, int bindingDefinitionCount, BindingDefinition* pBindingDefinitions)
{
	XrPath interactionProfilePath;
	xrStringToPath((XrInstance)&xr.instance, interactionProfile, &interactionProfilePath);
	bKey interactionProfilePathHash = BLOCK_KEY(B.path, (Path*)interactionProfilePath);

	bHnd interactionProfileHandle = BLOCK_CLAIM(B.profile, interactionProfilePathHash);
	auto pInteractionProfile = BLOCK_PTR(B.profile, interactionProfileHandle);

	pInteractionProfile->path = interactionProfilePath;

	for (int i = 0; i < bindingDefinitionCount; ++i) {
		XrPath bindingPath;
		xrStringToPath((XrInstance)&xr.instance, pBindingDefinitions[i].path, &bindingPath);
		XR_CHECK(RegisterBinding((XrInstance)&xr.instance, pInteractionProfile, (Path*)bindingPath, pBindingDefinitions[i].func));
	}

	return XR_SUCCESS;
}

#define XR_INTERACTION_PROFILE_KHR_SIMPLE_CONTROLLER "/interaction_profiles/khr/simple_controller"
#define XR_INTERACTION_PROFILE_OCULUS_TOUCH_CONTROLLER "/interaction_profiles/oculus/touch_controller"

#define XR_DEFAULT_INTERACTION_PROFILE XR_INTERACTION_PROFILE_OCULUS_TOUCH_CONTROLLER

#define XR_INPUT_SELECT_CLICK_LEFT_HAND    "/user/hand/left/input/select/click"
#define XR_INPUT_MENU_CLICK_LEFT_HAND      "/user/hand/left/input/menu/click"
#define XR_INPUT_GRIP_POSE_LEFT_HAND       "/user/hand/left/input/grip/pose"
#define XR_INPUT_AIM_POSE_LEFT_HAND        "/user/hand/left/input/aim/pose"
#define XR_INPUT_SQUEEZE_VALUE_LEFT_HAND   "/user/hand/left/input/squeeze/value"
#define XR_INPUT_SQUEEZE_CLICK_LEFT_HAND   "/user/hand/left/input/squeeze/click"
#define XR_INPUT_TRIGGER_VALUE_LEFT_HAND   "/user/hand/left/input/trigger/value"
#define XR_INPUT_TRIGGER_CLICK_LEFT_HAND   "/user/hand/left/input/trigger/click"
#define XR_OUTPUT_HAPTIC_LEFT_HAND         "/user/hand/left/output/haptic"

#define XR_INPUT_SELECT_CLICK_RIGHT_HAND   "/user/hand/right/input/select/click"
#define XR_INPUT_MENU_CLICK_RIGHT_HAND     "/user/hand/right/input/menu/click"
#define XR_INPUT_GRIP_POSE_RIGHT_HAND      "/user/hand/right/input/grip/pose"
#define XR_INPUT_AIM_POSE_RIGHT_HAND       "/user/hand/right/input/aim/pose"
#define XR_INPUT_SQUEEZE_VALUE_RIGHT_HAND  "/user/hand/right/input/squeeze/value"
#define XR_INPUT_SQUEEZE_CLICK_RIGHT_HAND  "/user/hand/right/input/squeeze/click"
#define XR_INPUT_TRIGGER_VALUE_RIGHT_HAND  "/user/hand/right/input/trigger/value"
#define XR_INPUT_TRIGGER_CLICK_RIGHT_HAND  "/user/hand/right/input/trigger/click"
#define XR_OUTPUT_HAPTIC_RIGHT_HAND        "/user/hand/right/output/haptic"

static XrResult InitStandardBindings()
{
#define BINDING_DEFINITION(_func, _path) {(int (*)(session_i, SubactionState*)) _func, _path}

	{
		BindingDefinition bindingDefinitions[] = {
			BINDING_DEFINITION(xrInputSelectClick_Left, XR_INPUT_SELECT_CLICK_LEFT_HAND),
			BINDING_DEFINITION(xrInputSelectClick_Right, XR_INPUT_SELECT_CLICK_RIGHT_HAND),

			BINDING_DEFINITION(xrInputMenuClick_Left, XR_INPUT_MENU_CLICK_LEFT_HAND),
			BINDING_DEFINITION(xrInputMenuClick_Right, XR_INPUT_MENU_CLICK_RIGHT_HAND),

			BINDING_DEFINITION(xrInputGripPose_Left, XR_INPUT_GRIP_POSE_LEFT_HAND),
			BINDING_DEFINITION(xrInputGripPose_Right, XR_INPUT_GRIP_POSE_RIGHT_HAND),

			BINDING_DEFINITION(xrInputAimPose_Left, XR_INPUT_AIM_POSE_LEFT_HAND),
			BINDING_DEFINITION(xrInputAimPose_Right, XR_INPUT_AIM_POSE_RIGHT_HAND),

			BINDING_DEFINITION(xrOutputHaptic_Left, XR_OUTPUT_HAPTIC_LEFT_HAND),
			BINDING_DEFINITION(xrOutputHaptic_Right, XR_OUTPUT_HAPTIC_RIGHT_HAND),

		};
		InitBinding(XR_INTERACTION_PROFILE_KHR_SIMPLE_CONTROLLER, COUNT(bindingDefinitions), bindingDefinitions);
	}

	{
		BindingDefinition bindingDefinitions[] = {
			BINDING_DEFINITION(xrInputSelectClick_Left, XR_INPUT_SELECT_CLICK_LEFT_HAND),
			BINDING_DEFINITION(xrInputSelectClick_Right, XR_INPUT_SELECT_CLICK_RIGHT_HAND),

			BINDING_DEFINITION(xrInputSqueezeValue_Left, XR_INPUT_SQUEEZE_VALUE_LEFT_HAND),
			BINDING_DEFINITION(xrInputSqueezeValue_Right, XR_INPUT_SQUEEZE_VALUE_RIGHT_HAND),

			BINDING_DEFINITION(xrInputSqueezeClick_Left, XR_INPUT_SQUEEZE_CLICK_LEFT_HAND),
			BINDING_DEFINITION(xrInputSqueezeClick_Right, XR_INPUT_SQUEEZE_CLICK_RIGHT_HAND),

			BINDING_DEFINITION(xrInputTriggerValue_Left, XR_INPUT_TRIGGER_VALUE_LEFT_HAND),
			BINDING_DEFINITION(xrInputTriggerValue_Right, XR_INPUT_TRIGGER_VALUE_RIGHT_HAND),

			BINDING_DEFINITION(xrInputTriggerClick_Left, XR_INPUT_TRIGGER_CLICK_LEFT_HAND),
			BINDING_DEFINITION(xrInputTriggerClick_Right, XR_INPUT_TRIGGER_CLICK_RIGHT_HAND),

			BINDING_DEFINITION(xrInputGripPose_Left, XR_INPUT_GRIP_POSE_LEFT_HAND),
			BINDING_DEFINITION(xrInputGripPose_Right, XR_INPUT_GRIP_POSE_RIGHT_HAND),

			BINDING_DEFINITION(xrInputAimPose_Left, XR_INPUT_AIM_POSE_LEFT_HAND),
			BINDING_DEFINITION(xrInputAimPose_Right, XR_INPUT_AIM_POSE_RIGHT_HAND),

			BINDING_DEFINITION(xrOutputHaptic_Left, XR_OUTPUT_HAPTIC_LEFT_HAND),
			BINDING_DEFINITION(xrOutputHaptic_Right, XR_OUTPUT_HAPTIC_RIGHT_HAND),

			BINDING_DEFINITION(xrInputMenuClick_Left, XR_INPUT_MENU_CLICK_LEFT_HAND),
			BINDING_DEFINITION(xrInputMenuClick_Right, XR_INPUT_MENU_CLICK_RIGHT_HAND),

		};
		InitBinding(XR_INTERACTION_PROFILE_OCULUS_TOUCH_CONTROLLER, COUNT(bindingDefinitions), bindingDefinitions);
	}

#undef BINDING_DEFINITION
	return XR_SUCCESS;
}

static inline XrResult GetActionState(
	Action*          pAction,
	Path*            pSubPath,
	SubactionState** ppState)
{
	auto hSubPath = BLOCK_HANDLE(B.path, pSubPath);
	if (pSubPath == NULL) {
		auto hState = pAction->hSubactionPaths[0];
		*ppState = BLOCK_PTR(B.state, hState);
		return XR_SUCCESS;
	}

	for (u32 i = 0; i < pAction->countSubactions; ++i) {
		if (hSubPath == pAction->hSubactionPaths[i]) {
			auto hState = pAction->hSubactionStates[i];
			auto handle = HANDLE_INDEX(hState);
			*ppState = BLOCK_PTR(B.state, hState);
			return XR_SUCCESS;
		}
	}

	return XR_ERROR_PATH_UNSUPPORTED;
}

/*
 * OpenXR Method Implementations
 */
XR_PROC
xrEnumerateApiLayerProperties(uint32_t              propertyCapacityInput,
                              uint32_t*             propertyCountOutput,
                              XrApiLayerProperties* properties)
{
	LOG_METHOD(xrEnumerateApiLayerProperties);
	return XR_SUCCESS;
}

XR_PROC
xrEnumerateInstanceExtensionProperties(const char*            layerName,
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
		{
			.type = XR_TYPE_EXTENSION_PROPERTIES,
			.extensionName = XR_EXT_USER_PRESENCE_EXTENSION_NAME,
			.extensionVersion = XR_EXT_user_presence_SPEC_VERSION,
		},
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
//		{
//			.type = XR_TYPE_EXTENSION_PROPERTIES,
//			.extensionName = XR_VARJO_QUAD_VIEWS_EXTENSION_NAME,
//			.extensionVersion = XR_VARJO_quad_views_SPEC_VERSION,
//		},
//		{
//			.type = XR_TYPE_EXTENSION_PROPERTIES,
//			.extensionName = XR_MSFT_SECONDARY_VIEW_CONFIGURATION_EXTENSION_NAME,
//			.extensionVersion = XR_MSFT_secondary_view_configuration_SPEC_VERSION,
//		},
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
//		{
//			.type = XR_TYPE_EXTENSION_PROPERTIES,
//			.extensionName = XR_MSFT_FIRST_PERSON_OBSERVER_EXTENSION_NAME,
//			.extensionVersion = XR_MSFT_first_person_observer_SPEC_VERSION,
//		},
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

XR_PROC xrCreateInstance(const XrInstanceCreateInfo* createInfo,
                         XrInstance*                 instance)
{
	LOG_METHOD(xrCreateInstance);

	if (xr.instance.applicationInfo.apiVersion != 0) {
		LOG_ERROR("XR_ERROR_LIMIT_REACHED\n");
		return XR_ERROR_LIMIT_REACHED;
	}

	for (u32 i = 0; i < createInfo->enabledApiLayerCount; ++i) {
		LOG("Enabled API Layer: %s\n", createInfo->enabledApiLayerNames[i]);
	}

	for (u32 i = 0; i < createInfo->enabledExtensionCount; ++i) {
		LOG("Enabled Extension: %s\n", createInfo->enabledExtensionNames[i]);
		if (strncmp(createInfo->enabledExtensionNames[i], XR_KHR_OPENGL_ENABLE_EXTENSION_NAME, XR_MAX_EXTENSION_NAME_SIZE) == 0)
			xr.instance.graphicsApi = XR_GRAPHICS_API_OPENGL;
		else if (strncmp(createInfo->enabledExtensionNames[i], XR_KHR_D3D11_ENABLE_EXTENSION_NAME, XR_MAX_EXTENSION_NAME_SIZE) == 0)
			xr.instance.graphicsApi = XR_GRAPHICS_API_D3D11_4;
		else if (strncmp(createInfo->enabledExtensionNames[i], XR_EXT_USER_PRESENCE_EXTENSION_NAME, XR_MAX_EXTENSION_NAME_SIZE) == 0)
			xr.instance.userPresenceEnabled = true;
	}

	memcpy(&xr.instance.applicationInfo, &createInfo->applicationInfo, sizeof(XrApplicationInfo));

	LOG("applicationName: %s\n"   , createInfo->applicationInfo.applicationName);
	LOG("applicationVersion: %d\n", createInfo->applicationInfo.applicationVersion);
	LOG("engineName: %s\n"        , createInfo->applicationInfo.engineName);
	LOG("engineVersion: %d\n"     , createInfo->applicationInfo.engineVersion);
	{
		int maj   = XR_VERSION_MAJOR(xr.instance.applicationInfo.apiVersion);
		int min   = XR_VERSION_MINOR(xr.instance.applicationInfo.apiVersion);
		int patch = XR_VERSION_PATCH(xr.instance.applicationInfo.apiVersion);
		printf("apiVersion: %d.%d.%d\n", maj, min, patch);
	}
	{
		int maj   = XR_VERSION_MAJOR(XR_CURRENT_API_VERSION);
		int min   = XR_VERSION_MINOR(XR_CURRENT_API_VERSION);
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
	xrInitialize();

	switch (xr.instance.graphicsApi) {
		case XR_GRAPHICS_API_OPENGL: {
			gl.CreateMemoryObjectsEXT = (PFNGLCREATEMEMORYOBJECTSEXTPROC)(void*)wglGetProcAddress("glCreateMemoryObjectsEXT");
			if (!gl.CreateMemoryObjectsEXT)	LOG_ERROR("Failed to load glCreateMemoryObjectsEXT\n");

			gl.ImportMemoryWin32HandleEXT = (PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC)(void*)wglGetProcAddress("glImportMemoryWin32HandleEXT");
			if (!gl.ImportMemoryWin32HandleEXT)	LOG_ERROR("Failed to load glImportMemoryWin32HandleEXT\n");

			gl.CreateTextures = (PFNGLCREATETEXTURESPROC)(void*)wglGetProcAddress("glCreateTextures");
			if (!gl.CreateTextures)	LOG_ERROR("Failed to load glCreateTextures\n");

			gl.TextureStorageMem2DEXT = (PFNGLTEXTURESTORAGEMEM2DEXTPROC)(void*)wglGetProcAddress("glTextureStorageMem2DEXT");
			if (!gl.TextureStorageMem2DEXT)	LOG_ERROR("Failed to load glTextureStorageMem2DEXT\n");

			return XR_SUCCESS;
		}
		case XR_GRAPHICS_API_D3D11_4: {
			return XR_SUCCESS;
		}
		case XR_GRAPHICS_API_VULKAN:
		default:
			LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED\n");
			return XR_ERROR_GRAPHICS_DEVICE_INVALID;
	}
}

XR_PROC xrDestroyInstance(XrInstance instance)
{
	LOG_METHOD(xrDestroyInstance);
	CHECK_INSTANCE(instance);
	memset(&xr.instance, 0, sizeof(Instance));
	return XR_SUCCESS;
}

XR_PROC xrGetInstanceProperties(XrInstance            instance,
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

//STRING_ENUM_METHOD(XrSessionState)

typedef enum XrCubeCorners: u8 {
	XR_CUBE_CORNER_LUB,
	XR_CUBE_CORNER_LUF,
	XR_CUBE_CORNER_LDB,
	XR_CUBE_CORNER_LDF,
	XR_CUBE_CORNER_RUB,
	XR_CUBE_CORNER_RUF,
	XR_CUBE_CORNER_RDB,
	XR_CUBE_CORNER_RDF,
	XR_CUBE_CORNER_COUNT,
} XrCubeCorners;

typedef struct XrEventDataSpaceBoundsChanged {
	XrStructureType             type;
	const void* XR_MAY_ALIAS    next;
	XrSpace                     space;
	vec3                        worldCorners[XR_CUBE_CORNER_COUNT];
	vec2                        uvCorners[XR_CUBE_CORNER_COUNT];
} XrEventDataSpaceBoundsChanged;

XR_PROC xrPollEvent(XrInstance         instance,
                    XrEventDataBuffer* eventData)
{
	LOG_METHOD_ONCE(xrPollEvent);
	CHECK_INSTANCE(instance);

	XrEventDataUnion* pEventData = (XrEventDataUnion*)eventData;
	if (MID_QRING_DEQUEUE(&xr.instance.eventDataQueue, xr.instance.queuedEventDataBuffers, pEventData) == MID_SUCCESS) {
//		switch (pEventData->dataBuffer.type) {
//
//			case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
//				LOG("SessionStateChanged: %s\n", string_XrSessionState(pEventData->sessionStateChanged.state));
//				Session* pSession = XR_OPAQUE_BLOCK_P(pEventData->sessionStateChanged.session);
//				pSession->activeSessionState = pEventData->sessionStateChanged.state;
//				break;
//			}
//
//			default: break;
//		}

		return XR_SUCCESS;
	}

	// technically could be worth iterating map in instance?
	for (u16 iSession = 0; iSession < XR_SESSIONS_CAPACITY; ++iSession) {

		session_h hSession = BLOCK_HANDLE_INDEX(xr.block.session, iSession);

		if (!BLOCK_OCCUPIED(xr.block.session, hSession))
			continue;

		Session* pSession = BLOCK_PTR(B.session, hSession);

		if (pSession->hActiveInteractionProfile != pSession->hPendingInteractionProfile) {

			eventData->type = XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED;
			auto pEventData = (XrEventDataInteractionProfileChanged*)eventData;
			pEventData->session = XR_TO_OPAQUE_H(XrSession, hSession);

			pSession->hActiveInteractionProfile = pSession->hPendingInteractionProfile;

			auto pActionInteractionProfile = BLOCK_PTR(B.profile, pSession->hActiveInteractionProfile);
			auto pActionInteractionProfilePath = (Path*)pActionInteractionProfile->path;
			printf("XrEventDataInteractionProfileChanged %s\n", pActionInteractionProfilePath->string);

			return XR_SUCCESS;
		}

		if (xr.instance.userPresenceEnabled && pSession->activeIsUserPresent != pSession->pendingIsUserPresent) {

			eventData->type = XR_TYPE_EVENT_DATA_USER_PRESENCE_CHANGED_EXT;

			auto pEventData = (XrEventDataUserPresenceChangedEXT*)eventData;
			pEventData->session = XR_TO_OPAQUE_H(XrSession, hSession);
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

#undef TRANSFER_ENUM_NAME

XR_PROC xrGetSystem(
	XrInstance             instance,
	const XrSystemGetInfo* getInfo,
	XrSystemId*            systemId)
{
	LOG_METHOD_ONCE(xrGetSystem);
	LogNextChain((XrBaseInStructure*)getInfo->next);
	CHECK_INSTANCE(instance);

	switch (getInfo->formFactor) {
		case XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY:
			xr.instance.systemFormFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
			xr.instance.systemId = SYSTEM_ID_HMD_VR_STEREO;
			*systemId = SYSTEM_ID_HMD_VR_STEREO;
			return XR_SUCCESS;
		case XR_FORM_FACTOR_HANDHELD_DISPLAY:
			xr.instance.systemFormFactor = XR_FORM_FACTOR_HANDHELD_DISPLAY;
			xr.instance.systemId = SYSTEM_ID_HANDHELD_AR;
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
	CHECK_INSTANCE(instance);

	XrViewConfigurationView viewConfig; xrGetViewConfigurationView(systemId, &viewConfig);

	switch ((SystemId)systemId) {
		case SYSTEM_ID_HANDHELD_AR:
			properties->systemId = SYSTEM_ID_HANDHELD_AR;
			properties->vendorId = 0;
			strncpy(properties->systemName, "MoxaicDesktop", XR_MAX_SYSTEM_NAME_SIZE);
			properties->graphicsProperties.maxLayerCount = XR_MIN_COMPOSITION_LAYERS_SUPPORTED;
			properties->graphicsProperties.maxSwapchainImageWidth = viewConfig.maxImageRectWidth;
			properties->graphicsProperties.maxSwapchainImageHeight = viewConfig.maxImageRectHeight;
			properties->trackingProperties.orientationTracking = XR_TRUE;
			properties->trackingProperties.positionTracking = XR_TRUE;
			return XR_SUCCESS;
		case SYSTEM_ID_HMD_VR_STEREO:
			properties->systemId = SYSTEM_ID_HMD_VR_STEREO;
			properties->vendorId = 0;
			strncpy(properties->systemName, "MoxaicHMD", XR_MAX_SYSTEM_NAME_SIZE);
			properties->graphicsProperties.maxLayerCount = XR_MIN_COMPOSITION_LAYERS_SUPPORTED;
			properties->graphicsProperties.maxSwapchainImageWidth = viewConfig.maxImageRectWidth;
			properties->graphicsProperties.maxSwapchainImageHeight = viewConfig.maxImageRectHeight;
			properties->trackingProperties.orientationTracking = XR_TRUE;
			properties->trackingProperties.positionTracking = XR_TRUE;
			return XR_SUCCESS;
		default:
			return XR_ERROR_HANDLE_INVALID;
	}

	switch (properties->next != NULL ? *(XrStructureType*)properties->next : 0) {
		case XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT:
			auto gazeProperties = (XrSystemEyeGazeInteractionPropertiesEXT *)properties->next;
			gazeProperties->supportsEyeGazeInteraction = XR_TRUE;
				break;
		default:
			break;
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

	static const XrEnvironmentBlendMode modes[] = {
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
			for (u32 i = 0; i < COUNT(modes); ++i)
				environmentBlendModes[i] = modes[i];
			break;
	}

	return XR_SUCCESS;
}

XR_PROC xrCreateSession(XrInstance                 instance,
                        const XrSessionCreateInfo* createInfo,
                        XrSession*                 session)
{
	LOG_METHOD(xrCreateSession);
	CHECK_INSTANCE(instance);

	if (xr.instance.systemId == XR_NULL_SYSTEM_ID) {
		LOG_ERROR("XR_ERROR_SYSTEM_INVALID\n");
		return XR_ERROR_SYSTEM_INVALID;
	}

	LOG("SystemId: %llu\n", createInfo->systemId);
	if (xr.instance.systemId != createInfo->systemId) {
		LOG_ERROR("XR_ERROR_SYSTEM_INVALID\n");
		return XR_ERROR_SYSTEM_INVALID;
	}

	if (xr.instance.graphics.d3d11.adapterLuid.LowPart == 0) {
		LOG_ERROR("XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING\n");
		return XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING;
	}

	session_i sessionIndex; xrClaimSessionId(&sessionIndex);
	LOG("Claimed SessionIndex %d\n", sessionIndex);

	session_h hSession = BLOCK_CLAIM(xr.block.session, sessionIndex);
	Session*  pSession = BLOCK_PTR(xr.block.session, hSession);
	memset(pSession, 0, sizeof(Session));
	pSession->index = sessionIndex;

	HANDLE compositorFenceHandle; xrGetCompositorTimeline(sessionIndex, &compositorFenceHandle);
	HANDLE sessionFenceHandle; xrGetSessionTimeline(sessionIndex, &sessionFenceHandle);

	if (createInfo->next == NULL) {
		LOG_ERROR("XR_ERROR_GRAPHICS_DEVICE_INVALID\n");
		return XR_ERROR_GRAPHICS_DEVICE_INVALID;
	}

	switch (*(XrStructureType*)createInfo->next) {
		case XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR: {
			printf("OpenXR Graphics Binding: XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR\n");
			XrGraphicsBindingOpenGLWin32KHR* binding = (XrGraphicsBindingOpenGLWin32KHR*)createInfo->next;

			pSession->binding.gl.hDC = binding->hDC;
			pSession->binding.gl.hGLRC = binding->hGLRC;

			break;
		}
		case XR_TYPE_GRAPHICS_BINDING_D3D11_KHR: {
			printf("OpenXR Graphics Binding: XR_TYPE_GRAPHICS_BINDING_D3D11_KHR\n");
			XrGraphicsBindingD3D11KHR* binding = (XrGraphicsBindingD3D11KHR*)createInfo->next;

			printf("XR D3D11 Device: %p\n", (void*)binding->device);
			// do I need cleanup goto? maybe. Probably
			if (binding->device == NULL) {
				LOG_ERROR("XR D3D11 Device Invalid.\n");
				return XR_ERROR_GRAPHICS_DEVICE_INVALID;
			}

			ID3D11Device5* device5;
			DX_CHECK(ID3D11Device_QueryInterface(binding->device, &IID_ID3D11Device5, (void**)&device5));
			printf("XR D3D11 Device5: %p\n", (void*)device5);
			if (device5 == NULL) {
				LOG_ERROR("XR D3D11 Device5 Invalid.\n");
				return XR_ERROR_GRAPHICS_DEVICE_INVALID;
			}

			ID3D11DeviceContext* context;
			ID3D11Device5_GetImmediateContext(device5, &context);
			ID3D11DeviceContext4* context4;
			DX_CHECK(ID3D11DeviceContext_QueryInterface(context, &IID_ID3D11DeviceContext4, (void**)&context4));
			printf("XR D3D11 Context4: %p\n", (void*)context4);
			if (context4 == NULL) {
				LOG_ERROR("XR D3D11 Context4 Invalid.\n");
				return XR_ERROR_GRAPHICS_DEVICE_INVALID;
			}
			ID3D11DeviceContext_Release(context);

			ID3D11Fence* compositorFence;
			printf("XR D3D11 Compositor Fence Handle: %p\n", compositorFenceHandle);
			ID3D11Device5_OpenSharedFence(device5, compositorFenceHandle, &IID_ID3D11Fence, (void**)&compositorFence);
			printf("XR D3D11 Compositor Fence: %p\n", (void*)compositorFence);
			if (compositorFence == NULL) {
				LOG_ERROR("XR D3D11 Compositor Fence Invalid.\n");
				return XR_ERROR_GRAPHICS_DEVICE_INVALID;
			}

			ID3D11Fence* sessionFence;
			printf("XR D3D11 Session Fence Handle: %p\n", sessionFenceHandle);
			ID3D11Device5_OpenSharedFence(device5, sessionFenceHandle, &IID_ID3D11Fence, (void**)&sessionFence);
			printf("XR D3D11 Session Fence: %p\n", (void*)sessionFence);
			if (sessionFence == NULL) {
				LOG_ERROR("XR D3D11 Session Fence Invalid.\n");
				return XR_ERROR_GRAPHICS_DEVICE_INVALID;
			}

			pSession->binding.d3d11.device5 = device5;
			pSession->binding.d3d11.context4 = context4;
			pSession->binding.d3d11.compositorFence = compositorFence;
			pSession->binding.d3d11.sessionFence = sessionFence;

			break;
		}
		case XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR: {
			printf("OpenXR Graphics Binding: XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR\n");
			XrGraphicsBindingVulkanKHR* binding = (XrGraphicsBindingVulkanKHR*)createInfo->next;

			pSession->binding.vk.instance = binding->instance;
			pSession->binding.vk.physicalDevice = binding->physicalDevice;
			pSession->binding.vk.device = binding->device;
			pSession->binding.vk.queueFamilyIndex = binding->queueFamilyIndex;
			pSession->binding.vk.queueIndex = binding->queueIndex;

			break;
		}
		default: {
			LOG_ERROR("XR_ERROR_GRAPHICS_DEVICE_INVALID\n");
			return XR_ERROR_GRAPHICS_DEVICE_INVALID;
		}
	}


	pSession->systemId = createInfo->systemId;
	pSession->hActiveReferenceSpace = HANDLE_DEFAULT;
	pSession->hActiveInteractionProfile = HANDLE_DEFAULT;
	pSession->hPendingInteractionProfile = HANDLE_DEFAULT;
	pSession->activeIsUserPresent = XR_FALSE;
	pSession->pendingIsUserPresent = XR_TRUE;

	*session = XR_TO_OPAQUE_H(XrSession, hSession);

	EnqueueEventDataSessionStateChanged(hSession, XR_SESSION_STATE_IDLE);
	EnqueueEventDataSessionStateChanged(hSession, XR_SESSION_STATE_READY);

	LOG("Created session %llu. %d sessions in use\n", (u64)*session, BLOCK_COUNT(B.session));
	return XR_SUCCESS;
}

XR_PROC xrDestroySession(XrSession session)
{
	LOG_METHOD(xrDestroySession);

	Session*  pSession = XR_OPAQUE_BLOCK_P(session);
	session_h hSession = XR_OPAQUE_BLOCK_H(session);
	BLOCK_RELEASE(B.session, hSession);

	xrReleaseSessionId(pSession->index);

	LOG("%d sessions in use\n", BLOCK_COUNT(B.session));
	return XR_SUCCESS;
}

XR_PROC xrEnumerateReferenceSpaces(
	XrSession             session,
	uint32_t              spaceCapacityInput,
	uint32_t*             spaceCountOutput,
	XrReferenceSpaceType* spaces)
{
	LOG_METHOD(xrEnumerateReferenceSpaces);

	Session*  pSession = XR_OPAQUE_BLOCK_P(session);
	session_h hSession = XR_OPAQUE_BLOCK_H(session);

	static const XrReferenceSpaceType supportedSpaces[] = {
		XR_REFERENCE_SPACE_TYPE_VIEW,
		XR_REFERENCE_SPACE_TYPE_LOCAL,
		XR_REFERENCE_SPACE_TYPE_STAGE,
		XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR,
		XR_REFERENCE_SPACE_TYPE_UNBOUNDED_MSFT,
	};

	*spaceCountOutput = COUNT(supportedSpaces);

	if (spaces == NULL)
		return XR_SUCCESS;

	if (spaceCapacityInput < COUNT(supportedSpaces))
		return XR_ERROR_SIZE_INSUFFICIENT;

	for (u32 i = 0; i < spaceCapacityInput && i < COUNT(supportedSpaces); ++i)
		spaces[i] = supportedSpaces[i];

	return XR_SUCCESS;
}

XR_PROC xrCreateReferenceSpace(XrSession                         session,
                               const XrReferenceSpaceCreateInfo* createInfo,
                               XrSpace*                          space)
{
	LOG("xrCreateReferenceSpace %s\n	" FORMAT_STRUCT_F(XrVector3f) "\n	" FORMAT_STRUCT_F(XrQuaternionf) "\n",
		string_XrReferenceSpaceType(createInfo->referenceSpaceType),
		EXPAND_STRUCT(XrVector3f,    createInfo->poseInReferenceSpace.position),
		EXPAND_STRUCT(XrQuaternionf, createInfo->poseInReferenceSpace.orientation));
	assert(createInfo->next == NULL);

	Session*  pSession = XR_OPAQUE_BLOCK_P(session);
	session_h hSession = XR_OPAQUE_BLOCK_H(session);

	space_h hSpace = BLOCK_CLAIM(xr.block.space, 0);
	HANDLE_CHECK(hSpace, XR_ERROR_LIMIT_REACHED);

	Space* pSpace = BLOCK_PTR(xr.block.space, hSpace);
	pSpace->type = createInfo->type;
	pSpace->hSession = BLOCK_HANDLE(B.session, pSession);
	pSpace->poseInSpace = createInfo->poseInReferenceSpace;
	pSpace->reference.spaceType = createInfo->referenceSpaceType;

	// auto switch to first created space
	if (!HANDLE_VALID(pSession->hActiveReferenceSpace)) {
		EnqueueEventDataReferenceSpaceChangePending(
			hSession, hSpace, 
			createInfo->referenceSpaceType,
			// this is not correct, supposed to be origin of new space in space of prior space
			createInfo->poseInReferenceSpace);
	}

	*space = (XrSpace)pSpace;

	LOG("%d spaces in use\n", BLOCK_COUNT(B.space));
	return XR_SUCCESS;
}

XR_PROC xrGetReferenceSpaceBoundsRect(XrSession            session,
                                      XrReferenceSpaceType referenceSpaceType,
                                      XrExtent2Df*         bounds)
{
	LOG_METHOD(xrGetReferenceSpaceBoundsRect);

	switch (referenceSpaceType) {
		case XR_REFERENCE_SPACE_TYPE_VIEW:
		case XR_REFERENCE_SPACE_TYPE_LOCAL:
		case XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR:
		case XR_REFERENCE_SPACE_TYPE_UNBOUNDED_MSFT:
		case XR_REFERENCE_SPACE_TYPE_COMBINED_EYE_VARJO:
		case XR_REFERENCE_SPACE_TYPE_LOCALIZATION_MAP_ML:
			return XR_SPACE_BOUNDS_UNAVAILABLE;

		case XR_REFERENCE_SPACE_TYPE_STAGE:
		default:
			break;
	}

	Session*  pSession = XR_OPAQUE_BLOCK_P(session);
	session_h hSession = XR_OPAQUE_BLOCK_H(session);
	xrGetReferenceSpaceBounds(pSession->index, bounds);

	LOG("xrGetReferenceSpaceBoundsRect %s\n	windowWidth: %f\n	windowHeight: %f\n",
		string_XrReferenceSpaceType(referenceSpaceType), bounds->width, bounds->height);

	return XR_SUCCESS;
}

XR_PROC xrCreateActionSpace(
	XrSession                      session,
	const XrActionSpaceCreateInfo* createInfo,
	XrSpace*                       space)
{
	XrVector3f    position    = createInfo->poseInActionSpace.position;
	XrQuaternionf orientation = createInfo->poseInActionSpace.orientation;
	LOG("xrCreateActionSpace\n	%s\n	%s " FORMAT_STRUCT_F(XrVector3f) "\n	" FORMAT_STRUCT_F(XrQuaternionf) "\n",
		((Action*)createInfo->action)->actionName, ((Path*)createInfo->subactionPath)->string, EXPAND_STRUCT(XrVector3f, position), EXPAND_STRUCT(XrQuaternionf, orientation));
	assert(createInfo->next == NULL);

	space_h hSpace = BLOCK_CLAIM(B.space, 0);
	HANDLE_CHECK(hSpace, XR_ERROR_LIMIT_REACHED);
	Space* pSpace = BLOCK_PTR(B.space, hSpace);
	pSpace->type = createInfo->type;
	pSpace->poseInSpace = createInfo->poseInActionSpace;
	pSpace->action.hAction = BLOCK_HANDLE(B.action, (Action*)createInfo->action);
	pSpace->action.hSubactionPath = BLOCK_HANDLE(B.path, (Path*)createInfo->subactionPath);

	Session*  pSession = XR_OPAQUE_BLOCK_P(session);
	session_h hSession = XR_OPAQUE_BLOCK_H(session);
	MAP_ADD(pSession->actionSpaces, hSpace, 0);

	*space = (XrSpace)pSpace;

	LOG("%d spaces in use\n", BLOCK_COUNT(B.space));
	return XR_SUCCESS;
}

XR_PROC xrLocateSpace(XrSpace          space,
                      XrSpace          baseSpace,
                      XrTime           time,
                      XrSpaceLocation* location)
{
	LOG_METHOD_ONCE(xrLocateSpace);

	Space*   pSpace = (Space*)space;
	Session* pSession = BLOCK_PTR(B.session, pSpace->hSession);

	XrBool32 isActive = false;
	XrEulerPosef eulerPose = {};
	switch (pSpace->type) {
		case XR_TYPE_ACTION_SPACE_CREATE_INFO:
			auto pSubPath = BLOCK_PTR(B.path, pSpace->action.hSubactionPath);
			auto pAction  = BLOCK_PTR(B.action, pSpace->action.hAction);

			auto hSession   = BLOCK_HANDLE(B.session, pSession);
			auto pActionSet = BLOCK_PTR(B.actionSet, pAction->hActionSet);

			if (hSession != pActionSet->hAttachedToSession) {
				LOG_ERROR("XR_ERROR_ACTIONSET_NOT_ATTACHED\n");
				return XR_ERROR_ACTIONSET_NOT_ATTACHED;
			}

			SubactionState* pState;
			if (GetActionState(pAction, pSubPath, &pState) == XR_ERROR_PATH_UNSUPPORTED) {
				LOG_ERROR("XR_ERROR_PATH_UNSUPPORTED\n");
				return XR_ERROR_PATH_UNSUPPORTED;
			}

			eulerPose = pState->eulerPoseValue;
			isActive = pState->isActive;

			break;
		case XR_TYPE_REFERENCE_SPACE_CREATE_INFO:

			switch (pSpace->reference.spaceType) {
				case XR_REFERENCE_SPACE_TYPE_LOCAL:
				case XR_REFERENCE_SPACE_TYPE_STAGE:
				case XR_REFERENCE_SPACE_TYPE_VIEW:
					xrGetHeadPose(pSession->index, &eulerPose);
					isActive = true;
					break;
				default:
					LOG_ERROR("XR_ERROR_VALIDATION_FAILURE reference space interprocessMode %s\n", string_XrReferenceSpaceType(pSpace->reference.spaceType));
					return XR_ERROR_VALIDATION_FAILURE;
			}

			break;
		default:
			LOG_ERROR("XR_ERROR_VALIDATION_FAILURE space interprocessMode %s\n", string_XrStructureType(pSpace->type));
			return XR_ERROR_VALIDATION_FAILURE;
	}

	switch (xr.instance.graphicsApi) {
		case XR_GRAPHICS_API_OPENGL: break;
		case XR_GRAPHICS_API_VULKAN: break;
		case XR_GRAPHICS_API_D3D11_4:
			XR_CONVERT_D3D11_EULER(eulerPose.euler);
			XR_CONVERT_DD11_POSITION(eulerPose.position);
			break;
		default:
			// Not all APIS might need adjustment?
			break;
	}

	location->pose.orientation = xrQuaternionFromEuler(eulerPose.euler);
	location->pose.position = eulerPose.position;
	location->locationFlags = isActive ? XR_SPACE_LOCATION_ALL_TRACKED : 0;

	if (location->next != NULL) {
		switch (*(XrStructureType*)location->next) {
			case XR_TYPE_SPACE_VELOCITY:
				XrSpaceVelocity* pVelocity = (XrSpaceVelocity*)location->next;
				break;
			default:
				LOG_ERROR("XR_ERROR_VALIDATION_FAILURE xrLocateSpace couldn't deal with pNext: ");
				LogNextChain(location->next);
				return XR_ERROR_VALIDATION_FAILURE;
		}
	};

	return XR_SUCCESS;
}

XR_PROC xrDestroySpace(XrSpace space)
{
	LOG_METHOD(xrDestroySpace);
	auto pSpace = (Space*)space;
	auto pSess = BLOCK_PTR(B.space, pSpace->hSession);
	bHnd hSpace = BLOCK_HANDLE(B.space, pSpace);
	BLOCK_RELEASE(B.space, hSpace);

	// Should I release things on pSpace?

	LOG("%d spaces in use\n", BLOCK_COUNT(B.space));
	return XR_SUCCESS;
}

XR_PROC xrEnumerateViewConfigurations(XrInstance               instance,
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
	for (u32 i = 0; i < COUNT(modes); ++i) {               \
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

	for (u32 i = 0; i < viewCapacityInput && i < *viewCountOutput; ++i)
		xrGetViewConfigurationView(systemId, &views[i]);

	return XR_SUCCESS;
}

/* GL Formats */
static const i64 colorGlSwapFormats[] = {
	GL_SRGB8_ALPHA8,
	GL_SRGB8,
};

static const i64 depthGlSwapFormats[] = {
	// unity can't output depth on opengl
	GL_DEPTH_COMPONENT16,
	GL_DEPTH24_STENCIL8
};

/* VK Formats */
static const i64 colorVkSwapFormats[] = {
	VK_FORMAT_R8G8B8A8_UNORM,
	VK_FORMAT_R8G8B8A8_SRGB,
};

static const i64 depthVkSwapFormats[] = {
	// unity supported
	VK_FORMAT_D16_UNORM,
	VK_FORMAT_D24_UNORM_S8_UINT,
};

/* DX Formats */
static const i64 colorDxSwapFormats[] = {
	DXGI_FORMAT_R8G8B8A8_UNORM,
	DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
};

static const i64 D3D11_DEPTH_SWAP_FORMATS[] = {
	// unity supported
	DXGI_FORMAT_D16_UNORM,
	DXGI_FORMAT_D32_FLOAT_S8X24_UINT,
};

static const i64 D3D11_DEPTH_SWAP_FORMATS_TYPELESS[] = {
	// unity supported
	[DXGI_FORMAT_D16_UNORM]            = DXGI_FORMAT_R16_TYPELESS,
	[DXGI_FORMAT_D32_FLOAT_S8X24_UINT] = DXGI_FORMAT_R32G8X24_TYPELESS,
};

static const i64 DXGI_TO_VK_FORMAT[] = {
	[DXGI_FORMAT_R8G8B8A8_UNORM]       = VK_FORMAT_R8G8B8A8_UNORM,
	[DXGI_FORMAT_R8G8B8A8_UNORM_SRGB]  = VK_FORMAT_R8G8B8A8_SRGB,
	[DXGI_FORMAT_D16_UNORM]            = VK_FORMAT_D16_UNORM,
	[DXGI_FORMAT_D32_FLOAT_S8X24_UINT] = VK_FORMAT_D24_UNORM_S8_UINT,
	[DXGI_FORMAT_R16_TYPELESS]         = VK_FORMAT_D16_UNORM,
	[DXGI_FORMAT_R32G8X24_TYPELESS]    = VK_FORMAT_D24_UNORM_S8_UINT,
};

/* Formats */
static const i64* TO_VK_FORMATS[XR_GRAPHICS_API_COUNT] = {
	[XR_GRAPHICS_API_OPENGL]  = DXGI_TO_VK_FORMAT,
	[XR_GRAPHICS_API_VULKAN]  = DXGI_TO_VK_FORMAT,
	[XR_GRAPHICS_API_D3D11_4] = DXGI_TO_VK_FORMAT,
};

static const int64_t* swapFormats[XR_GRAPHICS_API_COUNT][XR_SWAP_OUTPUT_COUNT] = {
	[XR_GRAPHICS_API_OPENGL] = {
		[XR_SWAP_OUTPUT_COLOR] = colorGlSwapFormats,
		[XR_SWAP_OUTPUT_DEPTH] = depthGlSwapFormats,
	},
	[XR_GRAPHICS_API_VULKAN] = {
		[XR_SWAP_OUTPUT_COLOR] = colorVkSwapFormats,
		[XR_SWAP_OUTPUT_DEPTH] = depthVkSwapFormats,
	},
	[XR_GRAPHICS_API_D3D11_4] = {
		[XR_SWAP_OUTPUT_COLOR] = colorDxSwapFormats,
		[XR_SWAP_OUTPUT_DEPTH] = D3D11_DEPTH_SWAP_FORMATS,
	},
};

constexpr int swapFormatCounts[XR_GRAPHICS_API_COUNT][XR_SWAP_OUTPUT_COUNT] = {
	[XR_GRAPHICS_API_OPENGL] = {
		[XR_SWAP_OUTPUT_COLOR] = COUNT(colorGlSwapFormats),
		[XR_SWAP_OUTPUT_DEPTH] = COUNT(depthGlSwapFormats),
	},
	[XR_GRAPHICS_API_VULKAN] = {
		[XR_SWAP_OUTPUT_COLOR] = COUNT(colorVkSwapFormats),
		[XR_SWAP_OUTPUT_DEPTH] = COUNT(depthVkSwapFormats),
	},
	[XR_GRAPHICS_API_D3D11_4] = {
		[XR_SWAP_OUTPUT_COLOR] = COUNT(colorDxSwapFormats),
		[XR_SWAP_OUTPUT_DEPTH] = COUNT(D3D11_DEPTH_SWAP_FORMATS),
	},
};

XR_PROC xrEnumerateSwapchainFormats(
	XrSession session,
	uint32_t  formatCapacityInput,
	uint32_t* formatCountOutput,
	int64_t*  formats)
{
	LOG_METHOD(xrEnumerateSwapchainFormats);

	Session*  pSession = XR_OPAQUE_BLOCK_P(session);
	session_h hSession = XR_OPAQUE_BLOCK_H(session);
	HANDLE_CHECK(hSession, XR_ERROR_HANDLE_INVALID);

	switch (xr.instance.graphicsApi) {
		case XR_GRAPHICS_API_OPENGL:
		case XR_GRAPHICS_API_VULKAN:
		case XR_GRAPHICS_API_D3D11_4:
			break;
		default:
			LOG_ERROR("xrEnumerateSwapchainFormats Graphics API not supported.");
			return XR_ERROR_RUNTIME_FAILURE;
	}

	const i64* colorFormats = swapFormats[xr.instance.graphicsApi][XR_SWAP_OUTPUT_COLOR];
	const i64* depthFormats = swapFormats[xr.instance.graphicsApi][XR_SWAP_OUTPUT_DEPTH];
	u32 colorCount = swapFormatCounts[xr.instance.graphicsApi][XR_SWAP_OUTPUT_COLOR];
	u32 depthCount = swapFormatCounts[xr.instance.graphicsApi][XR_SWAP_OUTPUT_DEPTH];
	*formatCountOutput = colorCount + depthCount;

	if (formats == NULL)
		return XR_SUCCESS;

	if (formatCapacityInput < colorCount + depthCount)
		return XR_ERROR_SIZE_INSUFFICIENT;

	int index = 0;
	for (u32 i = 0; i < colorCount; ++i) {
		printf("Enumerating Color Format: %llu\n", colorFormats[i]);
		formats[index++] = colorFormats[i];
	}
	for (u32 i = 0; i < depthCount; ++i) {
		printf("Enumerating Depth Format: %llu\n", depthFormats[i]);
		formats[index++] = depthFormats[i];
	}

	return XR_SUCCESS;
}

#define LOG_FLAGS(_flag, _bit)   \
	if (_flags & _flag)             \
		LOG("flag: " #_flag "\n");
#define LOG_XrSwapchainCreateFlags(_)                   \
	({                                                  \
		XrFlags64 _flags = _;                           \
		XR_LIST_BITS_XrSwapchainCreateFlags(LOG_FLAGS); \
	})

XR_PROC xrCreateSwapchain(
	XrSession                    session,
	const XrSwapchainCreateInfo* createInfo,
	XrSwapchain*                 swapchain)
{
	LOG_METHOD(xrCreateSwapchain);

	bool isColor = createInfo->usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
	bool isDepth = createInfo->usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	VkFormat vkFormat = TO_VK_FORMATS[xr.instance.graphicsApi][createInfo->format];
	XrSwapOutput output = isColor ? XR_SWAP_OUTPUT_COLOR : isDepth ? XR_SWAP_OUTPUT_DEPTH : XR_SWAP_OUTPUT_UNKNOWN;

	LOG("  output: %s\n", string_XrSwapOutput(output));
	LOG("  vkFormat: %s\n", string_VkFormat(vkFormat));
	LOG("  sampleCount: %u\n", createInfo->sampleCount);
	LOG("  windowWidth: %u\n", createInfo->width);
	LOG("  windowHeight: %u\n", createInfo->height);
	LOG("  faceCount: %u\n", createInfo->faceCount);
	LOG("  arraySize: %u\n", createInfo->arraySize);
	LOG("  mipCount: %u\n", createInfo->mipCount);

	LOG_XrSwapchainCreateFlags(createInfo->createFlags);

#define PRINT_CREATE_FLAGS(_flag, _bit)  \
	if (createInfo->createFlags & _flag) \
		printf("flag: " #_flag "\n");
	XR_LIST_BITS_XrSwapchainCreateFlags(PRINT_CREATE_FLAGS);
#undef PRINT_CREATE_FLAGS

#define PRINT_USAGE_FLAGS(_flag, _bit)  \
	if (createInfo->usageFlags & _flag) \
		printf("outputs: " #_flag "\n");
	XR_LIST_BITS_XrSwapchainUsageFlags(PRINT_USAGE_FLAGS);
#undef PRINT_USAGE_FLAGS

//	if (createInfo->createFlags & XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT) {
//		LOG_ERROR("XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT unsupported \n");
//		return XR_ERROR_FEATURE_UNSUPPORTED;
//	}

	if (output == XR_SWAP_OUTPUT_UNKNOWN) {
		LOG_ERROR("XR_ERROR_VALIDATION_FAILURE Swap is neither color nor depth!\n");
		return XR_ERROR_VALIDATION_FAILURE;
	}

	Session*  pSession = XR_OPAQUE_BLOCK_P(session);
	session_h hSession = XR_OPAQUE_BLOCK_H(session);

//	XrSwapConfig swapConfig; xrSwapConfig(pSess->systemId, &swapConfig);
//
//	if (createInfo->width == swapConfig.width && pSess->primaryViewConfigurationType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO) {
//		pSess->swapType = XR_SWAP_TYPE_MONO_SINGLE;
//		printf("Setting XR_SWAP_TYPE_MONO_SINGLE for session.\n");
//	} else if (createInfo->width == swapConfig.width && pSess->primaryViewConfigurationType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
//		pSess->swapType = XR_SWAP_TYPE_STEREO_SINGLE;
//		printf("Setting XR_SWAP_TYPE_STEREO_SINGLE for session.\n");
//	} else if (createInfo->width * 2 == swapConfig.width) {
//		pSess->swapType = XR_SWAP_TYPE_STEREO_DOUBLE_WIDE;
//		swapConfig.width *= 2;
//		printf("Setting XR_SWAP_TYPE_STEREO_DOUBLE_WIDE for session.\n");
//	} else if (createInfo->arraySize > 1) {
//		pSess->swapType = XR_SWAP_TYPE_STEREO_TEXTURE_ARRAY;
//		printf("Setting XR_SWAP_TYPE_STEREO_TEXTURE_ARRAY for session.\n");
//	}
//
//	if (createInfo->width != swapConfig.width || createInfo->height != swapConfig.height) {
//		LOG_ERROR("XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED Requested: %d %d Result: %d %d\n",
//			createInfo->width, createInfo->height, swapConfig.width, swapConfig.height);
//		return XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED;
//	}

	swap_h hSwap = BLOCK_CLAIM(B.swap, 0);
	HANDLE_CHECK(hSwap, XR_ERROR_LIMIT_REACHED);
	Swapchain* pSwap = BLOCK_PTR(B.swap, hSwap);
	swap_i     iNodeSwap = HANDLE_INDEX(hSwap);

	pSwap->hSession = hSession;
	pSwap->output = output;
	pSwap->info = (XrSwapchainInfo){
		.createFlags = createInfo->createFlags,
		.usageFlags = createInfo->usageFlags,
		.windowWidth = createInfo->width,
		.windowHeight = createInfo->height,
		.format = vkFormat,
		.sampleCount = createInfo->sampleCount,
		.faceCount = createInfo->faceCount,
		.arraySize = createInfo->arraySize,
		.mipCount = createInfo->mipCount,
	};
	xrCreateSwapchainImages(pSession->index, &pSwap->info, iNodeSwap);

	switch (xr.instance.graphicsApi) {
		case XR_GRAPHICS_API_OPENGL: {
			printf("Creating OpenGL Swap");
			#define DEFAULT_IMAGE_CREATE_INFO(_width, _height, _format, _memObject, _texture, _handle)                    \
				gl.CreateMemoryObjectsEXT(1, &_memObject);                                                                \
				gl.ImportMemoryWin32HandleEXT(_memObject, _width* _height * 4, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, _handle); \
				gl.CreateTextures(GL_TEXTURE_2D, 1, &_texture);                                                           \
				gl.TextureStorageMem2DEXT(_texture, 1, _format, _width, _height, _memObject, 0);
//			for (int i = 0; i < XR_SWAPCHAIN_IMAGE_COUNT; ++i) {
//				HANDLE colorHandle;
//				xrGetSwapchainImage(pSess->index, XR_SWAP_OUTPUT_FLAG_COLOR, &colorHandle);
//				DEFAULT_IMAGE_CREATE_INFO(pSwap->width, pSwap->height, GL_RGBA8, pSwap->texture[i].gl.memObject, pSwap->texture[i].gl.texture, colorHandle);
//				printf("Imported gl swaps texture. Texture: %d MemObject: %d\n", pSwap->texture[i].gl.texture, pSwap->texture[i].gl.memObject);
//			}
//			#undef DEFAULT_IMAGE_CREATE_INFO
			break;
		}
		case XR_GRAPHICS_API_D3D11_4: {
			LOG("Creating D3D11 Swap\n");
			ID3D11Device5* device5 = pSession->binding.d3d11.device5;
			ID3D11DeviceContext4* context4 = pSession->binding.d3d11.context4;
			for (int iImg = 0; iImg < XR_SWAPCHAIN_IMAGE_COUNT; ++iImg) {
				switch (output) {
					case XR_SWAP_OUTPUT_COLOR:
						HANDLE colorHandle;
						xrGetSwapchainImportedImage(pSession->index, iNodeSwap, iImg, &colorHandle);
						ASSERT(colorHandle != NULL, "colorHandle == NULL");
						LOG("Importing d3d11 color viewSwaps. Device: %p Handle: %p\n", (void*)device5, colorHandle);
						DX_CHECK(ID3D11Device5_OpenSharedResource1(device5, colorHandle, &IID_ID3D11Resource, (void**)&pSwap->texture[iImg].d3d11.transferResource));
						DX_CHECK(ID3D11Resource_QueryInterface(pSwap->texture[iImg].d3d11.transferResource, &IID_ID3D11Texture2D, (void**)&pSwap->texture[iImg].d3d11.transferTexture));
						// For color the local resource can directly be the transfer resource.
						pSwap->texture[iImg].d3d11.localResource = pSwap->texture[iImg].d3d11.transferResource;
						pSwap->texture[iImg].d3d11.localTexture = pSwap->texture[iImg].d3d11.transferTexture;
						break;
					case XR_SWAP_OUTPUT_DEPTH:
						HANDLE depthHandle;
						xrGetSwapchainImportedImage(pSession->index, iNodeSwap, iImg, &depthHandle);
						ASSERT(depthHandle != NULL, "depthHandle == NULL");
						LOG("Importing d3d11 transferTexture viewSwaps. Device: %p Handle: %p\n", (void*)device5, depthHandle);
						DX_CHECK(ID3D11Device5_OpenSharedResource1(device5, depthHandle, &IID_ID3D11Resource, (void**)&pSwap->texture[iImg].d3d11.transferResource));
						DX_CHECK(ID3D11Resource_QueryInterface(pSwap->texture[iImg].d3d11.transferResource, &IID_ID3D11Texture2D, (void**)&pSwap->texture[iImg].d3d11.transferTexture));
						LOG("Create d3d11 viewSwaps depth. Device: %p\n", (void*)device5);
						D3D11_TEXTURE2D_DESC desc = {
							.Width = createInfo->width,
							.Height = createInfo->height,
							.MipLevels = createInfo->mipCount,
							.ArraySize = createInfo->arraySize,
							.Format = D3D11_DEPTH_SWAP_FORMATS_TYPELESS[createInfo->format],
							.SampleDesc.Count = createInfo->sampleCount,
							.SampleDesc.Quality = 0,
							.Usage = D3D11_USAGE_DEFAULT,
							.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DEPTH_STENCIL,
							.CPUAccessFlags = 0,
							.MiscFlags = 0,
						};
						DX_CHECK(ID3D11Device5_CreateTexture2D(device5, &desc, NULL, &pSwap->texture[iImg].d3d11.localTexture));
						DX_CHECK(ID3D11Texture2D_QueryInterface(pSwap->texture[iImg].d3d11.localTexture, &IID_ID3D11Resource, (void**)&pSwap->texture[iImg].d3d11.localResource));
						break;
					default:
						LOG_ERROR("XR_ERROR_VALIDATION_FAILURE Swap is neither color nor depth!\n");
						return XR_ERROR_VALIDATION_FAILURE;
				}
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

XR_PROC xrDestroySwapchain(XrSwapchain swapchain)
{
	LOG_METHOD(xrDestroySwapchain);
	auto pSwap = (Swapchain*)swapchain;
	auto pSess = BLOCK_PTR(B.session, pSwap->hSession);

	switch (xr.instance.graphicsApi) {
		case XR_GRAPHICS_API_OPENGL: {
			printf("Destroying OpenGL Swap");
			break;
		}
		case XR_GRAPHICS_API_D3D11_4: {
			printf("Destroying D3D11 Swap\n");
			ID3D11Device5* device5 = pSess->binding.d3d11.device5;
			ID3D11DeviceContext4* context4 = pSess->binding.d3d11.context4;

			for (int i = 0; i < XR_SWAPCHAIN_IMAGE_COUNT; ++i) {
				if (pSwap->texture[i].d3d11.localResource != NULL)
					ID3D11Resource_Release(pSwap->texture[i].d3d11.localResource);
				if (pSwap->texture[i].d3d11.localTexture != NULL)
					ID3D11Texture2D_Release(pSwap->texture[i].d3d11.localTexture);
			}

			break;
		}
		case XR_GRAPHICS_API_VULKAN:
		default:
			return XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED;
	}

	bHnd hSwap = BLOCK_HANDLE(B.swap, pSwap);
	BLOCK_RELEASE(B.swap, hSwap);

	LOG("%d viewSwaps in use\n", BLOCK_COUNT(B.swap));
	return XR_SUCCESS;
}

XR_PROC xrEnumerateSwapchainImages(
	XrSwapchain                 swapchain,
	uint32_t                    imageCapacityInput,
	uint32_t*                   imageCountOutput,
	XrSwapchainImageBaseHeader* images)
{
	LOG_METHOD_ONCE(xrEnumerateSwapchainImages);

	*imageCountOutput = XR_SWAPCHAIN_IMAGE_COUNT;

	if (images == NULL)
		return XR_SUCCESS;

	if (imageCapacityInput != XR_SWAPCHAIN_IMAGE_COUNT)
		return XR_ERROR_SIZE_INSUFFICIENT;

	auto pSwap = (Swapchain*)swapchain;

	switch (images[0].type) {
		case XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR: {
			LOG("Enumerating gl Swapchain Images\n");
			auto pImage = (XrSwapchainImageOpenGLKHR*)images;
			for (u32 i = 0; i < imageCapacityInput && i < XR_SWAPCHAIN_IMAGE_COUNT; ++i) {
				pImage[i].image = pSwap->texture[i].gl.texture;
			}
			break;
		}
		case XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR: {
			LOG("Enumerating d3d11 Swapchain Images\n");
			auto pImage = (XrSwapchainImageD3D11KHR*)images;
			for (u32 i = 0; i < imageCapacityInput && i < XR_SWAPCHAIN_IMAGE_COUNT; ++i) {
				pImage[i].texture = pSwap->texture[i].d3d11.localTexture;
			}
			break;
		}
		default:
			LOG_ERROR("Swap interprocessMode not currently supported\n");
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
	auto pSess = BLOCK_PTR(B.session, pSwap->hSession);

	xrClaimSwapIndex(pSess->index, &pSwap->acquiredSwapIndex);

	*index = pSwap->acquiredSwapIndex;

	return XR_SUCCESS;
}

XR_PROC xrWaitSwapchainImage(
	XrSwapchain                     swapchain,
	const XrSwapchainImageWaitInfo* waitInfo)
{
	LOG_METHOD_ONCE(xrWaitSwapchainImage);
	assert(waitInfo->next == NULL);

	auto pSwap = (Swapchain*)swapchain;
	auto pSess = BLOCK_PTR(B.session, pSwap->hSession);

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
	auto pSess = BLOCK_PTR(B.session, pSwap->hSession);

	xrReleaseSwapIndex(pSess->index, pSwap->acquiredSwapIndex);

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

	Session*  pSession = XR_OPAQUE_BLOCK_P(session);
	session_h hSession = XR_OPAQUE_BLOCK_H(session);

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

	switch (pSession->primaryViewConfigurationType) {
		case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:
//			xrSetSwapTypeAndUsage(pSess->index, XR_SWAP_TYPE_MONO_SINGLE, XR_SWAP_USAGE_COLOR_AND_DEPTH);
			break;
		case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
			break;
		default:
			LOG_ERROR("PrimaryViewConfigurationType not supported.\n");
			return XR_ERROR_RUNTIME_FAILURE;
	}

	switch (xr.instance.graphicsApi) {
		case XR_GRAPHICS_API_OPENGL:
		case XR_GRAPHICS_API_VULKAN:
			PANIC("Graphics API not implemented.\n");
			break;
		case XR_GRAPHICS_API_D3D11_4:   {
			u64 initialTimelineValue = ID3D11Fence_GetCompletedValue(pSession->binding.d3d11.compositorFence);
			xrSetInitialCompositorTimelineValue(pSession->index, initialTimelineValue);
			break;
		}
		default:
			LOG_ERROR("Graphics API not supported.\n");
			return XR_ERROR_RUNTIME_FAILURE;
	}

	if (beginInfo->next != NULL) {
		XrSecondaryViewConfigurationSessionBeginInfoMSFT* secondBeginInfo = (XrSecondaryViewConfigurationSessionBeginInfoMSFT*)beginInfo->next;
		assert(secondBeginInfo->type == XR_TYPE_SECONDARY_VIEW_CONFIGURATION_SESSION_BEGIN_INFO_MSFT);
		assert(secondBeginInfo->next == NULL);
		for (u32 i = 0; i < secondBeginInfo->viewConfigurationCount; ++i) {
			printf("Secondary ViewConfiguration: %d", secondBeginInfo->enabledViewConfigurationTypes[i]);
		}
	}

	return XR_SUCCESS;
}

XR_PROC xrEndSession(
	XrSession session)
{
	LOG_METHOD(xrEndSession);

	Session*  pSession = XR_OPAQUE_BLOCK_P(session);
	session_h hSession = XR_OPAQUE_BLOCK_H(session);

	if (!pSession->running) {
		LOG_ERROR("XR_ERROR_SESSION_NOT_RUNNING\n");
		return XR_ERROR_SESSION_NOT_RUNNING;
	}

	if (pSession->activeSessionState != XR_SESSION_STATE_STOPPING) {
		LOG_ERROR("XR_ERROR_SESSION_NOT_STOPPING\n");
		return XR_ERROR_SESSION_NOT_STOPPING;
	}

	pSession->running = false;

	EnqueueEventDataSessionStateChanged(hSession, XR_SESSION_STATE_IDLE);

	if (pSession->exiting) EnqueueEventDataSessionStateChanged(hSession, XR_SESSION_STATE_EXITING);

	return XR_SUCCESS;
}

XR_PROC xrRequestExitSession(
	XrSession session)
{
	LOG_METHOD(xrRequestExitSession);

	Session*  pSession = XR_OPAQUE_BLOCK_P(session);
	session_h hSession = XR_OPAQUE_BLOCK_H(session);

	if (!pSession->running) {
		LOG_ERROR("XR_ERROR_SESSION_NOT_RUNNING\n");
		return XR_ERROR_SESSION_NOT_RUNNING;
	}

	pSession->exiting = true;

	switch (pSession->activeSessionState) {
		case XR_SESSION_STATE_FOCUSED:
			EnqueueEventDataSessionStateChanged(hSession, XR_SESSION_STATE_VISIBLE);
			FALLTHROUGH;

		case XR_SESSION_STATE_READY:
		case XR_SESSION_STATE_VISIBLE:
			EnqueueEventDataSessionStateChanged(hSession, XR_SESSION_STATE_SYNCHRONIZED);
			FALLTHROUGH;

		case XR_SESSION_STATE_SYNCHRONIZED:
			EnqueueEventDataSessionStateChanged(hSession, XR_SESSION_STATE_STOPPING);
			return XR_SUCCESS;

		default:
			LOG_ERROR("XR_ERROR_VALIDATION_FAILURE %s", string_XrSessionState(pSession->activeSessionState));
			return XR_ERROR_VALIDATION_FAILURE;
	}
}


XR_PROC xrWaitFrame(
	XrSession              session,
	const XrFrameWaitInfo* frameWaitInfo,
	XrFrameState*          frameState)
{
	LOG_METHOD_ONCE(xrWaitFrame);

	if (frameWaitInfo != NULL)
		assert(frameWaitInfo->next == NULL);

	Session*  pSession = XR_OPAQUE_BLOCK_P(session);
	session_h hSession = XR_OPAQUE_BLOCK_H(session);
	if (!pSession->running) {
		LOG_ERROR("XR_ERROR_SESSION_NOT_RUNNING\n");
		return XR_ERROR_SESSION_NOT_RUNNING;
	}

	bool synchronized = pSession->activeSessionState >= XR_SESSION_STATE_SYNCHRONIZED;
	uint64_t compositorTimelineValue;
	xrGetCompositorTimelineValue(pSession->index, &compositorTimelineValue);

	switch (xr.instance.graphicsApi) {
		case XR_GRAPHICS_API_OPENGL:    break;
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
	XrTime frameInterval = xrGetFrameInterval(pSession->index);

	frameState->predictedDisplayPeriod = frameInterval;
	frameState->predictedDisplayTime = currentTime + frameInterval;
	frameState->shouldRender = pSession->activeSessionState == XR_SESSION_STATE_VISIBLE ||
	                           pSession->activeSessionState == XR_SESSION_STATE_FOCUSED;

	XrTime frameWaited = atomic_fetch_add_explicit(&pSession->frameWaited, 1, memory_order_acq_rel);
	XrTimeWaitWin32(&pSession->frameBegan, frameWaited);

	return XR_SUCCESS;
}

XR_PROC xrBeginFrame(
	XrSession               session,
	const XrFrameBeginInfo* frameBeginInfo)
{
	LOG_METHOD_ONCE(xrBeginFrame);

	if (frameBeginInfo != NULL)
		assert(frameBeginInfo->next == NULL);

	Session*  pSession = XR_OPAQUE_BLOCK_P(session);
	session_h hSession = XR_OPAQUE_BLOCK_H(session);
	if (!pSession->running) {
		LOG_ERROR("XR_ERROR_SESSION_NOT_RUNNING\n");
		return XR_ERROR_SESSION_NOT_RUNNING;
	}

	bool frameWaitCalled = pSession->frameBegan == pSession->frameWaited - 1;
	if (!frameWaitCalled) {
		LOG_ERROR("XR_ERROR_CALL_ORDER_INVALID XrWaitFrame not called!\n");
		return XR_ERROR_CALL_ORDER_INVALID;
	}

	// consume the xrWaitFrame call even if frame discarded
	atomic_store_explicit(&pSession->frameBegan, pSession->frameWaited, memory_order_release);

	bool frameEndCalled = pSession->frameEnded == pSession->frameWaited - 1;
	if (!frameEndCalled) {
		LOG_ERROR("XR_FRAME_DISCARDED XrEndFrame not called!\n");
		return XR_FRAME_DISCARDED;
	}

	return XR_SUCCESS;
}

XR_PROC xrEndFrame(XrSession session,
                   const XrFrameEndInfo* frameEndInfo)
{
	LOG_METHOD_ONCE(xrEndFrame);
	assert(frameEndInfo->next == NULL);

	Session*  pSession = XR_OPAQUE_BLOCK_P(session);
	session_h hSession = XR_OPAQUE_BLOCK_H(session);

	if (!pSession->running ||
		pSession->activeSessionState == XR_SESSION_STATE_IDLE ||
		pSession->activeSessionState == XR_SESSION_STATE_EXITING) {
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

	/* Process Layers */
	for (u32 layer = 0; layer < frameEndInfo->layerCount; ++layer) {

		if (frameEndInfo->layers[layer] == NULL) {
			LOG_ERROR("XR_ERROR_LAYER_INVALID\n");
			return XR_ERROR_LAYER_INVALID;
		}

		auto device5 = pSession->binding.d3d11.device5;
		auto context4 = pSession->binding.d3d11.context4;

		switch (frameEndInfo->layers[layer]->type) {
			/* Projection Layer */
			case XR_TYPE_COMPOSITION_LAYER_PROJECTION: {
				auto pProjectionLayer = (XrCompositionLayerProjection*)frameEndInfo->layers[layer];
				assert(pProjectionLayer->viewCount == XR_MAX_VIEW_COUNT); // need to deal with all layers at some point

				for (u32 iView = 0; iView < pProjectionLayer->viewCount; ++iView) {
					auto pView = &pProjectionLayer->views[iView];

					/* Projection Layer View Color */
					{
						auto   pColorSwap = (Swapchain*)pView->subImage.swapchain;
						swap_h hColorSwap = BLOCK_HANDLE(B.swap, pColorSwap);
						swap_i iColorSwap = HANDLE_INDEX(hColorSwap);
						u8     iColorSwapImg = pColorSwap->acquiredSwapIndex;

						xrSetColorSwapId(pSession->index, iView, iColorSwap);

						LOG_ONCE("Color subImage %p " FORMAT_STRUCT_I(XrOffset2Di) " " FORMAT_STRUCT_I(XrExtent2Di) "\n",
							pView->subImage.swapchain,
							EXPAND_STRUCT(XrOffset2Di, pView->subImage.imageRect.offset),
							EXPAND_STRUCT(XrExtent2Di, pView->subImage.imageRect.extent));
					}

					switch (pView->next != NULL ? *(XrStructureType*)pView->next : 0) {
						/* Projection Layer View Depth */
						case XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR: {
							auto   pDepthInfo = (XrCompositionLayerDepthInfoKHR*)pView->next;
							auto   pDepthSwap = (Swapchain*)pDepthInfo->subImage.swapchain;
							swap_h hDepthSwap = BLOCK_HANDLE(B.swap, pDepthSwap);
							swap_i iDepthSwap = HANDLE_INDEX(hDepthSwap);
							u8     iDepthSwapImg = pDepthSwap->acquiredSwapIndex;

							ID3D11DeviceContext4_CopyResource(context4,
								pDepthSwap->texture[iDepthSwapImg].d3d11.transferResource,
								pDepthSwap->texture[iDepthSwapImg].d3d11.localResource);

							xrSetDepthInfo(pSession->index, pDepthInfo->minDepth, pDepthInfo->maxDepth, pDepthInfo->nearZ, pDepthInfo->farZ);
							xrSetDepthSwapId(pSession->index, iView, iDepthSwap);

							LOG_ONCE("Depth subImage %p " FORMAT_STRUCT_I(XrOffset2Di) " " FORMAT_STRUCT_I(XrExtent2Di) " %f %f %f %f\n",
								pDepthInfo->subImage.swapchain,
								EXPAND_STRUCT(XrOffset2Di, pDepthInfo->subImage.imageRect.offset),
								EXPAND_STRUCT(XrExtent2Di, pDepthInfo->subImage.imageRect.extent),
								pDepthInfo->minDepth, pDepthInfo->maxDepth, pDepthInfo->nearZ, pDepthInfo->farZ);

							break;
						}

						default: break;
					}
				}

				break;
			}

			case XR_TYPE_COMPOSITION_LAYER_ALPHA_BLEND_FB:
				LOG_ERROR("XR_TYPE_COMPOSITION_LAYER_ALPHA_BLEND_FB not implemented.");
				break;
			case XR_TYPE_COMPOSITION_LAYER_CUBE_KHR:
				LOG_ERROR("XR_TYPE_COMPOSITION_LAYER_CUBE_KHR not implemented.");
				break;
			case XR_TYPE_COMPOSITION_LAYER_QUAD:
				LOG_ERROR("XR_TYPE_COMPOSITION_LAYER_QUAD not implemented.");
				break;

			default:
				LOG_ERROR("Unknown Composition layer %d", frameEndInfo->layers[layer]->type);
		}
	}

	/* Wait for Graphics Queue To Finish */
	u64 sessionTimelineValue = ++pSession->sessionTimelineValue;
	switch (xr.instance.graphicsApi) {
		case XR_GRAPHICS_API_D3D11_4:   {
			ID3D11DeviceContext4* context4 = pSession->binding.d3d11.context4;
			ID3D11Fence*          sessionFence = pSession->binding.d3d11.sessionFence;

			ID3D11DeviceContext4_Flush(context4);
			ID3D11DeviceContext4_Signal(context4, sessionFence, sessionTimelineValue);
			ID3D11DeviceContext4_Wait(context4, sessionFence, sessionTimelineValue);

			// CPU Wait
			HANDLE eventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
			if (!eventHandle) {
				LOG_ERROR("Failed to create event handle.\n");
				return XR_ERROR_RUNTIME_FAILURE;
			}
			DX_CHECK(ID3D11Fence_SetEventOnCompletion(sessionFence, sessionTimelineValue, eventHandle));
			WaitForSingleObject(eventHandle, INFINITE);
			CloseHandle(eventHandle);

			break;
		}
		default:
			LOG_ERROR("Graphics API not supported.\n");
			return XR_ERROR_RUNTIME_FAILURE;
	}

	/* Finish Frame */
	xrSetSessionTimelineValue(pSession->index, sessionTimelineValue);
	xrProgressCompositorTimelineValue(pSession->index, 0);

	/* Progress state if needed */
	switch (pSession->activeSessionState) {
		case XR_SESSION_STATE_READY:
			EnqueueEventDataSessionStateChanged(hSession, XR_SESSION_STATE_SYNCHRONIZED);
			EnqueueEventDataSessionStateChanged(hSession, XR_SESSION_STATE_VISIBLE);
			EnqueueEventDataSessionStateChanged(hSession, XR_SESSION_STATE_FOCUSED);
			break;
		default: break;
	}

	/* Signal XrWaitFrame */
	XrTime frameBegan = atomic_load_explicit(&pSession->frameBegan, memory_order_acquire);
	XrTimeSignalWin32(&pSession->frameEnded, frameBegan);

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

	Session*  pSession = XR_OPAQUE_BLOCK_P(session);
	session_h hSession = XR_OPAQUE_BLOCK_H(session);

	for (u32 i = 0; i < viewCapacityInput; ++i) {
		XrEyeView eyeView;
		xrGetEyeView(pSession->index, i, &eyeView);

		switch (xr.instance.graphicsApi) {
			case XR_GRAPHICS_API_OPENGL:    break;
			case XR_GRAPHICS_API_VULKAN:    break;
			case XR_GRAPHICS_API_D3D11_4:
				XR_CONVERT_D3D11_EULER(eyeView.euler);
				XR_CONVERT_DD11_POSITION(eyeView.position);
				break;
			default: break;
		}

		float fovHalfXRad = eyeView.fovRad.x / 2.0f;
		float fovHalfYRad = eyeView.fovRad.y / 2.0f;
		float angleLeft;
		float angleRight;
		float angleUp;
		float angleDown;


		switch (pSession->swapClip) {
			default:
			case XR_SWAP_CLIP_NONE:
				angleLeft = -fovHalfXRad;
				angleRight = fovHalfXRad;

				switch (xr.instance.graphicsApi) {
					default:
					case XR_GRAPHICS_API_OPENGL:
					case XR_GRAPHICS_API_VULKAN:
						angleUp = fovHalfYRad;
						angleDown = -fovHalfYRad;
						break;
					case XR_GRAPHICS_API_D3D11_4:
						angleUp = fovHalfYRad;
						angleDown = -fovHalfYRad;

						// Some OXR implementations (Unity) cannot properly calculate the width and height of the projection matrices unless all the angles are negative.
						angleUp -= PI * 2;
						angleDown -= PI * 2;
						angleLeft -= PI * 2;
						angleRight -= PI * 2;

						break;
				}

				break;
			case XR_SWAP_CLIP_STRETCH:
				angleLeft = xrFloatLerp(-fovHalfXRad, fovHalfXRad, eyeView.upperLeftClip.x);
				angleRight = xrFloatLerp(-fovHalfXRad, fovHalfXRad, eyeView.lowerRightClip.x);

				switch (xr.instance.graphicsApi) {
					default:
					case XR_GRAPHICS_API_OPENGL:
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

	u32 pathHash = CalcDJB2(pathString, XR_MAX_PATH_LENGTH);
	for (u32 i = 0; i < XR_PATH_CAPACITY; ++i) {
		if (B.path.keys[i] != pathHash)
			continue;
		if (strncmp(B.path.blocks[i].string, pathString, XR_MAX_PATH_LENGTH)) {
			LOG_ERROR("Path Hash Collision! %s | %s\n", B.path.blocks[i].string, pathString);
			return XR_ERROR_PATH_INVALID;
		}
		printf("Path Handle Found: %d\n    %s\n", i, B.path.blocks[i].string);
		*path = (XrPath)&B.path.blocks[i];
		return XR_SUCCESS;
	}

	auto hPath = BLOCK_CLAIM(B.path, pathHash);
	HANDLE_CHECK(hPath, XR_ERROR_PATH_COUNT_EXCEEDED);

	auto pPath = BLOCK_PTR(B.path, hPath);
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

	auto hFoundActionSet = BLOCK_FIND(B.actionSet, actionSetNameHash);
	if (HANDLE_VALID(hFoundActionSet)) {
		LOG_ERROR("XR_ERROR_LOCALIZED_NAME_DUPLICATED %s\n", createInfo->actionSetName);
		return XR_ERROR_LOCALIZED_NAME_DUPLICATED;
	}

	auto hActionSet = BLOCK_CLAIM(B.actionSet, actionSetNameHash);
	HANDLE_CHECK(hActionSet, XR_ERROR_LIMIT_REACHED);

	auto pActionSet = BLOCK_PTR(B.actionSet, hActionSet);
	*pActionSet = (ActionSet){};

	pActionSet->hAttachedToSession = HANDLE_DEFAULT;

	strncpy((char*)&pActionSet->actionSetName, (const char*)&createInfo->actionSetName, XR_MAX_ACTION_SET_NAME_SIZE);
	strncpy((char*)&pActionSet->localizedActionSetName, (const char*)&createInfo->localizedActionSetName, XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE);
	pActionSet->priority = createInfo->priority;

	*actionSet = (XrActionSet)pActionSet;

	return XR_SUCCESS;
}

XR_PROC xrDestroyActionSet(XrActionSet actionSet)
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
	LOG_METHOD(xrCreateAction);
	assert(createInfo->next == NULL);

	auto pActionSet = (ActionSet*)actionSet;
	auto hActionSet = BLOCK_HANDLE(B.actionSet, pActionSet);
	HANDLE_CHECK(hActionSet, XR_ERROR_VALIDATION_FAILURE);

	if (HANDLE_VALID(pActionSet->hAttachedToSession)) {
		LOG_ERROR("XR_ERROR_ACTIONSETS_ALREADY_ATTACHED\n");
		return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
	}

	if (createInfo->countSubactionPaths > XR_MAX_SUBACTION_PATHS) {
		LOG_ERROR("XR_ERROR_PATH_COUNT_EXCEEDED\n");
		return XR_ERROR_PATH_COUNT_EXCEEDED;
	}

	u32 actionHash = CalcDJB2(createInfo->actionName, XR_MAX_ACTION_NAME_SIZE);
	if (HANDLE_VALID(BLOCK_FIND(B.action, actionHash))) {
		LOG_ERROR("XR_ERROR_NAME_DUPLICATED Action %s\n", createInfo->localizedActionName);
		return XR_ERROR_NAME_DUPLICATED;
	}

	auto hAction = BLOCK_CLAIM(B.action, actionHash);
	HANDLE_CHECK(hAction, XR_ERROR_LIMIT_REACHED);
	auto pAction = BLOCK_PTR(B.action, hAction);
	*pAction = (Action){};

	pAction->hActionSet = hActionSet;
	pAction->actionType = createInfo->actionType;
	pAction->countSubactions = createInfo->countSubactionPaths;
	for (u32 i = 0; i < createInfo->countSubactionPaths; ++i) {
		auto pSubPath = (Path*)createInfo->subactionPaths[i];
		bKey subPathHash = BLOCK_KEY(B.path, pSubPath);
		pAction->hSubactionPaths[i] = BLOCK_HANDLE(B.path, pSubPath);

		bHnd hState = BLOCK_CLAIM(B.state, subPathHash);
		auto pState = BLOCK_PTR(B.state, hState);
		pState->hAction = hAction;
		pAction->hSubactionStates[i] = hState;
	}
	strncpy((char*)&pAction->actionName, (const char*)&createInfo->actionName, XR_MAX_ACTION_SET_NAME_SIZE);
	strncpy((char*)&pAction->localizedActionName, (const char*)&createInfo->localizedActionName, XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE);

	*action = (XrAction)pAction;

	if (HANDLE_VALID(MAP_FIND(pActionSet->actions, actionHash))) {
		LOG_ERROR("XR_ERROR_NAME_DUPLICATED ActionSet %s Action %s\n", pActionSet->actionSetName, createInfo->localizedActionName);
		return XR_ERROR_NAME_DUPLICATED;
	}
	auto hMapAction = MAP_ADD(pActionSet->actions, hAction, actionHash);

	printf("	actionName: %s\n", pAction->actionName);
	printf("	localizedActionName %s\n", pAction->localizedActionName);
	printf("	actionType: %d\n", pAction->actionType);
	printf("	countSubactionPaths: %d\n", pAction->countSubactions);
	for (u32 i = 0; i < createInfo->countSubactionPaths; ++i) {
		Path* pPath = BLOCK_PTR(B.path, pAction->hSubactionPaths[i]);
		printf("		subactionPath: %s\n", pPath->string);
	}

	return XR_SUCCESS;
}

XR_PROC xrDestroyAction(XrAction action)
{
	LOG_METHOD(xrDestroyAction);
	LOG_ERROR("XR_ERROR_FUNCTION_UNSUPPORTED xrDestroyAction\n");

	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

static int CompareSubPath(const char* pSubPath, const char* pPath)
{
	while (*pSubPath != '\0') {
		if (*pSubPath != *pPath)
			return 0;

		pSubPath++;
		pPath++;
	}

	return 1;
}

XR_PROC xrSuggestInteractionProfileBindings(
	XrInstance                                  instance,
	const XrInteractionProfileSuggestedBinding* suggestedBindings)
{
	LOG_METHOD(xrSuggestInteractionProfileBindings);
	LogNextChain(suggestedBindings->next);
	assert(suggestedBindings->next == NULL);
	CHECK_INSTANCE(instance);

	auto pSuggestProfilePath = (Path*)suggestedBindings->interactionProfile;
	auto suggestProfilePathHash = BLOCK_KEY(B.path, pSuggestProfilePath);

	auto hSuggestProfile = BLOCK_FIND(B.profile, suggestProfilePathHash);
	HANDLE_CHECK(hSuggestProfile, XR_ERROR_PATH_UNSUPPORTED);

	auto pSuggestProfile = BLOCK_PTR(B.profile, hSuggestProfile);
	printf("Binding: %s %p\n", pSuggestProfilePath->string, (void*)pSuggestProfilePath);

	const XrActionSuggestedBinding* pSuggest = suggestedBindings->suggestedBindings;

	// Pre-emptively check everything so you don't end up with a half-finished binding suggestion that breaks things.
	// Since this method is rarely run the overhead should be fine.
	for (u32 i = 0; i < suggestedBindings->countSuggestedBindings; ++i) {
		auto pSuggestAction = (Action*)pSuggest[i].action;
		auto pSuggestActionSet = BLOCK_PTR(B.actionSet, pSuggestAction->hActionSet);
		if (HANDLE_VALID(pSuggestActionSet->hAttachedToSession)) {
			LOG_ERROR("XR_ERROR_ACTIONSETS_ALREADY_ATTACHED \n");
			return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
		}
		// Might want to check if there is space too?
	}

	int actsBound = 0;
	for (u32 i = 0; i < suggestedBindings->countSuggestedBindings; ++i) {
		auto pSuggestAction = (Action*)pSuggest[i].action;
		auto pSuggestBindPath = (Path*)pSuggest[i].binding;

		bKey bindPathHash = BLOCK_KEY(B.path, pSuggestBindPath);

		// MAP_FIND could be nested in BLOCK_HANDLE....
		bHnd hSuggestBind = MAP_FIND(pSuggestProfile->bindings, bindPathHash);
		if (!HANDLE_VALID(hSuggestBind)) {
			for (u32 subIndex = 0; subIndex < pSuggestAction->countSubactions; ++subIndex)
				pSuggestAction->hSubactionBindings[subIndex] = HANDLE_DEFAULT;

			printf("Could not find suggested Binding: %s on InteractionProfile: %s\n", pSuggestBindPath->string, pSuggestProfilePath->string);
			continue;
		}

		auto pSuggestBind = BLOCK_PTR(B.binding, hSuggestBind);

		if (pSuggestAction->countSubactions == 0) {
			pSuggestAction->hSubactionBindings[0] = hSuggestBind;

			printf("Bound Action: %s BindingPath: %s No Subaction\n", pSuggestAction->actionName, pSuggestBindPath->string);
			actsBound++;

			continue;
		}

		for (u32 subIndex = 0; subIndex < pSuggestAction->countSubactions; ++subIndex) {
			auto pSubPath = BLOCK_PTR(B.path, pSuggestAction->hSubactionPaths[subIndex]);
			if (!CompareSubPath(pSubPath->string, pSuggestBindPath->string))
				continue;

			pSuggestAction->hSubactionBindings[subIndex] = hSuggestBind;

			printf("Bound Action: %s BindingPath: %s Subaction Index: %d %s\n", pSuggestAction->actionName, pSuggestBindPath->string, subIndex, pSubPath->string);
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

	Session*  pSession = XR_OPAQUE_BLOCK_P(session);
	session_h hSession = XR_OPAQUE_BLOCK_H(session);

	for (u32 i = 0; i < attachInfo->countActionSets; ++i) {
		auto pActSet = (ActionSet*)attachInfo->actionSets[i];
		if (HANDLE_VALID(pActSet->hAttachedToSession)) {
			LOG_ERROR("XR_ERROR_ACTIONSETS_ALREADY_ATTACHED %s\n", pActSet->actionSetName);
			return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
		}
	}

	for (u32 i = 0; i < attachInfo->countActionSets; ++i) {
		auto pActSet = (ActionSet*)attachInfo->actionSets[i];
		auto hActSet = BLOCK_HANDLE(B.actionSet, pActSet);
		HANDLE_CHECK(hActSet, XR_ERROR_HANDLE_INVALID);

		auto actSetHash = BLOCK_KEY(B.actionSet, pActSet);
		MAP_ADD(pSession->actionSets, hActSet, actSetHash);

		memset(&pActSet->states, 0, sizeof(pActSet->states));

		pActSet->hAttachedToSession = hSession;
		printf("Attached ActionSet %s\n", pActSet->actionSetName);
	}

	if (!HANDLE_VALID(pSession->hPendingInteractionProfile)) {
		printf("Setting default interaction profile: %s\n", XR_DEFAULT_INTERACTION_PROFILE);

		XrPath profilePath;
		xrStringToPath((XrInstance)&xr.instance, XR_DEFAULT_INTERACTION_PROFILE, &profilePath);
		auto profileHash = BLOCK_KEY(B.path, (Path*)profilePath);
		auto hProfile = BLOCK_FIND(B.profile, profileHash);
		HANDLE_CHECK(hProfile, XR_ERROR_HANDLE_INVALID);

		pSession->hPendingInteractionProfile = hProfile;
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
	Session*  pSession = XR_OPAQUE_BLOCK_P(session);
	session_h hSession = XR_OPAQUE_BLOCK_H(session);

	InteractionProfile* pProfile = XR_NULL_HANDLE;
	if (HANDLE_VALID(pSession->hActiveInteractionProfile))
		pProfile = BLOCK_PTR(B.profile, pSession->hActiveInteractionProfile);
	else if (HANDLE_VALID(pSession->hPendingInteractionProfile))
		pProfile = BLOCK_PTR(B.profile, pSession->hPendingInteractionProfile);

	Path* pProfilePath = XR_NULL_HANDLE;
	if (pProfile != XR_NULL_HANDLE) {
		pProfilePath = (Path*)pProfile->path;
		LOG("Found InteractionProfile: %s %p\n", pProfilePath->string, (void*)pProfilePath);
	}

	// TODO need to check and see if the path has anything bound to it!!
	interactionProfile->interactionProfile = (XrPath)pProfilePath;

	return XR_SUCCESS;
}

static inline XrResult xrGetActionState(
	XrSession                   session,
	const XrActionStateGetInfo* getInfo,
	SubactionState**            ppState)
{
	Session*  pSession = XR_OPAQUE_BLOCK_P(session);
	session_h hSession = XR_OPAQUE_BLOCK_H(session);

	auto pAction = (Action*)getInfo->action;
	auto pActionSet = BLOCK_PTR(B.actionSet, pAction->hActionSet);
	auto pSubPath = (Path*)getInfo->subactionPath;

	if (hSession != pActionSet->hAttachedToSession) {
		LOG_ERROR("XR_ERROR_ACTIONSET_NOT_ATTACHED\n");
		return XR_ERROR_ACTIONSET_NOT_ATTACHED;
	}

	return GetActionState(pAction, pSubPath, ppState);
}

XR_PROC xrGetActionStateBoolean(
	XrSession                   session,
	const XrActionStateGetInfo* getInfo,
	XrActionStateBoolean*       state)
{
	LOG_METHOD_ONCE(xrGetActionStateBoolean);
	assert(getInfo->next == NULL);

	SubactionState* pState;
	if (xrGetActionState(session, getInfo, &pState) == XR_ERROR_PATH_UNSUPPORTED) {
		LOG_ERROR("XR_ERROR_PATH_UNSUPPORTED\n");
		return XR_ERROR_PATH_UNSUPPORTED;
	}

	state->currentState = pState->boolValue;
	state->changedSinceLastSync = pState->changedSinceLastSync;
	state->lastChangeTime = pState->lastChangeTime;
	state->isActive = pState->isActive;

	return XR_SUCCESS;
}

XR_PROC xrGetActionStateFloat(
	XrSession                   session,
	const XrActionStateGetInfo* getInfo,
	XrActionStateFloat*         state)
{
	LOG_METHOD_ONCE(xrGetActionStateFloat);
	assert(getInfo->next == NULL);

	SubactionState* pState;
	if (xrGetActionState(session, getInfo, &pState) == XR_ERROR_PATH_UNSUPPORTED) {
		LOG_ERROR("XR_ERROR_PATH_UNSUPPORTED\n");
		return XR_ERROR_PATH_UNSUPPORTED;
	}

	state->currentState = pState->floatValue;
	state->changedSinceLastSync = pState->changedSinceLastSync;
	state->lastChangeTime = pState->lastChangeTime;
	state->isActive = pState->isActive;

	return XR_SUCCESS;
}

XR_PROC xrGetActionStateVector2f(
	XrSession                   session,
	const XrActionStateGetInfo* getInfo,
	XrActionStateVector2f*      state)
{
	LOG_METHOD_ONCE(xrGetActionStateVector2f);
	assert(getInfo->next == NULL);

	SubactionState* pState;
	if (xrGetActionState(session, getInfo, &pState) == XR_ERROR_PATH_UNSUPPORTED) {
		LOG_ERROR("XR_ERROR_PATH_UNSUPPORTED\n");
		return XR_ERROR_PATH_UNSUPPORTED;
	}

	state->currentState = pState->vec2Value;
	state->changedSinceLastSync = pState->changedSinceLastSync;
	state->lastChangeTime = pState->lastChangeTime;
	state->isActive = pState->isActive;

	return XR_SUCCESS;
}

XR_PROC xrGetActionStatePose(
	XrSession                   session,
	const XrActionStateGetInfo* getInfo,
	XrActionStatePose*          state)
{
	LOG_METHOD_ONCE(xrGetActionStatePose);

	SubactionState* pState;
	if (xrGetActionState(session, getInfo, &pState) == XR_ERROR_PATH_UNSUPPORTED) {
		LOG_ERROR("XR_ERROR_PATH_UNSUPPORTED\n");
		return XR_ERROR_PATH_UNSUPPORTED;
	}

	state->isActive = XR_TRUE;

	return XR_SUCCESS;
}

XR_PROC xrSyncActions(
	XrSession                session,
	const XrActionsSyncInfo* syncInfo)
{
	LOG_METHOD_ONCE(xrSyncActions);
	LogNextChain(syncInfo->next);

	Session*  pSession = XR_OPAQUE_BLOCK_P(session);
	session_h hSession = XR_OPAQUE_BLOCK_H(session);
	if (pSession->activeSessionState != XR_SESSION_STATE_FOCUSED) {
		LOG("XR_SESSION_NOT_FOCUSED\n");
		return XR_SESSION_NOT_FOCUSED;
	}

	auto time = xrGetTime();
	for (u32 si = 0; si < syncInfo->countActiveActionSets; ++si) {

		auto pActionSet = (ActionSet*)syncInfo->activeActionSets[si].actionSet;
		auto actionSetPath = (Path*)syncInfo->activeActionSets[si].subactionPath;
		bKey actionSetHash = BLOCK_KEY(B.actionSet, pActionSet);

		for (u32 ai = 0; ai < pActionSet->actions.count; ++ai) {
			bHnd hAction = pActionSet->actions.handles[ai];
			auto pAction = BLOCK_PTR(B.action, hAction);

			for (u32 sai = 0; sai < pAction->countSubactions; ++sai) {
				bHnd hBind = pAction->hSubactionBindings[sai];
				if (!HANDLE_VALID(hBind)) {
					if (!pAction->errorLogged[sai]) {
						pAction->errorLogged[sai] = true;
						LOG("Warning! %s not bound!\n", pAction->actionName);
					}
					continue;
				}

				bHnd hState = pAction->hSubactionStates[sai];
				if (!HANDLE_VALID(hState)) {
					LOG("State Invalid! %s\n", pAction->actionName);
					continue;
				}

				auto pBind = BLOCK_PTR(B.binding, hBind);
				auto pState = BLOCK_PTR(B.state, hState);
				auto index = HANDLE_INDEX(hState);

				// need to understand this priority again
//				if (pState->lastSyncedPriority > pActSet->priority &&
//					pState->lastChangeTime == currentTime)
//					continue;

				auto priorState = *pState;
				pState->changedSinceLastSync = pBind->func(pSession->index, pState);
				pState->lastChangeTime = time;
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
	LOG("%s %d %s\n",
		string_XrViewConfigurationType(viewConfigurationType),
		viewIndex,
		string_XrVisibilityMaskTypeKHR(visibilityMaskType));

	// this is such an easy way to look stuff up, should I really change it to handles?
//	Session* pSession = (Session*)session;
//	Session* pSession = BLOCK_PTR(B.session, (session_h)(u16)(u64)session)
	Session* pSession = XR_OPAQUE_BLOCK_P(session);

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

		printf("XR D3D11 Device: %p %d\n", (void*)device, featureLevel);
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

//	LOG_ERROR("xrGetInstanceProcAddr XR_ERROR_FUNCTION_UNSUPPORTED %s\n", name);
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

#undef B

#undef MID_OPENXR_IMPLEMENTATION
#endif //MID_OPENXR_IMPLEMENTATION