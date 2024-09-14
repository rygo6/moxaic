
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
//// Mid OpenXR Constants
#define MIDXR_DEFAULT_WIDTH        1024
#define MIDXR_DEFAULT_HEIGHT       1024
#define MIDXR_DEFAULT_SAMPLES      1
#define MIDXR_OPENGL_MAJOR_VERSION 4
#define MIDXR_OPENGL_MINOR_VERSION 6

//
//// Mid OpenXR Types

typedef uint32_t XrHash;
#define CHECK(_command)                      \
	{                                          \
		XrResult result = _command;              \
		if (result != XR_SUCCESS) return result; \
	}
#define CONTAINER(_type, _capacity)                                                                  \
	typedef struct _type##Container {                                                                  \
		uint32_t count;                                                                                  \
		_type    data[_capacity];                                                                        \
	} _type##Container;                                                                                \
	static XrResult Claim##_type(_type##Container* p##_type##s, _type** pp##_type) {                   \
		if (p##_type##s->count >= COUNT(p##_type##s->data)) return XR_ERROR_LIMIT_REACHED;               \
		const uint32_t handle = p##_type##s->count++;                                                    \
		*pp##_type = &p##_type##s->data[handle];                                                         \
		return XR_SUCCESS;                                                                               \
	}                                                                                                  \
	static inline int Get##_type##Handle(const _type##Container* p##_type##s, const _type* p##_type) { \
		return p##_type - p##_type##s->data;                                                             \
	}
#define CONTAINER_HASHED(_type, _capacity)                                                           \
	typedef struct _type##Container {                                                                  \
		uint32_t count;                                                                                  \
		_type    data[_capacity];                                                                        \
		XrHash   hash[_capacity];                                                                        \
	} _type##Container;                                                                                \
	static XrResult Claim##_type(_type##Container* p##_type##s, _type** pp##_type, XrHash hash) {      \
		if (p##_type##s->count >= COUNT(p##_type##s->data)) return XR_ERROR_LIMIT_REACHED;               \
		const uint32_t handle = p##_type##s->count++;                                                    \
		p##_type##s->hash[handle] = hash;                                                                \
		*pp##_type = &p##_type##s->data[handle];                                                         \
		return XR_SUCCESS;                                                                               \
	}                                                                                                  \
	static inline int Get##_type##Hash(const _type##Container* p##_type##s, const _type* p##_type) {   \
		const int handle = p##_type - p##_type##s->data;                                                 \
		return p##_type##s->hash[handle];                                                                \
	}                                                                                                  \
	static inline _type* Get##_type##ByHash(_type##Container* p##_type##s, const XrHash hash) {        \
		for (int i = 0; i < p##_type##s->count; ++i) {                                                   \
			if (p##_type##s->hash[i] == hash)                                                              \
				return &p##_type##s->data[i];                                                                \
		}                                                                                                \
		return NULL;                                                                                     \
	}                                                                                                  \
	static inline int Get##_type##Handle(const _type##Container* p##_type##s, const _type* p##_type) { \
		return p##_type - p##_type##s->data;                                                             \
	}

#define MIDXR_MAX_PATHS 128
typedef struct Path {
	char string[XR_MAX_PATH_LENGTH];
} Path;
CONTAINER_HASHED(Path, MIDXR_MAX_PATHS);

typedef struct Binding {
	XrPath path;
	int (*func)(void*);
} Binding;
CONTAINER_HASHED(Binding, 32);

#define MIDXR_MAX_ACTIONS 32
typedef Binding* XrBinding;
CONTAINER(XrBinding, MIDXR_MAX_ACTIONS);

#define MIDXR_MAX_SUBACTION_PATHS 2
typedef struct Action {
	XrActionSet        actionSet;
	XrBindingContainer bindings;
	uint32_t           countSubactionPaths;
	XrPath             subactionPaths[MIDXR_MAX_SUBACTION_PATHS];
	XrActionType       actionType;
	char               actionName[XR_MAX_ACTION_NAME_SIZE];
	char               localizedActionName[XR_MAX_LOCALIZED_ACTION_NAME_SIZE];
} Action;
CONTAINER(Action, MIDXR_MAX_ACTIONS);

#define MIDXR_MAX_BOUND_ACTIONS 4
CONTAINER(XrAction, MIDXR_MAX_BOUND_ACTIONS);

// probably want to do this?
typedef struct InteractionProfile {
	XrPath path;
	BindingContainer bindings;
} InteractionProfile;
static InteractionProfile interactionProfiles;

