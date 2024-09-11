
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

#define CONTAINER(_type, _capacity)                                  \
	typedef struct _type##Container {                                  \
		_type    data[_capacity];                                        \
		uint32_t hash[_capacity];                                        \
		uint32_t count;                                                  \
	} _type##Container;                                                \
	static _type* Claim##_type(_type##Container* p##_type##s) {        \
		if (p##_type##s->count >= COUNT(p##_type##s->data)) return NULL; \
		const uint32_t handle = p##_type##s->count++;                    \
		return &p##_type##s->data[handle];                               \
	}

typedef struct Path {
	char string[XR_MAX_PATH_LENGTH];
	void* chained;
} Path;
#define PathCapacity 128
static int  PathCount = 0;
static Path Paths[PathCapacity];
static int  PathHashes[PathCapacity];

typedef struct Binding {
	const Path* pPath;
	int (*func)(void*);
} Binding;
#define BindingCapacity 128
static int     BindingCount = 0;
static Binding Bindings[BindingCapacity];
static int     BindingHashes[BindingCapacity];

#define MIDXR_MAX_INTERACTION_PROFILE_BINDINGS 16
typedef struct InteractionProfile {
	const Path* pPath;
	Binding bindings[MIDXR_MAX_INTERACTION_PROFILE_BINDINGS];
} InteractionProfile;

#define MIDXR_MAX_SUBACTION_PATHS 2
typedef struct Action {
	char         actionName[XR_MAX_ACTION_NAME_SIZE];
	XrActionType actionType;
	uint32_t     countSubactionPaths;
	const Path*  pSubactionPaths[MIDXR_MAX_SUBACTION_PATHS];
	char         localizedActionName[XR_MAX_LOCALIZED_ACTION_NAME_SIZE];
	//this only supports one suggested binding atm
	//	const Path* bindings[MIDXR_MAX_SUBACTION_PATH_COUNT];
	Binding* pBindings[MIDXR_MAX_SUBACTION_PATHS];
} Action;
#define ActionCapacity 64
static int ActionCount = 1;
Action     Actions[ActionCapacity];

CONTAINER(Action, 32);

typedef struct ActionSet {
	char            actionSetName[XR_MAX_ACTION_SET_NAME_SIZE];
	uint32_t        priority;
	ActionContainer actions;
} ActionSet;
CONTAINER(ActionSet, 4);

typedef struct Space {
	XrReferenceSpaceType referenceSpaceType;
	XrPosef              poseInReferenceSpace;
} Space;
CONTAINER(Space, 4);

typedef struct Session {
	XrGraphicsBindingOpenGLWin32KHR glBinding;
	SpaceContainer                  spaces;
} Session;
CONTAINER(Session, 2);

typedef struct Instance {
	char             applicationName[XR_MAX_APPLICATION_NAME_SIZE];
	XrFormFactor     systemFormFactor;
	SessionContainer sessions;
	ActionSetContainer actionSets;
} Instance;
CONTAINER(Instance, 2);

static InstanceContainer instances;

