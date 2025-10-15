#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdatomic.h>

#include "mid_openxr_runtime.h"
#include "mid_vulkan.h"

#include "node.h"

void xrInitialize()
{
	printf("Initializing Moxaic OpenXR Node.\n");
	isCompositor = false;
	mxcConnectInterprocessNode(false);
}

void xrGetViewConfigurationView(XrSystemId systemId, XrViewConfigurationView *pView)
{
	pView->recommendedImageRectWidth = DEFAULT_WIDTH;
	pView->maxImageRectWidth = DEFAULT_WIDTH;
	pView->recommendedImageRectHeight = DEFAULT_HEIGHT;
	pView->maxImageRectHeight = DEFAULT_HEIGHT;
	pView->recommendedSwapchainSampleCount = 1;
	pView->maxSwapchainSampleCount = 1;
}

// maybe should be external handle?
void xrClaimSessionId(session_i* pSessionIndex)
{
	LOG("Creating Moxaic OpenXR Session.\n");

	// I believe both a session and a composition layer will end up constituting different Nodes
	// and requesting a SessionId will simply mean the base compositionlayer index
	NodeHandle      hNode = RequestExternalNodeHandle(&pImportedExternalMemory->shared);
	MxcNodeContext* pNodeCtx = &node.context[hNode];

	pNodeCtx->interprocessMode = MXC_NODE_INTERPROCESS_MODE_IMPORTED;
	pNodeCtx->swapsSyncedHandle = pImportedExternalMemory->imports.swapsSyncedHandle;

	MxcNodeShared* pNodeShrd = node.pShared[hNode];
	pNodeShrd->compositorMode = MXC_COMPOSITOR_MODE_COMPUTE;

	LOG("Importing node handle %d as OpenXR session\n", hNode);

	*pSessionIndex = hNode; // openxr sessionId == moxaic node handle

	mxcIpcFuncEnqueue(hNode, MXC_INTERPROCESS_TARGET_NODE_OPENED);
}

void xrReleaseSessionId(session_i iSession)
{
	LOG("Releasing Moxaic OpenXR Session.\n");
	NodeHandle hNode = iSession;
	MxcNodeShared* pNodeShrd = node.pShared[hNode];

	mxcIpcFuncEnqueue(hNode, MXC_INTERPROCESS_TARGET_NODE_CLOSED);
}

XrResult xrSharedPollEvent(session_i iSession, XrEventDataUnion* pEventData)
{
	MxcNodeShared* pNodeShared = node.pShared[iSession];
	return MID_QRING_DEQUEUE(&pNodeShared->eventDataQueue, pNodeShared->queuedEventDataBuffers, pEventData)
	               == MID_SUCCESS ? XR_SUCCESS : XR_EVENT_UNAVAILABLE;
}

void xrGetReferenceSpaceBounds(session_i iSession, XrExtent2Df* pBounds)
{
	MxcNodeShared* pNodeShared = node.pShared[iSession];
	float radius = pNodeShared->compositorRadius * 2.0f;

	// we just assume depth is the same, and treat this like a cube
	*pBounds = (XrExtent2Df) {.width = radius, .height = radius };
}

void xrGetSessionTimeline(session_i iSession, HANDLE* pHandle)
{
	MxcNodeImports* pImportParam = &pImportedExternalMemory->imports;
	*pHandle = pImportParam->nodeTimelineHandle;
}

/// Set the new session timeline value to signal to the compositor there is a new frame.
// should this be finish frame? and merged with SetColorSwapId and SetDepthSwapid? Probably
void xrSetSessionTimelineValue(session_i iSession, uint64_t timelineValue)
{
	MxcNodeShared*  pNodeShared = node.pShared[iSession];
	atomic_store_explicit(&pNodeShared->timelineValue, timelineValue, memory_order_release);
}

void xrGetCompositorTimeline(session_i iSession, HANDLE* pHandle)
{
	MxcNodeImports* pImportParam = &pImportedExternalMemory->imports;
	*pHandle = pImportParam->compositorTimelineHandle;
}

void xrCreateSwapchainImages(session_i sessionId, const XrSwapchainInfo* pSwapInfo, swap_i iSwap)
{
	NodeHandle hNode = sessionId;
	auto pNodeCtxt = &node.context[sessionId];
	auto pNodeShrd = node.pShared[sessionId];

	assert(pNodeShrd->swapStates[iSwap] == XR_SWAP_STATE_UNITIALIZED && "Trying to create swapchain images with used swap index!");

	pNodeShrd->swapStates[iSwap] = XR_SWAP_STATE_REQUESTED;
	pNodeShrd->swapInfos[iSwap] = *pSwapInfo;

	mxcIpcFuncEnqueue(hNode, MXC_INTERPROCESS_TARGET_SYNC_SWAPS);
	WaitForSingleObject(pNodeCtxt->swapsSyncedHandle, INFINITE);

	if (pNodeShrd->swapStates[iSwap] == XR_SWAP_STATE_ERROR) {
		LOG_ERROR("OpenXR failed to acquire swap!");
		pNodeShrd->swapStates[iSwap] = XR_SWAP_STATE_UNITIALIZED;
	}
}

