//#define XR_EXTENSION_PROTOTYPES
//#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_VULKAN
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_reflection.h>
#include <openxr/openxr_loader_negotiation.h>

#ifndef COUNT
#define COUNT(_array) (sizeof(_array) / sizeof(_array[0]))
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__)
#define EXPORT __attribute__((visibility("default")))
#elif defined(_MSC_VER)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

#define MIDXR_DEFAULT_WIDTH        1024
#define MIDXR_DEFAULT_HEIGHT       1024
#define MIDXR_DEFAULT_SAMPLES      1
#define MIDXR_OPENGL_MAJOR_VERSION 4
#define MIDXR_OPENGL_MINOR_VERSION 6

static int xrSystemCount = 1;
static int xrInstanceCount = 1;

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

XRAPI_ATTR XrResult XRAPI_CALL xrGetSystem(
	XrInstance             instance,
	const XrSystemGetInfo* getInfo,
	XrSystemId*            systemId) {
	*systemId = (XrSystemId)(uint64_t)xrSystemCount++;
	return XR_SUCCESS;
}

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
	PFN_CASE("xrVoidFunction") \
	PFN_CASE("xrGetInstanceProcAddr") \
	PFN_CASE("xrEnumerateApiLayerProperties") \
	PFN_CASE("xrEnumerateInstanceExtensionProperties") \
	PFN_CASE("xrCreateInstance")

/* clang-format on */


#define PFN_CASE(c0, name)                \
	case c0 + sizeof(#name):                \
		*function = (PFN_xrVoidFunction)name; \
		return XR_SUCCESS;

XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(
	XrInstance          instance,
	const char*         name,
	PFN_xrVoidFunction* function) {
	printf("xrGetInstanceProcAddr %s\n", name);
	int hash = name[2] + strlen(name) + 1;
	switch (hash) {
		PFN_CASE('G', xrGetInstanceProcAddr)
		PFN_CASE('E', xrEnumerateApiLayerProperties)
		PFN_CASE('E', xrEnumerateInstanceExtensionProperties)
		PFN_CASE('C', xrCreateInstance)
		PFN_CASE('G', xrGetSystem)
		PFN_CASE('E', xrEnumerateViewConfigurationViews)
		PFN_CASE('G', xrGetOpenGLGraphicsRequirementsKHR)
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