#define CLAIM_SUBHANDLE(_parent, _type)                         \
	if (_parent->_type##s.count >= COUNT(_parent->_type##s.data)) \
		return XR_ERROR_LIMIT_REACHED;                              \
	const int handle = _parent->_type##s.count++;                 \
	_type*    p##_type = &_parent->_type##s.data[handle];         \
	*p##_type = (_type)

#define CLAIM_HANDLE2(_type)                   \
	if (_type##s.count >= COUNT(_type##s.data))  \
		return XR_ERROR_LIMIT_REACHED;             \
	const int handle = _type##s.count++;         \
	_type*    p##_type = &_type##s.data[handle]; \
	*p##_type = (_type)

//static Space* ClaimSpace(SpaceContainer* pSpaces, Space** ppSpace) {
//	if (pSpaces->count >= COUNT(pSpaces->data)) return NULL;
//	const int handle = pSpaces->count++;
//	return &pSpaces->data[handle];
//}

//#define InstanceCapacity 2
//static int      InstanceCount = 0;
//static Instance Instances[InstanceCapacity];
//static int      InstanceHashes[InstanceCapacity];


//
/// Mid OpenXR Utility
static uint32_t CalcDJB2(const char* str, int max) {
	char     c;
	int      i = 0;
	uint32_t hash = 5381;
	while ((c = *str++) && i++ < max)
		hash = ((hash << 5) + hash) + c;
	return hash;
}

#define CLAIM_HANDLE(_type)               \
	if (_type##Count >= _type##Capacity)    \
		return XR_ERROR_LIMIT_REACHED;        \
	const int handle = _type##Count++;      \
	_type*    p##_type = &_type##s[handle]; \
	*p##_type = (_type)

//
/// Mid Device Input Funcs

static int OculusLeftClick(float* pValue) {
	*pValue = 0;
	return 0;
}

static int OculusRightClick(float* pValue) {
	*pValue = 0;
	return 0;
}

//
/// Mid OpenXR Implementation

static int GetPathHash(const Path* pPath) {
	const int handle = pPath - Paths;
	return PathHashes[handle];
}

XRAPI_ATTR XrResult XRAPI_CALL xrPathToString(
	XrInstance instance,
	XrPath     path,
	uint32_t   bufferCapacityInput,
	uint32_t*  bufferCountOutput,
	char*      buffer) {
	Path* pPath = (Path*)path;
	strncpy(buffer, (const char*)&pPath->string, bufferCapacityInput < XR_MAX_PATH_LENGTH ? bufferCapacityInput : XR_MAX_PATH_LENGTH);
	*bufferCountOutput = strlen(buffer);
	return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrStringToPath(
	XrInstance  instance,
	const char* pathString,
	XrPath*     path) {
	const int hash = CalcDJB2(pathString, XR_MAX_PATH_LENGTH);
	for (int i = 0; PathHashes[i] != 0 && i < PathCapacity; ++i) {
		if (PathHashes[i] == hash) {
			*path = (XrPath)&Paths[i];
			return XR_SUCCESS;
		}
	}
	CLAIM_HANDLE(Path){};
	strncpy((char*)&pPath->string, pathString, XR_MAX_PATH_LENGTH);
	pPath->string[XR_MAX_PATH_LENGTH - 1] = '\0';
	PathHashes[handle] = CalcDJB2(pathString, XR_MAX_PATH_LENGTH);
	*path = (XrPath)pPath;
	return XR_SUCCESS;
}

//
/// Interaction



// this needs to register to interaction profiles
static XrResult RegisterBinding(Path* pPath, int (*const func)(void*)) {
	const int hash = GetPathHash(pPath);
	for (int i = 0; i < BindingCount; ++i) {
		if (BindingHashes[i] == hash) {
			fprintf(stderr, "Trying to register path hash twice! %s %d\n", pPath->string, hash);
			return XR_ERROR_PATH_INVALID;
		}
	}
	CLAIM_HANDLE(Binding){
		.pPath = pPath,
		.func = func,
	};
	BindingHashes[handle] = hash;
	pPath->chained = pBinding; // this should chain somehow
	return XR_SUCCESS;
}
static void InitStandardBindings(){
	if (BindingCount != 0)
		return;

	{
		const char* interactionProfile = "/interaction_profiles/oculus/touch_controller";
		Path* leftPath;
		xrStringToPath(NULL, "/user/hand/left/input/select/click", (XrPath*)&leftPath);
		RegisterBinding(leftPath, (int (*)(void*))OculusLeftClick);
		Path* rightPath;
		xrStringToPath(NULL, "/user/hand/right/input/select/click", (XrPath*)&rightPath);
		RegisterBinding(rightPath, (int (*)(void*))OculusRightClick);
	}
}


XRAPI_ATTR XrResult XRAPI_CALL xrCreateAction(
	XrActionSet               actionSet,
	const XrActionCreateInfo* createInfo,
	XrAction*                 action) {
	printf("Creating Action %s with %d subactions.\n", createInfo->actionName, createInfo->countSubactionPaths);
	ActionSet* pActionSet = (ActionSet*)actionSet;
	Action*    pAction = ClaimAction(&pActionSet->actions);
	if (pAction == NULL) return XR_ERROR_LIMIT_REACHED;
	*pAction = (Action){
		.actionType = createInfo->actionType,
		.countSubactionPaths = createInfo->countSubactionPaths,
	};
	if (createInfo->countSubactionPaths > MIDXR_MAX_SUBACTION_PATHS){
		printf("countSubactionPaths %d greater than MIDXR_MAX_SUBACTION_PATHS %d\n", createInfo->countSubactionPaths, MIDXR_MAX_SUBACTION_PATHS);
		return XR_ERROR_PATH_COUNT_EXCEEDED;
	}
	strncpy((char*)&pAction->actionName, (const char*)&createInfo->actionName, XR_MAX_ACTION_SET_NAME_SIZE);
	strncpy((char*)&pAction->localizedActionName, (const char*)&createInfo->localizedActionName, XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE);
	memcpy(&pAction->pSubactionPaths, createInfo->subactionPaths, pAction->countSubactionPaths * sizeof(XrPath));
	*action = (XrAction)pAction;
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateActionSet(
	XrInstance                   instance,
	const XrActionSetCreateInfo* createInfo,
	XrActionSet*                 actionSet) {
	printf("Creating ActionSet with %s\n", createInfo->actionSetName);
	Instance*  pInstance = (Instance*)instance;
	ActionSet* pActionSet = ClaimActionSet(&pInstance->actionSets);
	*pActionSet = (ActionSet){
		.priority = createInfo->priority,
	};
	strncpy((char*)&pActionSet->actionSetName, (const char*)&createInfo->actionSetName, XR_MAX_ACTION_SET_NAME_SIZE);
	*actionSet = (XrActionSet)pActionSet;
	return XR_SUCCESS;
}

static int CompareSubPath(const char* pSubPath, const char* pPath) {
	while (*pSubPath != '\0') {
		if (*pSubPath != *pPath )
			return 1;
		pSubPath++;
		pPath++;
	}
	return 0;
}

XRAPI_ATTR XrResult XRAPI_CALL xrSuggestInteractionProfileBindings(
	XrInstance                                  instance,
	const XrInteractionProfileSuggestedBinding* suggestedBindings) {
	const Path* pInteractionProfilePath = (Path*)suggestedBindings->interactionProfile;
	printf("Binding: %s\n", pInteractionProfilePath->string);
	const XrActionSuggestedBinding* pSuggestedBindings = suggestedBindings->suggestedBindings;
	for (int i = 0; i < suggestedBindings->countSuggestedBindings; ++i) {
		Action*     pActionToBind = (Action*)pSuggestedBindings[i].action;
		const Path* pBindingPath = (Path*)pSuggestedBindings[i].binding;
		printf("Action: %s BindingPath: %s\n", pActionToBind->actionName, pBindingPath->string);
		if (pActionToBind->countSubactionPaths == 0) {
			printf("No Subactions. Bound to zero.\n");
			pActionToBind->pBindings[0] = pBindingPath->chained;
			continue;
		}
		for (int j = 0; j < pActionToBind->countSubactionPaths; ++j) {
			const Path* pSubPath = (Path*)pActionToBind->pSubactionPaths[j];
			if (CompareSubPath(pSubPath->string, pBindingPath->string)) continue;
			printf("Bound to %d %s\n", j, pSubPath->string);
			pActionToBind->pBindings[j] = pBindingPath->chained;
		}
	}
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrAttachSessionActionSets(
	XrSession                                   session,
	const XrSessionActionSetsAttachInfo*        attachInfo) {
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateFloat(
	XrSession                   session,
	const XrActionStateGetInfo* getInfo,
	XrActionStateFloat*         state) {
	Session* pSession = (Session*)session;
	const Action* pAction = (Action*)getInfo->action;
	if (pAction->pBindings[0] == NULL) {
		fprintf(stderr, "%s not bound l\n", pAction->actionName);
		return XR_EVENT_UNAVAILABLE;
	}
	const Path* subactionPath = (Path*)getInfo->subactionPath;
	if (subactionPath == NULL) {
		float result;
		pAction->pBindings[0]->func(&result);
		state->currentState = result;
		state->changedSinceLastSync = true;
		state->lastChangeTime = 0;
		state->isActive = true;
		return XR_SUCCESS;
	}
	for (int i = 0; i < pAction->countSubactionPaths; ++i) {
		const Path* pSubPath = (Path*)pAction->pSubactionPaths[i];
		if (subactionPath == pSubPath) {
			float result;
			pAction->pBindings[i]->func(&result);
			state->currentState = result;
			state->changedSinceLastSync = true;
			state->lastChangeTime = 0;
			state->isActive = true;
			return XR_SUCCESS;
		}
	}
	fprintf(stderr, "%s subaction on %s not bound l\n", subactionPath->string,  pAction->actionName);
	return XR_EVENT_UNAVAILABLE;
}

//
/// Creation
XRAPI_ATTR XrResult XRAPI_CALL xrCreateReferenceSpace(
	XrSession                         session,
	const XrReferenceSpaceCreateInfo* createInfo,
	XrSpace*                          space) {
	Session* pSession = (Session*)session;
	Space* pSpace = ClaimSpace(&pSession->spaces);
	if (pSpace == NULL) return XR_ERROR_LIMIT_REACHED;
	*pSpace = (Space){
		.referenceSpaceType = createInfo->referenceSpaceType,
		.poseInReferenceSpace = createInfo->poseInReferenceSpace,
	};
	*space = (XrSpace)pSpace;
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSession(
	XrInstance                 instance,
	const XrSessionCreateInfo* createInfo,
	XrSession*                 session) {
	Instance* pInstance = (Instance*)instance;
	Session* pSession = ClaimSession(&pInstance->sessions);
	if (pSession == NULL) return XR_ERROR_LIMIT_REACHED;
	*pSession = (Session){};
	switch (*(XrStructureType*)createInfo->next) {
		case XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR:
			pSession->glBinding = *(XrGraphicsBindingOpenGLWin32KHR*)&createInfo->next;
			*session = (XrSession)pSession;
			return XR_SUCCESS;
		default:
			return XR_ERROR_GRAPHICS_DEVICE_INVALID;
	}
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetSystem(
	XrInstance             instance,
	const XrSystemGetInfo* getInfo,
	XrSystemId*            systemId) {
	Instance* pInstance = (Instance*)instance;
	pInstance->systemFormFactor = getInfo->formFactor;
	*systemId = (XrSystemId)&pInstance->systemFormFactor;
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateInstance(
	const XrInstanceCreateInfo* createInfo,
	XrInstance*                 instance) {
	Instance* pInstance = ClaimInstance(&instances);
	if (pInstance == NULL) return XR_ERROR_LIMIT_REACHED;
	*pInstance = (Instance){};
	strncpy((char*)&pInstance->applicationName, (const char*)&createInfo->applicationInfo, XR_MAX_APPLICATION_NAME_SIZE);
	*instance = (XrInstance)pInstance;
	return XR_SUCCESS;
}

//
/// Diagnostics

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

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateReferenceSpaces(
	XrSession                                   session,
	uint32_t                                    spaceCapacityInput,
	uint32_t*                                   spaceCountOutput,
	XrReferenceSpaceType*                       spaces) {
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

//
/// Event
static XrViewConfigurationType viewConfigurationType;

XRAPI_ATTR XrResult XRAPI_CALL xrBeginSession(
	XrSession                 session,
	const XrSessionBeginInfo* beginInfo) {
	viewConfigurationType = beginInfo->primaryViewConfigurationType;
	return XR_SUCCESS;
}

//
/// Polling
static XrSessionState SessionState = XR_SESSION_STATE_UNKNOWN;
static XrSessionState PendingSessionState = XR_SESSION_STATE_IDLE;

XRAPI_ATTR XrResult XRAPI_CALL xrPollEvent(
	XrInstance         instance,
	XrEventDataBuffer* eventData) {

	if (SessionState != PendingSessionState)  {

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

XRAPI_ATTR XrResult XRAPI_CALL xrSyncActions(
	XrSession                session,
	const XrActionsSyncInfo* syncInfo) {
	return XR_SUCCESS;
}


// modulo to ensure jump table is generated
// https://godbolt.org/z/rGz6TbsG1
#define PROC_ADDR_MODULO 84
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(
	XrInstance          instance,
	const char*         name,
	PFN_xrVoidFunction* function) {

	InitStandardBindings();

	printf("xrGetInstanceProcAddr %s\n", name);
#define CASE(_name, _djb2)                 \
	case _djb2 % PROC_ADDR_MODULO:           \
		*function = (PFN_xrVoidFunction)_name; \
		return XR_SUCCESS;
	const uint32_t djb2 = CalcDJB2(name, XR_MAX_RESULT_STRING_SIZE) % PROC_ADDR_MODULO;
	switch (djb2) {
		CASE(xrGetInstanceProcAddr, 2334007635)
		CASE(xrEnumerateApiLayerProperties, 1963032185)
		CASE(xrEnumerateInstanceExtensionProperties, 1017617012)
		CASE(xrCreateInstance, 799246840)
		CASE(xrGetSystem, 1809712692)
		CASE(xrEnumerateViewConfigurationViews, 4194621334)
		CASE(xrGetOpenGLGraphicsRequirementsKHR, 2661449710)
		CASE(xrCreateSession, 612805351)
		CASE(xrGetActionStateFloat, 1753855652)
		CASE(xrCreateReferenceSpace, 1138416702)
		CASE(xrCreateActionSet, 2376838285)
		CASE(xrCreateAction, 613291425)
		CASE(xrStringToPath, 1416299542)
		CASE(xrPathToString, 3598878870)
		CASE(xrSuggestInteractionProfileBindings, 3618139248)
		CASE(xrPollEvent, 2376322568)
		CASE(xrBeginSession, 1003855288)
		CASE(xrSyncActions, 2043237693)
		CASE(xrAttachSessionActionSets, 3383012517)
		CASE(xrEnumerateReferenceSpaces, 3445226179)
		default:
			return XR_ERROR_FUNCTION_UNSUPPORTED;
	}
#undef CASE
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