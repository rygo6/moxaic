
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef struct IUnknown IUnknown;

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_VULKAN
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_reflection.h>
#include <openxr/openxr_loader_negotiation.h>

//
/// Mid Common Utility
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
/// Mid OpenXR Constants
#define MIDXR_DEFAULT_WIDTH        1024
#define MIDXR_DEFAULT_HEIGHT       1024
#define MIDXR_DEFAULT_SAMPLES      1
#define MIDXR_OPENGL_MAJOR_VERSION 4
#define MIDXR_OPENGL_MINOR_VERSION 6

//
/// Mid OpenXR Types
// ya we want to do this
typedef struct XrInstanceContext {
	XrInstance  instance;
}XrInstanceContext;

//
/// Mid OpenXR Implementation

#define nil '\0'
#define PATH_CASE(_c0, _c1, _c2, _c3, _c4, _c5, _c6, _c7, _name)                                            \
	case _c0 + (_c1 << 4) + (_c2 << 8) + (_c3 << 12) + (_c4 << 16) + (_c5 << 20) + (_c6 << 24) + (_c7 << 26): \
		*bufferCountOutput = sizeof(_name) < bufferCapacityInput ? sizeof(_name) : bufferCapacityInput;        \
		memcpy(buffer, &_name, *bufferCountOutput);                                                             \
		return XR_SUCCESS;
XRAPI_ATTR XrResult XRAPI_CALL xrPathToString(
	XrInstance                                  instance,
	XrPath                                      path,
	uint32_t                                    bufferCapacityInput,
	uint32_t*                                   bufferCountOutput,
	char*                                       buffer) {
	switch (path) {
		PATH_CASE('u', 'h', 'l', nil, nil, nil, nil, nil, "/user/hand/left")
		PATH_CASE('u', 'h', 'r', nil, nil, nil, nil, nil, "/user/hand/right")
		PATH_CASE('u', 'h', 'l', 'i', 's', 'c', nil, nil, "/user/hand/left/input/select/click")
		PATH_CASE('u', 'h', 'r', 'i', 's', 'c', nil, nil, "/user/hand/right/input/select/click")
		default:
			return XR_ERROR_FUNCTION_UNSUPPORTED;
	}
}

XRAPI_ATTR XrResult XRAPI_CALL xrStringToPath(
	XrInstance  instance,
	const char* pathString,
	uint64_t*   path) {
	printf("xrStringToPath\n");
	*path = 0;
	int offset = 4;
	while (offset < 26 && *pathString++ != '\0') {
		if (*pathString == '/') {
			*path += *(pathString + 1) << offset;
			offset += 4;
		}
	}
	return *path != 0 ? XR_SUCCESS : XR_ERROR_PATH_INVALID;
}

static int xrActionSetCount = 1;

XRAPI_ATTR XrResult XRAPI_CALL xrCreateActionSet(
	XrInstance                   instance,
	const XrActionSetCreateInfo* createInfo,
	XrActionSet*                 actionSet) {
	*actionSet = (XrActionSet)(uint64_t)xrActionSetCount++;
	return XR_SUCCESS;
}

static int xrSpaceCount = 1;

XRAPI_ATTR XrResult XRAPI_CALL xrCreateReferenceSpace(
	XrSession                         session,
	const XrReferenceSpaceCreateInfo* createInfo,
	XrSpace*                          space) {
	*space = (XrSpace)(uint64_t)xrSpaceCount++;
	return XR_SUCCESS;
}

