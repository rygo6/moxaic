
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
static uint32_t CalcDJB2(const char *str, int max) {
	char c; int i = 0; uint32_t hash = 5381;
	while ((c = *str++) && i++ < max)
		hash = ((hash << 5) + hash) + c;
	return hash;
}

XRAPI_ATTR XrResult XRAPI_CALL xrPathToString(
	XrInstance instance,
	XrPath     path,
	uint32_t   bufferCapacityInput,
	uint32_t*  bufferCountOutput,
	char*      buffer) {
	switch ((uint32_t)path) {
#define CASE(_name, _djb2)                                                                          \
	case _djb2:                                                                                       \
		*bufferCountOutput = sizeof(_name) < bufferCapacityInput ? sizeof(_name) : bufferCapacityInput; \
		memcpy(buffer, &_name, *bufferCountOutput);                                                     \
		return XR_SUCCESS;

		CASE("/user/hand/left", 2556700407)
		CASE("/user/hand/right", 2773994890)
		CASE("/user/hand/left/input/select/click", 335458810)
		CASE("/user/hand/right/input/select/click", 1218722221)
#undef CASE
		default:
			return XR_ERROR_FUNCTION_UNSUPPORTED;
	}
}

XRAPI_ATTR XrResult XRAPI_CALL xrStringToPath(
	XrInstance  instance,
	const char* pathString,
	XrPath*   path) {
	printf("xrStringToPath\n");
	const uint32_t djb2 = CalcDJB2(pathString, XR_MAX_PATH_LENGTH);
	*path = (XrPath)djb2;
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

XRAPI_ATTR XrResult XRAPI_CALL xrCreateAction(
	XrActionSet               actionSet,
	const XrActionCreateInfo* createInfo,
	XrAction*                 action) {

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
	switch (*(XrStructureType*)createInfo->next) {
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

XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(
	XrInstance          instance,
	const char*         name,
	PFN_xrVoidFunction* function) {
	printf("xrGetInstanceProcAddr %s\n", name);
	const uint32_t djb2 = CalcDJB2(name, XR_MAX_RESULT_STRING_SIZE);
	switch (djb2) {
#define CASE(_name, _djb2)                 \
	case _djb2:                              \
		*function = (PFN_xrVoidFunction)_name; \
		return XR_SUCCESS;

		CASE(xrGetInstanceProcAddr, 2334007635)
		CASE(xrEnumerateApiLayerProperties, 1963032185)
		CASE(xrEnumerateInstanceExtensionProperties, 1017617012)
		CASE(xrCreateInstance, 799246840)
		CASE(xrGetSystem, 1809712692)
		CASE(xrEnumerateViewConfigurationViews, 4194621334)
		CASE(xrGetOpenGLGraphicsRequirementsKHR, 2661449710)
		CASE(xrCreateSession, 612805351)
		CASE(xrCreateReferenceSpace, 1138416702)
		CASE(xrCreateActionSet, 2376838285)
		CASE(xrCreateAction, 613291425)
		CASE(xrStringToPath, 1416299542)
		CASE(xrPathToString, 3598878870)
#undef CASE
		default:
			return XR_ERROR_FUNCTION_UNSUPPORTED;
	}
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