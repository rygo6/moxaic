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
	node_h          hNode    = RequestExternalNodeHandle(&pImportedExternalMemory->shared);
	MxcNodeContext* pNodeCtxt = BLOCK_PTR_H(node.context, hNode);

	pNodeCtxt->interprocessMode = MXC_NODE_INTERPROCESS_MODE_IMPORTED;
	pNodeCtxt->swapsSyncedHandle = pImportedExternalMemory->imports.swapsSyncedHandle;

	MxcNodeShared* pNodeShrd = ARRAY_H(node.pShared, hNode);
	pNodeShrd->compositorMode = MXC_COMPOSITOR_MODE_COMPUTE;

	LOG("Importing node handle %d as OpenXR session\n", hNode);

	*pSessionIndex = hNode; // openxr sessionId == moxaic node handle

	mxcIpcFuncEnqueue(hNode, MXC_INTERPROCESS_TARGET_NODE_OPENED);
}

void xrReleaseSessionId(session_i iSession)
{
	LOG("Releasing Moxaic OpenXR Session.\n");
	node_h hNode = iSession;
	MxcNodeContext* pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*  pNodeShrd = ARRAY_H(node.pShared, hNode);

	mxcIpcFuncEnqueue(hNode, MXC_INTERPROCESS_TARGET_NODE_CLOSED);
}

XrResult xrSharedPollEvent(session_i iSession, XrEventDataUnion* pEventData)
{
	node_h hNode = iSession;
	MxcNodeContext* pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*  pNodeShrd = ARRAY_H(node.pShared, hNode);

	return MID_QRING_DEQUEUE(&pNodeShrd->eventDataQueue, pNodeShrd->queuedEventDataBuffers, pEventData) == MID_SUCCESS ?
		XR_SUCCESS : XR_EVENT_UNAVAILABLE;
}

void xrGetReferenceSpaceBounds(session_i iSession, XrExtent2Df* pBounds)
{
	node_h hNode = iSession;
	MxcNodeContext* pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*  pNodeShrd = ARRAY_H(node.pShared, hNode);

	float radius = pNodeShrd->compositorRadius * 2.0f;

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
	node_h hNode = iSession;
	MxcNodeContext* pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*  pNodeShrd = ARRAY_H(node.pShared, hNode);
	atomic_store_explicit(&pNodeShrd->timelineValue, timelineValue, memory_order_release);
}

void xrGetCompositorTimeline(session_i iSession, HANDLE* pHandle)
{
	MxcNodeImports* pImportParam = &pImportedExternalMemory->imports;
	*pHandle = pImportParam->compositorTimelineHandle;
}

XrResult xrCreateSwapchainImages(session_i iSession, swap_i iSwap, const XrSwapInfo* pInfo)
{
	node_h hNode = iSession;
	MxcNodeContext* pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*  pNodeShrd = ARRAY_H(node.pShared, hNode);

	if (pNodeShrd->nodeSwapStates[iSwap] != XR_SWAP_STATE_UNITIALIZED) {
		LOG_ERROR("Trying to create swapchain images with used swap index!\n");
		return XR_ERROR_HANDLE_INVALID;
	}

	pNodeShrd->nodeSwapStates[iSwap] = XR_SWAP_STATE_REQUESTED;
	pNodeShrd->nodeSwapInfos[iSwap]  = *pInfo;

	mxcIpcFuncEnqueue(hNode, MXC_INTERPROCESS_TARGET_SYNC_SWAPS);
	WaitForSingleObject(pNodeCtxt->swapsSyncedHandle, INFINITE);

	if (pNodeShrd->nodeSwapStates[iSwap] == XR_SWAP_STATE_REQUESTED) {
		LOG_ERROR("Compositor failed to create Swap!\n");
		return XR_ERROR_HANDLE_INVALID;
	}

	if (pNodeShrd->nodeSwapStates[iSwap] == XR_SWAP_STATE_ERROR) {
		LOG_ERROR("Compositor error on swap creation!\n");
		pNodeShrd->nodeSwapStates[iSwap] = XR_SWAP_STATE_UNITIALIZED;
		return XR_ERROR_HANDLE_INVALID;
	}

	ASSERT(pNodeShrd->nodeSwapStates[iSwap] == XR_SWAP_STATE_READY, "Swap is not XR_SWAP_STATE_READY after creation!");
	return XR_SUCCESS;
}