static XrGraphicsBindingOpenGLWin32KHR glBinding;
static int                             xrSessionCount = 1;

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSession(
	XrInstance                 instance,
	const XrSessionCreateInfo* createInfo,
	XrSession*                 session) {
	switch ((XrStructureType)createInfo->next) {
		case XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR:
			glBinding = *(XrGraphicsBindingOpenGLWin32KHR*)&createInfo->next;
			break;
		default: break;
	}
	*session = (XrSession)(uint64_t)xrSessionCount++;
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetOpenGLGraphicsRequirementsKHR(
	XrInstance                       instance,
	XrSystemId                       systemId,
	XrGraphicsRequirementsOpenGLKHR* graphicsRequirements) {
	const XrVersion openglApiVersion = XR_MAKE_VERSION(MIDXR_OPENGL_MAJOR_VERSION, MIDXR_OPENGL_MINOR_VERSION, 0);
	*graphicsRequirements = (XrGraphicsRequirementsOpenGLKHR){
		.minApiVersionSupported = openglApiVersion,
		.maxApiVersionSupported = openglApiVersion,
	};
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateViewConfigurationViews(
	XrInstance               instance,
	XrSystemId               systemId,
	XrViewConfigurationType  viewConfigurationType,
	uint32_t                 viewCapacityInput,
	uint32_t*                viewCountOutput,
	XrViewConfigurationView* views) {
	switch (viewConfigurationType) {
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

static int xrSystemCount = 1;

XRAPI_ATTR XrResult XRAPI_CALL xrGetSystem(
	XrInstance             instance,
	const XrSystemGetInfo* getInfo,
	XrSystemId*            systemId) {
	*systemId = (XrSystemId)(uint64_t)xrSystemCount++;
	return XR_SUCCESS;
}

static int xrInstanceCount = 1;

XRAPI_ATTR XrResult XRAPI_CALL xrCreateInstance(
	const XrInstanceCreateInfo* createInfo,
	XrInstance*                 instance) {
	*instance = (XrInstance)(uint64_t)xrInstanceCount++;
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateInstanceExtensionProperties(
	const char*            layerName,
	uint32_t               propertyCapacityInput,
	uint32_t*              propertyCountOutput,
	XrExtensionProperties* properties) {
	printf("xrEnumerateInstanceExtensionProperties\n");
	const XrExtensionProperties extensionProperties[] = {
		{
			.type = XR_TYPE_EXTENSION_PROPERTIES,
			.extensionName = XR_KHR_OPENGL_ENABLE_EXTENSION_NAME,
			.extensionVersion = XR_KHR_opengl_enable_SPEC_VERSION,
		},
	};
	*propertyCountOutput = COUNT(extensionProperties);
	if (!properties) return XR_SUCCESS;
	memcpy(properties, &extensionProperties, sizeof(XrExtensionProperties) * propertyCapacityInput);
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateApiLayerProperties(
	uint32_t              propertyCapacityInput,
	uint32_t*             propertyCountOutput,
	XrApiLayerProperties* properties) {
	return XR_SUCCESS;
}

// sed -n 's/.*XRAPI_PTR \*PFN_\(xr[a-zA-Z0-9_]*\).*/PFN_CASE("\1") \\/p' /c/OpenXRSDK/OpenXR.Loader.1.1.38/include/openxr/openxr.h > output.txt
/* clang-format off */
#define PFN_FUNCS \
	PFN_CASE(xrGetInstanceProcAddr) \
	PFN_CASE(xrEnumerateApiLayerProperties) \
	PFN_CASE(xrEnumerateInstanceExtensionProperties) \
	PFN_CASE(xrCreateInstance) \
	PFN_CASE(xrGetSystem) \
	PFN_CASE(xrEnumerateViewConfigurationViews) \
	PFN_CASE(xrGetOpenGLGraphicsRequirementsKHR) \
	PFN_CASE(xrCreateSession) \
	PFN_CASE(xrCreateReferenceSpace)
/* clang-format on */

#define PFN_CASE(_c0, _c1, _c2, _c3, _name)         \
	case _c0 + (_c1 << 4) + (_c2 << 8) + (_c3 << 12): \
		*function = (PFN_xrVoidFunction)_name;          \
		return XR_SUCCESS;
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(
	XrInstance          instance,
	const char*         name,
	PFN_xrVoidFunction* function) {
	printf("xrGetInstanceProcAddr %s\n", name);
	const char* p = &name[3];
	uint32_t    hash = name[2], offset = 4;
	while (offset < 16 && *p++ != '\0') {
		if (*p >= 'A' && *p <= 'Z') {
			hash += *p << offset;
			offset += 4;
		}
	}
	switch (hash) {
		PFN_CASE('G', 'I', 'P', 'A', xrGetInstanceProcAddr)
		PFN_CASE('E', 'A', 'L', 'P', xrEnumerateApiLayerProperties)
		PFN_CASE('E', 'I', 'E', 'P', xrEnumerateInstanceExtensionProperties)
		PFN_CASE('C', 'I', nil, nil, xrCreateInstance)
		PFN_CASE('G', 'S', nil, nil, xrGetSystem)
		PFN_CASE('E', 'V', 'C', 'V', xrEnumerateViewConfigurationViews)
		PFN_CASE('G', 'O', 'G', 'L', xrGetOpenGLGraphicsRequirementsKHR)
		PFN_CASE('C', 'S', nil, nil, xrCreateSession)
		PFN_CASE('C', 'R', 'S', nil, xrCreateReferenceSpace)
		PFN_CASE('C', 'A', 'S', nil, xrCreateActionSet)
		PFN_CASE('S', 'T', 'P', nil, xrStringToPath)
		PFN_CASE('P', 'T', 'S', nil, xrPathToString)
		default:
			return XR_ERROR_FUNCTION_UNSUPPORTED;
	}
	// could do something with computed gotos?
	//	static void *jumpTable[] = {&&xrGetInstanceProcAddr, &&xrEnumerateApiLayerProperties, &&xrEnumerateInstanceExtensionProperties};
	//xrGetInstanceProcAddr:
	//xrEnumerateApiLayerProperties:
	//xrEnumerateInstanceExtensionProperties:
}

XRAPI_ATTR XrResult EXPORT XRAPI_CALL xrNegotiateLoaderRuntimeInterface(
	const XrNegotiateLoaderInfo* loaderInfo,
	XrNegotiateRuntimeRequest*   runtimeRequest) {
	printf("xrNegotiateLoaderRuntimeInterface\n");

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

	printf("xrNegotiateLoaderRuntimeInterface success\n");
	return XR_SUCCESS;
}