void xrGetSwapchainImportedImage(session_i iSession, swap_i iSwap, int imageId, HANDLE* pHandle)
{
	auto pNodeShrd = node.pShared[iSession];
	auto pImports = &pImportedExternalMemory->imports;
	assert(pNodeShrd->swapStates[iSwap] == XR_SWAP_STATE_CREATED && "Trying to get swap image that has not been created!!");
	*pHandle = pImports->swapImageHandles[iSwap][imageId];
}

void xrSetColorSwapId(session_i iSession, XrViewId viewId, swap_i iSwap)
{
	MxcNodeShared* pNodeShrd = node.pShared[iSession];
	pNodeShrd->viewSwaps[viewId].colorId = iSwap;
}

void xrSetDepthSwapId(session_i iSession, XrViewId viewId, swap_i iSwap)
{
	MxcNodeShared* pNodeShrd = node.pShared[iSession];
	pNodeShrd->viewSwaps[viewId].depthId = iSwap;
}

void xrSetDepthInfo(session_i iSession, float minDepth, float maxDepth, float nearZ, float farZ)
{
	MxcNodeShared* pNodeShrd = node.pShared[iSession];
	// pNodeShrd->processState.minDepth = minDepth;
	// pNodeShrd->processState.maxDepth = maxDepth;
	pNodeShrd->processState.depthNearZ = nearZ;
	pNodeShrd->processState.depthFarZ = farZ;
}

void xrClaimSwapIndex(session_i iSession, uint8_t* pIndex)
{
	MxcNodeShared* pNodeShared = node.pShared[iSession];
	uint64_t timelineValue = atomic_load_explicit(&pNodeShared->timelineValue, memory_order_acquire);
	uint8_t index = (timelineValue % VK_SWAP_COUNT);
	*pIndex = index;
//	printf("Claiming swap index %d timeline %llu\n", index, timelineValue);
//	uint8_t priorIndex = __atomic_exchange_n(&pNodeShared->swapClaimed[index], true, __ATOMIC_SEQ_CST);
//	assert(priorIndex == false);
}

void xrReleaseSwapIndex(session_i iSession, uint8_t index)
{
	MxcNodeShared* pNodeShared = node.pShared[iSession];
	uint64_t timelineValue = atomic_load_explicit(&pNodeShared->timelineValue, memory_order_acquire);
//	printf("Releasing swap index %d timeline %llu\n", index, timelineValue);
//	uint8_t priorIndex = __atomic_exchange_n(&pNodeShared->swapClaimed[index], false, __ATOMIC_SEQ_CST);
//	assert(priorIndex == true);
}

void xrSetInitialCompositorTimelineValue(session_i iSession, uint64_t timelineValue)
{
	MxcNodeShared* pNodeShared = node.pShared[iSession];
	u64 timelineCycleStartValue = timelineValue - (timelineValue % MXC_CYCLE_COUNT);
	// Wait two cycles out for good measure.
	pNodeShared->compositorBaseCycleValue = timelineCycleStartValue + (MXC_CYCLE_COUNT * 2);
//	printf("Setting compositorBaseCycleValue %llu\n", pNodeShared->compositorBaseCycleValue);
}

void xrGetCompositorTimelineValue(session_i iSession, uint64_t* pTimelineValue)
{
	MxcNodeShared* pNodeShared = node.pShared[iSession];
	*pTimelineValue = pNodeShared->compositorBaseCycleValue + MXC_CYCLE_POST_UPDATE_NODE_STATES_COMPLETE;
}

void xrProgressCompositorTimelineValue(session_i iSession, uint64_t timelineValue)
{
	MxcNodeShared* pNodeShared = node.pShared[iSession];
	pNodeShared->compositorBaseCycleValue += MXC_CYCLE_COUNT * pNodeShared->compositorCycleSkip;
}

XrTime xrGetFrameInterval(session_i sessionId)
{
	MxcNodeShared* pNodeShared = node.pShared[sessionId];
	double hz = 144.0 / (double)(pNodeShared->compositorCycleSkip);
	XrTime hzTime = xrHzToXrTime(hz);
//	LOG("xrGetFrameInterval compositorCycleSkip: %d hz: %f hzTime: %llu\n", pNodeShared->compositorCycleSkip, hz, hzTime);
	return hzTime;
}

