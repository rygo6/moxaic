#include "node.h"
#include "test_node.h"

#include "mid_openxr_runtime.h"
#include "mid_vulkan.h"

#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#define NOSERVICE
#define NOCRYPT
#define NOMCX
#define NOGDI
#include <windows.h>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

void midXrInitialize(XrGraphicsApi graphicsApi)
{
	printf("Initializing Moxaic OpenXR Node.\n");
	isCompositor = false;

	// Debating if each node should have a vulkan instance to read timeline semaphore
	// and run compute shaders. It would certainly simplify the code. However it may be more
	// performant to run such processes in local graphics context of OGL or DX11. For now
	// we are just relying on vulkan to simplify
	midVkInitialize();
	const MidVkContextCreateInfo contextCreateInfo = {
		// this should probably send physical device to set here to match compositor
		.queueFamilyCreateInfos = {
			[VKM_QUEUE_FAMILY_TYPE_DEDICATED_COMPUTE] = {
				.supportsGraphics = VKM_SUPPORT_NO,
				.supportsCompute = VKM_SUPPORT_YES,
				.supportsTransfer = VKM_SUPPORT_YES,
				.globalPriority = VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_EXT,
				.queueCount = 1,
				.pQueuePriorities = (float[]){1.0f},
			},
		},
	};
	midVkCreateContext(&contextCreateInfo);
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

	// compositor should not allocate framebuffers and other data until this point
	// as it needs to allocate them per node/session

	// Import framebuffers. These should really be a pool across all sessions
	{
		const int                    width = DEFAULT_WIDTH;
		const int                    height = DEFAULT_HEIGHT;
//		MxcNodeVkFramebufferTexture* pVkFramebufferTextures = pNodeContext->vkNodeFramebufferTextures;
//		MxcNodeGlFramebufferTexture* pGlFramebufferTextures = pNodeContext->glNodeFramebufferTextures;

#define DEFAULT_VK_IMAGE_CREATE_INFO(_width, _height, _format, _usage) \
	.imageCreateInfo = {                                               \
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                  \
		.pNext = VK_EXTERNAL_IMAGE_CREATE_INFO_PLATFORM,                     \
		.imageType = VK_IMAGE_TYPE_2D,                                 \
		.format = _format,                                             \
		.extent = {_width, _height, 1.0f},                             \
		.mipLevels = 1,                                                \
		.arrayLayers = 1,                                              \
		.samples = VK_SAMPLE_COUNT_1_BIT,                              \
		.usage = _usage,                                               \
	}
#define DEFAULT_IMAGE_CREATE_INFO(_width, _height, _format, _memObject, _texture, _handle)                   \
	gl.CreateMemoryObjectsEXT(1, &_memObject);                                                                \
	gl.ImportMemoryWin32HandleEXT(_memObject, _width* _height * 4, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, _handle); \
	gl.CreateTextures(GL_TEXTURE_2D, 1, &_texture);                                                           \
	gl.TextureStorageMem2DEXT(_texture, 1, _format, _width, _height, _memObject, 0);

		for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
			switch (graphicsApi){
				case XR_GRAPHICS_API_OPENGL: {
//					DEFAULT_IMAGE_CREATE_INFO(width, height, GL_RGBA8, pGlFramebufferTextures[i].color.memObject, pGlFramebufferTextures[i].color.texture, pImportParam->framebufferHandles[i].color)
					//			DEFAULT_IMAGE_CREATE_INFO(width, height, GL_RGBA8, pGlFramebufferTextures->normal.memObject, pGlFramebufferTextures->normal.texture,  pImportParam->framebufferHandles[i].normal);
					//			DEFAULT_IMAGE_CREATE_INFO(width, height, GL_RGBA8, pGlFramebufferTextures->color.memObject, pGlFramebufferTextures->color.texture,  pImportParam->framebufferHandles[i].color);
					break;
				}
				case XR_GRAPHICS_API_OPENGL_ES: break;
				case XR_GRAPHICS_API_VULKAN: {
//					const VkmTextureCreateInfo colorCreateInfo = {
//						.debugName = "ImportedColorFramebuffer",
//						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
//						.locality = MID_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
//						.externalHandle = pImportParam->framebufferHandles[i].color,
//						DEFAULT_VK_IMAGE_CREATE_INFO(width, height, MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_COLOR_INDEX], MIDVK_PASS_USAGES[MIDVK_PASS_ATTACHMENT_COLOR_INDEX]),
//					};
//					midvkCreateTexture(&colorCreateInfo, &pVkFramebufferTextures[i].color);
//					const VkmTextureCreateInfo normalCreateInfo = {
//						.debugName = "ImportedNormalFramebuffer",
//						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
//						.locality = MID_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
//						.externalHandle = pImportParam->framebufferHandles[i].normal,
//						DEFAULT_VK_IMAGE_CREATE_INFO(width, height, MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_NORMAL_INDEX], MIDVK_PASS_USAGES[MIDVK_PASS_ATTACHMENT_NORMAL_INDEX]),
//					};
//					midvkCreateTexture(&normalCreateInfo, &pVkFramebufferTextures[i].normal);
//					const VkmTextureCreateInfo gbufferCreateInfo = {
//						.debugName = "ImportedGBufferFramebuffer",
//						.imageCreateInfo = {
//							.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
//							.pNext = MIDVK_EXTERNAL_IMAGE_CREATE_INFO,
//							.imageType = VK_IMAGE_TYPE_2D,
//							.format = MXC_NODE_GBUFFER_FORMAT,
//							.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
//							.mipLevels = MXC_NODE_GBUFFER_LEVELS,
//							.arrayLayers = 1,
//							.samples = VK_SAMPLE_COUNT_1_BIT,
//							.usage = MXC_NODE_GBUFFER_USAGE,
//						},
//						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
//						.locality = MID_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
//						.externalHandle = pImportParam->framebufferHandles[i].gbuffer,
//					};
//					midvkCreateTexture(&gbufferCreateInfo, &pVkFramebufferTextures[i].gbuffer);
//					// Depth is not shared over IPC.
//					const VkmTextureCreateInfo depthCreateInfo = {
//						.debugName = "ImportedDepthFramebuffer",
//						.imageCreateInfo = {
//							.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
//							.imageType = VK_IMAGE_TYPE_2D,
//							.format = MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_DEPTH_INDEX],
//							.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
//							.mipLevels = 1,
//							.arrayLayers = 1,
//							.samples = VK_SAMPLE_COUNT_1_BIT,
//							.usage = MIDVK_PASS_USAGES[MIDVK_PASS_ATTACHMENT_DEPTH_INDEX],
//						},
//						.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
//						.locality = MID_LOCALITY_CONTEXT,
//					};
//					midvkCreateTexture(&depthCreateInfo, &pVkFramebufferTextures[i].depth);
					break;
				}
				case XR_GRAPHICS_API_D3D11_1: {
					break;
				}
				default:
					PANIC("Graphics API not supported");
			}
		}

		// We import semaphores in vulkan because right now its relying on timeline semaphore for all graphics apis
		const MidVkFenceCreateInfo nodeFenceCreateInfo = {
			.debugName = "NodeFenceImport",
			.locality = VK_LOCALITY_INTERPROCESS_IMPORTED_READONLY,
			.importHandle = pImportParam->nodeFenceHandle,
		};
		midVkCreateFence(&nodeFenceCreateInfo, &pNodeContext->vkNodeFence);
		const MidVkSemaphoreCreateInfo compTimelineCreateInfo = {
			.debugName = "CompositorTimelineSemaphoreImport",
			.locality = VK_LOCALITY_INTERPROCESS_IMPORTED_READONLY,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			.importHandle = pImportParam->compTimelineHandle,
		};
		midVkCreateSemaphore(&compTimelineCreateInfo, &pNodeContext->vkCompositorTimeline);
		const MidVkSemaphoreCreateInfo nodeTimelineCreateInfo = {
			.debugName = "NodeTimelineSemaphoreImport",
			.locality = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			.importHandle = pImportParam->nodeTimelineHandle,
		};
		midVkCreateSemaphore(&nodeTimelineCreateInfo, &pNodeContext->vkNodeTimeline);
	}
}