void xrGetSwapchainImportedImage(session_i iSession, swap_i iSwap, u32 iImg, HANDLE* pHandle)
{
	node_h hNode = iSession;
	MxcNodeContext* pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*  pNodeShrd = ARRAY_H(node.pShared, hNode);
	MxcNodeImports* pImports = &pImportedExternalMemory->imports;
	ASSERT(pNodeShrd->nodeSwapStates[iSwap] == XR_SWAP_STATE_READY, "Trying to get Swap Image which is not XR_SWAP_STATE_READY!");
	*pHandle = pImports->swapImageHandles[iSwap][iImg];
}

XrResult xrDestroySwapchainImages(session_i iSession, swap_i iSwap)
{
	node_h hNode = iSession;
	MxcNodeContext* pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*  pNodeShrd = ARRAY_H(node.pShared, hNode);

	if (pNodeShrd->nodeSwapStates[iSwap] != XR_SWAP_STATE_UNITIALIZED) {
		LOG_ERROR("Trying to destroy unitialized swapchain images!");
		return XR_ERROR_HANDLE_INVALID;
	}

	if (pNodeShrd->nodeSwapStates[iSwap] != XR_SWAP_STATE_DESTROYED) {
		LOG_ERROR("Trying to destroy already destroyed swapchain images!");
		return XR_ERROR_HANDLE_INVALID;
	}

	pNodeShrd->nodeSwapStates[iSwap] = XR_SWAP_STATE_DESTROYED;

	mxcIpcFuncEnqueue(hNode, MXC_INTERPROCESS_TARGET_SYNC_SWAPS);
	WaitForSingleObject(pNodeCtxt->swapsSyncedHandle, INFINITE);

	if (pNodeShrd->nodeSwapStates[iSwap] == XR_SWAP_STATE_DESTROYED) {
		LOG_ERROR("Compositor failed to destroy Swap!");
		return XR_ERROR_HANDLE_INVALID;
	}

	if (pNodeShrd->nodeSwapStates[iSwap] == XR_SWAP_STATE_ERROR) {
		LOG_ERROR("Compositor error when destroying swap!");
		pNodeShrd->nodeSwapStates[iSwap] = XR_SWAP_STATE_UNITIALIZED;
		return XR_ERROR_HANDLE_INVALID;
	}

	return XR_SUCCESS;
}

void xrSetColorSwapId(session_i iSession, XrViewId viewId, swap_i iSwap, u32 iImg)
{
	node_h hNode = iSession;
	MxcNodeContext* pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*  pNodeShrd = ARRAY_H(node.pShared, hNode);
	pNodeShrd->viewSwaps[viewId].iColorSwap = iSwap;
	pNodeShrd->viewSwaps[viewId].iColorImg = iImg;
	atomic_thread_fence(memory_order_release);
}

void xrSetDepthSwapId(session_i iSession, XrViewId viewId, swap_i iSwap, u32 iImg)
{
	node_h hNode = iSession;
	MxcNodeContext* pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*  pNodeShrd = ARRAY_H(node.pShared, hNode);
	pNodeShrd->viewSwaps[viewId].iDepthSwap = iSwap;
	pNodeShrd->viewSwaps[viewId].iDepthImg = iImg;
	atomic_thread_fence(memory_order_release);
}

void xrSetDepthInfo(session_i iSession, float minDepth, float maxDepth, float nearZ, float farZ)
{
	node_h hNode = iSession;
	MxcNodeContext* pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*  pNodeShrd = ARRAY_H(node.pShared, hNode);
	// pNodeShrd->processState.minDepth = minDepth;
	// pNodeShrd->processState.maxDepth = maxDepth;
	pNodeShrd->processState.depthNearZ = nearZ;
	pNodeShrd->processState.depthFarZ = farZ;
}

