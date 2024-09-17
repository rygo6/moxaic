
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
//// Mid OpenXR
void midXrInitialize();
void midXrWaitFrame();
void midXrBeginFrame();
void midXrEndFrame();

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
#define CHECK(_command)                          \
	({                                           \
		XrResult result = _command;              \
		if (result != XR_SUCCESS) return result; \
	})
#define CONTAINER(_type, _capacity)                                                                  \
	typedef struct _type##Container {                                                                \
		uint32_t count;                                                                              \
		_type    data[_capacity];                                                                    \
	} _type##Container;                                                                              \
	static XrResult Claim##_type(_type##Container* p##_type##s, _type** pp##_type)                   \
	{                                                                                                \
		if (p##_type##s->count >= COUNT(p##_type##s->data)) return XR_ERROR_LIMIT_REACHED;           \
		const uint32_t handle = p##_type##s->count++;                                                \
		*pp##_type = &p##_type##s->data[handle];                                                     \
		return XR_SUCCESS;                                                                           \
	}                                                                                                \
	static inline int Get##_type##Handle(const _type##Container* p##_type##s, const _type* p##_type) \
	{                                                                                                \
		return p##_type - p##_type##s->data;                                                         \
	}
#define CONTAINER_HASHED(_type, _capacity)                                                           \
	typedef struct _type##Container {                                                                \
		uint32_t count;                                                                              \
		_type    data[_capacity];                                                                    \
		XrHash   hash[_capacity];                                                                    \
	} _type##Container;                                                                              \
	static XrResult Claim##_type(_type##Container* p##_type##s, _type** pp##_type, XrHash hash)      \
	{                                                                                                \
		if (p##_type##s->count >= COUNT(p##_type##s->data)) return XR_ERROR_LIMIT_REACHED;           \
		const uint32_t handle = p##_type##s->count++;                                                \
		p##_type##s->hash[handle] = hash;                                                            \
		*pp##_type = &p##_type##s->data[handle];                                                     \
		return XR_SUCCESS;                                                                           \
	}                                                                                                \
	static inline int Get##_type##Hash(const _type##Container* p##_type##s, const _type* p##_type)   \
	{                                                                                                \
		const int handle = p##_type - p##_type##s->data;                                             \
		return p##_type##s->hash[handle];                                                            \
	}                                                                                                \
	static inline _type* Get##_type##ByHash(_type##Container* p##_type##s, const XrHash hash)        \
	{                                                                                                \
		for (int i = 0; i < p##_type##s->count; ++i) {                                               \
			if (p##_type##s->hash[i] == hash)                                                        \
				return &p##_type##s->data[i];                                                        \
		}                                                                                            \
		return NULL;                                                                                 \
	}                                                                                                \
	static inline int Get##_type##Handle(const _type##Container* p##_type##s, const _type* p##_type) \
	{                                                                                                \
		return p##_type - p##_type##s->data;                                                         \
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
	ActionContainer actions;
	XrSession       attachedToSession;
} ActionSet;
CONTAINER_HASHED(ActionSet, 4)

typedef struct Space {
	XrReferenceSpaceType referenceSpaceType;
	XrPosef              poseInReferenceSpace;
} Space;
CONTAINER(Space, 4)

typedef struct Session {
	XrInstance                      instance;
	XrTime                          lastPredictedDisplayTime;
	SpaceContainer                  spaces;
	ActionSetStateContainer         actionSetStates;
	XrGraphicsBindingOpenGLWin32KHR glBinding;
	XrViewConfigurationType         primaryViewConfigurationType;
} Session;
CONTAINER(Session, 2)

typedef struct Instance {
	char                        applicationName[XR_MAX_APPLICATION_NAME_SIZE];
	XrFormFactor                systemFormFactor;
	SessionContainer            sessions;
	ActionSetContainer          actionSets;
	InteractionProfileContainer interactionProfiles;
	PathContainer               paths;
} Instance;
CONTAINER(Instance, 2)

//
//// MidOpenXR Implementation
#ifdef MID_OPENXR_IMPLEMENTATION

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
/// Mid OpenXR Implementation
XRAPI_ATTR XrResult XRAPI_CALL xrPathToString(
	XrInstance instance,
	XrPath     path,
	uint32_t   bufferCapacityInput,
	uint32_t*  bufferCountOutput,
	char*      buffer)
{
	Instance* pInstance = (Instance*)instance;  // check its in instance ?
	Path*     pPath = (Path*)path;

	strncpy(buffer, pPath->string, bufferCapacityInput < XR_MAX_PATH_LENGTH ? bufferCapacityInput : XR_MAX_PATH_LENGTH);
	*bufferCountOutput = strlen(buffer);

	return XR_SUCCESS;
}
XRAPI_ATTR XrResult XRAPI_CALL xrStringToPath(
	XrInstance  instance,
	const char* pathString,
	XrPath*     path)
{
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
	CHECK(ClaimPath(&pInstance->paths, &pPath, pathHash));

	strncpy(pPath->string, pathString, XR_MAX_PATH_LENGTH);
	pPath->string[XR_MAX_PATH_LENGTH - 1] = '\0';
	*path = (XrPath)pPath;

	return XR_SUCCESS;
}

//
/// Interaction

static XrResult RegisterBinding(
	XrInstance        instance,
	BindingContainer* pBindings,
	Path*             pPath,
	int (*const func)(void*))
{
	Instance* pInstance = (Instance*)instance;

	const int pathHash = GetPathHash(&pInstance->paths, pPath);
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
	int (*const func)(void*);
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
		CHECK(ClaimInteractionProfile(&pInstance->interactionProfiles, &pBindingSet, bindingSetHash));

		pBindingSet->interactionProfile = bindingSetPath;

		for (int i = 0; i < COUNT(bindingDefinitions); ++i) {
			XrPath path;
			xrStringToPath(instance, bindingDefinitions[i].path, &path);
			RegisterBinding(instance, &pBindingSet->bindings, (Path*)path, bindingDefinitions[i].func);
		}
	}

	return XR_SUCCESS;
}


XRAPI_ATTR XrResult XRAPI_CALL xrCreateAction(
	XrActionSet               actionSet,
	const XrActionCreateInfo* createInfo,
	XrAction*                 action)
{
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
	XrActionSet*                 actionSet)
{
	printf("Creating ActionSet with %s\n", createInfo->actionSetName);
	Instance* pInstance = (Instance*)instance;

	XrHash     actionSetNameHash = CalcDJB2(createInfo->actionSetName, XR_MAX_ACTION_SET_NAME_SIZE);
	ActionSet* pActionSet;
	CHECK(ClaimActionSet(&pInstance->actionSets, &pActionSet, actionSetNameHash));

	pActionSet->priority = createInfo->priority;
	strncpy((char*)&pActionSet->actionSetName, (const char*)&createInfo->actionSetName, XR_MAX_ACTION_SET_NAME_SIZE);
	*actionSet = (XrActionSet)pActionSet;

	return XR_SUCCESS;
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
	Instance*    pInstance = (Instance*)instance;
	const Path*  pInteractionProfilePath = (Path*)suggestedBindings->interactionProfile;
	const XrHash interactionProfilePathHash = GetPathHash(&pInstance->paths, pInteractionProfilePath);
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
		Action*      pBindingAction = (Action*)pSuggestedBindings[i].action;
		ActionSet*   pBindingActionSet = (ActionSet*)pBindingAction->actionSet;
		const Path*  pBindingPath = (Path*)pSuggestedBindings[i].binding;
		const XrHash bindingPathHash = GetPathHash(&pInstance->paths, pBindingPath);
		Binding*     pBinding = GetBindingByHash(&pInteractionProfile->bindings, bindingPathHash);
		printf("Action: %s BindingPath: %s\n", pBindingAction->actionName, pBindingPath->string);

		if (pBindingAction->countSubactionPaths == 0) {
			printf("No Subactions. Bound to zero.\n");
			XrBinding* pActionBinding;
			ClaimXrBinding(&pBindingAction->subactionBindings[0], &pActionBinding);
			*pActionBinding = pBinding;
			continue;
		}

		for (int subactionIndex = 0; subactionIndex < pBindingAction->countSubactionPaths; ++subactionIndex) {
			const Path* pActionSubPath = (Path*)pBindingAction->subactionPaths[subactionIndex];
			if (CompareSubPath(pActionSubPath->string, pBindingPath->string))
				continue;
			printf("Bound to %d %s\n", subactionIndex, pActionSubPath->string);
			XrBinding* pActionBinding;
			ClaimXrBinding(&pBindingAction->subactionBindings[subactionIndex], &pActionBinding);
			*pActionBinding = pBinding;
		}
	}
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrAttachSessionActionSets(
	XrSession                            session,
	const XrSessionActionSetsAttachInfo* attachInfo)
{
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
		pClaimedActionSetState->actionSet = (XrActionSet)pAttachingActionSet;
	}

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetActionStateFloat(
	XrSession                   session,
	const XrActionStateGetInfo* getInfo,
	XrActionStateFloat*         state)
{
	const Action*    pGetAction = (Action*)getInfo->action;
	const Path*      pGetActionSubactionPath = (Path*)getInfo->subactionPath;
	const ActionSet* pGetActionSet = (ActionSet*)pGetAction->actionSet;

	Session*        pSession = (Session*)session;
	const Instance* pInstance = (Instance*)pSession->instance;
	const XrHash    getActionSetHash = GetActionSetHash(&pInstance->actionSets, pGetActionSet);
	ActionSetState* pGetActionSetState = GetActionSetStateByHash(&pSession->actionSetStates, getActionSetHash);

	const int    getActionHandle = GetActionHandle(&pGetActionSet->actions, pGetAction);
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

XRAPI_ATTR XrResult XRAPI_CALL xrCreateActionSpace(
	XrSession                      session,
	const XrActionSpaceCreateInfo* createInfo,
	XrSpace*                       space)
{
	return XR_SUCCESS;
}

//
/// Creation
XRAPI_ATTR XrResult XRAPI_CALL xrCreateReferenceSpace(
	XrSession                         session,
	const XrReferenceSpaceCreateInfo* createInfo,
	XrSpace*                          space)
{
	Session* pSession = (Session*)session;

	Space* pClaimedSpace;
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
	XrSession*                 session)
{
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

XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProperties(
	XrInstance            instance,
	XrInstanceProperties* instanceProperties)
{
	instanceProperties->runtimeVersion = XR_MAKE_VERSION(0, 1, 0);
	strncpy(instanceProperties->runtimeName, "Moxaic OpenXR", XR_MAX_RUNTIME_NAME_SIZE);
	return XR_SUCCESS;
}

#define MIDXR_SYSTEM_ID 1

XRAPI_ATTR XrResult XRAPI_CALL xrGetSystem(
	XrInstance             instance,
	const XrSystemGetInfo* getInfo,
	XrSystemId*            systemId)
{
	Instance* pInstance = (Instance*)instance;
	pInstance->systemFormFactor = getInfo->formFactor;
	*systemId = MIDXR_SYSTEM_ID;
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrGetSystemProperties(
	XrInstance          instance,
	XrSystemId          systemId,
	XrSystemProperties* properties)
{
	properties->systemId = MIDXR_SYSTEM_ID;
	properties->vendorId = 0;
	strncpy(properties->systemName, "moxaic", XR_MAX_SYSTEM_NAME_SIZE);
	properties->graphicsProperties.maxLayerCount = 3;
	properties->graphicsProperties.maxSwapchainImageWidth = DEFAULT_WIDTH;
	properties->graphicsProperties.maxSwapchainImageHeight = DEFAULT_HEIGHT;
	properties->trackingProperties.orientationTracking = XR_TRUE;
	properties->trackingProperties.positionTracking = XR_TRUE;

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrCreateInstance(
	const XrInstanceCreateInfo* createInfo,
	XrInstance*                 instance)
{
	Instance* pClaimedInstance;
	CHECK(ClaimInstance(&instances, &pClaimedInstance));

	strncpy((char*)&pClaimedInstance->applicationName, (const char*)&createInfo->applicationInfo, XR_MAX_APPLICATION_NAME_SIZE);
	*instance = (XrInstance)pClaimedInstance;

	InitStandardBindings(*instance);
	midXrInitialize();

	return XR_SUCCESS;
}

//
/// Diagnostics

XRAPI_ATTR XrResult XRAPI_CALL xrGetOpenGLGraphicsRequirementsKHR(
	XrInstance                       instance,
	XrSystemId                       systemId,
	XrGraphicsRequirementsOpenGLKHR* graphicsRequirements)
{
	const XrVersion openglApiVersion = XR_MAKE_VERSION(MIDXR_OPENGL_MAJOR_VERSION, MIDXR_OPENGL_MINOR_VERSION, 0);
	*graphicsRequirements = (XrGraphicsRequirementsOpenGLKHR){
		.minApiVersionSupported = openglApiVersion,
		.maxApiVersionSupported = openglApiVersion,
	};
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateReferenceSpaces(
	XrSession             session,
	uint32_t              spaceCapacityInput,
	uint32_t*             spaceCountOutput,
	XrReferenceSpaceType* spaces)
{
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateInstanceExtensionProperties(
	const char*            layerName,
	uint32_t               propertyCapacityInput,
	uint32_t*              propertyCountOutput,
	XrExtensionProperties* properties)
{
	const XrExtensionProperties extensionProperties[] = {
		{
			.type = XR_TYPE_EXTENSION_PROPERTIES,
			.extensionName = XR_KHR_OPENGL_ENABLE_EXTENSION_NAME,
			.extensionVersion = XR_KHR_opengl_enable_SPEC_VERSION,
		},
	};

	*propertyCountOutput = COUNT(extensionProperties);

	if (!properties)
		return XR_SUCCESS;

	memcpy(properties, &extensionProperties, sizeof(XrExtensionProperties) * propertyCapacityInput);

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateApiLayerProperties(
	uint32_t              propertyCapacityInput,
	uint32_t*             propertyCountOutput,
	XrApiLayerProperties* properties)
{
	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateViewConfigurationViews(
	XrInstance               instance,
	XrSystemId               systemId,
	XrViewConfigurationType  viewConfigurationType,
	uint32_t                 viewCapacityInput,
	uint32_t*                viewCountOutput,
	XrViewConfigurationView* views)
{
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

#ifndef GL_SRGB8_ALPHA8
#define GL_SRGB8_ALPHA8 0x8C43
#endif
#ifndef GL_SRGB8
#define GL_SRGB8 0x8C41
#endif
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateSwapchainFormats(
	XrSession session,
	uint32_t  formatCapacityInput,
	uint32_t* formatCountOutput,
	int64_t*  formats)
{
	*formatCountOutput = 2;

	if (formats == NULL)
		return XR_SUCCESS;

	if (formatCapacityInput < 2)
		return XR_ERROR_SIZE_INSUFFICIENT;

	formats[0] = GL_SRGB8_ALPHA8;
	formats[1] = GL_SRGB8;

	return XR_SUCCESS;
}


XRAPI_ATTR XrResult XRAPI_CALL xrCreateSwapchain(
	XrSession                    session,
	const XrSwapchainCreateInfo* createInfo,
	XrSwapchain*                 swapchain)
{
	return XR_SUCCESS;
}

//
/// Event
XRAPI_ATTR XrResult XRAPI_CALL xrBeginSession(
	XrSession                 session,
	const XrSessionBeginInfo* beginInfo)
{
	Session* pSession = (Session*)session;

	pSession->primaryViewConfigurationType = beginInfo->primaryViewConfigurationType;

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrWaitFrame(
	XrSession              session,
	const XrFrameWaitInfo* frameWaitInfo,
	XrFrameState*          frameState)
{
	Session* pSession = (Session*)session;

	midXrWaitFrame();

	const XrTime currentTime = GetXrTime();

	frameState->predictedDisplayPeriod = currentTime - pSession->lastPredictedDisplayTime;
	frameState->predictedDisplayTime = currentTime;
	frameState->shouldRender = XR_TRUE;

	return XR_SUCCESS;
}

XRAPI_ATTR XrResult XRAPI_CALL xrBeginFrame(
	XrSession               session,
	const XrFrameBeginInfo* frameBeginInfo)
{
	return XR_SUCCESS;
}

//
/// Polling
static XrSessionState SessionState = XR_SESSION_STATE_UNKNOWN;
static XrSessionState PendingSessionState = XR_SESSION_STATE_IDLE;

XRAPI_ATTR XrResult XRAPI_CALL xrPollEvent(
	XrInstance         instance,
	XrEventDataBuffer* eventData)
{
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

XRAPI_ATTR XrResult XRAPI_CALL xrSyncActions(
	XrSession                session,
	const XrActionsSyncInfo* syncInfo)
{
	Session*     pSession = (Session*)session;
	Instance*    pInstance = (Instance*)pSession->instance;
	const XrTime currentTime = GetXrTime();

	for (int sessionIndex = 0; sessionIndex < pSession->actionSetStates.count; ++sessionIndex) {
		ActionSet*      pActionSet = (ActionSet*)syncInfo->activeActionSets[sessionIndex].actionSet;
		XrHash          actionSetHash = GetActionSetHash(&pInstance->actionSets, pActionSet);
		ActionSetState* pActionSetState = GetActionSetStateByHash(&pSession->actionSetStates, actionSetHash);

		if (pActionSetState == NULL)
			continue;

		for (int actionIndex = 0; actionIndex < pActionSet->actions.count; ++actionIndex) {
			Action*      pAction = &pActionSet->actions.data[actionIndex];
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


// modulo to ensure jump table is generated
// https://godbolt.org/z/rGz6TbsG1
#define PROC_ADDR_MODULO 113
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance          instance,
													 const char*         name,
													 PFN_xrVoidFunction* function)
{
//	printf("xrGetInstanceProcAddr %s\n", name);

	// These only load once so it being a so if chain is not an issue.
#define CHECK_PROC_ADDR(_name)                                    \
	if (strncmp(name, #_name, XR_MAX_STRUCTURE_NAME_SIZE) == 0) { \
		*function = (PFN_xrVoidFunction)_name;                    \
		return XR_SUCCESS;                                        \
	}

	CHECK_PROC_ADDR(xrGetInstanceProcAddr)
	CHECK_PROC_ADDR(xrEnumerateApiLayerProperties)
	CHECK_PROC_ADDR(xrEnumerateInstanceExtensionProperties)
	CHECK_PROC_ADDR(xrCreateInstance)
	CHECK_PROC_ADDR(xrGetSystem)
	CHECK_PROC_ADDR(xrGetSystemProperties)
	CHECK_PROC_ADDR(xrEnumerateViewConfigurationViews)
	CHECK_PROC_ADDR(xrGetOpenGLGraphicsRequirementsKHR)
	CHECK_PROC_ADDR(xrCreateSession)
	CHECK_PROC_ADDR(xrGetActionStateFloat)
	CHECK_PROC_ADDR(xrCreateReferenceSpace)
	CHECK_PROC_ADDR(xrCreateActionSet)
	CHECK_PROC_ADDR(xrCreateAction)
	CHECK_PROC_ADDR(xrStringToPath)
	CHECK_PROC_ADDR(xrPathToString)
	CHECK_PROC_ADDR(xrSuggestInteractionProfileBindings)
	CHECK_PROC_ADDR(xrPollEvent)
	CHECK_PROC_ADDR(xrBeginSession)
	CHECK_PROC_ADDR(xrWaitFrame)
	CHECK_PROC_ADDR(xrBeginFrame)
	CHECK_PROC_ADDR(xrSyncActions)
	CHECK_PROC_ADDR(xrAttachSessionActionSets)
	CHECK_PROC_ADDR(xrEnumerateReferenceSpaces)
	CHECK_PROC_ADDR(xrCreateActionSpace)
	CHECK_PROC_ADDR(xrEnumerateSwapchainFormats)
	CHECK_PROC_ADDR(xrCreateSwapchain)
	CHECK_PROC_ADDR(xrGetInstanceProperties)

#undef CHECK_PROC_ADDR

	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

XRAPI_ATTR XrResult EXPORT XRAPI_CALL xrNegotiateLoaderRuntimeInterface(
	const XrNegotiateLoaderInfo* loaderInfo,
	XrNegotiateRuntimeRequest*   runtimeRequest)
{
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

#endif