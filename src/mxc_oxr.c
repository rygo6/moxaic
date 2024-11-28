#include "node.h"

#include "mid_openxr_runtime.h"
#include "mid_vulkan.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void midXrInitialize(XrGraphicsApi graphicsApi)
{
	printf("Initializing Moxaic OpenXR Node.\n");
	isCompositor = false;
	mxcConnectInterprocessNode(false);
}

void midXrCreateSession(XrGraphicsApi graphicsApi, XrHandle* pSessionHandle)
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

void midXrGetReferenceSpaceBounds(XrHandle sessionHandle, XrExtent2Df* pBounds)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	float radius = pNodeShared->compositorRadius * 2.0f;

	// we just assume depth is the same, and treat this like a cube
	*pBounds = (XrExtent2Df) {.width = radius, .height = radius };
}

void midXrClaimFence(XrHandle sessionHandle, HANDLE* pHandle)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcImportParam* pImportParam = &pImportedExternalMemory->importParam;
	MxcNodeShared*  pNodeShared = &pImportedExternalMemory->shared;
	*pHandle = pImportParam->compTimelineHandle;
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

void xrClaimSwapPoolImage(XrHandle sessionHandle, uint32_t* pIndex)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	uint64_t timelineValue = __atomic_load_n(&pNodeShared->timelineValue, __ATOMIC_ACQUIRE);
	*pIndex = (timelineValue % VK_SWAP_COUNT);
}

void xrStepWaitValue(XrHandle sessionHandle, uint64_t* pWaitValue)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	*pWaitValue = *pWaitValue - (*pWaitValue % MXC_CYCLE_COUNT);
	*pWaitValue = pNodeShared->compositorBaseCycleValue + MXC_CYCLE_COMPOSITOR_RECORD;
}

void midXrGetView(XrHandle sessionHandle, int viewIndex, XrView* pView)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;

	pView->pose.position = *(XrVector3f*)&pNodeShared->cameraPos.position;
	pView->pose.orientation = *(XrQuaternionf*)&pNodeShared->cameraPos.rotation;

//	quat inverseRotation = QuatInverse(pNodeShared->cameraPos.rotation);
//	pView->pose.orientation = *(XrQuaternionf*)&inverseRotation;

	float fovX = pNodeShared->globalSetState.proj.c0.r0;
	float fovY = pNodeShared->globalSetState.proj.c1.r1;
//	float fovX = pNodeShared->globalSetState.invProj.c0.r0;
//	float fovY = pNodeShared->globalSetState.invProj.c1.r1;
	float angleX = atan(1.0f / fovX);
	float angleY = atan(1.0f / fovY);

	pView->fov.angleLeft = Lerp(-angleX, angleX, pNodeShared->ulScreenUV.x);
	pView->fov.angleRight = Lerp(-angleX, angleX, pNodeShared->lrScreenUV.x);
	pView->fov.angleUp = Lerp(-angleY, angleY, pNodeShared->ulScreenUV.y);
	pView->fov.angleDown = Lerp(-angleY, angleY, pNodeShared->lrScreenUV.y);

//	pView->fov.angleLeft = Lerp(-angleX, angleX, pNodeShared->ulScreenUV.x);
//	pView->fov.angleRight = Lerp(-angleX, angleX, pNodeShared->lrScreenUV.x);
//	pView->fov.angleUp = Lerp(-angleY, angleY, pNodeShared->lrScreenUV.y);
//	pView->fov.angleDown = Lerp(-angleY, angleY, pNodeShared->ulScreenUV.y);

	int width = pNodeShared->globalSetState.framebufferSize.x;
	int height = pNodeShared->globalSetState.framebufferSize.y;

	// do this? the app could calculate the subimage and pass it back with endframe info
	// but xrEnumerateViewConfigurationViews instance based, not session based, so it can't be relied on
	// to get expected frame size
	// but I can get the different swap sizes via midXrClaimGlSwapchain
	// then have it calculate clip size off XrSpace and pass back the clipping size in end frame
	if (pView->next != NULL) {
		switch (*(XrStructureTypeExt*)pView->next) {
			case XR_TYPE_SUB_VIEW: {
				XrSubView* pSubView = (XrSubView*)pView->next;
				pSubView->imageRect.extent.width = width;
				pSubView->imageRect.extent.height = height;
				break;
			}
		}
	}

	//	float halfAngle = MID_DEG_TO_RAD(pNodeShared->camera.yFOV) * 0.5f;
//	pView->fov.angleLeft = -halfAngle;
//	pView->fov.angleRight = halfAngle;
//	pView->fov.angleUp = halfAngle;
//	pView->fov.angleDown = -halfAngle;
}

void midXrBeginFrame(XrHandle sessionHandle)
{
}

void midXrEndFrame(XrHandle sessionHandle)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	 __atomic_add_fetch(&pNodeShared->timelineValue, 1, __ATOMIC_RELAXED);
	pNodeShared->compositorBaseCycleValue += MXC_CYCLE_COUNT * pNodeShared->compositorCycleSkip;
}