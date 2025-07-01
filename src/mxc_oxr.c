#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"

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

	MxcNodeShared*  pNodeShared = &pImportedExternalMemory->shared;

	// I believe both a session and a composition layer will end up constituting different Nodes
	// and requesting a SessionIndex will simply mean the base compositionlayer index
	NodeHandle      nodeHandle = RequestExternalNodeHandle(pNodeShared);
	MxcNodeContext* pNodeContext = &node.ctxt[nodeHandle];
	*pNodeContext = (MxcNodeContext){};
	pNodeContext->type = MXC_NODE_INTERPROCESS_MODE_IMPORTED;
	pNodeContext->swapsSyncedHandle = pImportedExternalMemory->imports.swapsSyncedHandle;
	printf("Importing node handle %d as OpenXR session\n", nodeHandle);

	pNodeShared->appNodeHandle = nodeHandle;

	// openxr session = moxaic node
	*sessionIndex = nodeHandle;
}

void xrReleaseSessionIndex(XrSessionIndex sessionIndex)
{
	printf("Releasing Moxaic OpenXR Session.\n");
	auto pNodeShrd = &pImportedExternalMemory->shared;

	NodeHandle nodeHandle = sessionIndex;
	ReleaseNode(nodeHandle);

	pNodeShrd->appNodeHandle = -1; // I want to use the block stuff from mid oxr for this

	mxcIpcFuncEnqueue(&pNodeShrd->nodeInterprocessFuncQueue, MXC_INTERPROCESS_TARGET_NODE_CLOSED);
}

void xrClaimCompositionLayerIndex(XrSessionIndex* sessionIndex)
{
	// I believe both a session and a composition layer will end up constituting different Nodes
	// and requesting a SessionIndex will simply mean the base compositionlayer index
	// where Compositionlayers will also request an index of different nodes
}

void xrGetReferenceSpaceBounds(XrSessionIndex sessionIndex, XrExtent2Df* pBounds)
{
	MxcNodeShared* pNodeShared = node.pShared[sessionIndex];
	float radius = pNodeShared->compositorRadius * 2.0f;

	// we just assume depth is the same, and treat this like a cube
	*pBounds = (XrExtent2Df) {.width = radius, .height = radius };
}

void xrGetSessionTimeline(XrSessionIndex sessionIndex, HANDLE* pHandle)
{
	MxcNodeImports* pImportParam = &pImportedExternalMemory->imports;
	*pHandle = pImportParam->nodeTimelineHandle;
}

void xrSetSessionTimelineValue(XrSessionIndex sessionIndex, uint64_t timelineValue)
{
	MxcNodeShared*  pNodeShared = node.pShared[sessionIndex];
	atomic_store_explicit(&pNodeShared->timelineValue, timelineValue, memory_order_release);
}

void xrGetCompositorTimeline(XrSessionIndex sessionIndex, HANDLE* pHandle)
{
	MxcNodeImports* pImportParam = &pImportedExternalMemory->imports;
	*pHandle = pImportParam->compositorTimelineHandle;
}

void xrCreateSwapImages(XrSessionIndex sessionIndex, const XrSwapchainCreateInfo* createInfo, XrSwapType swapType)
{
	auto pNodeCtxt = &node.ctxt[sessionIndex];
	auto pImports = &pImportedExternalMemory->imports;
	MxcNodeShared* pNodeShrd = node.pShared[sessionIndex];

	if (pNodeShrd->swapType != swapType) {
		pImports->claimedColorSwapCount = 0;
		pImports->claimedDepthSwapCount = 0;
		pNodeShrd->swapType = swapType;
		pNodeShrd->swapWidth = createInfo->width;
		pNodeShrd->swapHeight = createInfo->height;

		mxcIpcFuncEnqueue(&pNodeShrd->nodeInterprocessFuncQueue, MXC_INTERPROCESS_TARGET_SYNC_SWAPS);

		WaitForSingleObject(pNodeCtxt->swapsSyncedHandle, INFINITE);

		if (pNodeShrd->swapType == XR_SWAP_TYPE_ERROR)
			LOG_ERROR("OXR could not acquire swapCtx!");
	}
}

