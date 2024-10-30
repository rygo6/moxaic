#pragma once

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

//
//// Mid OpenXR
typedef uint32_t XrHash;
typedef uint32_t XrHandle;


typedef enum XrGraphicsApi {
	XR_GRAPHICS_API_OPENGL,
	XR_GRAPHICS_API_OPENGL_ES,
	XR_GRAPHICS_API_VULKAN,
	XR_GRAPHICS_API_D3D11_1,
	XR_GRAPHICS_API_COUNT,
} XrGraphicsApi;

void midXrInitialize(XrGraphicsApi graphicsApi);
void midXrCreateSession(XrGraphicsApi graphicsApi, XrHandle* pSessionHandle);
void midXrGetReferenceSpaceBounds(XrHandle sessionHandle, XrExtent2Df* pBounds);
void midXrClaimFramebufferImages(XrHandle sessionHandle, int imageCount, HANDLE* pHandle);
void midXrAcquireSwapchainImage(XrHandle sessionHandle, uint32_t* pIndex);
void midXrWaitFrame(XrHandle sessionHandle);
void midXrGetViewConfiguration(XrHandle sessionHandle, int viewIndex, XrViewConfigurationView* pView);
void midXrGetView(XrHandle sessionHandle, int viewIndex, XrView* pView);
void midXrBeginFrame(XrHandle sessionHandle);
void midXrEndFrame(XrHandle sessionHandle);

//
//// Mid OpenXR Constants
#define MIDXR_DEFAULT_WIDTH        1024
#define MIDXR_DEFAULT_HEIGHT       1024
#define MIDXR_DEFAULT_SAMPLES      1
#define MIDXR_OPENGL_MAJOR_VERSION 4
#define MIDXR_OPENGL_MINOR_VERSION 6

#define XR_SWAP_COUNT 2

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
	uint32_t       imageArrayIndex;
} XrSubView;

typedef struct XrSpaceBounds {
	XrStructureType    type;
	void* XR_MAY_ALIAS next;
	XrRect2Di          imageRect;
} XrSpaceBounds;

//typedef struct XrFrameEndInfo {
//	XrStructureType    type;
//	void* XR_MAY_ALIAS next;
//	XrRect2Di          imageRect;
//	uint32_t       imageArrayIndex;
//} XrFrameEndInfo;

//typedef struct XrSubView {
//	XrStructureType    type;
//	void* XR_MAY_ALIAS next;
//	XrRect2Di          imageRect;
//	uint32_t       imageArrayIndex;
//} XrSubView;



// maybe?
//typedef struct XrFrameBeginSwapPoolInfo {
//	XrStructureType             type;
//	const void* XR_MAY_ALIAS    next;
//} XrFrameBeginSwapPoolInfo;


#define XR_CHECK(_command)                       \
	({                                           \
		XrResult result = _command;              \
		if (__builtin_expect(!!(_command), 0)) { \
			return result;                       \
		}                                        \
	})
#define CONTAINER(_type, _capacity)                                                        \
	typedef struct __attribute((aligned(4))) _type##Container {                            \
		uint32_t count;                                                                    \
		_type    data[_capacity];                                                          \
	} _type##Container;                                                                    \