void xrGetHeadPose(session_i iSession, XrEulerPosef* pPose)
{
	MxcNodeShared*  pNodeShared = node.pShared[iSession];

	pPose->euler    = *(XrVector3f*)&pNodeShared->cameraPose.euler;
	pPose->position = *(XrVector3f*)&pNodeShared->cameraPose.pos;
}

void xrGetEyeView(session_i iSession, view_i iView, XrEyeView *pEyeView)
{
	MxcNodeShared* pNodeShared = node.pShared[iSession];

	pEyeView->euler    = *(XrVector3f*)&pNodeShared->cameraPose.euler;
	pEyeView->position = *(XrVector3f*)&pNodeShared->cameraPose.pos;
	pEyeView->fovRad   = (XrVector2f){pNodeShared->camera.yFovRad, pNodeShared->camera.yFovRad};

	pEyeView->upperLeftClip  = *(XrVector2f*)&pNodeShared->clip.ulUV;
	pEyeView->lowerRightClip = *(XrVector2f*)&pNodeShared->clip.lrUV;

	// TODO this is to debug
	if (iView == 1)
		pEyeView->position.x += 0.1f;
}

#define UPDATE_CLICK(button, chirality)                           \
	({                                                            \
		auto pNodeShared = node.pShared[sessionId];            \
		atomic_thread_fence(memory_order_acquire);                \
		auto pController = &pNodeShared->chirality;               \
		auto changed = pState->isActive != pController->active || \
		               pState->boolValue != pController->button;  \
		pState->isActive = pController->active;                   \
		pState->boolValue = pController->button;                  \
		changed;                                                  \
	})
int xrInputSelectClick_Left(session_i sessionId, SubactionState* pState)
{
	return UPDATE_CLICK(selectClick, left);
}
int xrInputSelectClick_Right(session_i sessionId, SubactionState* pState)
{
	return UPDATE_CLICK(selectClick, right);
}
int xrInputSqueezeValue_Left(session_i sessionId, SubactionState* pState)
{
	return 0;
}
int xrInputSqueezeValue_Right(session_i sessionId, SubactionState* pState)
{
	return 0;
}
int xrInputSqueezeClick_Left(session_i sessionId, SubactionState* pState)
{
	return 0;
}
int xrInputSqueezeClick_Right(session_i sessionId, SubactionState* pState)
{
	return 0;
}
int xrInputTriggerValue_Left(session_i sessionId, SubactionState* pState)
{
	return UPDATE_CLICK(selectClick, left);
}
int xrInputTriggerValue_Right(session_i sessionId, SubactionState* pState)
{
	return UPDATE_CLICK(selectClick, right);
}
int xrInputTriggerClick_Left(session_i sessionId, SubactionState* pState)
{
	return UPDATE_CLICK(selectClick, left);
}
int xrInputTriggerClick_Right(session_i sessionId, SubactionState* pState)
{
	return UPDATE_CLICK(selectClick, right);
}
int xrInputMenuClick_Left(session_i sessionId, SubactionState* pState)
{
	return 0;
}
int xrInputMenuClick_Right(session_i sessionId, SubactionState* pState)
{
	return 0;
}

#define UPDATE_POSE(pose, chirality)                                                                          \
	({                                                                                                        \
		auto pNodeShared = node.pShared[sessionId];                                                        \
		atomic_thread_fence(memory_order_acquire);                                                            \
		auto pController = &pNodeShared->chirality;                                                           \
		auto changed = pState->isActive != pController->active ||                                             \
		               memcmp(&pState->eulerPoseValue.euler, &pController->pose.euler, sizeof(XrVector3f)) || \
		               memcmp(&pState->eulerPoseValue.position, &pController->pose.pos, sizeof(XrVector3f));  \
		pState->isActive = pController->active;                                                               \
		pState->eulerPoseValue.euler = *(XrVector3f*)&pController->pose.euler;                                \
		pState->eulerPoseValue.position = *(XrVector3f*)&pController->pose.pos;                               \
		changed;                                                                                              \
	})
int xrInputGripPose_Left(session_i sessionId, SubactionState* pState)
{
	return UPDATE_POSE(gripPose, left);
}
int xrInputGripPose_Right(session_i sessionId, SubactionState* pState)
{
	return UPDATE_POSE(gripPose, right);
}
int xrInputAimPose_Left(session_i sessionId, SubactionState* pState)
{
	return UPDATE_POSE(aimPose, left);
}
int xrInputAimPose_Right(session_i sessionId, SubactionState* pState)
{
	return UPDATE_POSE(aimPose, right);
}

int xrOutputHaptic_Left(session_i sessionId, SubactionState* pState)
{
	return 0;
}
int xrOutputHaptic_Right(session_i sessionId, SubactionState* pState)
{
	return 0;
}