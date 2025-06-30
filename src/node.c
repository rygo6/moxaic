#define WIN32_LEAN_AND_MEAN
#undef UNICODE
#include <winsock2.h>
#include <afunix.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

#include "mid_vulkan.h"

#include "node.h"
#include "test_node.h"

MxcActiveNodes activeNodes[MXC_COMPOSITOR_MODE_COUNT] = {};

// Compositor state in Compositor Process
MxcCompositorContext compositorContext = {};

// State imported into Node Process
// TODO this should be CompositeSharedMemory and deal with all nodes from another process
HANDLE                 importedExternalMemoryHandle = NULL;
MxcExternalNodeMemory* pImportedExternalMemory = NULL;

// this should become a midvk construct
size_t                     submitNodeQueueStart = 0;
size_t                     submitNodeQueueEnd = 0;
MxcQueuedNodeCommandBuffer submitNodeQueue[MXC_NODE_CAPACITY] = {};

MxcVulkanNodeContext vkNode = {};

struct Node node;

//////////////
//// Swap Pool
////
#define SWAP_ACQUIRE_BARRIER                                                 \
	.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,                    \
	.srcAccessMask = VK_ACCESS_2_NONE,                                       \
	.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT |                   \
					VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT |    \
					VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT | \
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,                 \
	.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,                            \
	.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,                   \
	.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,                   \
	VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED

#define SWAP_RELEASE_BARRIER                                 \
	.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, \
	.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,            \
	.dstStageMask = VK_PIPELINE_STAGE_2_NONE,                \
	.dstAccessMask = VK_ACCESS_2_NONE,                       \
	.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,   \
	.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,   \
	VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED


static void mxcCreateSwap(const MxcSwapInfo* pInfo, MxcSwap* pSwap)
{
	CHECK(pInfo->extent.width < 1024 || pInfo->extent.height < 1024, "Swap too small!")

	*pSwap = (MxcSwap){};
	pSwap->type = pInfo->type;

	/// Color
	{
		VkImageCreateInfo info = {
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			&(VkExternalMemoryImageCreateInfo){
				VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
				.handleTypes = MXC_EXTERNAL_FRAMEBUFFER_HANDLE_TYPE,
			},
			.imageType   = VK_IMAGE_TYPE_2D,
			.format      = VK_BASIC_PASS_FORMATS[VK_PASS_ATTACHMENT_INDEX_BASIC_COLOR],
			.extent      = pInfo->extent,
			.mipLevels   = 1,
			.arrayLayers = 1,
			.samples     = VK_SAMPLE_COUNT_1_BIT,
			.usage       = VK_BASIC_PASS_USAGES[VK_PASS_ATTACHMENT_INDEX_BASIC_COLOR],
		};
		vkCreateExternalPlatformTexture(&info, &pSwap->colorExternal);
		VkDedicatedTextureCreateInfo textureInfo = {
			.debugName        = "ExportedColorFramebuffer",
			.pImageCreateInfo = &info,
			.aspectMask       = VK_IMAGE_ASPECT_COLOR_BIT,
			.importHandle     = pSwap->colorExternal.handle,
			.handleType       = MXC_EXTERNAL_FRAMEBUFFER_HANDLE_TYPE,
			.locality         = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
		};
		vkCreateDedicatedTexture(&textureInfo, &pSwap->color);
	}

	/// Depth
	{
		VkImageCreateInfo info = {
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			&(VkExternalMemoryImageCreateInfo){
				VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
				.handleTypes = MXC_EXTERNAL_FRAMEBUFFER_HANDLE_TYPE,
			},
			.imageType   = VK_IMAGE_TYPE_2D,
			.format      = VK_FORMAT_R16_UNORM,
			.extent      = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
			.mipLevels   = 1,
			.arrayLayers = 1,
			.samples     = VK_SAMPLE_COUNT_1_BIT,
			.usage       = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		};
		vkCreateExternalPlatformTexture(&info, &pSwap->depthExternal);
		VkDedicatedTextureCreateInfo textureInfo = {
			.debugName        = "ExportedDepthFramebuffer",
			.pImageCreateInfo = &info,
			.aspectMask       = VK_IMAGE_ASPECT_COLOR_BIT,
			.importHandle     = pSwap->depthExternal.handle,
			.handleType       = MXC_EXTERNAL_FRAMEBUFFER_HANDLE_TYPE,
			.locality         = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
		};
		vkCreateDedicatedTexture(&textureInfo, &pSwap->depth);
	}

	/// GBuffer
	{
		vkCreateDedicatedTexture(
			&(VkDedicatedTextureCreateInfo){
				.debugName = "GBufferFramebuffer",
				.pImageCreateInfo = &(VkImageCreateInfo){
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = MXC_NODE_GBUFFER_FORMAT,
					.extent = pInfo->extent,
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = MXC_NODE_GBUFFER_USAGE,
				},
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.locality = VK_LOCALITY_CONTEXT},
			&pSwap->gbuffer);

		vkCreateDedicatedTexture(
			&(VkDedicatedTextureCreateInfo){
				.debugName = "GBufferFramebufferMip",
				.pImageCreateInfo = &(VkImageCreateInfo){
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = MXC_NODE_GBUFFER_FORMAT,
					// didn't use mips. Get rid of? Maybe not. If I did mip walking with linear sampling could still be much higher quality gbuffer.
					.extent = {pInfo->extent.width >> MXC_NODE_GBUFFER_FLATTENED_MIP_COUNT,
						pInfo->extent.height >> MXC_NODE_GBUFFER_FLATTENED_MIP_COUNT,
						pInfo->extent.depth},
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = MXC_NODE_GBUFFER_USAGE,
				},
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.locality = VK_LOCALITY_CONTEXT},
			&pSwap->gbufferMip);
	}

	if (VK_LOCALITY_INTERPROCESS_EXPORTED(pInfo->locality)) {
		// Transfer on main because not all transfer queues can do compute transfer
		VK_IMMEDIATE_COMMAND_BUFFER_CONTEXT(VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS)	{

			CMD_IMAGE_BARRIERS(cmd, {
				   VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				   .image            = pSwap->color.image,
				   .subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
				   VK_IMAGE_BARRIER_SRC_UNDEFINED,
				   .dstStageMask  = VK_PIPELINE_STAGE_2_NONE,
				   .dstAccessMask = VK_ACCESS_2_NONE,
				   // could I do this elsewhere?
				   .newLayout = pInfo->compositorMode == MXC_COMPOSITOR_MODE_COMPUTE ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				   VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				}, {
				   VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				   .image            = pSwap->depth.image,
				   .subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
				   VK_IMAGE_BARRIER_SRC_UNDEFINED,
				   .dstStageMask  = VK_PIPELINE_STAGE_2_NONE,
				   .dstAccessMask = VK_ACCESS_2_NONE,
				   // Doesn't get used in compositor. Depth gets blit to gbuffer.
				   .newLayout = VK_IMAGE_LAYOUT_GENERAL,
				   VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				}, {
				   VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				   .image            = pSwap->gbuffer.image,
				   .subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
				   VK_IMAGE_BARRIER_SRC_UNDEFINED,
				   .dstStageMask  = VK_PIPELINE_STAGE_2_NONE,
				   .dstAccessMask = VK_ACCESS_2_NONE,
				   // could I do this elsewhere?
				   .newLayout = pInfo->compositorMode == MXC_COMPOSITOR_MODE_COMPUTE ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				   VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				}, {
				   VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				   .image            = pSwap->gbufferMip.image,
				   .subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
				   VK_IMAGE_BARRIER_SRC_UNDEFINED,  // only ever used in compute blit
				   VK_IMAGE_BARRIER_DST_COMPUTE_NONE,
				   VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED
				});

		}
	}
}