typedef struct ActionState {
	// actualy layout from OXR to memcopy
	float                 currentState;
	XrBool32              changedSinceLastSync;
	XrTime                lastChangeTime;
	XrBool32              isActive;
} ActionState;
CONTAINER(ActionState, MIDXR_MAX_ACTIONS);

typedef struct ActionSetState {
	ActionStateContainer actionStates;
	XrActionSet          actionSet;
} ActionSetState;
CONTAINER_HASHED(ActionSetState, 4);

typedef struct ActionSet {
	char            actionSetName[XR_MAX_ACTION_SET_NAME_SIZE];
	uint32_t        priority;
	ActionContainer actions;
	XrSession       attachedToSession;
} ActionSet;
CONTAINER_HASHED(ActionSet, 4);

typedef struct Space {
	XrReferenceSpaceType referenceSpaceType;
	XrPosef              poseInReferenceSpace;
} Space;
CONTAINER(Space, 4);

typedef struct Session {
	XrInstance                      instance;
	XrGraphicsBindingOpenGLWin32KHR glBinding;
	SpaceContainer                  spaces;
	ActionSetStateContainer         actionSetStates;
} Session;
CONTAINER(Session, 2);

typedef struct Instance {
	char               applicationName[XR_MAX_APPLICATION_NAME_SIZE];
	XrFormFactor       systemFormFactor;
	SessionContainer   sessions;
	ActionSetContainer actionSets;
	BindingContainer   bindings;
	PathContainer      paths;
} Instance;
CONTAINER(Instance, 2);

static InstanceContainer instances;

//
//// Mid OpenXR Utility
static uint32_t CalcDJB2(const char* str, int max) {
	char     c;
	int      i = 0;
	uint32_t hash = 5381;
	while ((c = *str++) && i++ < max)
		hash = ((hash << 5) + hash) + c;
	return hash;
}

