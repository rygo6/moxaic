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

// shoul dbe claim node? that introduces non-oxr terms into this though
void xrClaimSession(XrHandle* pSessionHandle)
{
	printf("Creating Moxaic OpenXR Session.\n");

	MxcImportParam* pImportParam = &pImportedExternalMemory->importParam;
	MxcNodeShared*  pNodeShared = &pImportedExternalMemory->shared;

	NodeHandle      nodeHandle = RequestExternalNodeHandle(pNodeShared);
	MxcNodeContext* pNodeContext = &nodeContexts[nodeHandle];
	*pNodeContext = (MxcNodeContext){};
	pNodeContext->type = MXC_NODE_TYPE_INTERPROCESS_VULKAN_IMPORTED;
	pNodeContext->pNodeShared = pNodeShared;
	printf("Importing node handle %d as OpenXR session\n", nodeHandle);

	// openxr session = moxaic node
	*pSessionHandle = nodeHandle;
}

void xrReleaseSession(XrHandle sessionHandle)
{
	printf("Releasing Moxaic OpenXR Session.\n");
	NodeHandle nodeHandle = sessionHandle;
	ReleaseNode(nodeHandle);
}

void midXrGetReferenceSpaceBounds(XrHandle sessionHandle, XrExtent2Df* pBounds)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	float radius = pNodeShared->compositorRadius * 2.0f;

	// we just assume depth is the same, and treat this like a cube
	*pBounds = (XrExtent2Df) {.width = radius, .height = radius };
}

void xrGetSessionTimeline(XrHandle sessionHandle, HANDLE* pHandle)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcImportParam* pImportParam = &pImportedExternalMemory->importParam;
	MxcNodeShared*  pNodeShared = &pImportedExternalMemory->shared;
	*pHandle = pImportParam->nodeTimelineHandle;
}

void xrSetSessionTimelineValue(XrHandle sessionHandle, uint64_t timelineValue)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcNodeShared*  pNodeShared = pNodeContext->pNodeShared;
	__atomic_store_n(&pNodeShared->timelineValue, timelineValue, __ATOMIC_RELEASE);
}

void xrGetCompositorTimeline(XrHandle sessionHandle, HANDLE* pHandle)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcImportParam* pImportParam = &pImportedExternalMemory->importParam;
	MxcNodeShared*  pNodeShared = &pImportedExternalMemory->shared;
	*pHandle = pImportParam->compositorTimelineHandle;
}

void midXrClaimFramebufferImages(XrHandle sessionHandle, int imageCount, HANDLE* pHandle)
{
	CHECK(imageCount != VK_SWAP_COUNT, "Required swap image count does not match imported swap count!")
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];

	MxcImportParam* pImportParam = &pImportedExternalMemory->importParam;
	MxcNodeShared*  pNodeShared = &pImportedExternalMemory->shared;

	for (int i = 0; i < imageCount; ++i) {
		printf("Claiming framebuffer handle %p\n", pImportParam->framebufferHandles[i].color);
		pHandle[i] = pImportParam->framebufferHandles[i].color;
	}
}

void xrClaimSwapPoolImage(XrHandle sessionHandle, uint8_t* pIndex)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	uint64_t timelineValue = __atomic_load_n(&pNodeShared->timelineValue, __ATOMIC_ACQUIRE);
	uint8_t index = (timelineValue % VK_SWAP_COUNT);
	*pIndex = index;
//	printf("Claiming swap index %d timeline %llu\n", index, timelineValue);
//	uint8_t priorIndex = __atomic_exchange_n(&pNodeShared->swapClaimed[index], true, __ATOMIC_SEQ_CST);
//	assert(priorIndex == false);
}

void xrReleaseSwapPoolImage(XrHandle sessionHandle, uint8_t index)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	uint64_t timelineValue = __atomic_load_n(&pNodeShared->timelineValue, __ATOMIC_ACQUIRE);
//	printf("Releasing swap index %d timeline %llu\n", index, timelineValue);
//	uint8_t priorIndex = __atomic_exchange_n(&pNodeShared->swapClaimed[index], false, __ATOMIC_SEQ_CST);
//	assert(priorIndex == true);
}

// not full sure if this should be done!?
//#define UNSYNCED_FRAME_SKIP 16

void xrSetInitialCompositorTimelineValue(XrHandle sessionHandle, uint64_t timelineValue)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	timelineValue = timelineValue - (timelineValue % MXC_CYCLE_COUNT);
	pNodeShared->compositorBaseCycleValue = timelineValue + MXC_CYCLE_COUNT;
//	printf("Setting compositorBaseCycleValue %llu\n", pNodeShared->compositorBaseCycleValue);
}

