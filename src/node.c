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

// Compositor and Node Process both use
MxcNodeContext nodeContext[MXC_NODE_CAPACITY] = {};

// Node state in Compositor Process
u16                   nodeCount = 0;
MxcNodeCompositorData nodeCompositorData[MXC_NODE_CAPACITY] = {};
MxcNodeShared*        pDuplicatedNodeShared[MXC_NODE_CAPACITY] = {};

MxcActiveNodes activeNodes[MXC_COMPOSITOR_MODE_COUNT] = {};

// Only used for local thread nodes. Node from other process will use shared memory.
// Maybe I could just always make it use shared memory for consistency’s sake?
MxcNodeShared localNodeShared[MXC_NODE_CAPACITY] = {};

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

constexpr int MXC_SWAP_TYPE_BLOCK_INDEX_BY_TYPE[] = {
	[XR_SWAP_TYPE_UNKNOWN]              = 0,
	[XR_SWAP_TYPE_MONO_SINGLE]          = 0,
	[XR_SWAP_TYPE_STEREO_SINGLE]        = 0,
	[XR_SWAP_TYPE_STEREO_TEXTURE_ARRAY] = 1,
	[XR_SWAP_TYPE_STEREO_DOUBLE_WIDE]   = 2,
};
constexpr int MXC_SWAP_TYPE_BLOCK_COUNT = 3;

static struct {

	struct {
		BLOCK_DECL(MxcSwap, XR_SESSIONS_CAPACITY) swap[MXC_SWAP_TYPE_BLOCK_COUNT];
	} block;

} node;

void mxcCreateSwap(const MxcSwapInfo* pInfo, MxcSwap* pSwap)
{
	CHECK(pInfo->extent.width < 1024 || pInfo->extent.height < 1024, "Swap too small!")

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
		vkWin32CreateExternalTexture(&info, &pSwap->colorExternal);
		VkTextureCreateInfo textureInfo = {
			.debugName        = "ExportedColorFramebuffer",
			.pImageCreateInfo = &info,
			.aspectMask       = VK_IMAGE_ASPECT_COLOR_BIT,
			.importHandle     = pSwap->colorExternal.handle,
			.handleType       = MXC_EXTERNAL_FRAMEBUFFER_HANDLE_TYPE,
			.locality         = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
		};
		vkCreateTexture(&textureInfo, &pSwap->color);
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
		vkWin32CreateExternalTexture(&info, &pSwap->depthExternal);
		VkTextureCreateInfo textureInfo = {
			.debugName        = "ExportedDepthFramebuffer",
			.pImageCreateInfo = &info,
			.aspectMask       = VK_IMAGE_ASPECT_COLOR_BIT,
			.importHandle     = pSwap->depthExternal.handle,
			.handleType       = MXC_EXTERNAL_FRAMEBUFFER_HANDLE_TYPE,
			.locality         = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
		};
		vkCreateTexture(&textureInfo, &pSwap->depth);
	}

	/// GBuffer
	{
		vkCreateTexture(
			&(VkTextureCreateInfo){
				.debugName        = "GBufferFramebuffer",
				.pImageCreateInfo = &(VkImageCreateInfo){
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.imageType   = VK_IMAGE_TYPE_2D,
					.format      = MXC_NODE_GBUFFER_FORMAT,
					.extent      = pInfo->extent,
					.mipLevels   = 1,
					.arrayLayers = 1,
					.samples     = VK_SAMPLE_COUNT_1_BIT,
					.usage       = MXC_NODE_GBUFFER_USAGE,
				},
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.locality   = VK_LOCALITY_CONTEXT},
			&pSwap->gbuffer);

		vkCreateTexture(
			&(VkTextureCreateInfo){
				.debugName        = "GBufferFramebufferMip",
				.pImageCreateInfo = &(VkImageCreateInfo){
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.imageType = VK_IMAGE_TYPE_2D,
					.format    = MXC_NODE_GBUFFER_FORMAT,
					// didn't use mips. Get rid of? Maybe not. If I did mip walking with linear sampling could still be much higher quality gbuffer.
					.extent      = {pInfo->extent.width >> MXC_NODE_GBUFFER_FLATTENED_MIP_COUNT,
									pInfo->extent.height >> MXC_NODE_GBUFFER_FLATTENED_MIP_COUNT,
									pInfo->extent.depth},
					.mipLevels   = 1,
					.arrayLayers = 1,
					.samples     = VK_SAMPLE_COUNT_1_BIT,
					.usage       = MXC_NODE_GBUFFER_USAGE,
				},
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.locality   = VK_LOCALITY_CONTEXT},
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

/////////////////////////
//// Compositor Lifecycle
////
void mxcRequestAndRunCompositorNodeThread(const VkSurfaceKHR surface, void* (*runFunc)(struct MxcCompositorContext*))
{
	vkSemaphoreCreateInfoExt semaphoreCreateInfo = {
		.debugName = "CompTimelineSemaphore",
		.locality = VK_LOCALITY_INTERPROCESS_EXPORTED_READONLY,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
	};
	vkCreateSemaphoreExt(&semaphoreCreateInfo, &compositorContext.timeline);

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
	vkSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)compositorContext.gfxCmd, "CompositorCmd");

	CHECK(pthread_create(&compositorContext.threadId, NULL, (void* (*)(void*))runFunc, &compositorContext), "Comp Node thread creation failed!");
	LOG("Request and Run CompNode Thread Success.\n");
}