static void mxcDestroySwap(MxcSwap* pSwap)
{
	pSwap->type = XR_SWAP_TYPE_UNKNOWN;
	vkDestroyDedicatedTexture(&pSwap->color);
	vkDestroyDedicatedTexture(&pSwap->depth);
	vkDestroyDedicatedTexture(&pSwap->gbuffer);
	vkDestroyDedicatedTexture(&pSwap->gbufferMip);
	vkDestroyExternalPlatformTexture(&pSwap->colorExternal);
	vkDestroyExternalPlatformTexture(&pSwap->depthExternal);
}

/////////////////////////
//// Compositor Lifecycle
////
void mxcRequestAndRunCompositorNodeThread(const VkSurfaceKHR surface, void* (*runFunc)(struct MxcCompositorContext*))
{
	vkSemaphoreCreateInfoExt semaphoreCreateInfo = {
		.locality = VK_LOCALITY_INTERPROCESS_EXPORTED_READONLY,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
	};
	vkCreateSemaphoreExt(&semaphoreCreateInfo, &compositorContext.timeline);
	compositorContext.timelineHandle = vkGetSemaphoreExternalHandle(compositorContext.timeline);
	VK_SET_DEBUG(compositorContext.timeline);

	vkCreateSwapContext(surface, VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS, &compositorContext.swapCtx);

	VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
	};
	VK_CHECK(vkCreateCommandPool(vk.context.device, &graphicsCommandPoolCreateInfo, VK_ALLOC, &compositorContext.gfxPool));
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = compositorContext.gfxPool,
		.commandBufferCount = 1,
	};
	VK_CHECK(vkAllocateCommandBuffers(vk.context.device, &commandBufferAllocateInfo, &compositorContext.gfxCmd));
	VK_SET_DEBUG(compositorContext.gfxPool);
	VK_SET_DEBUG(compositorContext.gfxCmd);

	CHECK(pthread_create(&compositorContext.threadId, NULL, (void* (*)(void*))runFunc, &compositorContext), "Compositor Node thread creation failed!");
	LOG("Request and Run Compositor Node Thread Success.\n");
}

///////////////////
//// Node Lifecycle
////
NodeHandle RequestLocalNodeHandle()
{
	// TODO this claim/release handle needs to be a pooling logic
	NodeHandle hNode = atomic_fetch_add(&node.ct, 1);
	node.pShared[hNode] = calloc(1, sizeof(MxcNodeShared));
	node.ctxt[hNode].pNodeShared = node.pShared[hNode];
	return hNode;
}

NodeHandle RequestExternalNodeHandle(MxcNodeShared* pNodeShared)
{
	NodeHandle hNode = atomic_fetch_add(&node.ct, 1);
	node.pShared[hNode] = pNodeShared;
	node.ctxt[hNode].pNodeShared = node.pShared[hNode];
	return hNode;
}

void ReleaseNode(NodeHandle handle)
{
	assert((compositorContext.baseCycleValue % MXC_CYCLE_COUNT) == MXC_CYCLE_UPDATE_WINDOW_STATE && "Trying to ReleaseNode not in MXC_CYCLE_UPDATE_WINDOW_STATE");
	assert(node.ctxt[handle].type != MXC_NODE_INTERPROCESS_MODE_NONE);
	auto pNodeCtxt = &node.ctxt[handle];
	auto pNodeShrd = node.pShared[handle];
	auto pActiveNode = &activeNodes[pNodeShrd->compositorMode];

	ATOMIC_FENCE_BLOCK {
		int i = 0;
		for (; i < pActiveNode->ct; ++i) {
			if (pActiveNode->handles[i] == handle)
				break;
		}
		for (; i < pActiveNode->ct - 1; ++i) {
			pActiveNode->handles[i] = pActiveNode->handles[i + 1];
		}
		pActiveNode->ct--;
	}

	int newCount = atomic_fetch_sub(&node.ct, 1) - 1;
	LOG("Releasing Node %d. Count %d.\n", handle, newCount);
}