void xrGetCompositorTimelineValue(XrHandle sessionHandle, bool synchronized, uint64_t* pTimelineValue)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	*pTimelineValue = pNodeShared->compositorBaseCycleValue + MXC_CYCLE_POST_UPDATE_NODE_STATES_COMPLETE;
}

void xrProgressCompositorTimelineValue(XrHandle sessionHandle, uint64_t timelineValue, bool synchronized)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcNodeShared*  pNodeShared = pNodeContext->pNodeShared;
	pNodeShared->compositorBaseCycleValue += MXC_CYCLE_COUNT * pNodeShared->compositorCycleSkip;
}

static XrTime HzToXrTime(double hz)
{
	double seconds = 1.0 / hz;
	return (XrTime)(seconds * 1e9);
}

XrTime xrGetFrameInterval(XrHandle sessionHandle, bool synchronized)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	double hz = 240.0 / (double)(pNodeShared->compositorCycleSkip);
	XrTime hzTime = HzToXrTime(hz);
//	printf("xrGetFrameInterval compositorCycleSkip: %d hz: %f hzTime: %llu\n", pNodeShared->compositorCycleSkip, hz, hzTime);
	return hzTime;
}

void xrGetHeadPose(XrHandle sessionHandle, XrPosef* pPose)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;

	mat4 invView = pNodeShared->globalSetState.invView;
	invView = Mat4YInvert(invView);
	invView = Mat4XInvert(invView);
	quat rotation = RotFromMat4(invView);
	vec3 position = PosFromMat4(invView);

	pPose->position = *(XrVector3f*)&position;
	pPose->orientation = *(XrQuaternionf*)&rotation;
}

void xrGetEyeView(XrHandle sessionHandle, int viewIndex, XrView* pView)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;

	mat4 invView = pNodeShared->globalSetState.invView;
	invView = Mat4YInvert(invView);
	invView = Mat4XInvert(invView);
	quat rotation = RotFromMat4(invView);
	vec3 position = PosFromMat4(invView);

	pView->pose.position = *(XrVector3f*)&position;
	pView->pose.orientation = *(XrQuaternionf*)&rotation;

	mat4 proj = pNodeShared->globalSetState.proj;
	float fovX = proj.c0.r0;
	float fovY = proj.c1.r1;
	float angleX = atan(1.0f / fovX);
	float angleY = atan(1.0f / fovY);

	pView->fov.angleLeft = Lerp(-angleX, angleX, pNodeShared->ulScreenUV.x);
	pView->fov.angleRight = Lerp(-angleX, angleX, pNodeShared->lrScreenUV.x);
	pView->fov.angleUp = Lerp(-angleY, angleY, pNodeShared->ulScreenUV.y);
	pView->fov.angleDown = Lerp(-angleY, angleY, pNodeShared->lrScreenUV.y);

	// do I need to worry about this?
//	pView->fov.angleLeft = Lerp(-angleX, angleX, pNodeShared->ulScreenUV.x) * (viewProj.c2.r0 - 1.0f);
//	pView->fov.angleRight = Lerp(-angleX, angleX, pNodeShared->lrScreenUV.x) * (viewProj.c2.r0 + 1.0f);
//	pView->fov.angleUp = Lerp(-angleY, angleY, pNodeShared->ulScreenUV.y) * (viewProj.c2.r1 + 1.0f);
//	pView->fov.angleDown = Lerp(-angleY, angleY, pNodeShared->lrScreenUV.y) * (viewProj.c2.r1 - 1.0f);

//		float halfAngle = MID_DEG_TO_RAD(pNodeShared->camera.yFOV) * 0.5f;
//		pView->fov.angleLeft = -halfAngle;
//		pView->fov.angleRight = halfAngle;
//		pView->fov.angleUp = halfAngle;
//		pView->fov.angleDown = -halfAngle;

	int width = pNodeShared->globalSetState.framebufferSize.x;
	int height = pNodeShared->globalSetState.framebufferSize.y;

	// do this? the app could calculate the subimage and pass it back with endframe info
	// but xrEnumerateViewConfigurationViews instance based, not session based, so it can't be relied on to get expected frame size
	// visibility mask is not checked every render, so that cannot be used
	// only way to control rendered area every frame by existing standard is changing the swap size
	// We could use foveation? Not all foveration APIs offer explicitly width height.
	// We might actually need swaps of different sizes, and different heights/widths
	// We do need an in-spec solution if this is to work with everything initally
	if (pView->next != NULL) {
		XrStructureTypeExt* type = (XrStructureTypeExt*)pView->next;
		switch (*type) {
			case XR_TYPE_SUB_VIEW: {
				XrSubView* pSubView = (XrSubView*)pView->next;
				pSubView->imageRect.extent.width = width;
				pSubView->imageRect.extent.height = height;
				break;
			}
			default:
				printf("EyeView->Next Unknown: %d\n", *type);
		}
	}
}