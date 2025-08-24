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
void xrClaimSessionId(XrSessionId* pSessionId)
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

	*pSessionId = hNode; // openxr sessionId == moxaic node handle

	mxcIpcFuncEnqueue(&pNodeShrd->nodeInterprocessFuncQueue, MXC_INTERPROCESS_TARGET_NODE_OPENED);
}

void xrReleaseSessionId(XrSessionId sessionId)
{
	LOG("Releasing Moxaic OpenXR Session.\n");
	auto pNodeShrd = &pImportedExternalMemory->shared;

	NodeHandle nodeHandle = sessionId;
	ReleaseCompositorNodeActive(nodeHandle);

	mxcIpcFuncEnqueue(&pNodeShrd->nodeInterprocessFuncQueue, MXC_INTERPROCESS_TARGET_NODE_CLOSED);
}

void xrClaimCompositionLayerIndex(XrSessionId* sessionId)
{
	// I believe both a session and a composition layer will end up constituting different Nodes
	// and requesting a SessionId will simply mean the base compositionlayer index
	// where Compositionlayers will also request an index of different nodes
}

void xrGetReferenceSpaceBounds(XrSessionId sessionId, XrExtent2Df* pBounds)
{
	MxcNodeShared* pNodeShared = node.pShared[sessionId];
	float radius = pNodeShared->compositorRadius * 2.0f;

	// we just assume depth is the same, and treat this like a cube
	*pBounds = (XrExtent2Df) {.width = radius, .height = radius };
}

void xrGetSessionTimeline(XrSessionId sessionId, HANDLE* pHandle)
{
	MxcNodeImports* pImportParam = &pImportedExternalMemory->imports;
	*pHandle = pImportParam->nodeTimelineHandle;
}

/// Set the new session timeline value to signal to the compositor there is a new frame.
// should this be finish frame? and merged with SetColorSwapId and SetDepthSwapid? Probably
void xrSetSessionTimelineValue(XrSessionId sessionId, uint64_t timelineValue)
{
	MxcNodeShared*  pNodeShared = node.pShared[sessionId];
	atomic_store_explicit(&pNodeShared->timelineValue, timelineValue, memory_order_release);
}

void xrGetCompositorTimeline(XrSessionId sessionId, HANDLE* pHandle)
{
	MxcNodeImports* pImportParam = &pImportedExternalMemory->imports;
	*pHandle = pImportParam->compositorTimelineHandle;
}

void xrCreateSwapchainImages(XrSessionId sessionId, const XrSwapchainInfo* pSwapInfo, XrSwapchainId swapId)
{
	auto pNodeCtxt = &node.context[sessionId];
	auto pNodeShrd = node.pShared[sessionId];

	assert(pNodeShrd->swapStates[swapId] == XR_SWAP_STATE_UNITIALIZED && "Trying to create swapchain images with used swap index!");

	pNodeShrd->swapStates[swapId] = XR_SWAP_STATE_REQUESTED;
	pNodeShrd->swapInfos[swapId] = *pSwapInfo;

	mxcIpcFuncEnqueue(&pNodeShrd->nodeInterprocessFuncQueue, MXC_INTERPROCESS_TARGET_SYNC_SWAPS);
	WaitForSingleObject(pNodeCtxt->swapsSyncedHandle, INFINITE);

	if (pNodeShrd->swapStates[swapId] == XR_SWAP_STATE_ERROR) {
		LOG_ERROR("OpenXR failed to acquire swap!");
		pNodeShrd->swapStates[swapId] = XR_SWAP_STATE_UNITIALIZED;
	}
}

void xrGetSwapchainImportedImage(XrSessionId sessionId, XrSwapchainId swapIndex, int imageId, HANDLE* pHandle)
{
	auto pNodeShrd = node.pShared[sessionId];
	auto pImports = &pImportedExternalMemory->imports;
	assert(pNodeShrd->swapStates[swapIndex] == XR_SWAP_STATE_CREATED && "Trying to get swap image that has not been created!!");
	*pHandle = pImports->swapImageHandles[swapIndex][imageId];
}

void xrSetColorSwapId(XrSessionId sessionId, XrViewId viewId, XrSwapchainId swapId)
{
	MxcNodeShared* pNodeShrd = node.pShared[sessionId];
	pNodeShrd->viewSwaps[viewId].colorId = swapId;
}

void xrSetDepthSwapId(XrSessionId sessionId, XrViewId viewId, XrSwapchainId swapId)
{
	MxcNodeShared* pNodeShrd = node.pShared[sessionId];
	pNodeShrd->viewSwaps[viewId].depthId = swapId;
}

void xrSetDepthInfo(XrSessionId sessionId, float minDepth, float maxDepth, float nearZ, float farZ)
{
	MxcNodeShared* pNodeShrd = node.pShared[sessionId];
	// pNodeShrd->processState.minDepth = minDepth;
	// pNodeShrd->processState.maxDepth = maxDepth;
	pNodeShrd->processState.depthNearZ = nearZ;
	pNodeShrd->processState.depthFarZ = farZ;
}

void xrClaimSwapIndex(XrSessionId sessionId, uint8_t* pIndex)
{
	MxcNodeShared* pNodeShared = node.pShared[sessionId];
	uint64_t timelineValue = atomic_load_explicit(&pNodeShared->timelineValue, memory_order_acquire);
	uint8_t index = (timelineValue % VK_SWAP_COUNT);
	*pIndex = index;
//	printf("Claiming swap index %d timeline %llu\n", index, timelineValue);
//	uint8_t priorIndex = __atomic_exchange_n(&pNodeShared->swapClaimed[index], true, __ATOMIC_SEQ_CST);
//	assert(priorIndex == false);
}