#define CLOSE_HANDLE(_handle)                                                     \
	if (!CloseHandle(_handle)) {                                                  \
		DWORD dwError = GetLastError();                                           \
		printf("Could not close (%s) object handle (%lu).\n", #_handle, dwError); \
	}
static int CleanupNode(NodeHandle hNode)
{
	assert((compositorContext.baseCycleValue % MXC_CYCLE_COUNT) == MXC_CYCLE_UPDATE_WINDOW_STATE && "Trying to ReleaseNode not in MXC_CYCLE_UPDATE_WINDOW_STATE");
	auto pNodeCtxt = &node.ctxt[hNode];
	auto pNodeShared = node.pShared[hNode];

	int  iSwapBlock = MXC_SWAP_TYPE_BLOCK_INDEX_BY_TYPE[pNodeCtxt->swapType];

	switch (pNodeCtxt->type) {
		case MXC_NODE_INTERPROCESS_MODE_THREAD:
			vkFreeCommandBuffers(vk.context.device, pNodeCtxt->pool, 1, &pNodeCtxt->cmd);
			vkDestroyCommandPool(vk.context.device, pNodeCtxt->pool, VK_ALLOC);
			int result = pthread_join(pNodeCtxt->threadId, NULL);
			if (result != 0) {
				perror("Thread join failed");
			}

			free(pNodeShared);
			node.pShared[hNode] = NULL;

			break;
		case MXC_NODE_INTERPROCESS_MODE_EXPORTED:

			// really need a different way to do this
//			CMD_WRITE_SINGLE_SETS(vk.context.device,
//				BIND_WRITE_NODE_COLOR(nodeCompositorData[hNode].nodeSet, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL),
//				BIND_WRITE_NODE_GBUFFER(nodeCompositorData[hNode].nodeSet, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL));
			VkWriteDescriptorSet writeSets[] = {
				{
					VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = node.cstData[hNode].nodeSet,
					.dstBinding = 1,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.pImageInfo = &(VkDescriptorImageInfo){
						.sampler = vk.context.nearestSampler,
						.imageView = VK_NULL_HANDLE,
						.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
					},
				},
				{
					VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = node.cstData[hNode].nodeSet,
					.dstBinding = 2,
					.descriptorCount = 1,
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.pImageInfo = &(VkDescriptorImageInfo){
						.sampler = vk.context.nearestSampler,
						.imageView = VK_NULL_HANDLE,
						.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
					},
				},
			};
			vkUpdateDescriptorSets(vk.context.device, COUNT(writeSets), writeSets, 0, NULL);

			for (int i = 0; i < MXC_NODE_SWAP_CAPACITY; ++i) {
				if (!HANDLE_VALID(pNodeCtxt->hSwaps[i]))
					continue;

				// We are fully clearing but at some point we want a mechanic to recycle and share node swaps
				auto pSwap = BLOCK_RELEASE(node.block.swap[iSwapBlock], pNodeCtxt->hSwaps[i]);
				mxcDestroySwap(pSwap);
			}
			CLOSE_HANDLE(pNodeCtxt->swapsSyncedHandle);
			CLOSE_HANDLE(pNodeCtxt->nodeTimelineHandle);
			CHECK_WIN32(UnmapViewOfFile(pNodeCtxt->pExportedExternalMemory));
			CLOSE_HANDLE(pNodeCtxt->exportedExternalMemoryHandle);
			CLOSE_HANDLE(pNodeCtxt->processHandle);

			node.pShared[hNode] = NULL;

			break;
		case MXC_NODE_INTERPROCESS_MODE_IMPORTED:
			// The process which imported the handle must close them.
			for (int i = 0; i < MXC_NODE_SWAP_CAPACITY; ++i) {
				CLOSE_HANDLE(pImportedExternalMemory->imports.colorSwapHandles[i]);
				CLOSE_HANDLE(pImportedExternalMemory->imports.depthSwapHandles[i]);
			}
			CLOSE_HANDLE(pImportedExternalMemory->imports.swapsSyncedHandle);
			CLOSE_HANDLE(pImportedExternalMemory->imports.nodeTimelineHandle);
			CLOSE_HANDLE(pImportedExternalMemory->imports.compositorTimelineHandle);
			CHECK_WIN32(UnmapViewOfFile(pImportedExternalMemory));
			CLOSE_HANDLE(importedExternalMemoryHandle);
			CLOSE_HANDLE(pNodeCtxt->processHandle);
			break;
		default: PANIC("Node type not supported");
	}

	*pNodeCtxt = (MxcNodeContext){};

	return 0;
}

void mxcRequestNodeThread(void* (*runFunc)(struct MxcNodeContext*), NodeHandle* pNodeHandle)
{
	printf("Requesting Node Thread.\n");
	auto handle = RequestLocalNodeHandle();

	auto pNodeCtxt = &node.ctxt[handle];
	*pNodeCtxt = (MxcNodeContext){};
	pNodeCtxt->type = MXC_NODE_INTERPROCESS_MODE_THREAD;
	pNodeCtxt->swapsSyncedHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
	pNodeCtxt->compositorTimeline = compositorContext.timeline;

	auto pNodeShrd = node.pShared[handle];
	*pNodeShrd = (MxcNodeShared){};
	pNodeCtxt->pNodeShared = pNodeShrd;
	pNodeShrd->rootPose.rot = QuatFromEuler(pNodeShrd->rootPose.euler);

	pNodeShrd->camera.yFovRad = RAD_FROM_DEG(45.0f);
	pNodeShrd->camera.zNear = 0.1f;
	pNodeShrd->camera.zFar = 100.0f;

	pNodeShrd->compositorRadius = 0.5;
	pNodeShrd->compositorCycleSkip = 8;

	vkSemaphoreCreateInfoExt semaphoreCreateInfo = {
		.debugName = "NodeTimelineSemaphore",
		.locality = VK_LOCALITY_CONTEXT,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
	};
	vkCreateSemaphoreExt(&semaphoreCreateInfo, &pNodeCtxt->nodeTimeline);

	VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
	};
	VK_CHECK(vkCreateCommandPool(vk.context.device, &graphicsCommandPoolCreateInfo, VK_ALLOC, &pNodeCtxt->pool));
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = pNodeCtxt->pool,
		.commandBufferCount = 1,
	};
	VK_CHECK(vkAllocateCommandBuffers(vk.context.device, &commandBufferAllocateInfo, &pNodeCtxt->cmd));
	vkSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)pNodeCtxt->cmd, "TestNode");

	MxcNodeCompositorData* pNodeCompositorData = &node.cstData[handle];
	// do not clear since set data is preallocated