//
//// Mid Device Input Funcs
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
XRAPI_ATTR XrResult XRAPI_CALL xrPathToString(
	XrInstance instance,
	XrPath     path,
	uint32_t   bufferCapacityInput,
	uint32_t*  bufferCountOutput,
	char*      buffer) {
	Instance* pInstance = (Instance*)instance;
	Path* pPath = (Path*)path;
	strncpy(buffer, pPath->string, bufferCapacityInput < XR_MAX_PATH_LENGTH ? bufferCapacityInput : XR_MAX_PATH_LENGTH);
	*bufferCountOutput = strlen(buffer);
	return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrStringToPath(
	XrInstance  instance,
	const char* pathString,
	XrPath*     path) {
	Instance* pInstance = (Instance*)instance;
	const int hash = CalcDJB2(pathString, XR_MAX_PATH_LENGTH);
	for (int i = 0; i < pInstance->paths.count; ++i) {
		if (pInstance->paths.hash[i] == hash) {
			*path = (XrPath)&pInstance->paths.data[i];
			return XR_SUCCESS;
		}
	}
	Path* pPath;
	CHECK(ClaimPath(&pInstance->paths, &pPath, hash));
	strncpy(pPath->string, pathString, XR_MAX_PATH_LENGTH);
	pPath->string[XR_MAX_PATH_LENGTH - 1] = '\0';
	*path = (XrPath)pPath;
	return XR_SUCCESS;
}

//
/// Interaction

static XrResult RegisterBinding(XrInstance instance, Path* pPath, int (*const func)(void*)) {
	Instance* pInstance = (Instance*)instance;
	const int pathHash = GetPathHash(&pInstance->paths, pPath);
	for (int i = 0; i < pInstance->bindings.count; ++i) {
		if (GetPathHash(&pInstance->paths, (Path*)pInstance->bindings.data[i].path) == pathHash) {
			fprintf(stderr, "Trying to register path hash twice! %s %d\n", pPath->string, pathHash);
			return XR_ERROR_PATH_INVALID;
		}
	}
	Binding* pBinding;
	ClaimBinding(&pInstance->bindings, &pBinding, pathHash);
	pBinding->path = (XrPath)pPath;
	pBinding->func = func;
	return XR_SUCCESS;
}
static XrResult InitStandardBindings(XrInstance instance){
	Instance* pInstance = (Instance*)instance;
	{
		const char* interactionProfile = "/interaction_profiles/oculus/touch_controller";
		Path* leftPath;
		xrStringToPath(instance, "/user/hand/left/input/select/click", (XrPath*)&leftPath);
		RegisterBinding(instance, leftPath, (int (*)(void*))OculusLeftClick);
		Path* rightPath;
		xrStringToPath(instance, "/user/hand/right/input/select/click", (XrPath*)&rightPath);
		RegisterBinding(instance, rightPath, (int (*)(void*))OculusRightClick);
	}
	return XR_SUCCESS;
}


XRAPI_ATTR XrResult XRAPI_CALL xrCreateAction(
	XrActionSet               actionSet,
	const XrActionCreateInfo* createInfo,
	XrAction*                 action) {
	ActionSet* pActionSet = (ActionSet*)actionSet;
	if (pActionSet->attachedToSession != NULL)
		return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
	if (createInfo->countSubactionPaths > MIDXR_MAX_SUBACTION_PATHS)
		return XR_ERROR_PATH_COUNT_EXCEEDED;
	Action* pAction;
	CHECK(ClaimAction(&pActionSet->actions, &pAction));
	strncpy((char*)&pAction->actionName, (const char*)&createInfo->actionName, XR_MAX_ACTION_SET_NAME_SIZE);
	pAction->actionType = createInfo->actionType;
	pAction->countSubactionPaths = createInfo->countSubactionPaths;
	memcpy(&pAction->subactionPaths, createInfo->subactionPaths, pAction->countSubactionPaths * sizeof(XrPath));
	strncpy((char*)&pAction->localizedActionName, (const char*)&createInfo->localizedActionName, XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE);
	pAction->actionSet = actionSet;
	*action = (XrAction)pAction;
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateActionSet(
	XrInstance                   instance,
	const XrActionSetCreateInfo* createInfo,
	XrActionSet*                 actionSet) {
	printf("Creating ActionSet with %s\n", createInfo->actionSetName);
	Instance*  pInstance = (Instance*)instance;
	XrHash     actionSetNameHash = CalcDJB2(createInfo->actionSetName, XR_MAX_ACTION_SET_NAME_SIZE);
	ActionSet* pActionSet;
	CHECK(ClaimActionSet(&pInstance->actionSets, &pActionSet, actionSetNameHash));
	pActionSet->priority = createInfo->priority;
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
	Instance* pInstance = (Instance*)instance;

	const Path* pInteractionProfilePath = (Path*)suggestedBindings->interactionProfile;
	printf("Binding: %s\n", pInteractionProfilePath->string);
	const XrActionSuggestedBinding* pSuggestedBindings = suggestedBindings->suggestedBindings;

	for (int i = 0; i < suggestedBindings->countSuggestedBindings; ++i) {
		Action* pAction= (Action*)pSuggestedBindings[i].action;
		ActionSet* pActionSet = (ActionSet*)pAction->actionSet;
		if (pActionSet->attachedToSession != NULL)
			return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;
	}

	for (int i = 0; i < suggestedBindings->countSuggestedBindings; ++i) {
		Action*        pActionToBind = (Action*)pSuggestedBindings[i].action;
		const Path*    pBindingPath = (Path*)pSuggestedBindings[i].binding;
		const XrHash   bindingPathHash = GetPathHash(&pInstance->paths, pBindingPath);
		Binding* pBinding = GetBindingByHash(&pInstance->bindings, bindingPathHash);
		printf("Action: %s BindingPath: %s\n", pActionToBind->actionName, pBindingPath->string);

		if (pActionToBind->countSubactionPaths == 0) {
			printf("No Subactions. Bound to zero.\n");
			XrBinding* pActionBinding;
			ClaimXrBinding(&pActionToBind->bindings, &pActionBinding);
			*pActionBinding = pBinding;

//			XrAction* pAction;
//			ClaimXrAction(&pBinding->actions, &pAction);
//			*pAction = (XrAction)pActionToBind;
			continue;
		}

		for (int j = 0; j < pActionToBind->countSubactionPaths; ++j) {
			const Path* pActionSubPath = (Path*)pActionToBind->subactionPaths[j];
			if (CompareSubPath(pActionSubPath->string, pBindingPath->string))
				continue;
			printf("Bound to %d %s\n", j, pActionSubPath->string);
			XrBinding* pActionBinding;
			ClaimXrBinding(&pActionToBind->bindings, &pActionBinding);
			*pActionBinding = pBinding;

//			XrAction* pAction;
//			ClaimXrAction(&pBinding->actions, &pAction);
//			*pAction = (XrAction)pActionToBind;
		}
	}
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrAttachSessionActionSets(
	XrSession                            session,
	const XrSessionActionSetsAttachInfo* attachInfo) {
	Session* pSession = (Session*)session;
	if (pSession->actionSetStates.count != 0)
		return XR_ERROR_ACTIONSETS_ALREADY_ATTACHED;

	Instance* pInstance = (Instance*)pSession->instance;
	for (int i = 0; i < attachInfo->countActionSets; ++i) {
		ActionSet*      pAttachingActionSet = (ActionSet*)attachInfo->actionSets[i];
		XrHash          attachingActionSetHash = GetActionSetHash(&pInstance->actionSets, pAttachingActionSet);
		ActionSetState* pClaimedActionSetState;
		CHECK(ClaimActionSetState(&pSession->actionSetStates, &pClaimedActionSetState, attachingActionSetHash));
		pClaimedActionSetState->actionStates.count = pAttachingActionSet->actions.count;
		pClaimedActionSetState->actionSet =  (XrActionSet)pAttachingActionSet;
	}

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateFloat(
	XrSession                   session,
	const XrActionStateGetInfo* getInfo,
	XrActionStateFloat*         state) {

	Session* pSession = (Session*)session;
	const Instance* pInstance = (Instance*)pSession->instance;

	const Action* pAction = (Action*)getInfo->action;
	const ActionSet* pActionSet = (ActionSet*)pAction->actionSet;
	const int actionHandle = GetActionHandle(&pActionSet->actions, pAction);
	const XrHash     actionSetHash = GetActionSetHash(&pInstance->actionSets, pActionSet);

	ActionSetState* pActionSetState =  GetActionSetStateByHash(&pSession->actionSetStates, actionSetHash);
	ActionState* pActionState = &pActionSetState->actionStates.data[actionHandle];

	const Path* subactionPath = (Path*)getInfo->subactionPath;
	if (subactionPath == NULL) {
		memcpy(&state->currentState, &pActionState->currentState, sizeof(ActionState));
		pActionState->changedSinceLastSync = false;
		return XR_SUCCESS;
	}

	for (int i = 0; i < pAction->countSubactionPaths; ++i) {
		const Path* pSubPath = (Path*)pAction->subactionPaths[i];
		if (subactionPath == pSubPath) {
			memcpy(&state->currentState, &pActionState->currentState, sizeof(ActionState));
			pActionState->changedSinceLastSync = false;
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
	Space*   pClaimedSpace;
	CHECK(ClaimSpace(&pSession->spaces, &pClaimedSpace));
	*pClaimedSpace = (Space){
		.referenceSpaceType = createInfo->referenceSpaceType,
		.poseInReferenceSpace = createInfo->poseInReferenceSpace,
	};
	*space = (XrSpace)pClaimedSpace;
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateSession(
	XrInstance                 instance,
	const XrSessionCreateInfo* createInfo,
	XrSession*                 session) {
	Instance* pInstance = (Instance*)instance;
	Session*  pClaimedSession;
	CHECK(ClaimSession(&pInstance->sessions, &pClaimedSession));
	pClaimedSession->instance = instance;
	switch (*(XrStructureType*)createInfo->next) {
		case XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR:
			pClaimedSession->glBinding = *(XrGraphicsBindingOpenGLWin32KHR*)&createInfo->next;
			*session = (XrSession)pClaimedSession;
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
	Instance* pClaimedInstance;
	CHECK(ClaimInstance(&instances, &pClaimedInstance));
	*pClaimedInstance = (Instance){};
	strncpy((char*)&pClaimedInstance->applicationName, (const char*)&createInfo->applicationInfo, XR_MAX_APPLICATION_NAME_SIZE);
	*instance = (XrInstance)pClaimedInstance;
	InitStandardBindings(*instance);
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
	Session* pSession = (Session*)session;
	Instance* pInstance = (Instance*)pSession->instance;

//	for (int i = 0; i < pInstance->bindings.count; ++i) {
//		pInstance->bindings.data[i].
//	}


	for (int i = 0; i < pSession->actionSetStates.count; ++i) {
		ActionSet* pActionSet = (ActionSet*)syncInfo->activeActionSets[i].actionSet;
		XrHash actionSetHash = GetActionSetHash(&pInstance->actionSets, pActionSet);
		ActionSetState* pActionSetState = GetActionSetStateByHash(&pSession->actionSetStates, actionSetHash);
		if (pActionSetState == NULL)
			continue;

		for (int j = 0; j < pActionSet->actions.count; ++j){
			Action* pAction = &pActionSet->actions.data[j];
			ActionState* pActionState = &pActionSetState->actionStates.data[j];

			!!! no this just needs to set the highest priorit binding!!!!
			for (int k = 0; k < pAction->bindings.count; ++k) {
				Binding* pBinding = (Binding*)&pAction->bindings.data[k];
				pBinding->func(pActionState);
			}

		}
		pInstance->bindings


	}


	return XR_SUCCESS;
}


// modulo to ensure jump table is generated
// https://godbolt.org/z/rGz6TbsG1
#define PROC_ADDR_MODULO 84
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(
	XrInstance          instance,
	const char*         name,
	PFN_xrVoidFunction* function) {

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