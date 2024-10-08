#include "node.h"
#include "test_node.h"

#include "mid_openxr_runtime.h"
#include "mid_vulkan.h"

#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#include <windows.h>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

void midXrInitialize()
{
	printf("Initializing Moxaic node.\n");
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

void midXrCreateSession(XrHandle* pSessionHandle)
{
	MxcImportParam* pImportParam = &pImportedExternalMemory->importParam;
	MxcNodeShared*  pNodeShared = &pImportedExternalMemory->shared;

	NodeHandle      nodeHandle = RequestExternalNodeHandle(pNodeShared);
	MxcNodeContext* pNodeContext = &nodeContexts[nodeHandle];
	*pNodeContext = (MxcNodeContext){};
	pNodeContext->type = MXC_NODE_TYPE_INTERPROCESS_VULKAN_IMPORTED;
	pNodeContext->pNodeShared = pNodeShared;
	printf("Importing node handle %d as openxr session\n", nodeHandle);

	// openxr session = moxaic node
	*pSessionHandle = nodeHandle;

	// compositor should not allocate framebuffers and other data until this point
	// as it needs to allocate them per node/session

	// Import framebuffers. These should really be a pool across all sessions
	{
		PFNGLCREATEMEMORYOBJECTSEXTPROC glCreateMemoryObjectsEXT = (PFNGLCREATEMEMORYOBJECTSEXTPROC)wglGetProcAddress("glCreateMemoryObjectsEXT");
		if (!glCreateMemoryObjectsEXT) {
			printf("Failed to load glCreateMemoryObjectsEXT\n");
		}
		PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC glImportMemoryWin32HandleEXT = (PFNGLIMPORTMEMORYWIN32HANDLEEXTPROC)wglGetProcAddress("glImportMemoryWin32HandleEXT");
		if (!glImportMemoryWin32HandleEXT) {
			printf("Failed to load glImportMemoryWin32HandleEXT\n");
		}
		PFNGLCREATETEXTURESPROC glCreateTextures = (PFNGLCREATETEXTURESPROC)wglGetProcAddress("glCreateTextures");
		if (!glCreateTextures) {
			printf("Failed to load glCreateTextures\n");
		}
		PFNGLTEXTURESTORAGEMEM2DEXTPROC glTextureStorageMem2DEXT = (PFNGLTEXTURESTORAGEMEM2DEXTPROC)wglGetProcAddress("glTextureStorageMem2DEXT");
		if (!glTextureStorageMem2DEXT) {
			printf("Failed to load glTextureStorageMem2DEXT\n");
		}

		const int                    width = DEFAULT_WIDTH;
		const int                    height = DEFAULT_HEIGHT;
		MxcNodeVkFramebufferTexture* pVkFramebufferTextures = pNodeContext->vkNodeFramebufferTextures;
		MxcNodeGlFramebufferTexture* pGlFramebufferTextures = pNodeContext->glNodeFramebufferTextures;

#define DEFAULT_VK_IMAGE_CREATE_INFO(_width, _height, _format, _usage) \
	.imageCreateInfo = {                                               \
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,                  \
		.pNext = MIDVK_EXTERNAL_IMAGE_CREATE_INFO,                     \
		.imageType = VK_IMAGE_TYPE_2D,                                 \
		.format = _format,                                             \
		.extent = {_width, _height, 1.0f},                             \
		.mipLevels = 1,                                                \
		.arrayLayers = 1,                                              \
		.samples = VK_SAMPLE_COUNT_1_BIT,                              \
		.usage = _usage,                                               \
	}
#define DEFAULT_IMAGE_CREATE_INFO(_width, _height, _format, _memObject, _texture, _handle)                   \
	glCreateMemoryObjectsEXT(1, &_memObject);                                                                \
	glImportMemoryWin32HandleEXT(_memObject, _width* _height * 4, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, _handle); \
	glCreateTextures(GL_TEXTURE_2D, 1, &_texture);                                                           \
	glTextureStorageMem2DEXT(_texture, 1, _format, _width, _height, _memObject, 0);

		for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
			const VkmTextureCreateInfo colorCreateInfo = {
				.debugName = "ImportedColorFramebuffer",
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.locality = MID_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
				.externalHandle = pImportParam->framebufferHandles[i].color,
				DEFAULT_VK_IMAGE_CREATE_INFO(width, height, MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_COLOR_INDEX], MIDVK_PASS_USAGES[MIDVK_PASS_ATTACHMENT_COLOR_INDEX]),
			};
			midvkCreateTexture(&colorCreateInfo, &pVkFramebufferTextures[i].color);
			const VkmTextureCreateInfo normalCreateInfo = {
				.debugName = "ImportedNormalFramebuffer",
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.locality = MID_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
				.externalHandle = pImportParam->framebufferHandles[i].normal,
				DEFAULT_VK_IMAGE_CREATE_INFO(width, height, MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_NORMAL_INDEX], MIDVK_PASS_USAGES[MIDVK_PASS_ATTACHMENT_NORMAL_INDEX]),
			};
			midvkCreateTexture(&normalCreateInfo, &pVkFramebufferTextures[i].normal);
			const VkmTextureCreateInfo gbufferCreateInfo = {
				.debugName = "ImportedGBufferFramebuffer",
				.imageCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.pNext = MIDVK_EXTERNAL_IMAGE_CREATE_INFO,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = MXC_NODE_GBUFFER_FORMAT,
					.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
					.mipLevels = MXC_NODE_GBUFFER_LEVELS,
					.arrayLayers = 1,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = MXC_NODE_GBUFFER_USAGE,
				},
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.locality = MID_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
				.externalHandle = pImportParam->framebufferHandles[i].gbuffer,
			};
			midvkCreateTexture(&gbufferCreateInfo, &pVkFramebufferTextures[i].gbuffer);
			// Depth is not shared over IPC.
			const VkmTextureCreateInfo depthCreateInfo = {
				.debugName = "ImportedDepthFramebuffer",
				.imageCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_DEPTH_INDEX],
					.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = MIDVK_PASS_USAGES[MIDVK_PASS_ATTACHMENT_DEPTH_INDEX],
				},
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.locality = MID_LOCALITY_CONTEXT,
			};
			midvkCreateTexture(&depthCreateInfo, &pVkFramebufferTextures[i].depth);

			DEFAULT_IMAGE_CREATE_INFO(width, height, GL_RGBA8, pGlFramebufferTextures[i].color.memObject, pGlFramebufferTextures[i].color.texture, pImportParam->framebufferHandles[i].color)
			//			DEFAULT_IMAGE_CREATE_INFO(width, height, GL_RGBA8, pGlFramebufferTextures->normal.memObject, pGlFramebufferTextures->normal.texture,  pImportParam->framebufferHandles[i].normal);
			//			DEFAULT_IMAGE_CREATE_INFO(width, height, GL_RGBA8, pGlFramebufferTextures->color.memObject, pGlFramebufferTextures->color.texture,  pImportParam->framebufferHandles[i].color);
		}

		const MidVkFenceCreateInfo nodeFenceCreateInfo = {
			.debugName = "NodeFenceImport",
			.locality = MID_LOCALITY_INTERPROCESS_IMPORTED_READONLY,
			.externalHandle = pImportParam->nodeFenceHandle,
		};
		midVkCreateFence(&nodeFenceCreateInfo, &pNodeContext->vkNodeFence);
		const MidVkSemaphoreCreateInfo compTimelineCreateInfo = {
			.debugName = "CompositorTimelineSemaphoreImport",
			.locality = MID_LOCALITY_INTERPROCESS_IMPORTED_READONLY,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			.externalHandle = pImportParam->compTimelineHandle,
		};
		midVkCreateSemaphore(&compTimelineCreateInfo, &pNodeContext->vkCompositorTimeline);
		const MidVkSemaphoreCreateInfo nodeTimelineCreateInfo = {
			.debugName = "NodeTimelineSemaphoreImport",
			.locality = MID_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			.externalHandle = pImportParam->nodeTimelineHandle,
		};
		midVkCreateSemaphore(&nodeTimelineCreateInfo, &pNodeContext->vkNodeTimeline);
	}
}

