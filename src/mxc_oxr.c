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

void xrSwapConfig(XrSystemId systemId, XrSwapConfig* pConfig)
{
	pConfig->width = DEFAULT_WIDTH;
	pConfig->height = DEFAULT_HEIGHT;
	pConfig->samples = 1;
	pConfig->outputs = 1;
}


// maybe should be external handle?
void xrClaimSessionIndex(XrSessionIndex* sessionIndex)
{
	printf("Creating Moxaic OpenXR Session.\n");

	MxcNodeImports* pImportParam = &pImportedExternalMemory->imports;
	MxcNodeShared*  pNodeShared = &pImportedExternalMemory->shared;

	NodeHandle      nodeHandle = RequestExternalNodeHandle(pNodeShared);
	MxcNodeContext* pNodeContext = &nodeContexts[nodeHandle];
	*pNodeContext = (MxcNodeContext){};
	pNodeContext->type = MXC_NODE_TYPE_INTERPROCESS_VULKAN_IMPORTED;
	pNodeContext->pNodeShared = pNodeShared;
	pNodeContext->swapsSyncedHandle = pImportedExternalMemory->imports.swapsSyncedHandle;
	printf("Importing node handle %d as OpenXR session\n", nodeHandle);

	// openxr session = moxaic node
	*sessionIndex = nodeHandle;
}

void xrReleaseSessionIndex(XrSessionIndex sessionIndex)
{
	printf("Releasing Moxaic OpenXR Session.\n");
	NodeHandle nodeHandle = sessionIndex;
	ReleaseNode(nodeHandle);
}

void xrGetReferenceSpaceBounds(XrSessionIndex sessionIndex, XrExtent2Df* pBounds)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionIndex];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	float radius = pNodeShared->compositorRadius * 2.0f;

	// we just assume depth is the same, and treat this like a cube
	*pBounds = (XrExtent2Df) {.width = radius, .height = radius };
}

void xrGetSessionTimeline(XrSessionIndex sessionIndex, HANDLE* pHandle)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionIndex];
	MxcNodeImports* pImportParam = &pImportedExternalMemory->imports;
	MxcNodeShared*  pNodeShared = &pImportedExternalMemory->shared;
	*pHandle = pImportParam->nodeTimelineHandle;
}

void xrSetSessionTimelineValue(XrSessionIndex sessionIndex, uint64_t timelineValue)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionIndex];
	MxcNodeShared*  pNodeShared = pNodeContext->pNodeShared;
	atomic_store_explicit(&pNodeShared->timelineValue, timelineValue, memory_order_release);
}

void xrGetCompositorTimeline(XrSessionIndex sessionIndex, HANDLE* pHandle)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionIndex];
	MxcNodeImports* pImportParam = &pImportedExternalMemory->imports;
	MxcNodeShared*  pNodeShared = &pImportedExternalMemory->shared;
	*pHandle = pImportParam->compositorTimelineHandle;
}

void xrCreateSwapImages(XrSessionIndex sessionIndex, XrSwapType swapType)
{
	auto pNodeCtxt = &nodeContexts[sessionIndex];
	auto pImports = &pImportedExternalMemory->imports;
	auto pNodeShrd = &pImportedExternalMemory->shared;
	if (pNodeShrd->swapType != swapType) {
		pImports->claimedColorSwapCount = 0;
		pImports->claimedDepthSwapCount = 0;
		pNodeShrd->swapType = swapType;
		mxcIpcFuncEnqueue(&pNodeShrd->nodeInterprocessFuncQueue, MXC_INTERPROCESS_TARGET_SYNC_SWAPS);
		printf("Waiting on swap claim.\n"); /// really we should wait elsewhere
		WaitForSingleObject(pNodeCtxt->swapsSyncedHandle, INFINITE);
	}
}

void xrClaimSwapImage(XrSessionIndex sessionIndex, XrSwapOutputFlags usage, HANDLE* pHandle)
{
	auto pNodeCtxt = &nodeContexts[sessionIndex];
	auto pImports = &pImportedExternalMemory->imports;
	auto pNodeShrd = &pImportedExternalMemory->shared;
	switch (usage) {
		case XR_SWAP_OUTPUT_FLAG_COLOR:
			*pHandle = pImports->colorSwapHandles[pImports->claimedColorSwapCount++];
			break;
		case XR_SWAP_OUTPUT_FLAG_DEPTH:
			*pHandle = pImports->depthSwapHandles[pImports->claimedDepthSwapCount++];
			break;
	}
}