//	*pNodeCompositorData = (MxcNodeCompositorData){};
//	pNodeCompositorData->rootPose.rotation = QuatFromEuler(pNodeCompositorData->rootPose.euler);

	*pNodeHandle = handle;

	CHECK(pthread_create(&node.ctxt[handle].threadId, NULL, (void* (*)(void*))runFunc, pNodeCtxt), "Node thread creation failed!");

	printf("Request Node Thread Success. Handle: %d\n", handle);
	// todo this needs error handling
}

////////////////
//// Node Render
////
static const VkImageUsageFlags NODE_PASS_USAGES[] = { // Do I want this?
	[VK_PASS_ATTACHMENT_INDEX_BASIC_COLOR] = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
	[VK_PASS_ATTACHMENT_INDEX_BASIC_NORMAL] = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
	[VK_PASS_ATTACHMENT_INDEX_BASIC_DEPTH] = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
};

void mxcCreateNodeRenderPass()
{
	VkRenderPassCreateInfo2 renderPassCreateInfo2 = {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
		.attachmentCount = 3,
		.pAttachments = (VkAttachmentDescription2[]){
			[VK_PASS_ATTACHMENT_INDEX_BASIC_COLOR] = {
				VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
				.format = VK_BASIC_PASS_FORMATS[VK_PASS_ATTACHMENT_INDEX_BASIC_COLOR],
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			},
			[VK_PASS_ATTACHMENT_INDEX_BASIC_NORMAL] = {
				VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
				.format = VK_BASIC_PASS_FORMATS[VK_PASS_ATTACHMENT_INDEX_BASIC_NORMAL],
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			},
			[VK_PASS_ATTACHMENT_INDEX_BASIC_DEPTH] = {
				VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
				.format = VK_BASIC_PASS_FORMATS[VK_PASS_ATTACHMENT_INDEX_BASIC_DEPTH],
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			},
		},
		.subpassCount = 1,
		.pSubpasses = (VkSubpassDescription2[]){
			{
				VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.colorAttachmentCount = 2,
				.pColorAttachments = (VkAttachmentReference2[]){
					[VK_PASS_ATTACHMENT_INDEX_BASIC_COLOR] = {
						VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
						.attachment = VK_PASS_ATTACHMENT_INDEX_BASIC_COLOR,
						.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					},
					[VK_PASS_ATTACHMENT_INDEX_BASIC_NORMAL] = {
						VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
						.attachment = VK_PASS_ATTACHMENT_INDEX_BASIC_NORMAL,
						.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					},
				},
				.pDepthStencilAttachment = &(VkAttachmentReference2){
					VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
					.attachment = VK_PASS_ATTACHMENT_INDEX_BASIC_DEPTH,
					.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				},
			},
		},
		.dependencyCount = 2,
		.pDependencies = (VkSubpassDependency2[]){
			// from here https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#combined-graphicspresent-queue
			{
				VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dependencyFlags = 0,
			},
			{
				VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
				.srcSubpass = 0,
				.dstSubpass = VK_SUBPASS_EXTERNAL,
				.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
				.dependencyFlags = 0,
			},
		},
	};
	VK_CHECK(vkCreateRenderPass2(vk.context.device, &renderPassCreateInfo2, VK_ALLOC, &vkNode.basicPass));
	vkSetDebugName(VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)vkNode.basicPass, "NodeRenderPass");
}

//////////////////
//// IPC LifeCycle
////
#define SOCKET_PATH "C:\\temp\\moxaic_socket"

static struct {
	SOCKET    listenSocket;
	pthread_t thread;
} ipcServer;
const char serverIPCAckMessage[] = "CONNECT-MOXAIC-COMPOSITOR-0.0.0";
const char nodeIPCAckMessage[] = "CONNECT-MOXAIC-NODE-0.0.0";

// these should really be CHECK_WIN32_ERROR_HANDLE or something
// Checks WIN32 error code. Expects 1 for success.
#define WIN32_CHECK(_command, _message)                             \
	{                                                               \
		int _result = (_command);                                   \
		if (__builtin_expect(!(_result), 0)) {                      \
			fprintf(stderr, "%s: %ld\n", _message, GetLastError()); \
			goto ExitError;                                         \
		}                                                           \
	}
// Checks WSA error code. Expects 0 for success.
#define WSA_CHECK(_command, _message)                                 \
	{                                                                 \
		int _result = (_command);                                     \
		if (__builtin_expect(!!(_result), 0)) {                       \
			fprintf(stderr, "%s: %d\n", _message, WSAGetLastError()); \
			goto ExitError;                                           \
		}                                                             \
	}

