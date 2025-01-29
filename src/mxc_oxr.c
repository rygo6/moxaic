#include "node.h"

#include "mid_openxr_runtime.h"
#include "mid_vulkan.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void midXrInitialize()
{
	printf("Initializing Moxaic OpenXR Node.\n");
	isCompositor = false;
	mxcConnectInterprocessNode(false);
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
	__atomic_store_n(&pNodeShared->timelineValue, timelineValue, __ATOMIC_RELEASE);
}

void xrGetCompositorTimeline(XrSessionIndex sessionIndex, HANDLE* pHandle)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionIndex];
	MxcNodeImports* pImportParam = &pImportedExternalMemory->imports;
	MxcNodeShared*  pNodeShared = &pImportedExternalMemory->shared;
	*pHandle = pImportParam->compositorTimelineHandle;
}

void xrClaimFramebufferImages(XrSessionIndex sessionIndex, int imageCount, HANDLE* pHandle)
{
	CHECK(imageCount != VK_SWAP_COUNT, "Required swap image count does not match imported swap count!")
	MxcNodeContext* pNodeContext = &nodeContexts[sessionIndex];

	MxcNodeImports* pImportParam = &pImportedExternalMemory->imports;
	MxcNodeShared*  pNodeShared = &pImportedExternalMemory->shared;

	for (int i = 0; i < imageCount; ++i) {
//		printf("Claiming framebuffer handle %p\n", pImportParam->framebufferHandles[i].color);
//		pHandle[i] = pImportParam->framebufferHandles[i].color;
	}
}

void xrClaimSwapPoolImage(XrSessionIndex sessionIndex, uint8_t* pIndex)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionIndex];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	uint64_t timelineValue = __atomic_load_n(&pNodeShared->timelineValue, __ATOMIC_ACQUIRE);
	uint8_t index = (timelineValue % VK_SWAP_COUNT);
	*pIndex = index;
//	printf("Claiming swap index %d timeline %llu\n", index, timelineValue);
//	uint8_t priorIndex = __atomic_exchange_n(&pNodeShared->swapClaimed[index], true, __ATOMIC_SEQ_CST);
//	assert(priorIndex == false);
}

void xrReleaseSwapPoolImage(XrSessionIndex sessionIndex, uint8_t index)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionIndex];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	uint64_t timelineValue = __atomic_load_n(&pNodeShared->timelineValue, __ATOMIC_ACQUIRE);
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

void xrGetHeadPose(XrSessionIndex sessionIndex, XrVector3f* pEuler, XrVector3f* pPos)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionIndex];
	MxcNodeShared*  pNodeShared = pNodeContext->pNodeShared;
	*pEuler = *(XrVector3f*)&pNodeShared->cameraPose.euler;
	*pPos = *(XrVector3f*)&pNodeShared->cameraPose.position;
}

void xrGetEyeView(XrSessionIndex sessionIndex, uint8_t viewIndex, XrEyeView *pEyeView)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionIndex];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	pEyeView->euler = *(XrVector3f*)&pNodeShared->cameraPose.euler;
	pEyeView->position = *(XrVector3f*)&pNodeShared->cameraPose.position;
	pEyeView->fovRad = (XrVector2f){pNodeShared->camera.yFovRad, pNodeShared->camera.yFovRad};
	pEyeView->upperLeftClip = *(XrVector2f*)&pNodeShared->ulScreenUV;
	pEyeView->lowerRightClip = *(XrVector2f*)&pNodeShared->lrScreenUV;
}