void xrClaimSwapImage(XrSessionIndex sessionIndex, XrSwapOutputFlags usage, HANDLE* pHandle)
{
	auto pImports = &pImportedExternalMemory->imports;
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
	MxcNodeShared* pNodeShrd = node.pShared[sessionIndex];
	pNodeShrd->processState.depth.minDepth = minDepth;
	pNodeShrd->processState.depth.maxDepth = maxDepth;
	// These might come reverse due to reverse Z
//	pNodeShrd->depthState.nearZ = min(nearZ, farZ);
//	pNodeShrd->depthState.farZ = max(nearZ, farZ);
	pNodeShrd->processState.depth.nearZ = nearZ;
	pNodeShrd->processState.depth.farZ = farZ;
}

void xrClaimSwapIndex(XrSessionIndex sessionIndex, uint8_t* pIndex)
{
	MxcNodeShared* pNodeShared = node.pShared[sessionIndex];
	uint64_t timelineValue = atomic_load_explicit(&pNodeShared->timelineValue, memory_order_acquire);
	uint8_t index = (timelineValue % VK_SWAP_COUNT);
	*pIndex = index;
//	printf("Claiming swap index %d timeline %llu\n", index, timelineValue);
//	uint8_t priorIndex = __atomic_exchange_n(&pNodeShared->swapClaimed[index], true, __ATOMIC_SEQ_CST);
//	assert(priorIndex == false);
}

void xrReleaseSwapIndex(XrSessionIndex sessionIndex, uint8_t index)
{
	MxcNodeShared* pNodeShared = node.pShared[sessionIndex];
	uint64_t timelineValue = atomic_load_explicit(&pNodeShared->timelineValue, memory_order_acquire);
//	printf("Releasing swap index %d timeline %llu\n", index, timelineValue);
//	uint8_t priorIndex = __atomic_exchange_n(&pNodeShared->swapClaimed[index], false, __ATOMIC_SEQ_CST);
//	assert(priorIndex == true);
}

// not full sure if this should be done!?
//#define UNSYNCED_FRAME_SKIP 16

void xrSetInitialCompositorTimelineValue(XrSessionIndex sessionIndex, uint64_t timelineValue)
{
	MxcNodeShared* pNodeShared = node.pShared[sessionIndex];
	timelineValue = timelineValue - (timelineValue % MXC_CYCLE_COUNT);
	pNodeShared->compositorBaseCycleValue = timelineValue + MXC_CYCLE_COUNT;
//	printf("Setting compositorBaseCycleValue %llu\n", pNodeShared->compositorBaseCycleValue);
}

void xrGetCompositorTimelineValue(XrSessionIndex sessionIndex, bool synchronized, uint64_t* pTimelineValue)
{
	MxcNodeShared* pNodeShared = node.pShared[sessionIndex];
	*pTimelineValue = pNodeShared->compositorBaseCycleValue + MXC_CYCLE_POST_UPDATE_NODE_STATES_COMPLETE;
}

void xrProgressCompositorTimelineValue(XrSessionIndex sessionIndex, uint64_t timelineValue, bool synchronized)
{
	MxcNodeShared*  pNodeShared = node.pShared[sessionIndex];
	pNodeShared->compositorBaseCycleValue += MXC_CYCLE_COUNT * pNodeShared->compositorCycleSkip;
}

XrTime xrGetFrameInterval(XrSessionIndex sessionIndex, bool synchronized)
{
	MxcNodeShared* pNodeShared = node.pShared[sessionIndex];
	double hz = 240.0 / (double)(pNodeShared->compositorCycleSkip);
	XrTime hzTime = xrHzToXrTime(hz);
//	printf("xrGetFrameInterval compositorCycleSkip: %d hz: %f hzTime: %llu\n", pNodeShared->compositorCycleSkip, hz, hzTime);
	return hzTime;
}