#define CONTAINER_HASHED(_type, _capacity)                                                                \
	typedef struct _type##Container {                                                                     \
		uint32_t count;                                                                                   \
		_type    data[_capacity];                                                                         \
		XrHash   hash[_capacity];                                                                         \
	} _type##Container;                                                                                   \
	static XrResult Claim##_type(_type##Container* p##_type##s, _type** pp##_type, XrHash hash)           \
	{                                                                                                     \
		if (p##_type##s->count >= COUNT(p##_type##s->data)) return XR_ERROR_LIMIT_REACHED;                \
		const uint32_t handle = p##_type##s->count++;                                                     \
		p##_type##s->hash[handle] = hash;                                                                 \
		*pp##_type = &p##_type##s->data[handle];                                                          \
		return XR_SUCCESS;                                                                                \
	}                                                                                                     \
	static inline XrHash Get##_type##Hash(const _type##Container* p##_type##s, const _type* p##_type)     \
	{                                                                                                     \
		const int handle = p##_type - p##_type##s->data;                                                  \
		return p##_type##s->hash[handle];                                                                 \
	}                                                                                                     \
	static inline _type* Get##_type##ByHash(_type##Container* p##_type##s, const XrHash hash)             \
	{                                                                                                     \
		for (int i = 0; i < p##_type##s->count; ++i) {                                                    \
			if (p##_type##s->hash[i] == hash)                                                             \
				return &p##_type##s->data[i];                                                             \
		}                                                                                                 \
		return NULL;                                                                                      \
	}                                                                                                     \

// ya we want to switch to do this below rather than the above macro monstrosities
typedef struct Container {
	uint32_t count;
	__attribute((aligned(8)))
	// could maybe switch this to be handles in larger pool
	uint8_t  data;
} Container;

// maybe this should return the handle?
#define ClaimHandle(_container, _pValue) XR_CHECK(_ClaimHandle((Container*)&_container, sizeof(_container.data[0]), COUNT(_container.data), (void**)&_pValue))
static XrResult _ClaimHandle(Container* pContainer, int stride, int capacity, void** ppValue)
{
	if (pContainer->count >= capacity) return XR_ERROR_LIMIT_REACHED;
	const uint32_t handle = pContainer->count++;
	*ppValue = &pContainer->data + (handle * stride);
	return XR_SUCCESS;
}
#define GetHandle(_container, _pValue) _GetHandle((Container*)&_container, sizeof(_container.data[0]), _pValue)
static XrHandle _GetHandle(const Container* pContainer, int stride, const void* pValue)
{
	return (XrHandle)((pValue - (void*)&pContainer->data) / stride);
}

#define MIDXR_MAX_PATHS 128
typedef struct Path {
	char string[XR_MAX_PATH_LENGTH];
} Path;
CONTAINER_HASHED(Path, MIDXR_MAX_PATHS)

typedef struct Binding {
	XrPath path;
	int (*func)(void*);
} Binding;
CONTAINER_HASHED(Binding, 16)

typedef struct InteractionProfile {
	XrPath           interactionProfile;
	BindingContainer bindings;
} InteractionProfile;
CONTAINER_HASHED(InteractionProfile, 16)

#define MIDXR_MAX_ACTIONS 16
typedef Binding* XrBinding;
CONTAINER(XrBinding, MIDXR_MAX_ACTIONS)

#define MIDXR_MAX_SUBACTION_PATHS 2
typedef struct Action {
	XrActionSet        actionSet;
	uint32_t           countSubactionPaths;
	XrBindingContainer subactionBindings[MIDXR_MAX_SUBACTION_PATHS];
	XrPath             subactionPaths[MIDXR_MAX_SUBACTION_PATHS];
	XrActionType       actionType;
	char               actionName[XR_MAX_ACTION_NAME_SIZE];
	char               localizedActionName[XR_MAX_LOCALIZED_ACTION_NAME_SIZE];
} Action;
CONTAINER(Action, MIDXR_MAX_ACTIONS)

#define MIDXR_MAX_BOUND_ACTIONS 4
CONTAINER(XrAction, MIDXR_MAX_BOUND_ACTIONS)

typedef struct SubactionState {
	uint32_t lastSyncedPriority;

	// actual layout from OXR to memcpy
	float    currentState;
	XrBool32 changedSinceLastSync;
	XrTime   lastChangeTime;
	XrBool32 isActive;
} SubactionState;
typedef struct ActionState {
	SubactionState subactionStates[MIDXR_MAX_SUBACTION_PATHS];
} ActionState;
CONTAINER(ActionState, MIDXR_MAX_ACTIONS)

typedef struct ActionSetState {
	ActionStateContainer actionStates;
	XrActionSet          actionSet;
} ActionSetState;
CONTAINER_HASHED(ActionSetState, 4)

typedef struct ActionSet {
	char            actionSetName[XR_MAX_ACTION_SET_NAME_SIZE];
	uint32_t        priority;
	XrSession       attachedToSession;
	ActionContainer Actions;
} ActionSet;
CONTAINER_HASHED(ActionSet, 4)

typedef struct Space {
	XrReferenceSpaceType referenceSpaceType;
	XrPosef              poseInReferenceSpace;
} Space;
CONTAINER(Space, 4)

#define MIDXR_SWAP_COUNT 2
typedef struct Swapchain {
	// we should replace XrHandle pointers with uint16_t handle offsets
	XrSession session;

	uint32_t swapIndex;
	union {
		struct {
			GLuint texture;
			GLuint memObject;
		} gl[MIDXR_SWAP_COUNT];
		ID3D11Texture2D* d3d11[MIDXR_SWAP_COUNT];
	} color;

	XrSwapchainUsageFlags usageFlags;
	int64_t               format;
	uint32_t              sampleCount;
	uint32_t              width;
	uint32_t              height;
	uint32_t              faceCount;
	uint32_t              arraySize;
	uint32_t              mipCount;
} Swapchain;
CONTAINER(Swapchain, 4)

//typedef struct XrGlTexture {
//	GLuint memObject;
//	GLuint texture;
//} XrGlTexture;
//typedef struct XrGlFramebufferTexture {
//	MxcGlTexture color;
//	MxcGlTexture normal;
//	MxcGlTexture depth;
//	MxcGlTexture gbuffer;
//} XrGlFramebufferTexture;

#define MIDXR_MAX_SESSIONS 4
typedef struct Session {
	XrInstance              instance;
	XrTime                  lastPredictedDisplayTime;
	Space                   referenceSpace;
	SwapchainContainer      Swapchains;
	ActionSetStateContainer actionSetStates;

	XrViewConfigurationType primaryViewConfigurationType;
	union {
		XrGraphicsBindingOpenGLWin32KHR gl;
//		XrGraphicsBindingD3D11KHR       d3d11;
		XrGraphicsBindingVulkanKHR      vk;
	} binding;

} Session;
CONTAINER(Session, MIDXR_MAX_SESSIONS)

#define MIDXR_MAX_INSTANCES 2
typedef struct Instance {
	char                        applicationName[XR_MAX_APPLICATION_NAME_SIZE];
	XrSystemId                  systemId;
	XrFormFactor                systemFormFactor;
	SessionContainer            sessions;
	ActionSetContainer          actionSets;
	InteractionProfileContainer interactionProfiles;
	PathContainer               paths;

	XrGraphicsApi graphicsApi;
	union {
		struct {
			ID3D11Device*         device;
			ID3D11Device1*        device1;
			LUID                  adapterLuid;
			D3D_FEATURE_LEVEL     minFeatureLevel;
		} d3d11;
	} graphics;

} Instance;
CONTAINER(Instance, MIDXR_MAX_INSTANCES)
// only 1 probably
//static Instance instance;

// I think I want to put all of the above in an arena pool

//
//// MidOpenXR Implementation
#ifdef MID_OPENXR_IMPLEMENTATION

#define ENABLE_DEBUG_LOG_METHOD
#ifdef ENABLE_DEBUG_LOG_METHOD
#define LOG_METHOD(_name) printf(#_name "\n")
#else
#define LOG_METHOD(_name)
#endif

#ifdef WIN32
#ifndef MID_WIN32_DEBUG
#define MID_WIN32_DEBUG
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


static InstanceContainer instances;

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

XrTime GetXrTime()
{
	LARGE_INTEGER frequency;
	LARGE_INTEGER counter;
	if (!QueryPerformanceFrequency(&frequency)) {
		return 0;
	}
	if (!QueryPerformanceCounter(&counter)) {
		return 0;
	}
	uint64_t nanoseconds = (uint64_t)((counter.QuadPart * 1e9) / frequency.QuadPart);
	return nanoseconds;
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
	XrInstance        instance,
	BindingContainer* pBindings,
	Path*             pPath,
	int (*func)(void*))
{
	Instance* pInstance = (Instance*)instance;

	int pathHash = GetPathHash(&pInstance->paths, pPath);
	for (int i = 0; i < pBindings->count; ++i) {
		if (GetPathHash(&pInstance->paths, (Path*)pBindings->data[i].path) == pathHash) {
			fprintf(stderr, "Trying to register path hash twice! %s %d\n", pPath->string, pathHash);
			return XR_ERROR_PATH_INVALID;
		}
	}

	Binding* pBinding;
	ClaimBinding(pBindings, &pBinding, pathHash);
	pBinding->path = (XrPath)pPath;
	pBinding->func = func;

	return XR_SUCCESS;
}

typedef struct BindingDefinition {
	int (*func)(void*);
	const char path[XR_MAX_PATH_LENGTH];
} BindingDefinition;
static XrResult InitStandardBindings(XrInstance instance)
{
	Instance* pInstance = (Instance*)instance;

	{
#define OCULUS_INTERACTION_PROFILE       "/interaction_profiles/oculus/touch_controller"
#define BINDING_DEFINITION(_func, _path) (int (*)(void*)) _func, _path
		const BindingDefinition bindingDefinitions[] = {
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
#undef BINDING_DEFINITION

		XrPath bindingSetPath;
		xrStringToPath(instance, OCULUS_INTERACTION_PROFILE, &bindingSetPath);
		XrHash bindingSetHash = GetPathHash(&pInstance->paths, (const Path*)bindingSetPath);

		InteractionProfile* pBindingSet;
		XR_CHECK(ClaimInteractionProfile(&pInstance->interactionProfiles, &pBindingSet, bindingSetHash));

		pBindingSet->interactionProfile = bindingSetPath;

		for (int i = 0; i < COUNT(bindingDefinitions); ++i) {
			XrPath path;
			xrStringToPath(instance, bindingDefinitions[i].path, &path);
			RegisterBinding(instance, &pBindingSet->bindings, (Path*)path, bindingDefinitions[i].func);
		}
	}

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateApiLayerProperties(
	uint32_t              propertyCapacityInput,
	uint32_t*             propertyCountOutput,
	XrApiLayerProperties* properties)
{
	LOG_METHOD(xrEnumerateApiLayerProperties);
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateInstanceExtensionProperties(
	const char*            layerName,
	uint32_t               propertyCapacityInput,
	uint32_t*              propertyCountOutput,
	XrExtensionProperties* properties)
{
	LOG_METHOD(xrEnumerateInstanceExtensionProperties);

	const XrExtensionProperties extensionProperties[] = {
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
	};

	*propertyCountOutput = COUNT(extensionProperties);

	if (!properties)
		return XR_SUCCESS;

	memcpy(properties, &extensionProperties, sizeof(XrExtensionProperties) * propertyCapacityInput);

	return XR_SUCCESS;
}

static struct {
	PFNGLCREATEMEMORYOBJECTSEXTPROC     CreateMemoryObjectsEXT;
	PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC ImportMemoryWin32HandleEXT;
	PFNGLCREATETEXTURESPROC             CreateTextures;
	PFNGLTEXTURESTORAGEMEM2DEXTPROC     TextureStorageMem2DEXT;
} gl;

XRAPI_ATTR XrResult XRAPI_CALL xrCreateInstance(
	const XrInstanceCreateInfo* createInfo,
	XrInstance*                 instance)
{
	LOG_METHOD(xrCreateInstance);

	Instance* pInstance;
	ClaimHandle(instances, pInstance);

	for (int i = 0; i < createInfo->enabledApiLayerCount; ++i) {
		printf("Enabled API Layer: %s\n", createInfo->enabledApiLayerNames[i]);
	}

	for (int i = 0; i < createInfo->enabledExtensionCount; ++i) {
		printf("Enabled Extension: %s\n", createInfo->enabledExtensionNames[i]);
		if (strncmp(createInfo->enabledExtensionNames[i], XR_KHR_OPENGL_ENABLE_EXTENSION_NAME, XR_MAX_EXTENSION_NAME_SIZE) == 0)
			pInstance->graphicsApi = XR_GRAPHICS_API_OPENGL;
		if (strncmp(createInfo->enabledExtensionNames[i], XR_KHR_D3D11_ENABLE_EXTENSION_NAME, XR_MAX_EXTENSION_NAME_SIZE) == 0)
			pInstance->graphicsApi = XR_GRAPHICS_API_D3D11_1;
	}

	strncpy((char*)&pInstance->applicationName, (const char*)&createInfo->applicationInfo, XR_MAX_APPLICATION_NAME_SIZE);
	*instance = (XrInstance)pInstance;

	InitStandardBindings(*instance);
	midXrInitialize(pInstance->graphicsApi);

	switch (pInstance->graphicsApi){
		case XR_GRAPHICS_API_OPENGL:
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
			break;
		case XR_GRAPHICS_API_OPENGL_ES: break;
		case XR_GRAPHICS_API_VULKAN:    break;
		case XR_GRAPHICS_API_D3D11_1:   {

			// do I need to close these?
//			ID3D11Device*        renderDevice;
//			ID3D11DeviceContext* renderContext;
//			DX_REQUIRE(D3D11CreateDevice(
//				NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG,
//				(D3D_FEATURE_LEVEL[]) {D3D_FEATURE_LEVEL_11_1}, 1, D3D11_SDK_VERSION,
//				&renderDevice, &pInstance->graphics.d3d11.minFeatureLevel, &renderContext))
//			assert(pInstance->graphics.d3d11.minFeatureLevel == D3D_FEATURE_LEVEL_11_1);
//			DX_REQUIRE(ID3D11Device_QueryInterface(renderDevice, &IID_ID3D11Device1, (void**)&pInstance->graphics.d3d11.device1))
//			DX_REQUIRE(ID3D11DeviceContext_QueryInterface(renderContext, &IID_ID3D11DeviceContext1, (void**) &pInstance->graphics.d3d11.context1))
//			printf("DX11 Adapter Feature Level: 0x%X Device: %p context %p\n",
//					pInstance->graphics.d3d11.minFeatureLevel, pInstance->graphics.d3d11.device1, pInstance->graphics.d3d11.context1);

//			ID3D11Debug* debug;
//			DX_REQUIRE(ID3D11Device1_QueryInterface(pInstance->graphics.d3d11.device1, &IID_ID3D11Debug, (void**)&debug))
//			ID3D11Debug_ReportLiveDeviceObjects(debug, D3D11_RLDO_SUMMARY);

//			ID3D11Debug_Release(debug);
//			ID3D11Device_Release(renderDevice);
//			ID3D11DeviceContext_Release(renderContext);

			pInstance->graphics.d3d11.minFeatureLevel = D3D_FEATURE_LEVEL_11_1;

			IDXGIFactory1* factory = NULL;
			DX_CHECK(CreateDXGIFactory1(&IID_IDXGIFactory, (void**)&factory));

			IDXGIAdapter* adapter = NULL;
			for (UINT i = 0; IDXGIFactory1_EnumAdapters(factory, i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
				DXGI_ADAPTER_DESC desc;
				DX_CHECK(IDXGIAdapter_GetDesc(adapter, &desc));
				pInstance->graphics.d3d11.adapterLuid = desc.AdapterLuid;
				wprintf(L"DX11 Adapter %d Name: %ls Description: %ld:%lu\n",
						i, desc.Description, desc.AdapterLuid.HighPart,	desc.AdapterLuid.LowPart);
				break;
			}

			IDXGIFactory4* factory4;
			DX_CHECK(IDXGIFactory1_QueryInterface(factory, &IID_IDXGIFactory4, (void**)&factory4));

			IDXGIAdapter* adapter2 = NULL;
			IDXGIFactory4_EnumAdapterByLuid(factory4, pInstance->graphics.d3d11.adapterLuid, &IID_IDXGIAdapter, (void**)&adapter2);
			DXGI_ADAPTER_DESC desc2;
			DX_CHECK(IDXGIAdapter_GetDesc(adapter2, &desc2));
			wprintf(L"DX11 Adapter2 Name: %ls Description: %ld:%lu\n", desc2.Description, desc2.AdapterLuid.HighPart, desc2.AdapterLuid.LowPart);


			printf("Adapters %p == %p\n", adapter, adapter2);

			IDXGIAdapter_Release(adapter);
			IDXGIFactory1_Release(factory);

			break;
		}
		case XR_GRAPHICS_API_COUNT:     break;
	}

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroyInstance(
	XrInstance instance)
{
	LOG_METHOD(xrDestroyInstance);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProperties(
	XrInstance            instance,
	XrInstanceProperties* instanceProperties)
{
	LOG_METHOD(xrGetInstanceProperties);

	instanceProperties->runtimeVersion = XR_MAKE_VERSION(0, 1, 0);
	strncpy(instanceProperties->runtimeName, "Moxaic OpenXR", XR_MAX_RUNTIME_NAME_SIZE);

	return XR_SUCCESS;
}

static XrSessionState SessionState = XR_SESSION_STATE_UNKNOWN;
static XrSessionState PendingSessionState = XR_SESSION_STATE_IDLE;

XRAPI_ATTR XrResult XRAPI_CALL xrPollEvent(
	XrInstance         instance,
	XrEventDataBuffer* eventData)
{
	LOG_METHOD(xrPollEvent);

	if (SessionState != PendingSessionState) {
		// for debugging, this needs to be manually set elsewhere
		if (PendingSessionState == XR_SESSION_STATE_IDLE)
			PendingSessionState = XR_SESSION_STATE_READY;

		eventData->type = XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED;

		XrEventDataSessionStateChanged* pEventData = (XrEventDataSessionStateChanged*)eventData;
		pEventData->state = PendingSessionState;

		SessionState = PendingSessionState;

		return XR_SUCCESS;
	}

	return XR_EVENT_UNAVAILABLE;
}

XRAPI_ATTR XrResult XRAPI_CALL xrResultToString(
	XrInstance instance,
	XrResult   value,
	char       buffer[XR_MAX_RESULT_STRING_SIZE])
{
	LOG_METHOD(xrResultToString);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrStructureTypeToString(
	XrInstance      instance,
	XrStructureType value,
	char            buffer[XR_MAX_STRUCTURE_NAME_SIZE])
{
	LOG_METHOD(xrStructureTypeToString);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

#define XR_HEAD_MOUNTED_DISPLAY_SYSTEM_ID 1
#define XR_HANDHELD_DISPLAY_SYSTEM_ID 2
XRAPI_ATTR XrResult XRAPI_CALL xrGetSystem(
	XrInstance             instance,
	const XrSystemGetInfo* getInfo,
	XrSystemId*            systemId)
{
	LOG_METHOD(xrGetSystem);

	Instance* pInstance = (Instance*)instance;

	assert(getInfo->next == NULL);

	switch (getInfo->formFactor){
		case XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY:
			pInstance->systemFormFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
			pInstance->systemId = XR_HEAD_MOUNTED_DISPLAY_SYSTEM_ID;
			*systemId = XR_HEAD_MOUNTED_DISPLAY_SYSTEM_ID;
			break;
		case XR_FORM_FACTOR_HANDHELD_DISPLAY:
			pInstance->systemFormFactor = XR_FORM_FACTOR_HANDHELD_DISPLAY;
			pInstance->systemId = XR_HANDHELD_DISPLAY_SYSTEM_ID;
			*systemId = XR_HANDHELD_DISPLAY_SYSTEM_ID;
			break;
		default:
			*systemId = XR_NULL_SYSTEM_ID;
			return XR_ERROR_FORM_FACTOR_UNSUPPORTED;
	}

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetSystemProperties(
	XrInstance          instance,
	XrSystemId          systemId,
	XrSystemProperties* properties)
{
	LOG_METHOD(xrGetSystemProperties);

	switch (systemId){

		case XR_HEAD_MOUNTED_DISPLAY_SYSTEM_ID:
			properties->systemId = XR_HEAD_MOUNTED_DISPLAY_SYSTEM_ID;
			properties->vendorId = 0;
			strncpy(properties->systemName, "MoxaicHMD", XR_MAX_SYSTEM_NAME_SIZE);
			properties->graphicsProperties.maxLayerCount = XR_MIN_COMPOSITION_LAYERS_SUPPORTED;
			properties->graphicsProperties.maxSwapchainImageWidth = DEFAULT_WIDTH;
			properties->graphicsProperties.maxSwapchainImageHeight = DEFAULT_HEIGHT;
			properties->trackingProperties.orientationTracking = XR_TRUE;
			properties->trackingProperties.positionTracking = XR_TRUE;
			break;
		case XR_HANDHELD_DISPLAY_SYSTEM_ID:
			properties->systemId = XR_HANDHELD_DISPLAY_SYSTEM_ID;
			properties->vendorId = 0;
			strncpy(properties->systemName, "MoxaicHandheld", XR_MAX_SYSTEM_NAME_SIZE);
			properties->graphicsProperties.maxLayerCount = XR_MIN_COMPOSITION_LAYERS_SUPPORTED;
			properties->graphicsProperties.maxSwapchainImageWidth = DEFAULT_WIDTH;
			properties->graphicsProperties.maxSwapchainImageHeight = DEFAULT_HEIGHT;
			properties->trackingProperties.orientationTracking = XR_TRUE;
			properties->trackingProperties.positionTracking = XR_TRUE;
			break;
		default:
			return XR_ERROR_HANDLE_INVALID;
	}

	return XR_SUCCESS;
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

	XrHandle createdHandle;
	midXrCreateSession(pInstance->graphicsApi, &createdHandle);

	Session* pClaimedSession;
	ClaimHandle(pInstance->sessions, pClaimedSession);
	XrHandle claimedHandle = GetHandle(pInstance->sessions, pClaimedSession);

	// these should be the same but probably want to get these handles better...
	assert(createdHandle == claimedHandle);

	pClaimedSession->instance = instance;

	switch (*(XrStructureType*)createInfo->next) {
		case XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR:
			printf("OpenXR Graphics Binding: XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR\n");
			pClaimedSession->binding.gl = *(XrGraphicsBindingOpenGLWin32KHR*)&createInfo->next;
			*session = (XrSession)pClaimedSession;
			return XR_SUCCESS;
		case XR_TYPE_GRAPHICS_BINDING_D3D11_KHR:
			printf("OpenXR Graphics Binding: XR_TYPE_GRAPHICS_BINDING_D3D11_KHR\n");

			XrGraphicsBindingD3D11KHR binding = *(XrGraphicsBindingD3D11KHR*)createInfo->next;

			ID3D11Device*             device = binding.device;
			printf("D3D11 Device: %p\n", device);
			if (device == NULL) {
				fprintf(stderr, "D3D11 Device Invalid.\n");
				return XR_ERROR_GRAPHICS_DEVICE_INVALID;
			}

			ID3D11Device1*            device1;
			DX_CHECK(ID3D11Device_QueryInterface(device, &IID_ID3D11Device1, (void**)&device1));
			printf("D3D11 Device1: %p\n", device1);
			if (device1 == NULL) {
				fprintf(stderr, "D3D11 Device Invalid.\n");
				return XR_ERROR_GRAPHICS_DEVICE_INVALID;
			}

			D3D_FEATURE_LEVEL featureLevel = ID3D11Device1_GetFeatureLevel(device1);
			printf("D3D11 Feature Level: %d\n", featureLevel);

			pInstance->graphics.d3d11.device = device;
			pInstance->graphics.d3d11.device1 = device1;

			*session = (XrSession)pClaimedSession;
			return XR_SUCCESS;
		case XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR:
			printf("OpenXR Graphics Binding: XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR\n");
			pClaimedSession->binding.vk = *(XrGraphicsBindingVulkanKHR*)&createInfo->next;
			*session = (XrSession)pClaimedSession;
			return XR_SUCCESS;
		default:
			return XR_ERROR_GRAPHICS_DEVICE_INVALID;
	}
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySession(
	XrSession session)
{
	LOG_METHOD(xrDestroySession);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
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
		XR_REFERENCE_SPACE_TYPE_STAGE,
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
	LOG_METHOD(xrCreateReferenceSpace);

	Session* pSession = (Session*)session;

	pSession->referenceSpace = (Space){
		.referenceSpaceType = createInfo->referenceSpaceType,
		.poseInReferenceSpace = createInfo->poseInReferenceSpace,
	};
	*space = (XrSpace)&pSession->referenceSpace;

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
	XrHandle sessionHandle = GetHandle(pInstance->sessions, pSession);

	midXrGetReferenceSpaceBounds(sessionHandle, bounds);

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
	LOG_METHOD(xrCreateActionSpace);
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrLocateSpace(
	XrSpace          space,
	XrSpace          baseSpace,
	XrTime           time,
	XrSpaceLocation* location)
{
	LOG_METHOD(xrLocateSpace);
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySpace(
	XrSpace space)
{
	LOG_METHOD(xrDestroySpace);
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

	const XrViewConfigurationType modes[] = {
		XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO,
		XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
//		XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET,
//		XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT,
//		XR_VIEW_CONFIGURATION_TYPE_PRIMARY_QUAD_VARJO,
	};

	*viewConfigurationTypeCountOutput = COUNT(modes);

	if (viewConfigurationTypes == NULL)
		return XR_SUCCESS;

	if (viewConfigurationTypeCapacityInput < COUNT(modes))
		return XR_ERROR_SIZE_INSUFFICIENT;

	for (int i = 0; i < COUNT(modes); ++i) {
		viewConfigurationTypes[i] = modes[i];
	}

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetViewConfigurationProperties(
	XrInstance                     instance,
	XrSystemId                     systemId,
	XrViewConfigurationType        viewConfigurationType,
	XrViewConfigurationProperties* configurationProperties)
{
	LOG_METHOD(xrGetViewConfigurationProperties);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
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

	switch (viewConfigurationType) {
		default:
		case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO:
			printf("XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO\n");
			*viewCountOutput = 1;
			break;
		case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO:
			printf("XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO\n");
			// this can force it to mono
//			*viewCountOutput = 2;
			*viewCountOutput = 1;
			break;
		case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET:
			printf("XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO_WITH_FOVEATED_INSET\n");
			*viewCountOutput = 4;
			break;
		case XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT:
			printf("XR_VIEW_CONFIGURATION_TYPE_SECONDARY_MONO_FIRST_PERSON_OBSERVER_MSFT\n");
			*viewCountOutput = 1;  // not sure
			break;
	}

	for (int i = 0; i < viewCapacityInput && i < *viewCountOutput; ++i) {
		views[i] = (XrViewConfigurationView){
			.recommendedImageRectWidth = MIDXR_DEFAULT_WIDTH,
			.maxImageRectWidth = MIDXR_DEFAULT_WIDTH,
			.recommendedImageRectHeight = MIDXR_DEFAULT_HEIGHT,
			.maxImageRectHeight = MIDXR_DEFAULT_HEIGHT,
			.recommendedSwapchainSampleCount = MIDXR_DEFAULT_SAMPLES,
			.maxSwapchainSampleCount = MIDXR_DEFAULT_SAMPLES,
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

	Session* pSession = (Session*)session;
	Instance* pInstance = (Instance*)pSession->instance;

	switch (pInstance->graphicsApi) {
		case XR_GRAPHICS_API_OPENGL: {
			const int64_t swapFormats[] = {
				GL_SRGB8_ALPHA8,
//				GL_SRGB8,
			};
			*formatCountOutput = COUNT(swapFormats);

			if (formats == NULL)
				return XR_SUCCESS;
			if (formatCapacityInput < COUNT(swapFormats))
				return XR_ERROR_SIZE_INSUFFICIENT;

			for (int i = 0; i < COUNT(swapFormats); ++i)
				formats[i] = swapFormats[i];

			break;
		}
		case XR_GRAPHICS_API_OPENGL_ES: break;
		case XR_GRAPHICS_API_VULKAN:    {
			const int64_t swapFormats[] = {
				VK_FORMAT_R8G8B8A8_SRGB,
			};
			*formatCountOutput = COUNT(swapFormats);

			if (formats == NULL)
				return XR_SUCCESS;
			if (formatCapacityInput < COUNT(swapFormats))
				return XR_ERROR_SIZE_INSUFFICIENT;

			for (int i = 0; i < COUNT(swapFormats); ++i)
				formats[i] = swapFormats[i];

			break;
		}
		case XR_GRAPHICS_API_D3D11_1:   {
			const int64_t swapFormats[] = {
				DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			};
			*formatCountOutput = COUNT(swapFormats);

			if (formats == NULL)
				return XR_SUCCESS;
			if (formatCapacityInput < COUNT(swapFormats))
				return XR_ERROR_SIZE_INSUFFICIENT;

			for (int i = 0; i < COUNT(swapFormats); ++i)
				formats[i] = swapFormats[i];

			break;
		}
		default:
			fprintf(stderr, "xrEnumerateSwapchainFormats Graphics API not supported.");
			return XR_ERROR_RUNTIME_FAILURE;
	}

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSwapchain(
	XrSession                    session,
	const XrSwapchainCreateInfo* createInfo,
	XrSwapchain*                 swapchain)
{
	LOG_METHOD(xrCreateSwapchain);

	Session*  pSession = (Session*)session;
	Instance* pInstance = (Instance*)pSession->instance;
	XrHandle  sessionHandle = GetHandle(pInstance->sessions, pSession);

	Swapchain* pSwapchain;
	ClaimHandle(pSession->Swapchains, pSwapchain);

	pSwapchain->session = session;

	pSwapchain->usageFlags = createInfo->usageFlags;
	pSwapchain->format = createInfo->format;
	pSwapchain->sampleCount = createInfo->sampleCount;
	pSwapchain->width = createInfo->width;
	pSwapchain->height = createInfo->height;
	pSwapchain->faceCount = createInfo->faceCount;
	pSwapchain->arraySize = createInfo->arraySize;
	pSwapchain->mipCount = createInfo->mipCount;

	printf("Create swapchain usageFlags: %llu format: %lld sampleCount %d width: %d height %d faceCount: %d arraySize: %d mipCount %d\n",
		   createInfo->usageFlags, createInfo->format, createInfo->sampleCount, createInfo->width, createInfo->height, createInfo->faceCount, createInfo->arraySize, createInfo->mipCount);

	HANDLE colorHandles[MIDXR_SWAP_COUNT];
	midXrClaimFramebufferImages(sessionHandle, MIDXR_SWAP_COUNT, colorHandles);

	switch (pInstance->graphicsApi) {
		case XR_GRAPHICS_API_OPENGL:
			assert(pSwapchain->format == GL_SRGB8_ALPHA8);
#define DEFAULT_IMAGE_CREATE_INFO(_width, _height, _format, _memObject, _texture, _handle)                    \
	gl.CreateMemoryObjectsEXT(1, &_memObject);                                                                \
	gl.ImportMemoryWin32HandleEXT(_memObject, _width* _height * 4, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, _handle); \
	gl.CreateTextures(GL_TEXTURE_2D, 1, &_texture);                                                           \
	gl.TextureStorageMem2DEXT(_texture, 1, _format, _width, _height, _memObject, 0);
			for (int i = 0; i < XR_SWAP_COUNT; ++i) {
				DEFAULT_IMAGE_CREATE_INFO(pSwapchain->width, pSwapchain->height, GL_RGBA8, pSwapchain->color.gl[i].memObject, pSwapchain->color.gl[i].texture, colorHandles[i]);
				printf("Imported gl swap texture: %d memObject: %d\n", pSwapchain->color.gl[i].texture, pSwapchain->color.gl[i].memObject);
			}
			break;
		case XR_GRAPHICS_API_D3D11_1:
			assert(pSwapchain->format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
			for (int i = 0; i < XR_SWAP_COUNT; ++i) {
				printf("Importing d3d11 device: %p handle: %p texture: %p\n", pInstance->graphics.d3d11.device1, colorHandles[i],  pSwapchain->color.d3d11[i]);
				DX_CHECK(ID3D11Device1_OpenSharedResource1(pInstance->graphics.d3d11.device1, colorHandles[i], &IID_ID3D11Texture2D, (void**)&pSwapchain->color.d3d11[i]));
				printf("Imported d3d11 swap texture: %p\n", pSwapchain->color.d3d11[i]);
			}
			break;
		case XR_GRAPHICS_API_VULKAN:
			assert(pSwapchain->format == VK_FORMAT_R8G8B8A8_SRGB);
//			assert(pSwapchain->format == VK_FORMAT_R8G8B8A8_UNORM);
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
	return XR_ERROR_FUNCTION_UNSUPPORTED;
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateSwapchainImages(
	XrSwapchain                 swapchain,
	uint32_t                    imageCapacityInput,
	uint32_t*                   imageCountOutput,
	XrSwapchainImageBaseHeader* images)
{
	LOG_METHOD(xrEnumerateSwapchainImages);

	*imageCountOutput = MIDXR_SWAP_COUNT;
	if (images == NULL)
		return XR_SUCCESS;

	if (imageCapacityInput < MIDXR_SWAP_COUNT)
		return XR_ERROR_SIZE_INSUFFICIENT;

	Swapchain* pSwapchain = (Swapchain*)swapchain;

	switch (images[0].type) {
		case XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR: {
			XrSwapchainImageOpenGLKHR* pImage = (XrSwapchainImageOpenGLKHR*)images;
			for (int i = 0; i < imageCapacityInput && i < MIDXR_SWAP_COUNT; ++i) {
				pImage[i].image = pSwapchain->color.gl[i].texture;
			}
			break;
		}
		case XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR: {
			XrSwapchainImageD3D11KHR* pImage = (XrSwapchainImageD3D11KHR*)images;
			for (int i = 0; i < imageCapacityInput && i < MIDXR_SWAP_COUNT; ++i) {
				pImage[i].texture = pSwapchain->color.d3d11[i];
			}
			break;
		}
		default:
			fprintf(stderr, "Swap type not currently supported\n");
			return XR_ERROR_HANDLE_INVALID;
	}

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrAcquireSwapchainImage(
	XrSwapchain                        swapchain,
	const XrSwapchainImageAcquireInfo* acquireInfo,
	uint32_t*                          index)
{
	LOG_METHOD(xrAcquireSwapchainImage);

	Swapchain* pSwapchain = (Swapchain*)swapchain;
	Session*   pSession = (Session*)pSwapchain->session;
	Instance*  pInstance = (Instance*)pSession->instance;
	XrHandle   sessionHandle = GetHandle(pInstance->sessions, pSession);

	midXrAcquireSwapchainImage(sessionHandle, &pSwapchain->swapIndex);
	*index = pSwapchain->swapIndex;

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrWaitSwapchainImage(
	XrSwapchain                     swapchain,
	const XrSwapchainImageWaitInfo* waitInfo)
{
	LOG_METHOD(xrWaitSwapchainImage);
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrReleaseSwapchainImage(
	XrSwapchain                        swapchain,
	const XrSwapchainImageReleaseInfo* releaseInfo)
{
	LOG_METHOD(xrReleaseSwapchainImage);
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrBeginSession(
	XrSession                 session,
	const XrSessionBeginInfo* beginInfo)
{
	LOG_METHOD(xrBeginSession);

	Session* pSession = (Session*)session;
	pSession->primaryViewConfigurationType = beginInfo->primaryViewConfigurationType;

	printf("Session ViewConfigurationType: %d\n", beginInfo->primaryViewConfigurationType);

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEndSession(
	XrSession session)
{
	LOG_METHOD(xrEndSession);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrRequestExitSession(
	XrSession session)
{
	LOG_METHOD(xrRequestExitSession);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrWaitFrame(
	XrSession              session,
	const XrFrameWaitInfo* frameWaitInfo,
	XrFrameState*          frameState)
{
	LOG_METHOD(xrWaitFrame);

	Session*  pSession = (Session*)session;
	Instance* pInstance = (Instance*)pSession->instance;
	XrHandle  sessionHandle = GetHandle(pInstance->sessions, pSession);

	midXrWaitFrame(sessionHandle);

	XrTime currentTime = GetXrTime();

	frameState->predictedDisplayPeriod = currentTime - pSession->lastPredictedDisplayTime;
	frameState->predictedDisplayTime = currentTime;
	frameState->shouldRender = XR_TRUE;

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrBeginFrame(
	XrSession               session,
	const XrFrameBeginInfo* frameBeginInfo)
{
	LOG_METHOD(xrBeginFrame);

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEndFrame(
	XrSession             session,
	const XrFrameEndInfo* frameEndInfo)
{
	LOG_METHOD(xrEndFrame);

	Session* pSession = (Session*)session;
	Instance* pInstance = (Instance*)pSession->instance;
	XrHandle sessionHandle = GetHandle(pInstance->sessions, pSession);
	midXrEndFrame(sessionHandle);
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
	LOG_METHOD(xrLocateViews);

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

	Session* pSession = (Session*)session;
	Instance* pInstance = (Instance*)pSession->instance;
	const XrHandle sessionHandle = GetHandle(pInstance->sessions, pSession);

	for (int i = 0; i < viewCapacityInput; ++i) {
		midXrGetView(sessionHandle, i, &views[i]);
	}

	// get view poses and fovs
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrStringToPath(
	XrInstance  instance,
	const char* pathString,
	XrPath*     path)
{
	LOG_METHOD(xrStringToPath);

	Instance* pInstance = (Instance*)instance;

	const int pathHash = CalcDJB2(pathString, XR_MAX_PATH_LENGTH);
	for (int i = 0; i < pInstance->paths.count; ++i) {
		if (pInstance->paths.hash[i] != pathHash)
			continue;
		if (strncmp(pInstance->paths.data[i].string, pathString, XR_MAX_PATH_LENGTH)) {
			fprintf(stderr, "Path Hash Collision! %s | %s\n", pInstance->paths.data[i].string, pathString);
			return XR_ERROR_PATH_INVALID;
		}
		*path = (XrPath)&pInstance->paths.data[i];
		return XR_SUCCESS;
	}

	Path* pPath;
	XR_CHECK(ClaimPath(&pInstance->paths, &pPath, pathHash));
	XrHandle pathHandle = GetHandle(pInstance->paths, pPath);
	printf("Path handle claimed: %d\n", pathHandle);

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
	LOG_METHOD(xrPathToString);

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
	LOG_METHOD(xrCreateActionSet);

	printf("Creating ActionSet with %s\n", createInfo->actionSetName);
	Instance* pInstance = (Instance*)instance;

	XrHash     actionSetNameHash = CalcDJB2(createInfo->actionSetName, XR_MAX_ACTION_SET_NAME_SIZE);
	ActionSet* pActionSet;
	XR_CHECK(ClaimActionSet(&pInstance->actionSets, &pActionSet, actionSetNameHash));

	pActionSet->priority = createInfo->priority;
	strncpy((char*)&pActionSet->actionSetName, (const char*)&createInfo->actionSetName, XR_MAX_ACTION_SET_NAME_SIZE);
	*actionSet = (XrActionSet)pActionSet;

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroyActionSet(
	XrActionSet actionSet)
{
	LOG_METHOD(xrDestroyActionSet);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateAction(
	XrActionSet               actionSet,
	const XrActionCreateInfo* createInfo,
	XrAction*                 action)
{
	LOG_METHOD(xrCreateAction);

	ActionSet* pActionSet = (ActionSet*)actionSet;

	if (pActionSet->attachedToSession != NULL)
		return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
	if (createInfo->countSubactionPaths > MIDXR_MAX_SUBACTION_PATHS)
		return XR_ERROR_PATH_COUNT_EXCEEDED;

	Action* pAction;
	ClaimHandle(pActionSet->Actions, pAction);

	strncpy((char*)&pAction->actionName, (const char*)&createInfo->actionName, XR_MAX_ACTION_SET_NAME_SIZE);
	pAction->actionType = createInfo->actionType;
	pAction->countSubactionPaths = createInfo->countSubactionPaths;
	memcpy(&pAction->subactionPaths, createInfo->subactionPaths, pAction->countSubactionPaths * sizeof(XrPath));
	strncpy((char*)&pAction->localizedActionName, (const char*)&createInfo->localizedActionName, XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE);
	pAction->actionSet = actionSet;
	*action = (XrAction)pAction;

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroyAction(
	XrAction action)
{
	LOG_METHOD(xrDestroyAction);
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

	Instance* pInstance = (Instance*)instance;
	Path*     pInteractionProfilePath = (Path*)suggestedBindings->interactionProfile;
	XrHash    interactionProfilePathHash = GetPathHash(&pInstance->paths, pInteractionProfilePath);
	printf("Binding: %s\n", pInteractionProfilePath->string);
	const XrActionSuggestedBinding* pSuggestedBindings = suggestedBindings->suggestedBindings;

	InteractionProfile* pInteractionProfile = GetInteractionProfileByHash(&pInstance->interactionProfiles, interactionProfilePathHash);
	if (pInteractionProfile == NULL) {
		fprintf(stderr, "Could not find interaction profile binding set! %s %d\n", pInteractionProfilePath->string);
		return XR_ERROR_PATH_UNSUPPORTED;
	}

	for (int i = 0; i < suggestedBindings->countSuggestedBindings; ++i) {
		Action*    pAction = (Action*)pSuggestedBindings[i].action;
		ActionSet* pActionSet = (ActionSet*)pAction->actionSet;
		if (pActionSet->attachedToSession != NULL)
			return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
	}

	for (int i = 0; i < suggestedBindings->countSuggestedBindings; ++i) {
		Action*    pBindingAction = (Action*)pSuggestedBindings[i].action;
		ActionSet* pBindingActionSet = (ActionSet*)pBindingAction->actionSet;
		Path*      pBindingPath = (Path*)pSuggestedBindings[i].binding;
		XrHash     bindingPathHash = GetPathHash(&pInstance->paths, pBindingPath);
		Binding*   pBinding = GetBindingByHash(&pInteractionProfile->bindings, bindingPathHash);
		printf("Action: %s BindingPath: %s\n", pBindingAction->actionName, pBindingPath->string);

		if (pBindingAction->countSubactionPaths == 0) {
			printf("No Subactions. Bound to zero.\n");
			XrBinding* pActionBinding;
			ClaimHandle(pBindingAction->subactionBindings[0], pActionBinding);
			*pActionBinding = pBinding;
			continue;
		}

		for (int subactionIndex = 0; subactionIndex < pBindingAction->countSubactionPaths; ++subactionIndex) {
			Path* pActionSubPath = (Path*)pBindingAction->subactionPaths[subactionIndex];
			if (CompareSubPath(pActionSubPath->string, pBindingPath->string))
				continue;
			printf("Bound to %d %s\n", subactionIndex, pActionSubPath->string);
			XrBinding* pActionBinding;
			ClaimHandle(pBindingAction->subactionBindings[subactionIndex], pActionBinding);
			*pActionBinding = pBinding;
		}
	}
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrAttachSessionActionSets(
	XrSession                            session,
	const XrSessionActionSetsAttachInfo* attachInfo)
{
	LOG_METHOD(xrAttachSessionActionSets);

	Session* pSession = (Session*)session;

	if (pSession->actionSetStates.count != 0)
		return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;

	Instance* pInstance = (Instance*)pSession->instance;
	for (int i = 0; i < attachInfo->countActionSets; ++i) {
		ActionSet*      pAttachingActionSet = (ActionSet*)attachInfo->actionSets[i];
		XrHash          attachingActionSetHash = GetActionSetHash(&pInstance->actionSets, pAttachingActionSet);
		ActionSetState* pClaimedActionSetState;
		XR_CHECK(ClaimActionSetState(&pSession->actionSetStates, &pClaimedActionSetState, attachingActionSetHash));

		pClaimedActionSetState->actionStates.count = pAttachingActionSet->Actions.count;
		pClaimedActionSetState->actionSet = (XrActionSet)pAttachingActionSet;
	}

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetCurrentInteractionProfile(
	XrSession                  session,
	XrPath                     topLevelUserPath,
	XrInteractionProfileState* interactionProfile)
{
	LOG_METHOD(xrGetCurrentInteractionProfile);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateBoolean(
	XrSession                   session,
	const XrActionStateGetInfo* getInfo,
	XrActionStateBoolean*       state)
{
	LOG_METHOD(xrGetActionStateBoolean);
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateFloat(
	XrSession                   session,
	const XrActionStateGetInfo* getInfo,
	XrActionStateFloat*         state)
{
	LOG_METHOD(xrGetActionStateFloat);

	Action*    pGetAction = (Action*)getInfo->action;
	Path*      pGetActionSubactionPath = (Path*)getInfo->subactionPath;
	ActionSet* pGetActionSet = (ActionSet*)pGetAction->actionSet;

	Session*        pSession = (Session*)session;
	Instance*       pInstance = (Instance*)pSession->instance;
	XrHash          getActionSetHash = GetActionSetHash(&pInstance->actionSets, pGetActionSet);
	ActionSetState* pGetActionSetState = GetActionSetStateByHash(&pSession->actionSetStates, getActionSetHash);

	XrHandle     getActionHandle = GetHandle(pGetActionSet->Actions, pGetAction);
	ActionState* pGetActionState = &pGetActionSetState->actionStates.data[getActionHandle];

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

	fprintf(stderr, "%s subaction on %s not bound l\n", pGetActionSubactionPath->string, pGetAction->actionName);
	return XR_EVENT_UNAVAILABLE;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateVector2f(
	XrSession                   session,
	const XrActionStateGetInfo* getInfo,
	XrActionStateVector2f*      state)
{
	LOG_METHOD(xrGetActionStateVector2f);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStatePose(
	XrSession                   session,
	const XrActionStateGetInfo* getInfo,
	XrActionStatePose*          state)
{
	LOG_METHOD(xrGetActionStatePose);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrSyncActions(
	XrSession                session,
	const XrActionsSyncInfo* syncInfo)
{
	LOG_METHOD(xrSyncActions);

	Session*     pSession = (Session*)session;
	Instance*    pInstance = (Instance*)pSession->instance;
	XrTime currentTime = GetXrTime();

	for (int sessionIndex = 0; sessionIndex < pSession->actionSetStates.count; ++sessionIndex) {
		ActionSet*      pActionSet = (ActionSet*)syncInfo->activeActionSets[sessionIndex].actionSet;
		XrHash          actionSetHash = GetActionSetHash(&pInstance->actionSets, pActionSet);
		ActionSetState* pActionSetState = GetActionSetStateByHash(&pSession->actionSetStates, actionSetHash);

		if (pActionSetState == NULL)
			continue;

		for (int actionIndex = 0; actionIndex < pActionSet->Actions.count; ++actionIndex) {
			Action*      pAction = &pActionSet->Actions.data[actionIndex];
			ActionState* pActionState = &pActionSetState->actionStates.data[actionIndex];

			for (int subActionIndex = 0; subActionIndex < pAction->countSubactionPaths; ++subActionIndex) {
				XrBindingContainer* pSubactionBindingContainer = &pAction->subactionBindings[subActionIndex];
				SubactionState*     pSubactionState = &pActionState->subactionStates[subActionIndex];

				if (pSubactionState->lastSyncedPriority > pActionSet->priority &&
					pSubactionState->lastChangeTime == currentTime)
					continue;

				const float lastState = pSubactionState->currentState;
				for (int bindingIndex = 0; bindingIndex < pSubactionBindingContainer->count; ++bindingIndex) {
					Binding* pBinding = (Binding*)pSubactionBindingContainer->data[bindingIndex];
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
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrApplyHapticFeedback(
	XrSession                 session,
	const XrHapticActionInfo* hapticActionInfo,
	const XrHapticBaseHeader* hapticFeedback)
{
	LOG_METHOD(xrApplyHapticFeedback);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrStopHapticFeedback(
	XrSession                 session,
	const XrHapticActionInfo* hapticActionInfo)
{
	LOG_METHOD(xrStopHapticFeedback);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrSessionBeginDebugUtilsLabelRegionEXT(
	XrSession                   session,
	const XrDebugUtilsLabelEXT* labelInfo)
{
	LOG_METHOD(xrSessionBeginDebugUtilsLabelRegionEXT);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrSessionEndDebugUtilsLabelRegionEXT(
	XrSession session)
{
	LOG_METHOD(xrSessionEndDebugUtilsLabelRegionEXT);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrSessionInsertDebugUtilsLabelEXT(
	XrSession                   session,
	const XrDebugUtilsLabelEXT* labelInfo)
{
	LOG_METHOD(xrSessionInsertDebugUtilsLabelEXT);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetOpenGLGraphicsRequirementsKHR(
	XrInstance                       instance,
	XrSystemId                       systemId,
	XrGraphicsRequirementsOpenGLKHR* graphicsRequirements)
{
	LOG_METHOD(xrGetOpenGLGraphicsRequirementsKHR);

	const XrVersion openglApiVersion = XR_MAKE_VERSION(MIDXR_OPENGL_MAJOR_VERSION, MIDXR_OPENGL_MINOR_VERSION, 0);
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

	Instance* pInstance = (Instance*)instance;

	if (pInstance->systemId != systemId){
		fprintf(stderr, "Invalid System ID.");
		return XR_ERROR_SYSTEM_INVALID;
	}

	graphicsRequirements->adapterLuid = pInstance->graphics.d3d11.adapterLuid;
	graphicsRequirements->minFeatureLevel = pInstance->graphics.d3d11.minFeatureLevel;

	printf("D3D11GraphicsRequirements LUID: %ld:%lu FeatureLevel: %d\n",
		   graphicsRequirements->adapterLuid.HighPart, graphicsRequirements->adapterLuid.LowPart, graphicsRequirements->minFeatureLevel);

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateHandTrackerEXT(
	XrSession                         session,
	const XrHandTrackerCreateInfoEXT* createInfo,
	XrHandTrackerEXT*                 handTracker)
{
	LOG_METHOD(xrCreateHandTrackerEXT);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroyHandTrackerEXT(
	XrHandTrackerEXT handTracker)
{
	LOG_METHOD(xrDestroyHandTrackerEXT);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrLocateHandJointsEXT(
	XrHandTrackerEXT                 handTracker,
	const XrHandJointsLocateInfoEXT* locateInfo,
	XrHandJointLocationsEXT*         locations)
{
	LOG_METHOD(xrLocateHandJointsEXT);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSpatialAnchorMSFT(
	XrSession                            session,
	const XrSpatialAnchorCreateInfoMSFT* createInfo,
	XrSpatialAnchorMSFT*                 anchor)
{
	LOG_METHOD(xrCreateSpatialAnchorMSFT);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSpatialAnchorSpaceMSFT(
	XrSession                                 session,
	const XrSpatialAnchorSpaceCreateInfoMSFT* createInfo,
	XrSpace*                                  space)
{
	LOG_METHOD(xrCreateSpatialAnchorSpaceMSFT);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySpatialAnchorMSFT(
	XrSpatialAnchorMSFT anchor)
{
	LOG_METHOD(xrDestroySpatialAnchorMSFT);
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
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSceneObserverMSFT(
	XrSession                            session,
	const XrSceneObserverCreateInfoMSFT* createInfo,
	XrSceneObserverMSFT*                 sceneObserver)
{
	LOG_METHOD(xrCreateSceneObserverMSFT);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySceneObserverMSFT(
	XrSceneObserverMSFT sceneObserver)
{
	LOG_METHOD(xrDestroySceneObserverMSFT);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSceneMSFT(
	XrSceneObserverMSFT          sceneObserver,
	const XrSceneCreateInfoMSFT* createInfo,
	XrSceneMSFT*                 scene)
{
	LOG_METHOD(xrCreateSceneMSFT);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrDestroySceneMSFT(
	XrSceneMSFT scene)
{
	LOG_METHOD(xrDestroySceneMSFT);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrComputeNewSceneMSFT(
	XrSceneObserverMSFT              sceneObserver,
	const XrNewSceneComputeInfoMSFT* computeInfo)
{
	LOG_METHOD(xrComputeNewSceneMSFT);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetSceneComputeStateMSFT(
	XrSceneObserverMSFT      sceneObserver,
	XrSceneComputeStateMSFT* state)
{
	LOG_METHOD(xrGetSceneComputeStateMSFT);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetSceneComponentsMSFT(
	XrSceneMSFT                         scene,
	const XrSceneComponentsGetInfoMSFT* getInfo,
	XrSceneComponentsMSFT*              components)
{
	LOG_METHOD(xrGetSceneComponentsMSFT);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrLocateSceneComponentsMSFT(
	XrSceneMSFT                            scene,
	const XrSceneComponentsLocateInfoMSFT* locateInfo,
	XrSceneComponentLocationsMSFT*         locations)
{
	LOG_METHOD(xrLocateSceneComponentsMSFT);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetSceneMeshBuffersMSFT(
	XrSceneMSFT                          scene,
	const XrSceneMeshBuffersGetInfoMSFT* getInfo,
	XrSceneMeshBuffersMSFT*              buffers)
{
	LOG_METHOD(xrGetSceneMeshBuffersMSFT);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrConvertWin32PerformanceCounterToTimeKHR(
	XrInstance                                  instance,
	const LARGE_INTEGER*                        performanceCounter,
	XrTime*                                     time)
{
	LOG_METHOD(xrConvertWin32PerformanceCounterToTimeKHR);
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(
	XrInstance          instance,
	const char*         name,
	PFN_xrVoidFunction* function)
{
#ifdef ENABLE_DEBUG_LOG_METHOD
	printf("xrGetInstanceProcAddr: %s\n", name);
#endif

#define CHECK_PROC_ADDR(_name)                                    \
	if (strncmp(name, #_name, XR_MAX_STRUCTURE_NAME_SIZE) == 0) { \
		*function = (PFN_xrVoidFunction)_name;                    \
		return XR_SUCCESS;                                        \
	}

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
	CHECK_PROC_ADDR(xrSessionBeginDebugUtilsLabelRegionEXT)
	CHECK_PROC_ADDR(xrSessionEndDebugUtilsLabelRegionEXT)
	CHECK_PROC_ADDR(xrSessionInsertDebugUtilsLabelEXT)

	CHECK_PROC_ADDR(xrCreateHandTrackerEXT)
	CHECK_PROC_ADDR(xrDestroyHandTrackerEXT)
	CHECK_PROC_ADDR(xrLocateHandJointsEXT)
	CHECK_PROC_ADDR(xrCreateSpatialAnchorMSFT)
	CHECK_PROC_ADDR(xrCreateSpatialAnchorSpaceMSFT)
	CHECK_PROC_ADDR(xrDestroySpatialAnchorMSFT)
	CHECK_PROC_ADDR(xrEnumerateSceneComputeFeaturesMSFT)
	CHECK_PROC_ADDR(xrCreateSceneObserverMSFT)
	CHECK_PROC_ADDR(xrDestroySceneObserverMSFT)
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

	return XR_SUCCESS;
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
		loaderInfo->minApiVersion > XR_CURRENT_API_VERSION ||
		runtimeRequest->structType != XR_LOADER_INTERFACE_STRUCT_RUNTIME_REQUEST ||
		runtimeRequest->structVersion != XR_RUNTIME_INFO_STRUCT_VERSION ||
		runtimeRequest->structSize != sizeof(XrNegotiateRuntimeRequest)) {
		fprintf(stderr, "xrNegotiateLoaderRuntimeInterface fail\n");
		return XR_ERROR_INITIALIZATION_FAILED;
	}

	runtimeRequest->getInstanceProcAddr = xrGetInstanceProcAddr;
	runtimeRequest->runtimeInterfaceVersion = XR_CURRENT_LOADER_API_LAYER_VERSION;
	runtimeRequest->runtimeApiVersion = XR_CURRENT_API_VERSION;

	return XR_SUCCESS;
}

#endif //MID_OPENXR_IMPLEMENTATION