///////////////////
//// Node Lifecycle
////
NodeHandle RequestLocalNodeHandle()
{
	// TODO this claim/release handle needs to be a pooling logic
	NodeHandle handle = atomic_fetch_add(&nodeCount, 1);
	localNodeShared[handle] = (MxcNodeShared){};
	pDuplicatedNodeShared[handle] = &localNodeShared[handle];
	return handle;
}
NodeHandle RequestExternalNodeHandle(MxcNodeShared* pNodeShared)
{
	NodeHandle handle = atomic_fetch_add(&nodeCount, 1);
	pDuplicatedNodeShared[handle] = pNodeShared;
	return handle;
}
void ReleaseNode(NodeHandle handle)
{
	assert(nodeContext[handle].type != MXC_NODE_INTERPROCESS_MODE_NONE);
	int newCount = atomic_fetch_sub(&nodeCount, 1);
	printf("Releasing Node %d. Count %d.\n", handle, newCount);
}

#define CLOSE_HANDLE(_handle)                                                     \
	if (!CloseHandle(_handle)) {                                                  \
		DWORD dwError = GetLastError();                                           \
		printf("Could not close (%s) object handle (%lu).\n", #_handle, dwError); \
	}
static int CleanupNode(NodeHandle handle)
{
	auto pNodeCtxt = &nodeContext[handle];

	ReleaseNode(handle);

//	{ as long as mxcInterprocessQueuePoll is called after waiting on MXC_CYCLE_PROCESS_INPUT we don't have to wait here
//		// we need to wait one cycle to get the compositor cmd buffer cleared
//		uint64_t compBaseCycleValue = compositorNodeContext.compBaseCycleValue - (compositorNodeContext.compBaseCycleValue % MXC_CYCLE_COUNT);
//		vkmTimelineWait(midVk.context.device, compBaseCycleValue + MXC_CYCLE_COUNT, compositorNodeContext.compTimeline);
//	}

	// Close external handles
	switch (pNodeCtxt->type) {
		case MXC_NODE_INTERPROCESS_MODE_THREAD:
			vkFreeCommandBuffers(vk.context.device, pNodeCtxt->pool, 1, &pNodeCtxt->cmd);
			vkDestroyCommandPool(vk.context.device, pNodeCtxt->pool, VK_ALLOC);
			int result = pthread_join(pNodeCtxt->threadId, NULL);
			if (result != 0) {
				perror("Thread join failed");
			}
			break;
		case MXC_NODE_INTERPROCESS_MODE_EXPORTED:

			// Don't close handles unless we are actually free'ing
//			for (int i = 0; i < MXC_NODE_SWAP_CAPACITY; ++i) {
//				CLOSE_HANDLE(pNodeCtxt->swap[i].colorExternal.handle);
//				CLOSE_HANDLE(pNodeCtxt->swap[i].depthExternal.handle);
//			}
//			CLOSE_HANDLE(pNodeCtxt->compositorTimelineHandle);
//			CLOSE_HANDLE(pNodeCtxt->nodeTimelineHandle);

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
			if (!UnmapViewOfFile(pImportedExternalMemory)) {
				DWORD dwError = GetLastError();
				printf("Could not unmap view of file (%lu).\n", dwError);
			}
			CLOSE_HANDLE(pNodeCtxt->processHandle);
			break;
		default: PANIC("Node type not supported");
	}

	int swapBlockIndex = MXC_SWAP_TYPE_BLOCK_INDEX_BY_TYPE[pNodeCtxt->swapType];
	for (int i = 0; i < MXC_NODE_SWAP_CAPACITY; ++i) {
		if (HANDLE_VALID(pNodeCtxt->hSwaps[i]))
			BLOCK_RELEASE(node.block.swap[swapBlockIndex], pNodeCtxt->hSwaps[i]);
	}


	// Do I actually want to release data?!
	// This needs an option

	// must first release command buffer
//	vkDestroySemaphore(vk.context.device, pNodeCtxt->nodeTimeline, VK_ALLOC);

	// Clear compositor data
//	VkWriteDescriptorSet writeSets[] = {
//		{
//			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
//			.dstSet = nodeCompositorData[handle].nodeSet,
//			.dstBinding = 1,
//			.descriptorCount = 1,
//			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//			.pImageInfo = &(VkDescriptorImageInfo){
//				.imageView = VK_NULL_HANDLE,
//				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//			},
//		},
//		{
//			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
//			.dstSet = nodeCompositorData[handle].nodeSet,
//			.dstBinding = 2,
//			.descriptorCount = 1,
//			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//			.pImageInfo = &(VkDescriptorImageInfo){
//				.imageView = VK_NULL_HANDLE,
//				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//			},
//		},
//		{
//			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
//			.dstSet = nodeCompositorData[handle].nodeSet,
//			.dstBinding = 3,
//			.descriptorCount = 1,
//			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//			.pImageInfo = &(VkDescriptorImageInfo){
//				.imageView = VK_NULL_HANDLE,
//				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//			},
//		},
//	};
//	vkUpdateDescriptorSets(vk.context.device, COUNT(writeSets), writeSets, 0, NULL);

	// destroy images... this will eventually just return back to some pool
	for (int i = 0; i < VK_SWAP_COUNT; ++i) {
//		vkDestroyImageView(vk.context.device, pNodeContext->swaps[i].color.view, VK_ALLOC);
//		vkDestroyImage(vk.context.device, pNodeContext->swaps[i].color.image, VK_ALLOC);
//		vkFreeMemory(vk.context.device, pNodeContext->swaps[i].color.memory, VK_ALLOC);
//
//		vkDestroyImageView(vk.context.device, pNodeContext->swaps[i].normal.view, VK_ALLOC);
//		vkDestroyImage(vk.context.device, pNodeContext->swaps[i].normal.image, VK_ALLOC);
//		vkFreeMemory(vk.context.device, pNodeContext->swaps[i].normal.memory, VK_ALLOC);
//
//		vkDestroyImageView(vk.context.device, pNodeContext->swaps[i].depth.view, VK_ALLOC);
//		vkDestroyImage(vk.context.device, pNodeContext->swaps[i].depth.image, VK_ALLOC);
//		vkFreeMemory(vk.context.device, pNodeContext->swaps[i].depth.memory, VK_ALLOC);
//
//		vkDestroyImageView(vk.context.device, pNodeContext->swaps[i].gbuffer.view, VK_ALLOC);
//		vkDestroyImage(vk.context.device, pNodeContext->swaps[i].gbuffer.image, VK_ALLOC);
//		vkFreeMemory(vk.context.device, pNodeContext->swaps[i].gbuffer.memory, VK_ALLOC);
	}

	return 0;
}

void mxcRequestNodeThread(void* (*runFunc)(struct MxcNodeContext*), NodeHandle* pNodeHandle)
{
	printf("Requesting Node Thread.\n");
	auto handle = RequestLocalNodeHandle();

	auto pNodeCtxt = &nodeContext[handle];
	*pNodeCtxt = (MxcNodeContext){};
	pNodeCtxt->type = MXC_NODE_INTERPROCESS_MODE_THREAD;
	pNodeCtxt->swapsSyncedHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
	pNodeCtxt->compositorTimeline = compositorContext.timeline;

	// maybe have the thread allocate this and submit it once ready to get rid of < 1 check
	// then could also get rid of pNodeContext->pNodeShared?
	// no how would something on another process do that
	auto pNodeShrd = &localNodeShared[handle];
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

	MxcNodeCompositorData* pNodeCompositorData = &nodeCompositorData[handle];
	// do not clear since set data is preallocated
//	*pNodeCompositorData = (MxcNodeCompositorData){};
//	pNodeCompositorData->rootPose.rotation = QuatFromEuler(pNodeCompositorData->rootPose.euler);

	*pNodeHandle = handle;

	CHECK(pthread_create(&nodeContext[handle].threadId, NULL, (void* (*)(void*))runFunc, pNodeCtxt), "Node thread creation failed!");

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
	}

	// Initialize Context
	{
		*pExtNodeMem = (MxcExternalNodeMemory){};
		pImports = &pExtNodeMem->imports;
		pNodeShrd = &pExtNodeMem->shared;

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

		switch (pNodeShrd->compositorMode) {
			case MXC_COMPOSITOR_MODE_QUAD:
			case MXC_COMPOSITOR_MODE_TESSELATION:
			case MXC_COMPOSITOR_MODE_TASK_MESH:
			case MXC_COMPOSITOR_MODE_COMPUTE:
				break;
			default:
				LOG_ERROR("Compositor mode not supported!");
				goto ExitError;
		}

		// Claim handle and add to active nodes
		NodeHandle handle = RequestExternalNodeHandle(pNodeShrd);
		{
			auto pActiveNode = &activeNodes[pNodeShrd->compositorMode];
			pActiveNode->handles[pActiveNode->ct] = handle;
			pActiveNode->ct++;
			atomic_thread_fence(memory_order_release);
		}

		// TODO we probably do want to clear? put all the buffers and sets into one big shared buffer
//		pNodeCompLocal = &nodeCstLocal[handle];
		// Don't clear. State is recycled and pre-alloced on compositor creation.
//		*pNodeCompositorLocal = (MxcNodeCompositorLocal){};

		// Init Node Context
		pNodeCtxt = &nodeContext[handle];
		*pNodeCtxt = (MxcNodeContext){};

		pNodeCtxt->type = MXC_NODE_INTERPROCESS_MODE_EXPORTED;

		pNodeCtxt->swapsSyncedHandle = CreateEvent(NULL, FALSE, FALSE, NULL);

		pNodeCtxt->compositorTimeline = compositorContext.timeline;
		pNodeCtxt->compositorTimelineHandle = vkGetSemaphoreExternalHandle(pNodeCtxt->compositorTimeline);

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

		printf("Exporting node handle %d\n", handle);
	}

	// Create node data
	{
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
			"Duplicate timeline handle fail.");
	}

	// Send shared memory handle
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
		printf("Process Node Export Success.\n");
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

		pNodeContext = &nodeContext[handle];
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
int mxcIpcFuncEnqueue(MxcRingBuffer* pBuffer, MxcIpcFunc target)
{
	atomic_thread_fence(memory_order_acquire);
	MxcRingBufferHandle head = pBuffer->head;
	MxcRingBufferHandle tail = pBuffer->tail;
	if (head + 1 == tail) {
		LOG_ERROR("Ring buf wrapped!\n");
		return 1;
	}
	pBuffer->targets[head] = target;
	pBuffer->head = (head + 1) % MXC_RING_BUFFER_CAPACITY;
	atomic_thread_fence(memory_order_release);
	return 0;
}