void xrReleaseSwapIndex(XrSessionId sessionId, uint8_t index)
{
	MxcNodeShared* pNodeShared = node.pShared[sessionId];
	uint64_t timelineValue = atomic_load_explicit(&pNodeShared->timelineValue, memory_order_acquire);
//	printf("Releasing swap index %d timeline %llu\n", index, timelineValue);
//	uint8_t priorIndex = __atomic_exchange_n(&pNodeShared->swapClaimed[index], false, __ATOMIC_SEQ_CST);
//	assert(priorIndex == true);
}

void xrSetInitialCompositorTimelineValue(XrSessionId sessionId, uint64_t timelineValue)
{
	MxcNodeShared* pNodeShared = node.pShared[sessionId];
	u64 timelineCycleStartValue = timelineValue - (timelineValue % MXC_CYCLE_COUNT);
	pNodeShared->compositorBaseCycleValue = timelineCycleStartValue + MXC_CYCLE_COUNT;
//	printf("Setting compositorBaseCycleValue %llu\n", pNodeShared->compositorBaseCycleValue);
}

void xrGetCompositorTimelineValue(XrSessionId sessionId, bool synchronized, uint64_t* pTimelineValue)
{
	MxcNodeShared* pNodeShared = node.pShared[sessionId];
	*pTimelineValue = pNodeShared->compositorBaseCycleValue + MXC_CYCLE_POST_UPDATE_NODE_STATES_COMPLETE;
}

void xrProgressCompositorTimelineValue(XrSessionId sessionId, uint64_t timelineValue, bool synchronized)
{
	MxcNodeShared*  pNodeShared = node.pShared[sessionId];
	pNodeShared->compositorBaseCycleValue += MXC_CYCLE_COUNT * pNodeShared->compositorCycleSkip;
}

XrTime xrGetFrameInterval(XrSessionId sessionId, bool synchronized)
{
	MxcNodeShared* pNodeShared = node.pShared[sessionId];
	double hz = 240.0 / (double)(pNodeShared->compositorCycleSkip);
	XrTime hzTime = xrHzToXrTime(hz);
//	printf("xrGetFrameInterval compositorCycleSkip: %d hz: %f hzTime: %llu\n", pNodeShared->compositorCycleSkip, hz, hzTime);
	return hzTime;
}

void xrGetHeadPose(XrSessionId sessionId, XrEulerPosef* pPose)
{
	MxcNodeShared*  pNodeShared = node.pShared[sessionId];
	pPose->euler = *(XrVector3f*)&pNodeShared->cameraPose.euler;
	pPose->position = *(XrVector3f*)&pNodeShared->cameraPose.pos;
}

void xrGetEyeView(XrSessionId sessionId, uint8_t viewIndex, XrEyeView *pEyeView)
{
	MxcNodeShared* pNodeShared = node.pShared[sessionId];
	pEyeView->euler = *(XrVector3f*)&pNodeShared->cameraPose.euler;
	pEyeView->position = *(XrVector3f*)&pNodeShared->cameraPose.pos;
	pEyeView->fovRad = (XrVector2f){pNodeShared->camera.yFovRad, pNodeShared->camera.yFovRad};
	pEyeView->upperLeftClip = *(XrVector2f*)&pNodeShared->clip.ulUV;
	pEyeView->lowerRightClip = *(XrVector2f*)&pNodeShared->clip.lrUV;
	if (viewIndex == 1)
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
int xrInputSelectClick_Left(XrSessionId sessionId, SubactionState* pState)
{
	return UPDATE_CLICK(selectClick, left);
}
int xrInputSelectClick_Right(XrSessionId sessionId, SubactionState* pState)
{
	return UPDATE_CLICK(selectClick, right);
}
int xrInputSqueezeValue_Left(XrSessionId sessionId, SubactionState* pState)
{
	return 0;
}
int xrInputSqueezeValue_Right(XrSessionId sessionId, SubactionState* pState)
{
	return 0;
}
int xrInputSqueezeClick_Left(XrSessionId sessionId, SubactionState* pState)
{
	return 0;
}
int xrInputSqueezeClick_Right(XrSessionId sessionId, SubactionState* pState)
{
	return 0;
}
int xrInputTriggerValue_Left(XrSessionId sessionId, SubactionState* pState)
{
	return UPDATE_CLICK(selectClick, left);
}
int xrInputTriggerValue_Right(XrSessionId sessionId, SubactionState* pState)
{
	return UPDATE_CLICK(selectClick, right);
}
int xrInputTriggerClick_Left(XrSessionId sessionId, SubactionState* pState)
{
	return UPDATE_CLICK(selectClick, left);
}
int xrInputTriggerClick_Right(XrSessionId sessionId, SubactionState* pState)
{
	return UPDATE_CLICK(selectClick, right);
}
int xrInputMenuClick_Left(XrSessionId sessionId, SubactionState* pState)
{
	return 0;
}
int xrInputMenuClick_Right(XrSessionId sessionId, SubactionState* pState)
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
int xrInputGripPose_Left(XrSessionId sessionId, SubactionState* pState)
{
	return UPDATE_POSE(gripPose, left);
}
int xrInputGripPose_Right(XrSessionId sessionId, SubactionState* pState)
{
	return UPDATE_POSE(gripPose, right);
}
int xrInputAimPose_Left(XrSessionId sessionId, SubactionState* pState)
{
	return UPDATE_POSE(aimPose, left);
}
int xrInputAimPose_Right(XrSessionId sessionId, SubactionState* pState)
{
	return UPDATE_POSE(aimPose, right);
}

int xrOutputHaptic_Left(XrSessionId sessionId, SubactionState* pState)
{
	return 0;
}
int xrOutputHaptic_Right(XrSessionId sessionId, SubactionState* pState)
{
	return 0;
}

#pragma GCC diagnostic pop