/// Called when compositor accepts connection
static void InterprocessServerAcceptNodeConnection()
{
	printf("Accepting connections on: '%s'\n", SOCKET_PATH);
	MxcNodeContext*         pNodeCtxt = NULL;
	MxcNodeShared*          pNodeShrd = NULL;
	MxcNodeImports*         pImports = NULL;
	HANDLE                  hNodeProc = INVALID_HANDLE_VALUE;
	HANDLE                  hExtNodeMem = INVALID_HANDLE_VALUE;
	MxcExternalNodeMemory*  pExtNodeMem = NULL;
	DWORD                   nodeProcId = 0;

	SOCKET clientSocket = accept(ipcServer.listenSocket, NULL, NULL);
	WSA_CHECK(clientSocket == INVALID_SOCKET, "Accept failed");
	printf("Accepted Connection.\n");

	// Receive Node Ack Message
	{
		char buffer[sizeof(nodeIPCAckMessage)] = {};
		int  receiveLength = recv(clientSocket, buffer, sizeof(nodeIPCAckMessage), 0);
		WSA_CHECK(receiveLength == SOCKET_ERROR || receiveLength == 0, "Recv nodeIPCAckMessage failed");
		printf("Received node ack: %s Size: %d\n", buffer, receiveLength);
		CHECK(strcmp(buffer, nodeIPCAckMessage), "Unexpected node message");
	}

	// Send Server Ack message
	{
		printf("Sending server ack: %s size: %llu\n", serverIPCAckMessage, strlen(serverIPCAckMessage));
		int sendResult = send(clientSocket, serverIPCAckMessage, strlen(serverIPCAckMessage), 0);
		WSA_CHECK(sendResult == SOCKET_ERROR || sendResult == 0, "Send server ack failed");
	}

	// Receive Node Process Handle
	{
		int receiveLength = recv(clientSocket, (char*)&nodeProcId, sizeof(DWORD), 0);
		WSA_CHECK(receiveLength == SOCKET_ERROR || receiveLength == 0, "Recv node process id failed");
		printf("Received node process id: %lu Size: %d\n", nodeProcId, receiveLength);
		CHECK(nodeProcId == 0, "Invalid node process id");
	}

	// Create Shared Memory
	{
		hNodeProc = OpenProcess(PROCESS_DUP_HANDLE, FALSE, nodeProcId);
		WIN32_CHECK(hNodeProc != NULL && hNodeProc != INVALID_HANDLE_VALUE, "Duplicate process handle failed");
		hExtNodeMem = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(MxcExternalNodeMemory), NULL);
		WIN32_CHECK(hExtNodeMem != NULL, "Could not create file mapping object");
		pExtNodeMem = MapViewOfFile(hExtNodeMem, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MxcExternalNodeMemory));
		WIN32_CHECK(pExtNodeMem != NULL, "Could not map view of file");
		*pExtNodeMem = (MxcExternalNodeMemory){};
		pImports = &pExtNodeMem->imports;
		pNodeShrd = &pExtNodeMem->shared;

	}

	// Claim Node Handle
	NodeHandle hNode = RequestExternalNodeHandle(pNodeShrd);

	// Initialize Context
	{
		// Init Node Shared
		pNodeShrd->rootPose.rot = QuatFromEuler(pNodeShrd->rootPose.euler);
		pNodeShrd->camera.yFovRad = RAD_FROM_DEG(45.0f);
		pNodeShrd->camera.zNear = 0.1f;
		pNodeShrd->camera.zFar = 100.0f;
		pNodeShrd->compositorRadius = 0.5;
		pNodeShrd->compositorCycleSkip = 8;
//		pNodeShrd->compositorMode = MXC_COMPOSITOR_MODE_QUAD;
//		pNodeShrd->compositorMode = MXC_COMPOSITOR_MODE_TESSELATION;
		pNodeShrd->compositorMode = MXC_COMPOSITOR_MODE_COMPUTE;

		// Init Node Context
		pNodeCtxt = &node.ctxt[hNode];
		*pNodeCtxt = (MxcNodeContext){};
		pNodeCtxt->type = MXC_NODE_INTERPROCESS_MODE_EXPORTED;
		pNodeCtxt->swapsSyncedHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
		pNodeCtxt->compositorTimeline = compositorContext.timeline;
		pNodeCtxt->compositorTimelineHandle = compositorContext.timelineHandle;

		vkSemaphoreCreateInfoExt semaphoreCreateInfo = {
			.debugName = "NodeTimelineSemaphoreExport",
			.locality = VK_LOCALITY_INTERPROCESS_EXPORTED_READWRITE,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		};
		vkCreateSemaphoreExt(&semaphoreCreateInfo, &pNodeCtxt->nodeTimeline);
		pNodeCtxt->nodeTimelineHandle = vkGetSemaphoreExternalHandle(pNodeCtxt->nodeTimeline);
		pNodeCtxt->pNodeShared = pNodeShrd;
		pNodeCtxt->processId = nodeProcId;
		pNodeCtxt->processHandle = hNodeProc;
		pNodeCtxt->exportedExternalMemoryHandle = hExtNodeMem;
		pNodeCtxt->pExportedExternalMemory = pExtNodeMem;

		// Duplicate Handles
		HANDLE currentHandle = GetCurrentProcess();
		WIN32_CHECK(DuplicateHandle(
						currentHandle, pNodeCtxt->swapsSyncedHandle,
						hNodeProc, &pImports->swapsSyncedHandle,
						0, false, DUPLICATE_SAME_ACCESS),
			"Duplicate nodeFenceHandle handle fail.");
		WIN32_CHECK(DuplicateHandle(
						currentHandle, pNodeCtxt->nodeTimelineHandle,
						hNodeProc, &pImports->nodeTimelineHandle,
						0, false, DUPLICATE_SAME_ACCESS),
			"Duplicate nodeTimeline handle fail.");
		WIN32_CHECK(DuplicateHandle(
						currentHandle, pNodeCtxt->compositorTimelineHandle,
						hNodeProc, &pImports->compositorTimelineHandle,
						0, false, DUPLICATE_SAME_ACCESS),
			"Duplicate compositor timeline handle fail.");
	}

	// Send Shared Memory
	{
		HANDLE duplicatedExternalNodeMemoryHandle;
		WIN32_CHECK(DuplicateHandle(
						GetCurrentProcess(), pNodeCtxt->exportedExternalMemoryHandle,
						hNodeProc, &duplicatedExternalNodeMemoryHandle,
						0, false, DUPLICATE_SAME_ACCESS),
					"Duplicate sharedMemory handle fail.");
		printf("Sending duplicatedExternalNodeMemoryHandle: %p Size: %llu\n", duplicatedExternalNodeMemoryHandle, sizeof(HANDLE));
		int sendResult = send(clientSocket, (const char*)&duplicatedExternalNodeMemoryHandle, sizeof(HANDLE), 0);
		WSA_CHECK(sendResult == SOCKET_ERROR || sendResult == 0, "Send shared mem handle failed");
		LOG("Process Node Export Success.\n");

	}

	// Add Active node
	{
		auto pActiveNode = &activeNodes[pNodeShrd->compositorMode];
		pActiveNode->handles[pActiveNode->ct] = hNode;
		atomic_fetch_add(&pActiveNode->ct, 1);
		LOG("Added active node handle: %d\n", hNode);
		goto ExitSuccess;
	}