void midXrCreateSwapchain(XrHandle swapHandle)
{
}

void midXrBeginSession(XrHandle sessionHandle)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];

	int result = pthread_create(&pNodeContext->threadId, NULL, (void* (*)(void*))mxcTestNodeThread, pNodeContext);
	REQUIRE(result == 0, "Node thread creation failed!")
}

void midXrClaimGlSwapchain(XrHandle sessionHandle, int imageCount, GLuint* pImages)
{
	REQUIRE(imageCount == MIDVK_SWAP_COUNT, "Requires Gl swap image count does not match imported swap count!")
	const MxcNodeContext* const pNodeContext = &nodeContexts[sessionHandle];

	for (int i = 0; i < imageCount; ++i) {
		pImages[i] = pNodeContext->glNodeFramebufferTextures[i].color.texture;
	}
}

void midXrWaitFrame(XrHandle sessionHandle)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	midVkTimelineWait(midVk.context.device, pNodeShared->compositorBaseCycleValue + MXC_CYCLE_COMPOSITOR_RECORD, pNodeContext->vkCompositorTimeline);
}

void midXrGetView(XrHandle sessionHandle, int viewIndex, XrView* pView)
{
	MxcNodeContext* pNodeContext = &nodeContexts[sessionHandle];
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;
	float halfAngle = MID_DEG_TO_RAD(pNodeShared->camera.yFOV) * 0.5f;

	pView->pose.position = *(XrVector3f*)&pNodeShared->cameraPos.position;
	pView->pose.orientation = *(XrQuaternionf*)&pNodeShared->cameraPos.rotation;
	pView->fov.angleLeft = -halfAngle;
	pView->fov.angleRight = halfAngle;
	pView->fov.angleUp = halfAngle;
	pView->fov.angleDown = -halfAngle;
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