void xrGetHeadPose(XrSessionIndex sessionIndex, XrEulerPosef* pPose)
{
	MxcNodeShared*  pNodeShared = node.pShared[sessionIndex];
	pPose->euler = *(XrVector3f*)&pNodeShared->cameraPose.euler;
	pPose->position = *(XrVector3f*)&pNodeShared->cameraPose.pos;
}

void xrGetEyeView(XrSessionIndex sessionIndex, uint8_t viewIndex, XrEyeView *pEyeView)
{
	MxcNodeShared* pNodeShared = node.pShared[sessionIndex];
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
		auto pNodeShared = node.pShared[sessionIndex];            \
		atomic_thread_fence(memory_order_acquire);                \
		auto pController = &pNodeShared->chirality;               \
		auto changed = pState->isActive != pController->active || \
		               pState->boolValue != pController->button;  \
		pState->isActive = pController->active;                   \
		pState->boolValue = pController->button;                  \
		changed;                                                  \
	})
int xrInputSelectClick_Left(XrSessionIndex sessionIndex, SubactionState* pState)
{
	return UPDATE_CLICK(selectClick, left);
}
int xrInputSelectClick_Right(XrSessionIndex sessionIndex, SubactionState* pState)
{
	return UPDATE_CLICK(selectClick, right);
}
int xrInputSqueezeValue_Left(XrSessionIndex sessionIndex, SubactionState* pState)
{
	return 0;
}
int xrInputSqueezeValue_Right(XrSessionIndex sessionIndex, SubactionState* pState)
{
	return 0;
}
int xrInputSqueezeClick_Left(XrSessionIndex sessionIndex, SubactionState* pState)
{
	return 0;
}
int xrInputSqueezeClick_Right(XrSessionIndex sessionIndex, SubactionState* pState)
{
	return 0;
}
int xrInputTriggerValue_Left(XrSessionIndex sessionIndex, SubactionState* pState)
{
	return UPDATE_CLICK(selectClick, left);
}
int xrInputTriggerValue_Right(XrSessionIndex sessionIndex, SubactionState* pState)
{
	return UPDATE_CLICK(selectClick, right);
}
int xrInputTriggerClick_Left(XrSessionIndex sessionIndex, SubactionState* pState)
{
	return UPDATE_CLICK(selectClick, left);
}
int xrInputTriggerClick_Right(XrSessionIndex sessionIndex, SubactionState* pState)
{
	return UPDATE_CLICK(selectClick, right);
}
int xrInputMenuClick_Left(XrSessionIndex sessionIndex, SubactionState* pState)
{
	return 0;
}
int xrInputMenuClick_Right(XrSessionIndex sessionIndex, SubactionState* pState)
{
	return 0;
}

#define UPDATE_POSE(pose, chirality)                                                                          \
	({                                                                                                        \
		auto pNodeShared = node.pShared[sessionIndex];                                                        \
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
int xrInputGripPose_Left(XrSessionIndex sessionIndex, SubactionState* pState)
{
	return UPDATE_POSE(gripPose, left);
}
int xrInputGripPose_Right(XrSessionIndex sessionIndex, SubactionState* pState)
{
	return UPDATE_POSE(gripPose, right);
}
int xrInputAimPose_Left(XrSessionIndex sessionIndex, SubactionState* pState)
{
	return UPDATE_POSE(aimPose, left);
}
int xrInputAimPose_Right(XrSessionIndex sessionIndex, SubactionState* pState)
{
	return UPDATE_POSE(aimPose, right);
}

int xrOutputHaptic_Left(XrSessionIndex sessionIndex, SubactionState* pState)
{
	return 0;
}
int xrOutputHaptic_Right(XrSessionIndex sessionIndex, SubactionState* pState)
{
	return 0;
}

#pragma GCC diagnostic pop