ExitError:
	if (pImports != NULL) {
//		for (int i = 0; i < VK_SWAP_COUNT; ++i) {
//			if (pImportParam->framebufferHandles[i].color != INVALID_HANDLE_VALUE)
//				CloseHandle(pImportParam->framebufferHandles[i].color);
//			if (pImportParam->framebufferHandles[i].normal != INVALID_HANDLE_VALUE)
//				CloseHandle(pImportParam->framebufferHandles[i].normal);
//			if (pImportParam->framebufferHandles[i].gbuffer != INVALID_HANDLE_VALUE)
//				CloseHandle(pImportParam->framebufferHandles[i].gbuffer);
//		}
//		if (pImportParam->nodeFenceHandle != INVALID_HANDLE_VALUE)
//			CloseHandle(pImportParam->nodeFenceHandle);
		if (pImports->nodeTimelineHandle != INVALID_HANDLE_VALUE)
			CloseHandle(pImports->nodeTimelineHandle);
		if (pImports->compositorTimelineHandle != INVALID_HANDLE_VALUE)
			CloseHandle(pImports->compositorTimelineHandle);
	}
	if (pExtNodeMem != NULL)
		UnmapViewOfFile(pExtNodeMem);
	if (hExtNodeMem != INVALID_HANDLE_VALUE)
		CloseHandle(hExtNodeMem);
	if (hNodeProc != INVALID_HANDLE_VALUE)
		CloseHandle(hNodeProc);
ExitSuccess:
	if (clientSocket != INVALID_SOCKET)
		closesocket(clientSocket);
}

/// Server thread loop running on compositor
static void* RunInterProcessServer(void* arg)
{
	SOCKADDR_UN address = {.sun_family = AF_UNIX};
	WSADATA     wsaData = {};

	// Unlink/delete sock file in case it was left from before
	unlink(SOCKET_PATH);

	CHECK(strncpy_s(address.sun_path, sizeof address.sun_path, SOCKET_PATH, (sizeof SOCKET_PATH) - 1), "Address copy failed");
	WSA_CHECK(WSAStartup(MAKEWORD(2, 2), &wsaData), "WSAStartup failed");

	ipcServer.listenSocket = socket(AF_UNIX, SOCK_STREAM, 0);
	WSA_CHECK(ipcServer.listenSocket == INVALID_SOCKET, "Socket failed");
	WSA_CHECK(bind(ipcServer.listenSocket, (struct sockaddr*)&address, sizeof(address)), "Socket bind failed");
	WSA_CHECK(listen(ipcServer.listenSocket, SOMAXCONN), "Listen failed");

	while (atomic_load_explicit(&isRunning, memory_order_acquire))
		InterprocessServerAcceptNodeConnection();

ExitError:
	if (ipcServer.listenSocket != INVALID_SOCKET)
		closesocket(ipcServer.listenSocket);

	unlink(SOCKET_PATH);
	WSACleanup();
	return NULL;
}

/// Start Server Compositor
void mxcInitializeInterprocessServer()
{
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	printf("Min size of shared mem. Allocation granularity: %lu\n", systemInfo.dwAllocationGranularity);

	ipcServer.listenSocket = INVALID_SOCKET;
	CHECK(pthread_create(&ipcServer.thread, NULL, RunInterProcessServer, NULL), "IPC server pipe creation Fail!");
}

/// Shutdown Server Compositor
void mxcShutdownInterprocessServer()
{
	if (ipcServer.listenSocket != INVALID_SOCKET)
		closesocket(ipcServer.listenSocket);

	unlink(SOCKET_PATH);
	WSACleanup();
}