void xrSetDepthInfo(XrSessionIndex sessionIndex, float minDepth, float maxDepth, float nearZ, float farZ)
{
	auto pNodeCtxt = &nodeContexts[sessionIndex];
	auto pImports = &pImportedExternalMemory->imports;
	auto pNodeShrd = &pImportedExternalMemory->shared;
	pNodeShrd->depthState.minDepth = minDepth;
	pNodeShrd->depthState.maxDepth = maxDepth;
	// These might come reverse due to reverse Z
//	pNodeShrd->depthState.nearZ = min(nearZ, farZ);
//	pNodeShrd->depthState.farZ = max(nearZ, farZ);
	pNodeShrd->depthState.nearZ = nearZ;
	pNodeShrd->depthState.farZ = farZ;
}

void xrClaimSwapIndex(XrSessionIndex sessionIndex, uint8_t* pIndex)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionIndex];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	uint64_t timelineValue = atomic_load_explicit(&pNodeShared->timelineValue, memory_order_acquire);
	uint8_t index = (timelineValue % VK_SWAP_COUNT);
	*pIndex = index;
//	printf("Claiming swap index %d timeline %llu\n", index, timelineValue);
//	uint8_t priorIndex = __atomic_exchange_n(&pNodeShared->swapClaimed[index], true, __ATOMIC_SEQ_CST);
//	assert(priorIndex == false);
}

void xrReleaseSwapIndex(XrSessionIndex sessionIndex, uint8_t index)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionIndex];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	uint64_t timelineValue = atomic_load_explicit(&pNodeShared->timelineValue, memory_order_acquire);
//	printf("Releasing swap index %d timeline %llu\n", index, timelineValue);
//	uint8_t priorIndex = __atomic_exchange_n(&pNodeShared->swapClaimed[index], false, __ATOMIC_SEQ_CST);
//	assert(priorIndex == true);
}

// not full sure if this should be done!?
//#define UNSYNCED_FRAME_SKIP 16

void xrSetInitialCompositorTimelineValue(XrSessionIndex sessionIndex, uint64_t timelineValue)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionIndex];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	timelineValue = timelineValue - (timelineValue % MXC_CYCLE_COUNT);
	pNodeShared->compositorBaseCycleValue = timelineValue + MXC_CYCLE_COUNT;
//	printf("Setting compositorBaseCycleValue %llu\n", pNodeShared->compositorBaseCycleValue);
}

void xrGetCompositorTimelineValue(XrSessionIndex sessionIndex, bool synchronized, uint64_t* pTimelineValue)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionIndex];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	*pTimelineValue = pNodeShared->compositorBaseCycleValue + MXC_CYCLE_POST_UPDATE_NODE_STATES_COMPLETE;
}

void xrProgressCompositorTimelineValue(XrSessionIndex sessionIndex, uint64_t timelineValue, bool synchronized)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionIndex];
	MxcNodeShared*  pNodeShared = pNodeContext->pNodeShared;
	pNodeShared->compositorBaseCycleValue += MXC_CYCLE_COUNT * pNodeShared->compositorCycleSkip;
}

XrTime xrGetFrameInterval(XrSessionIndex sessionIndex, bool synchronized)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionIndex];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	double hz = 240.0 / (double)(pNodeShared->compositorCycleSkip);
	XrTime hzTime = xrHzToXrTime(hz);
//	printf("xrGetFrameInterval compositorCycleSkip: %d hz: %f hzTime: %llu\n", pNodeShared->compositorCycleSkip, hz, hzTime);
	return hzTime;
}

void xrGetControllerPose(XrSessionIndex sessionIndex, XrEulerPosef* pPose)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionIndex];
	MxcNodeShared*  pNodeShared = pNodeContext->pNodeShared;
	pPose->euler = *(XrVector3f*)&pNodeShared->cameraPose.euler;
	pPose->position = *(XrVector3f*)&pNodeShared->cameraPose.position;
}

void xrGetHeadPose(XrSessionIndex sessionIndex, XrEulerPosef* pPose)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionIndex];
	MxcNodeShared*  pNodeShared = pNodeContext->pNodeShared;
	pPose->euler = *(XrVector3f*)&pNodeShared->cameraPose.euler;
	pPose->position = *(XrVector3f*)&pNodeShared->cameraPose.position;
}

void xrGetEyeView(XrSessionIndex sessionIndex, uint8_t viewIndex, XrEyeView *pEyeView)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionIndex];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	pEyeView->euler = *(XrVector3f*)&pNodeShared->cameraPose.euler;
	pEyeView->position = *(XrVector3f*)&pNodeShared->cameraPose.position;
	pEyeView->fovRad = (XrVector2f){pNodeShared->camera.yFovRad, pNodeShared->camera.yFovRad};
	pEyeView->upperLeftClip = *(XrVector2f*)&pNodeShared->ulClipUV;
	pEyeView->lowerRightClip = *(XrVector2f*)&pNodeShared->lrClipUV;
}