int mxcIpcDequeue(MxcRingBuffer* pBuffer, NodeHandle nodeHandle)
{
	atomic_thread_fence(memory_order_acquire);
	MxcRingBufferHandle head = pBuffer->head;
	MxcRingBufferHandle tail = pBuffer->tail;
	if (head == tail)
		return 1;

	LOG("IPC Polling %d %d...\n", head, tail);
	MxcIpcFunc target = (MxcIpcFunc)(pBuffer->targets[tail]);
	pBuffer->tail = (tail + 1) % MXC_RING_BUFFER_CAPACITY;
	atomic_thread_fence(memory_order_release);

	LOG("Calling IPC Target %d...\n", target);
	MXC_IPC_FUNCS[target](nodeHandle);
	return 0;
}

static void ipcFuncNodeClosed(NodeHandle handle)
{
	LOG("Closing %d\n", handle);
	CleanupNode(handle);
}
static void ipcFuncClaimSwap(NodeHandle hNode)
{
	LOG("Claiming Swap for Node %d\n", hNode);

	auto pNodeCtx = &nodeContext[hNode];
	auto pNodeShrd = pDuplicatedNodeShared[hNode];
	auto pNodeCompData = &nodeCompositorData[hNode];

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
		if (pSwap->color.image == NULL) {
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