//// Create Node on Server Compositor to send over IPC
void mxcConnectInterprocessNode(bool createTestNode)
{
	if (pImportedExternalMemory != NULL && importedExternalMemoryHandle != NULL) {
		printf("IPC already connected, skipping.\n");
		return;
	}

	printf("Connecting on: '%s'\n", SOCKET_PATH);
	MxcNodeContext*         pNodeContext = NULL;
	MxcNodeImports*         pNodeImports = NULL;
	MxcNodeShared*          pNodeShared = NULL;
	MxcExternalNodeMemory*  pExternalNodeMemory = NULL;
	SOCKET                  clientSocket = INVALID_SOCKET;
	HANDLE                  externalNodeMemoryHandle = INVALID_HANDLE_VALUE;

	// Setup and connect
	{
		SOCKADDR_UN address = {.sun_family = AF_UNIX};
		CHECK(strncpy_s(address.sun_path, sizeof address.sun_path, SOCKET_PATH, (sizeof SOCKET_PATH) - 1), "Address copy failed");
		WSADATA wsaData = {};
		WSA_CHECK(WSAStartup(MAKEWORD(2, 2), &wsaData), "WSAStartup failed");
		clientSocket = socket(AF_UNIX, SOCK_STREAM, 0);
		WSA_CHECK(clientSocket == INVALID_SOCKET, "Socket creation failed");
		WSA_CHECK(connect(clientSocket, (struct sockaddr*)&address, sizeof(address)), "Connect failed");
		printf("Connected to server.\n");
	}

	// Send ack
	{
		printf("Sending node ack: %s size: %llu\n", nodeIPCAckMessage, strlen(nodeIPCAckMessage));
		int sendResult = send(clientSocket, nodeIPCAckMessage, (int)strlen(nodeIPCAckMessage), 0);
		WSA_CHECK(sendResult == SOCKET_ERROR, "Send node ack failed");
	}

	// Receive and check ack
	{
		char buffer[sizeof(serverIPCAckMessage)] = {};
		int  receiveLength = recv(clientSocket, buffer, sizeof(serverIPCAckMessage), 0);
		WSA_CHECK(receiveLength == SOCKET_ERROR || receiveLength == 0, "Recv compositor ack failed");
		printf("Received from server: %s size: %d\n", buffer, receiveLength);
		WSA_CHECK(strcmp(buffer, serverIPCAckMessage), "Unexpected compositor ack");
	}

	// Send process id
	{
		DWORD currentProcessId = GetCurrentProcessId();
		printf("Sending process handle: %lu size: %llu\n", currentProcessId, sizeof(DWORD));
		int sendResult = send(clientSocket, (const char*)&currentProcessId, sizeof(DWORD), 0);
		WSA_CHECK(sendResult == SOCKET_ERROR, "Send process id failed");
	}

	// Receive shared memory
	{
		printf("Waiting to receive externalNodeMemoryHandle.\n");
		int receiveLength = recv(clientSocket, (char*)&externalNodeMemoryHandle, sizeof(HANDLE), 0);
		WSA_CHECK(receiveLength == SOCKET_ERROR || receiveLength == 0, "Recv externalNodeMemoryHandle failed");
		printf("Received externalNodeMemoryHandle: %p Size: %d\n", externalNodeMemoryHandle, receiveLength);

		pExternalNodeMemory = MapViewOfFile(externalNodeMemoryHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MxcExternalNodeMemory));
		WIN32_CHECK(pExternalNodeMemory != NULL, "Map pExternalNodeMemory failed");
		pImportedExternalMemory = pExternalNodeMemory;
		importedExternalMemoryHandle = externalNodeMemoryHandle;

		pNodeImports = &pExternalNodeMemory->imports;
		pNodeShared = &pExternalNodeMemory->shared;
	}

	if (!createTestNode)
		goto ExitSuccess;

	// Request and setup handle data
	{
		NodeHandle handle = RequestExternalNodeHandle(pNodeShared);

		pNodeContext = &node.ctxt[handle];
		*pNodeContext = (MxcNodeContext){};

		pNodeContext->type = MXC_NODE_INTERPROCESS_MODE_IMPORTED;
		pNodeContext->pNodeShared = pNodeShared;
		pNodeContext->pNodeImports = pNodeImports;

		pNodeContext->swapsSyncedHandle = pImportedExternalMemory->imports.swapsSyncedHandle;
		assert(pNodeContext->swapsSyncedHandle != NULL);

		pNodeShared->cstNodeHandle = handle;
		pNodeShared->appNodeHandle = -1;

		printf("Importing node handle %d\n", handle);
	}

	// Create node data
	{
		vkSemaphoreCreateInfoExt compTimelineCreateInfo = {
			.debugName = "CompositorTimelineSemaphoreImport",
			.locality = VK_LOCALITY_INTERPROCESS_IMPORTED_READONLY,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			.importHandle = pNodeImports->compositorTimelineHandle,
		};
		vkCreateSemaphoreExt(&compTimelineCreateInfo, &pNodeContext->compositorTimeline);
		vkSemaphoreCreateInfoExt nodeTimelineCreateInfo = {
			.debugName = "NodeTimelineSemaphoreImport",
			.locality = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			.importHandle = pNodeImports->nodeTimelineHandle,
		};
		vkCreateSemaphoreExt(&nodeTimelineCreateInfo, &pNodeContext->nodeTimeline);

		VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
		};
		VK_CHECK(vkCreateCommandPool(vk.context.device, &graphicsCommandPoolCreateInfo, VK_ALLOC, &pNodeContext->pool));
		VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = pNodeContext->pool,
			.commandBufferCount = 1,
		};
		VK_CHECK(vkAllocateCommandBuffers(vk.context.device, &commandBufferAllocateInfo, &pNodeContext->cmd));
		vkSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)pNodeContext->cmd, "TestNode");
	}

	// Start node thread
	{
		__atomic_thread_fence(__ATOMIC_RELEASE);
		CHECK(pthread_create(&pNodeContext->threadId, NULL, (void* (*)(void*))mxcTestNodeThread, pNodeContext), "Node Process Import thread creation failed!");
		printf("Node Request Process Import Success.\n");
		goto ExitSuccess;
	}

ExitError:
	// do cleanup
	// need a NodeContext cleanup method
ExitSuccess:
	if (clientSocket != INVALID_SOCKET)
		closesocket(clientSocket);
	WSACleanup();
}
//// Shutdown Node from Server
// I don't know if I'd ever want to do this?
void mxcShutdownInterprocessNode()
{
//	for (int i = 0; i < nodeCount; ++i) {
//		// make another queue to evade ptr?
//		mxcIpcFuncEnqueue(&pDuplicatedNodeShared[i]->nodeInterprocessFuncQueue, MXC_INTERPROCESS_TARGET_NODE_CLOSED);
//	}
}

///////////////////
//// IPC Func Queue
////
int midRingEnqueue(MxcRingBuffer* pBuffer, MxcRingBufferHandle target)
{
	ATOMIC_FENCE_BLOCK {
		MxcRingBufferHandle head = pBuffer->head;
		MxcRingBufferHandle tail = pBuffer->tail;
		if (head + 1 == tail) {
			LOG_ERROR("Ring Buffer Wrapped!\n");
			return 1;
		}
		pBuffer->targets[head] = target;
		pBuffer->head = (head + 1) % MXC_RING_BUFFER_CAPACITY;
	}
	return 0;
}