void midXrGetReferenceSpaceBounds(XrHandle sessionHandle, XrExtent2Df* pBounds)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	float radius = pNodeShared->compositorRadius * 2.0f;

	// we just assume depth is the same, and treat this like a cube
	*pBounds = (XrExtent2Df) {.width = radius, .height = radius };
}

void midXrClaimFramebufferImages(XrHandle sessionHandle, int imageCount, HANDLE* pHandle)
{
	CHECK(imageCount != MIDVK_SWAP_COUNT, "Required swap image count does not match imported swap count!")
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];

	MxcImportParam* pImportParam = &pImportedExternalMemory->importParam;
	MxcNodeShared*  pNodeShared = &pImportedExternalMemory->shared;

	for (int i = 0; i < imageCount; ++i) {
		printf("Claiming framebuffer handle %p\n", pImportParam->framebufferHandles[i].color);
		pHandle[i] = pImportParam->framebufferHandles[i].color;
	}
}

void midXrAcquireSwapchainImage(XrHandle sessionHandle, uint32_t* pIndex)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	__atomic_thread_fence(__ATOMIC_ACQUIRE);
	int framebufferIndex = pNodeShared->timelineValue % MIDVK_SWAP_COUNT;
	*pIndex = framebufferIndex;
}

void midXrWaitFrame(XrHandle sessionHandle)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	midVkTimelineWait(vk.context.device, pNodeShared->compositorBaseCycleValue + MXC_CYCLE_COMPOSITOR_RECORD, pNodeContext->vkCompositorTimeline);
}