void xrSetInitialCompositorTimelineValue(session_i iSession, uint64_t timelineValue)
{
	node_h hNode = iSession;
	MxcNodeContext* pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*  pNodeShrd = ARRAY_H(node.pShared, hNode);

	u64 timelineCycleStartValue = timelineValue - (timelineValue % MXC_CYCLE_COUNT);
	// Wait two cycles out for good measure.
	pNodeShrd->compositorBaseCycleValue = timelineCycleStartValue + (MXC_CYCLE_COUNT * 2);
//	printf("Setting compositorBaseCycleValue %llu\n", pNodeShared->compositorBaseCycleValue);
}

void xrGetCompositorTimelineValue(session_i iSession, uint64_t* pTimelineValue)
{
	node_h hNode = iSession;
	MxcNodeContext* pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*  pNodeShrd = ARRAY_H(node.pShared, hNode);

	*pTimelineValue = pNodeShrd->compositorBaseCycleValue + MXC_CYCLE_POST_UPDATE_NODE_STATES_COMPLETE;
}

void xrProgressCompositorTimelineValue(session_i iSession, uint64_t timelineValue)
{
	node_h hNode = iSession;
	MxcNodeContext* pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*  pNodeShrd = ARRAY_H(node.pShared, hNode);

	pNodeShrd->compositorBaseCycleValue += MXC_CYCLE_COUNT * pNodeShrd->compositorCycleSkip;
}

XrTime xrGetFrameInterval(session_i iSession)
{
	node_h hNode = iSession;
	MxcNodeContext* pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*  pNodeShrd = ARRAY_H(node.pShared, hNode);

	double hz = 144.0 / (double)(pNodeShrd->compositorCycleSkip);
	XrTime hzTime = xrHzToXrTime(hz);
//	LOG("xrGetFrameInterval compositorCycleSkip: %d hz: %f hzTime: %llu\n", pNodeShared->compositorCycleSkip, hz, hzTime);
	return hzTime;
}

void xrGetHeadPose(session_i iSession, XrEulerPosef* pPose)
{
	node_h hNode = iSession;
	MxcNodeContext* pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*  pNodeShrd = ARRAY_H(node.pShared, hNode);

	pPose->euler    = *(XrVector3f*)&pNodeShrd->cameraPose.euler;
	pPose->position = *(XrVector3f*)&pNodeShrd->cameraPose.pos;
}

void xrGetEyeView(session_i iSession, view_i iView, XrEyeView *pEyeView)
{
	node_h hNode = iSession;
	MxcNodeContext* pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*  pNodeShrd = ARRAY_H(node.pShared, hNode);

	pEyeView->euler    = *(XrVector3f*)&pNodeShrd->cameraPose.euler;
	pEyeView->position = *(XrVector3f*)&pNodeShrd->cameraPose.pos;
	pEyeView->fovRad   = (XrVector2f){pNodeShrd->camera.yFovRad, pNodeShrd->camera.yFovRad};

	pEyeView->upperLeftClip  = *(XrVector2f*)&pNodeShrd->clip.ulUV;
	pEyeView->lowerRightClip = *(XrVector2f*)&pNodeShrd->clip.lrUV;

	// TODO this is to debug
	if (iView == 1)
		pEyeView->position.x += 0.1f;
}

#define UPDATE_CLICK(button, chirality)    \
    ({                                     \
        MxcNodeShared* pNodeShrd = ARRAY_H(node.pShared, (node_h)sessionId); \
        atomic_thread_fence(memory_order_acquire); \
        MxcController* pController = &pNodeShrd->chirality; \
        bool changed = pState->isActive != pController->active || \
                       pState->boolValue != pController->button; \
        pState->isActive = pController->active; \
        pState->boolValue = pController->button; \
        changed; \
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
        MxcNodeShared* pNodeShrd = ARRAY_H(node.pShared, (node_h)sessionId); \
		atomic_thread_fence(memory_order_acquire);                                                            \
		MxcController* pController = &pNodeShrd->chirality;                                                           \
		bool changed = pState->isActive != pController->active ||                                             \
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