int midRingDequeue(MxcRingBuffer* pBuffer, MxcRingBufferHandle *pTarget)
{
	ATOMIC_FENCE_BLOCK {
		MxcRingBufferHandle head = pBuffer->head;
		MxcRingBufferHandle tail = pBuffer->tail;
		if (head == tail) return 1;
		*pTarget = (MxcIpcFunc)(pBuffer->targets[tail]);
		pBuffer->tail = (tail + 1) % MXC_RING_BUFFER_CAPACITY;
	}
	return 0;
}

int mxcIpcFuncEnqueue(MxcRingBuffer* pBuffer, MxcIpcFunc target) {
	return midRingEnqueue(pBuffer, target);
}

int mxcIpcFuncDequeue(MxcRingBuffer* pBuffer, NodeHandle nodeHandle)
{
	MxcRingBufferHandle target;
	if (midRingDequeue(pBuffer, &target)) return 1;
	LOG("Calling IPC Target %d...\n", target);
	MXC_IPC_FUNCS[target](nodeHandle);
	return 0;
}

static void ipcFuncNodeClosed(NodeHandle handle)
{
	LOG("Closing %d\n", handle);
	ReleaseNode(handle);
	CleanupNode(handle);
}

static void ipcFuncClaimSwap(NodeHandle hNode)
{
	LOG("Claiming Swap for Node %d\n", hNode);

	auto pNodeCtx = &node.ctxt[hNode];
	auto pNodeShrd = node.pShared[hNode];
	auto pNodeCompData = &node.cstData[hNode];

	bool needsExport = pNodeCtx->type != MXC_NODE_INTERPROCESS_MODE_THREAD;
	int  swapCt      = XR_SWAP_TYPE_COUNTS[pNodeShrd->swapType] * XR_SWAP_COUNT;
	if (pNodeShrd->swapWidth != DEFAULT_WIDTH || pNodeShrd->swapHeight != DEFAULT_HEIGHT) {
		LOG_ERROR("Requested swapCtx size not available! %d %d\n", pNodeShrd->swapWidth, pNodeShrd->swapHeight);
		pNodeShrd->swapType = XR_SWAP_TYPE_ERROR;
		return;
	}

	MxcSwapInfo swapInfo = {
		.type           = pNodeShrd->swapType,
		.yScale         = MXC_SWAP_SCALE_FULL,
		.xScale         = MXC_SWAP_SCALE_FULL,
		.extent         = {pNodeShrd->swapWidth, pNodeShrd->swapHeight, 1},
		.compositorMode = pNodeShrd->compositorMode,
		.locality       = VK_LOCALITY_INTERPROCESS_EXPORTED_READWRITE,
	};

	pNodeCtx->swapType = pNodeShrd->swapType; // we need something elegant to signify this is duplicated state between shared import and context
	int swapBlockIndex = MXC_SWAP_TYPE_BLOCK_INDEX_BY_TYPE[pNodeShrd->swapType];
	for (int si = 0; si < swapCt; ++si) {

		bHnd hSwap = BLOCK_CLAIM(node.block.swap[swapBlockIndex], 0);
		if (!HANDLE_VALID(hSwap)) {
			LOG_ERROR("Fail to claim swaps!\n");
			goto ExitError;
		}

		pNodeCtx->hSwaps[si] = hSwap;
		auto pSwap = BLOCK_PTR(node.block.swap[swapBlockIndex], hSwap);

		// Should we release or always recreate images?
		// Until they are sharing different size probably better to release
		if (pSwap->type == XR_SWAP_TYPE_UNKNOWN) {
			mxcCreateSwap(&swapInfo, pSwap);
			printf("Created Swap %d: %d %d\n", si, pNodeShrd->swapWidth, pNodeShrd->swapHeight);
		} else {
			printf("Recycled Swap %d: %d %d\n", si, pNodeShrd->swapWidth, pNodeShrd->swapHeight);
		}

		// pack VkHandles for hot access
		auto pCompSwap = &pNodeCompData->swaps[si];
		pCompSwap->color          = pSwap->color.image;
		pCompSwap->colorView      = pSwap->color.view;
		pCompSwap->depth          = pSwap->depth.image;
		pCompSwap->depthView      = pSwap->depth.view;
		pCompSwap->gBuffer        = pSwap->gbuffer.image;
		pCompSwap->gBufferView    = pSwap->gbuffer.view;
		pCompSwap->gBufferMip     = pSwap->gbufferMip.image;
		pCompSwap->gBufferMipView = pSwap->gbufferMip.view;

		if (needsExport) {
			WIN32_CHECK(DuplicateHandle(
					GetCurrentProcess(), pSwap->colorExternal.handle,
					pNodeCtx->processHandle, &pNodeCtx->pExportedExternalMemory->imports.colorSwapHandles[si],
					0, false, DUPLICATE_SAME_ACCESS),
				"Duplicate localTexture handle fail");
			WIN32_CHECK(DuplicateHandle(
					GetCurrentProcess(), pSwap->depthExternal.handle,
					pNodeCtx->processHandle, &pNodeCtx->pExportedExternalMemory->imports.depthSwapHandles[si],
					0, false, DUPLICATE_SAME_ACCESS),
				"Duplicate depth handle fail");
		}
	}

	SetEvent(pNodeCtx->swapsSyncedHandle);

ExitError:
	// ya we need some error handling
}
const MxcIpcFuncPtr MXC_IPC_FUNCS[] = {
	[MXC_INTERPROCESS_TARGET_NODE_CLOSED] = (MxcIpcFuncPtr const)ipcFuncNodeClosed,
	[MXC_INTERPROCESS_TARGET_SYNC_SWAPS] = (MxcIpcFuncPtr const)ipcFuncClaimSwap,
};