// this isn't associated with sessions, its associated with instance, so sizes need to be global... ?
void midXrGetViewConfiguration(XrHandle sessionHandle, int viewIndex, XrViewConfigurationView* pView)
{
	*pView = (XrViewConfigurationView){
		.recommendedImageRectWidth = MIDXR_DEFAULT_WIDTH,
		.maxImageRectWidth = MIDXR_DEFAULT_WIDTH,
		.recommendedImageRectHeight = MIDXR_DEFAULT_HEIGHT,
		.maxImageRectHeight = MIDXR_DEFAULT_HEIGHT,
		.recommendedSwapchainSampleCount = MIDXR_DEFAULT_SAMPLES,
		.maxSwapchainSampleCount = MIDXR_DEFAULT_SAMPLES,
	};
}

void midXrGetView(XrHandle sessionHandle, int viewIndex, XrView* pView)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;

	pView->pose.position = *(XrVector3f*)&pNodeShared->cameraPos.position;
	pView->pose.orientation = *(XrQuaternionf*)&pNodeShared->cameraPos.rotation;

	float fovX = pNodeShared->globalSetState.proj.c0.r0;
	float fovY = pNodeShared->globalSetState.proj.c1.r1;
	float angleX = atan(1.0f / fovX);
	float angleY = atan(1.0f / fovY);
	pView->fov.angleLeft = Lerp(-angleX, angleX, pNodeShared->ulScreenUV.x);
	pView->fov.angleRight = Lerp(-angleX, angleX, pNodeShared->lrScreenUV.x);
	pView->fov.angleUp = Lerp(-angleY, angleY, pNodeShared->lrScreenUV.y);
	pView->fov.angleDown = Lerp(-angleY, angleY, pNodeShared->ulScreenUV.y);

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
	__atomic_fetch_add(&pNodeShared->timelineValue, 1, __ATOMIC_RELEASE);
}