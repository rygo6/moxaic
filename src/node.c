#define WIN32_LEAN_AND_MEAN
#undef UNICODE
#include <winsock2.h>
#include <afunix.h>
#include <stdio.h>

#include <pthread.h>
#include <assert.h>

#include "node.h"
#include "test_node.h"
#include "mid_vulkan.h"

MxcNodeSwapPool nodeSwapPool[MXC_SWAP_SCALE_COUNT][MXC_SWAP_SCALE_COUNT];

// Compositor and Node Process both use
MxcNodeContext           nodeContexts[MXC_NODE_CAPACITY] = {};

// Node state in Compositor Process
size_t                   nodeCount = 0;
MxcNodeCompositorData    nodeCompositorData[MXC_NODE_CAPACITY] = {};
MxcNodeShared*           activeNodesShared[MXC_NODE_CAPACITY] = {};

// Only used for local thread nodes. Node from other process will use shared memory.
// Maybe I could just always make it use shared memory for consistency’s sake?
MxcNodeShared localNodesShared[MXC_NODE_CAPACITY] = {};

// Compositor state in Compositor Process
MxcCompositorNodeContext compositorNodeContext = {};

// State imported into Node Process
HANDLE                 importedExternalMemoryHandle = NULL;
MxcExternalNodeMemory* pImportedExternalMemory = NULL;

// this should become a midvk construct
size_t                     submitNodeQueueStart = 0;
size_t                     submitNodeQueueEnd = 0;
MxcQueuedNodeCommandBuffer submitNodeQueue[MXC_NODE_CAPACITY] = {};

void mxcRequestAndRunCompositorNodeThread(const VkSurfaceKHR surface, void* (*runFunc)(const struct MxcCompositorNodeContext*))
{
	MidVkSemaphoreCreateInfo semaphoreCreateInfo = {
		.debugName = "CompTimelineSemaphore",
		.locality = VK_LOCALITY_INTERPROCESS_EXPORTED_READONLY,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE};
	midVkCreateSemaphore(&semaphoreCreateInfo, &compositorNodeContext.compositorTimeline);
	vkCreateSwapContext(surface, VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS, &compositorNodeContext.swap);

	VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = vk.context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
	};
	VK_CHECK(vkCreateCommandPool(vk.context.device, &graphicsCommandPoolCreateInfo, VK_ALLOC, &compositorNodeContext.pool));
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = compositorNodeContext.pool,
		.commandBufferCount = 1,
	};
	VK_CHECK(vkAllocateCommandBuffers(vk.context.device, &commandBufferAllocateInfo, &compositorNodeContext.cmd));
	vkSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)compositorNodeContext.cmd, "CompCmd");

	CHECK(pthread_create(&compositorNodeContext.threadId, NULL, (void* (*)(void*))runFunc, &compositorNodeContext), "Comp Node thread creation failed!");
	printf("Request and Run CompNode Thread Success.\n");
}

static NodeHandle RequestLocalNodeHandle()
{
	NodeHandle handle = __atomic_fetch_add(&nodeCount, 1, __ATOMIC_RELEASE);
	localNodesShared[handle] = (MxcNodeShared){};
	activeNodesShared[handle] = &localNodesShared[handle];
	return handle;
}
NodeHandle RequestExternalNodeHandle(MxcNodeShared* pNodeShared)
{
	NodeHandle handle = __atomic_fetch_add(&nodeCount, 1, __ATOMIC_RELEASE);
	activeNodesShared[handle] = pNodeShared;
	return handle;
}
void ReleaseNode(NodeHandle handle)
{
	assert(nodeContexts[handle].type != MXC_NODE_TYPE_NONE);
	int newCount = __atomic_sub_fetch(&nodeCount, 1, __ATOMIC_RELEASE);
	printf("Releasing Node %d. Count %d.\n", handle, newCount);
}

#define CLOSE_HANDLE(_handle)                                                     \
	if (!CloseHandle(_handle)) {                                                  \
		DWORD dwError = GetLastError();                                           \
		printf("Could not close (%s) object handle (%lu).\n", #_handle, dwError); \
	}
static int CleanupNode(NodeHandle handle)
{
	MxcNodeContext* pNodeContext = &nodeContexts[handle];

	ReleaseNode(handle);

//	{ as long as mxcInterprocessQueuePoll is called after waiting on MXC_CYCLE_PROCESS_INPUT we don't have to wait here
//		// we need to wait one cycle to get the compositor cmd buffer cleared
//		uint64_t compBaseCycleValue = compositorNodeContext.compBaseCycleValue - (compositorNodeContext.compBaseCycleValue % MXC_CYCLE_COUNT);
//		vkmTimelineWait(midVk.context.device, compBaseCycleValue + MXC_CYCLE_COUNT, compositorNodeContext.compTimeline);
//	}

	// Close external handles
	switch (pNodeContext->type) {
		case MXC_NODE_TYPE_THREAD:
			vkFreeCommandBuffers(vk.context.device, pNodeContext->pool, 1, &pNodeContext->cmd);
			vkDestroyCommandPool(vk.context.device, pNodeContext->pool, VK_ALLOC);
			int result = pthread_join(pNodeContext->threadId, NULL);
			if (result != 0) {
				perror("Thread join failed");
			}
			break;
		case MXC_NODE_TYPE_INTERPROCESS_VULKAN_EXPORTED:
			// we don't want to close local handle, only the duplicated handle
//			for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
//				CLOSE_HANDLE(vkGetMemoryExternalHandle(pNodeContext->vkNodeFramebufferTextures[i].color.memory));
//				CLOSE_HANDLE(vkGetMemoryExternalHandle(pNodeContext->vkNodeFramebufferTextures[i].normal.memory));
//				CLOSE_HANDLE(vkGetMemoryExternalHandle(pNodeContext->vkNodeFramebufferTextures[i].gbuffer.memory));
//			}
			CLOSE_HANDLE(vkGetSemaphoreExternalHandle(pNodeContext->nodeTimeline));
			break;
		case MXC_NODE_TYPE_INTERPROCESS_VULKAN_IMPORTED:
			// this should probably run on the node when closing, but it also seems to clean itself up fine ?
//			for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
//				CLOSE_HANDLE(pImportedExternalMemory->importParam.framebufferHandles[i].color);
//				CLOSE_HANDLE(pImportedExternalMemory->importParam.framebufferHandles[i].normal);
//				CLOSE_HANDLE(pImportedExternalMemory->importParam.framebufferHandles[i].gbuffer);
//			}
//			CLOSE_HANDLE(pImportedExternalMemory->importParam.nodeTimelineHandle);
//			CLOSE_HANDLE(pImportedExternalMemory->importParam.compTimelineHandle);
//			if (!UnmapViewOfFile(pImportedExternalMemory)) {
//				DWORD dwError = GetLastError();
//				printf("Could not unmap view of file (%d).\n", dwError);
//			}
//			CLOSE_HANDLE(pImportedExternalMemory);
//			CLOSE_HANDLE(pNodeContext->processHandle);
			break;
		default: PANIC("Node type not supported");
	}

	// must first release command buffer
	vkDestroySemaphore(vk.context.device, pNodeContext->nodeTimeline, VK_ALLOC);

	// Clear compositor data
	VkWriteDescriptorSet writeSets[] = {
		(VkWriteDescriptorSet) {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = nodeCompositorData[handle].set,
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &(VkDescriptorImageInfo){
				.imageView = VK_NULL_HANDLE,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			},
		},
		(VkWriteDescriptorSet) {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = nodeCompositorData[handle].set,
			.dstBinding = 2,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &(VkDescriptorImageInfo){
				.imageView = VK_NULL_HANDLE,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			},
		},
		(VkWriteDescriptorSet) {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = nodeCompositorData[handle].set,
			.dstBinding = 3,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &(VkDescriptorImageInfo){
				.imageView = VK_NULL_HANDLE,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			},
		}
	};
	vkUpdateDescriptorSets(vk.context.device, COUNT(writeSets), writeSets, 0, NULL);

	// destroy images... this will eventually just return back to some pool
	for (int i = 0; i < VK_SWAP_COUNT; ++i) {
		vkDestroyImageView(vk.context.device, pNodeContext->swaps[i].color.view, VK_ALLOC);
		vkDestroyImage(vk.context.device, pNodeContext->swaps[i].color.image, VK_ALLOC);
		vkFreeMemory(vk.context.device, pNodeContext->swaps[i].color.memory, VK_ALLOC);

		vkDestroyImageView(vk.context.device, pNodeContext->swaps[i].normal.view, VK_ALLOC);
		vkDestroyImage(vk.context.device, pNodeContext->swaps[i].normal.image, VK_ALLOC);
		vkFreeMemory(vk.context.device, pNodeContext->swaps[i].normal.memory, VK_ALLOC);

		vkDestroyImageView(vk.context.device, pNodeContext->swaps[i].depth.view, VK_ALLOC);
		vkDestroyImage(vk.context.device, pNodeContext->swaps[i].depth.image, VK_ALLOC);
		vkFreeMemory(vk.context.device, pNodeContext->swaps[i].depth.memory, VK_ALLOC);

		vkDestroyImageView(vk.context.device, pNodeContext->swaps[i].gbuffer.view, VK_ALLOC);
		vkDestroyImage(vk.context.device, pNodeContext->swaps[i].gbuffer.image, VK_ALLOC);
		vkFreeMemory(vk.context.device, pNodeContext->swaps[i].gbuffer.memory, VK_ALLOC);
	}

	return 0;
}

void mxcRequestNodeThread(void* (*runFunc)(const struct MxcNodeContext*), NodeHandle* pNodeHandle)
{
	printf("Requesting Node Thread.\n");
	const NodeHandle handle = RequestLocalNodeHandle();

	MxcNodeContext* pNodeContext = &nodeContexts[handle];
	*pNodeContext = (MxcNodeContext){};
	pNodeContext->compositorTimeline = compositorNodeContext.compositorTimeline;
	pNodeContext->type = MXC_NODE_TYPE_THREAD;

	// maybe have the thread allocate this and submit it once ready to get rid of < 1 check
	// then could also get rid of pNodeContext->pNodeShared?
	// no how would something on another process do that
	MxcNodeShared* pNodeShared = &localNodesShared[handle];
	*pNodeShared = (MxcNodeShared){};
	pNodeContext->pNodeShared = pNodeShared;
	pNodeShared->rootPose.rotation = QuatFromEuler(pNodeShared->rootPose.euler);

	pNodeShared->camera.yFOV = 45.0f;
	pNodeShared->camera.zNear = 0.1f;
	pNodeShared->camera.zFar = 100.0f;

	pNodeShared->compositorRadius = 0.5;
	pNodeShared->compositorCycleSkip = 8;

//	VkExternalFenceCreateInfo nodeFenceCreateInfo = {
//		.debugName = "NodeFence",
//		.locality = VK_LOCALITY_CONTEXT,
//	};
//	vkCreateExternalFence(&nodeFenceCreateInfo, &pNodeContext->vkNodeFence);

	MidVkSemaphoreCreateInfo semaphoreCreateInfo = {
		.debugName = "NodeTimelineSemaphore",
		.locality = VK_LOCALITY_CONTEXT,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
	};
	midVkCreateSemaphore(&semaphoreCreateInfo, &pNodeContext->nodeTimeline);

	VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = vk.context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
	};
	VK_CHECK(vkCreateCommandPool(vk.context.device, &graphicsCommandPoolCreateInfo, VK_ALLOC, &pNodeContext->pool));
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = pNodeContext->pool,
		.commandBufferCount = 1,
	};
	VK_CHECK(vkAllocateCommandBuffers(vk.context.device, &commandBufferAllocateInfo, &pNodeContext->cmd));
	vkSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)pNodeContext->cmd, "TestNode");

	VkFramebufferTextureCreateInfo framebufferInfo = {
		.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1},
		.locality = VK_LOCALITY_CONTEXT,
	};
	for (int i = 0; i < VK_SWAP_COUNT; ++i)
		mxcCreateSwap(NULL, &framebufferInfo, &pNodeContext->swaps[i]);

	MxcNodeCompositorData* pNodeCompositorData = &nodeCompositorData[handle];
	// do not clear since set data is prealloced
//	*pNodeCompositorData = (MxcNodeCompositorData){};

	pNodeCompositorData->rootPose.rotation = QuatFromEuler(pNodeCompositorData->rootPose.euler);
	for (int i = 0; i < VK_SWAP_COUNT; ++i) {
		pNodeCompositorData->framebuffers[i].color = nodeContexts[handle].swaps[i].color.image;
		pNodeCompositorData->framebuffers[i].normal = nodeContexts[handle].swaps[i].normal.image;
		pNodeCompositorData->framebuffers[i].gBuffer = nodeContexts[handle].swaps[i].gbuffer.image;
		pNodeCompositorData->framebuffers[i].colorView = nodeContexts[handle].swaps[i].color.view;
		pNodeCompositorData->framebuffers[i].normalView = nodeContexts[handle].swaps[i].normal.view;
		pNodeCompositorData->framebuffers[i].gBufferView = nodeContexts[handle].swaps[i].gbuffer.view;

#define ACQUIRE_BARRIER                                                      \
	.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,                    \
	.srcAccessMask = VK_ACCESS_2_NONE,                                       \
	.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT |                   \
					VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT |    \
					VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT | \
					VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,                 \
	.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,                            \
	.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,                   \
	.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,                   \
	.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,                          \
	VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED
		pNodeCompositorData->framebuffers[i].acquireBarriers[0] = (VkImageMemoryBarrier2){
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.image = pNodeCompositorData->framebuffers[i].color,
			ACQUIRE_BARRIER,
		};
		pNodeCompositorData->framebuffers[i].acquireBarriers[1] = (VkImageMemoryBarrier2){
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.image = pNodeCompositorData->framebuffers[i].normal,
			ACQUIRE_BARRIER,
		};
		pNodeCompositorData->framebuffers[i].acquireBarriers[2] = (VkImageMemoryBarrier2){
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.image = pNodeCompositorData->framebuffers[i].gBuffer,
			ACQUIRE_BARRIER,
		};
#undef ACQUIRE_BARRIER

#define RELEASE_BARRIER                                      \
	.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, \
	.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,            \
	.dstStageMask = VK_PIPELINE_STAGE_2_NONE,                \
	.dstAccessMask = VK_ACCESS_2_NONE,                       \
	.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,   \
	.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,   \
	.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,          \
	VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED
		pNodeCompositorData->framebuffers[i].releaseBarriers[0] = (VkImageMemoryBarrier2){
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.image = pNodeCompositorData->framebuffers[i].color,
			RELEASE_BARRIER,
		};
		pNodeCompositorData->framebuffers[i].releaseBarriers[1] = (VkImageMemoryBarrier2){
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.image = pNodeCompositorData->framebuffers[i].normal,
			RELEASE_BARRIER,
		};
		pNodeCompositorData->framebuffers[i].releaseBarriers[2] = (VkImageMemoryBarrier2){
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.image = pNodeCompositorData->framebuffers[i].gBuffer,
			RELEASE_BARRIER,
		};
#undef RELEASE_BARRIER
	}

	*pNodeHandle = handle;

	CHECK(pthread_create(&nodeContexts[handle].threadId, NULL, (void* (*)(void*))runFunc, pNodeContext), "Node thread creation failed!");

	printf("Request Node Thread Success. Handle: %d\n", handle);
	// todo this needs error handling
}

//
/// Node Render
// Do I want this?
static const VkImageUsageFlags NODE_PASS_USAGES[] = {
	[MIDVK_PASS_ATTACHMENT_COLOR_INDEX] = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
	[MIDVK_PASS_ATTACHMENT_NORMAL_INDEX] = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
	[MIDVK_PASS_ATTACHMENT_DEPTH_INDEX] = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
};
void mxcCreateNodeRenderPass()
{
	VkRenderPassCreateInfo2 renderPassCreateInfo2 = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
		.attachmentCount = 3,
		.pAttachments = (VkAttachmentDescription2[]){
			[MIDVK_PASS_ATTACHMENT_COLOR_INDEX] = {
				.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
				.format = VK_BASIC_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_COLOR_INDEX],
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			},
			[MIDVK_PASS_ATTACHMENT_NORMAL_INDEX] = {
				.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
				.format = VK_BASIC_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_NORMAL_INDEX],
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			},
			[MIDVK_PASS_ATTACHMENT_DEPTH_INDEX] = {
				.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
				.format = VK_BASIC_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_DEPTH_INDEX],
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
				.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.colorAttachmentCount = 2,
				.pColorAttachments = (VkAttachmentReference2[]){
					[MIDVK_PASS_ATTACHMENT_COLOR_INDEX] = {
						.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
						.attachment = MIDVK_PASS_ATTACHMENT_COLOR_INDEX,
						.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					},
					[MIDVK_PASS_ATTACHMENT_NORMAL_INDEX] = {
						.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
						.attachment = MIDVK_PASS_ATTACHMENT_NORMAL_INDEX,
						.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					},
				},
				.pDepthStencilAttachment = &(VkAttachmentReference2){
					.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
					.attachment = MIDVK_PASS_ATTACHMENT_DEPTH_INDEX,
					.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				},
			},
		},
		.dependencyCount = 2,
		.pDependencies = (VkSubpassDependency2[]){
			// from here https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples#combined-graphicspresent-queue
			{
				.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dependencyFlags = 0,
			},
			{
				.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
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
	VK_CHECK(vkCreateRenderPass2(vk.context.device, &renderPassCreateInfo2, VK_ALLOC, &vk.context.nodeRenderPass));
	vkSetDebugName(VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)vk.context.nodeRenderPass, "NodeRenderPass");
}

void mxcCreateSwap(const MxcSwapInfo* pInfo, const VkFramebufferTextureCreateInfo* pTextureCreateInfo, MxcSwap* pSwap)
{
#if defined(D3D11)
	#define EXTERNAL_FRAMEBUFFER_HANDLE_TYPE VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT
#elif defined(D3D12)
	#define EXTERNAL_FRAMEBUFFER_HANDLE_TYPE VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT
#endif

	{
		VkImageCreateInfo info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.pNext = &(VkExternalMemoryImageCreateInfo){
				.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
				.handleTypes = EXTERNAL_FRAMEBUFFER_HANDLE_TYPE,
			},
			.imageType = VK_IMAGE_TYPE_2D,
			.format = VK_BASIC_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_COLOR_INDEX],
			.extent = pTextureCreateInfo->extent,
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.usage = VK_BASIC_PASS_USAGES[MIDVK_PASS_ATTACHMENT_COLOR_INDEX],
		};
		vkWin32CreateExternalTexture(&info, &pSwap->colorExternal);
		VkTextureCreateInfo textureInfo = {
			.debugName = "ExportedColorFramebuffer",
			.pImageCreateInfo = &info,
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.importHandle = pSwap->colorExternal.handle,
			.handleType = EXTERNAL_FRAMEBUFFER_HANDLE_TYPE,
			.locality = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
		};
		vkCreateTexture(&textureInfo, &pSwap->color);
	}

	{
		VkImageCreateInfo info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.pNext = &(VkExternalMemoryImageCreateInfo){
				.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
				.handleTypes = EXTERNAL_FRAMEBUFFER_HANDLE_TYPE,
			},
			.imageType = VK_IMAGE_TYPE_2D,
			.format = VK_BASIC_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_NORMAL_INDEX],
			.extent = pTextureCreateInfo->extent,
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.usage = VK_BASIC_PASS_USAGES[MIDVK_PASS_ATTACHMENT_NORMAL_INDEX],
		};
		vkWin32CreateExternalTexture(&info, &pSwap->normalExternal);
		VkTextureCreateInfo textureInfo = {
			.debugName = "ExportedNormalFramebuffer",
			.pImageCreateInfo = &info,
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.importHandle = pSwap->normalExternal.handle,
			.handleType = EXTERNAL_FRAMEBUFFER_HANDLE_TYPE,
			.locality = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
		};
		vkCreateTexture(&textureInfo, &pSwap->normal);
	}

	{
		VkImageCreateInfo info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.pNext = &(VkExternalMemoryImageCreateInfo){
				.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
				.handleTypes = EXTERNAL_FRAMEBUFFER_HANDLE_TYPE,
			},
			.imageType = VK_IMAGE_TYPE_2D,
			.format = MXC_NODE_GBUFFER_FORMAT,
			.extent = pTextureCreateInfo->extent,
			.mipLevels = MXC_NODE_GBUFFER_LEVELS,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.usage = MXC_NODE_GBUFFER_USAGE,
		};
		vkWin32CreateExternalTexture(&info, &pSwap->gbufferExternal);
		VkTextureCreateInfo textureInfo = {
			.debugName = "ExportedGBufferFramebuffer",
			.pImageCreateInfo = &info,
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.importHandle = pSwap->gbufferExternal.handle,
			.handleType = EXTERNAL_FRAMEBUFFER_HANDLE_TYPE,
			.locality = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
		};
		vkCreateTexture(&textureInfo, &pSwap->gbuffer);
	}

	// If exporting, transition to interprocess state and skip depth creation since depth is not sent over IPC currently
	if (pTextureCreateInfo->locality == VK_LOCALITY_INTERPROCESS_EXPORTED_READWRITE || pTextureCreateInfo->locality == VK_LOCALITY_INTERPROCESS_EXPORTED_READONLY) {
		// we need to transition these out of undefined initially because the transition in the other process won't update layout to avoid initial validation error on transition
		VkImageMemoryBarrier2 interProcessBarriers[] = {
			VKM_COLOR_IMAGE_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMG_BARRIER_EXTERNAL_ACQUIRE_SHADER_READ, pSwap->color.image),
			VKM_COLOR_IMAGE_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMG_BARRIER_EXTERNAL_ACQUIRE_SHADER_READ, pSwap->normal.image),
			VKM_COLOR_IMAGE_BARRIER_MIPS(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMG_BARRIER_EXTERNAL_ACQUIRE_SHADER_READ, pSwap->gbuffer.image, 0, MXC_NODE_GBUFFER_LEVELS),
		};
		VK_DEVICE_FUNC(CmdPipelineBarrier2);
		VkCommandBuffer cmd = midVkBeginImmediateTransferCommandBuffer();
		CmdPipelineImageBarriers2(cmd, COUNT(interProcessBarriers), interProcessBarriers);
		midVkEndImmediateTransferCommandBuffer(cmd);

		return;
	}

	VkTextureCreateInfo depthCreateInfo = {
		.debugName = "ImportedDepthFramebuffer",
		.pImageCreateInfo = &(VkImageCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = VK_BASIC_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_DEPTH_INDEX],
			.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.usage = VK_BASIC_PASS_USAGES[MIDVK_PASS_ATTACHMENT_DEPTH_INDEX],
		},
		.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
		.locality = VK_LOCALITY_CONTEXT,
	};
	vkCreateTexture(&depthCreateInfo, &pSwap->depth);

}

//
/// IPC
#define SOCKET_PATH "C:\\temp\\moxaic_socket"

static struct {
	SOCKET    listenSocket;
	pthread_t thread;
} ipcServer;
const static char serverIPCAckMessage[] = "CONNECT-MOXAIC-COMPOSITOR-0.0.0";
const static char nodeIPCAckMessage[] = "CONNECT-MOXAIC-NODE-0.0.0";

// Checks WIN32 error code. Expects 1 for success.
#define WIN32_CHECK(_command, _message)                             \
	{                                                               \
		int _result = (_command);                                   \
		if (__builtin_expect(!(_result), 0)) {                      \
			fprintf(stderr, "%s: %ld\n", _message, GetLastError()); \
			goto Exit;                                              \
		}                                                           \
	}
// Checks WSA error code. Expects 0 for success.
#define WSA_CHECK(_command, _message)                                 \
	{                                                                 \
		int _result = (_command);                                     \
		if (__builtin_expect(!!(_result), 0)) {                       \
			fprintf(stderr, "%s: %d\n", _message, WSAGetLastError()); \
			goto Exit;                                                \
		}                                                             \
	}

static void InterprocessServerAcceptNodeConnection()
{
	printf("Accepting connections on: '%s'\n", SOCKET_PATH);
	MxcNodeContext*        pNodeContext = NULL;
	MxcNodeShared*         pNodeShared = NULL;
	MxcRingBuffer*         pTargetQueue = NULL;
	MxcImportParam*        pImportParam = NULL;
	MxcNodeCompositorData* pNodeCompositorData = NULL;
	HANDLE                 nodeProcessHandle = INVALID_HANDLE_VALUE;
	HANDLE                 externalNodeMemoryHandle = INVALID_HANDLE_VALUE;
	MxcExternalNodeMemory* pExternalNodeMemory = NULL;
	DWORD                  nodeProcessId = 0;

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
		int receiveLength = recv(clientSocket, (char*)&nodeProcessId, sizeof(DWORD), 0);
		WSA_CHECK(receiveLength == SOCKET_ERROR || receiveLength == 0, "Recv node process id failed");
		printf("Received node process id: %lu Size: %d\n", nodeProcessId, receiveLength);
		CHECK(nodeProcessId == 0, "Invalid node process id");
	}

	// Create shared memory
	{
		nodeProcessHandle = OpenProcess(PROCESS_DUP_HANDLE, FALSE, nodeProcessId);
		WIN32_CHECK(nodeProcessHandle != NULL && nodeProcessHandle != INVALID_HANDLE_VALUE, "Duplicate process handle failed");
		externalNodeMemoryHandle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(MxcExternalNodeMemory), NULL);
		WIN32_CHECK(externalNodeMemoryHandle != NULL, "Could not create file mapping object");
		pExternalNodeMemory = MapViewOfFile(externalNodeMemoryHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MxcExternalNodeMemory));
		WIN32_CHECK(pExternalNodeMemory != NULL, "Could not map view of file");
		*pExternalNodeMemory = (MxcExternalNodeMemory){};
	}

	// Request and setup handle data
	{
		pImportParam = &pExternalNodeMemory->importParam;
		pNodeShared = &pExternalNodeMemory->shared;
		pNodeShared->rootPose.rotation = QuatFromEuler(pNodeShared->rootPose.euler);

		pNodeShared->camera.yFOV = 45.0f;
		pNodeShared->camera.zNear = 0.1f;
		pNodeShared->camera.zFar = 100.0f;
		pNodeShared->compositorRadius = 0.5;
		pNodeShared->compositorCycleSkip = 8;

		const NodeHandle handle = RequestExternalNodeHandle(pNodeShared);
		pNodeCompositorData = &nodeCompositorData[handle];
		// do not clear since it is set data is prealloced
//		*pNodeCompositorData = (MxcNodeCompositorData){};
		pNodeContext = &nodeContexts[handle];
		*pNodeContext = (MxcNodeContext){};
		pNodeContext->compositorTimeline = compositorNodeContext.compositorTimeline;
		pNodeContext->type = MXC_NODE_TYPE_INTERPROCESS_VULKAN_EXPORTED;
		pNodeContext->swapType = MXC_SWAP_TYPE_SINGLE;
		pNodeContext->processId = nodeProcessId;
		pNodeContext->processHandle = nodeProcessHandle;
		pNodeContext->exportedExternalMemoryHandle = externalNodeMemoryHandle;
		pNodeContext->pExportedExternalMemory = pExternalNodeMemory;
		pNodeContext->pNodeShared = pNodeShared;

		printf("Exporting node handle %d\n", handle);
	}

	// Create node data
	{
		MidVkSemaphoreCreateInfo semaphoreCreateInfo = {
			.debugName = "NodeTimelineSemaphoreExport",
			.locality = VK_LOCALITY_INTERPROCESS_EXPORTED_READWRITE,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		};
		midVkCreateSemaphore(&semaphoreCreateInfo, &pNodeContext->nodeTimeline);

		// we aren't going to make framebuffers here, it needs to first extablish with the app to know what cateogry of framebuffer to make
		switch (pNodeContext->swapType) {
			case MXC_SWAP_TYPE_SINGLE:               {
				VkFramebufferTextureCreateInfo framebufferInfo = {
					.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1},
					.locality = VK_LOCALITY_INTERPROCESS_EXPORTED_READWRITE,
				};
				for (int i = 0; i < VK_SWAP_COUNT; ++i)
					mxcCreateSwap(NULL, &framebufferInfo, &pNodeContext->swaps[i]);
				break;
			}
			case MXC_SWAP_TYPE_TEXTURE_ARRAY: break;
			case MXC_SWAP_TYPE_DOUBLE_WIDE:   {
				VkFramebufferTextureCreateInfo framebufferInfo = {
					.extent = {DEFAULT_WIDTH * 2, DEFAULT_HEIGHT, 1},
					.locality = VK_LOCALITY_INTERPROCESS_EXPORTED_READWRITE,
				};
				for (int i = 0; i < VK_SWAP_COUNT; ++i)
					mxcCreateSwap(NULL, &framebufferInfo, &pNodeContext->swaps[i]);
				break;
				}
			default: PANIC("Swap Type not supported.");
		}

		const HANDLE currentHandle = GetCurrentProcess();
		for (int i = 0; i < VK_SWAP_COUNT; ++i) {
			WIN32_CHECK(DuplicateHandle(
						  currentHandle, pNodeContext->swaps[i].colorExternal.handle,
						  nodeProcessHandle, &pImportParam->framebufferHandles[i].color,
						  0, false, DUPLICATE_SAME_ACCESS),
					  "Duplicate glColor handle fail");
			WIN32_CHECK(DuplicateHandle(
						  currentHandle, pNodeContext->swaps[i].normalExternal.handle,
						  nodeProcessHandle, &pImportParam->framebufferHandles[i].normal,
						  0, false, DUPLICATE_SAME_ACCESS),
					  "Duplicate normal handle fail.");
			WIN32_CHECK(DuplicateHandle(
						  currentHandle, pNodeContext->swaps[i].gbufferExternal.handle,
						  nodeProcessHandle, &pImportParam->framebufferHandles[i].gbuffer,
						  0, false, DUPLICATE_SAME_ACCESS),
					  "Duplicate gbuffer handle fail.");
		}
//		WIN32_CHECK(DuplicateHandle(
//					  currentHandle, pNodeContext->nodeFenceExternal.handle,
//					  nodeProcessHandle, &pImportParam->nodeFenceHandle,
//					  0, false, DUPLICATE_SAME_ACCESS),
//				  "Duplicate nodeFenceHandle handle fail.");
		WIN32_CHECK(DuplicateHandle(
					  currentHandle, vkGetSemaphoreExternalHandle(pNodeContext->nodeTimeline),
					  nodeProcessHandle, &pImportParam->nodeTimelineHandle,
					  0, false, DUPLICATE_SAME_ACCESS),
				  "Duplicate nodeTimeline handle fail.");
		WIN32_CHECK(DuplicateHandle(
					  currentHandle, vkGetSemaphoreExternalHandle(compositorNodeContext.compositorTimeline),
					  nodeProcessHandle, &pImportParam->compositorTimelineHandle,
					  0, false, DUPLICATE_SAME_ACCESS),
				  "Duplicate compeTimeline handle fail.");

		pNodeCompositorData->rootPose.rotation = QuatFromEuler(pNodeCompositorData->rootPose.euler);\

		uint32_t graphicsQueueIndex = vk.context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index;

		for (int i = 0; i < VK_SWAP_COUNT; ++i) {

			pNodeCompositorData->framebuffers[i].color = pNodeContext->swaps[i].color.image;
			pNodeCompositorData->framebuffers[i].normal = pNodeContext->swaps[i].normal.image;
			pNodeCompositorData->framebuffers[i].gBuffer = pNodeContext->swaps[i].gbuffer.image;
			pNodeCompositorData->framebuffers[i].colorView = pNodeContext->swaps[i].color.view;
			pNodeCompositorData->framebuffers[i].normalView = pNodeContext->swaps[i].normal.view;
			pNodeCompositorData->framebuffers[i].gBufferView = pNodeContext->swaps[i].gbuffer.view;

#define ACQUIRE_BARRIER                                      \
	.srcStageMask = VK_PIPELINE_STAGE_2_NONE,                \
	.srcAccessMask = VK_ACCESS_2_NONE,                       \
	.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, \
	.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,            \
	.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,   \
	.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,   \
	.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,         \
	.dstQueueFamilyIndex = graphicsQueueIndex,               \
	.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE
			pNodeCompositorData->framebuffers[i].acquireBarriers[0] = (VkImageMemoryBarrier2){
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = pNodeCompositorData->framebuffers[i].color,
				ACQUIRE_BARRIER,
			};
			pNodeCompositorData->framebuffers[i].acquireBarriers[1] = (VkImageMemoryBarrier2){
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = pNodeCompositorData->framebuffers[i].normal,
				ACQUIRE_BARRIER,
			};
			pNodeCompositorData->framebuffers[i].acquireBarriers[2] = (VkImageMemoryBarrier2){
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = pNodeCompositorData->framebuffers[i].gBuffer,
				ACQUIRE_BARRIER,
			};
#undef ACQUIRE_BARRIER

#define RELEASE_BARRIER                                      \
	.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, \
	.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT,            \
	.dstStageMask = VK_PIPELINE_STAGE_2_NONE,                \
	.dstAccessMask = VK_ACCESS_2_NONE,                       \
	.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,   \
	.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,                    \
	.srcQueueFamilyIndex = graphicsQueueIndex,               \
	.dstQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,         \
	.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE
			pNodeCompositorData->framebuffers[i].releaseBarriers[0] = (VkImageMemoryBarrier2){
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = pNodeCompositorData->framebuffers[i].color,
				RELEASE_BARRIER,
			};
			pNodeCompositorData->framebuffers[i].releaseBarriers[1] = (VkImageMemoryBarrier2){
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = pNodeCompositorData->framebuffers[i].normal,
				RELEASE_BARRIER,
			};
			pNodeCompositorData->framebuffers[i].releaseBarriers[2] = (VkImageMemoryBarrier2){
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = pNodeCompositorData->framebuffers[i].gBuffer,
				RELEASE_BARRIER,
			};
#undef RELEASE_BARRIER
		}
	}

	// Send shared memory handle
	{
		HANDLE duplicatedExternalNodeMemoryHandle;
		WIN32_CHECK(DuplicateHandle(
						GetCurrentProcess(), pNodeContext->exportedExternalMemoryHandle,
						nodeProcessHandle, &duplicatedExternalNodeMemoryHandle,
						0, false, DUPLICATE_SAME_ACCESS),
					"Duplicate sharedMemory handle fail.");
		printf("Sending duplicatedExternalNodeMemoryHandle: %p Size: %llu\n", duplicatedExternalNodeMemoryHandle, sizeof(HANDLE));
		int sendResult = send(clientSocket, (const char*)&duplicatedExternalNodeMemoryHandle, sizeof(HANDLE), 0);
		WSA_CHECK(sendResult == SOCKET_ERROR || sendResult == 0, "Send shared memory handle failed");
		printf("Process Node Export Success.\n");
		goto ExitSuccess;
	}

Exit:
	if (pImportParam != NULL) {
		for (int i = 0; i < VK_SWAP_COUNT; ++i) {
			if (pImportParam->framebufferHandles[i].color != INVALID_HANDLE_VALUE)
				CloseHandle(pImportParam->framebufferHandles[i].color);
			if (pImportParam->framebufferHandles[i].normal != INVALID_HANDLE_VALUE)
				CloseHandle(pImportParam->framebufferHandles[i].normal);
			if (pImportParam->framebufferHandles[i].gbuffer != INVALID_HANDLE_VALUE)
				CloseHandle(pImportParam->framebufferHandles[i].gbuffer);
		}
//		if (pImportParam->nodeFenceHandle != INVALID_HANDLE_VALUE)
//			CloseHandle(pImportParam->nodeFenceHandle);
		if (pImportParam->nodeTimelineHandle != INVALID_HANDLE_VALUE)
			CloseHandle(pImportParam->nodeTimelineHandle);
		if (pImportParam->compositorTimelineHandle != INVALID_HANDLE_VALUE)
			CloseHandle(pImportParam->compositorTimelineHandle);
	}
	if (pExternalNodeMemory != NULL)
		UnmapViewOfFile(pExternalNodeMemory);
	if (externalNodeMemoryHandle != INVALID_HANDLE_VALUE)
		CloseHandle(externalNodeMemoryHandle);
	if (nodeProcessHandle != INVALID_HANDLE_VALUE)
		CloseHandle(nodeProcessHandle);
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
	WSA_CHECK(bind(ipcServer.listenSocket, (struct sockaddr*)&address, sizeof(address)), "Bind failed");
	WSA_CHECK(listen(ipcServer.listenSocket, SOMAXCONN), "Listen failed");

	while (isRunning) {
		InterprocessServerAcceptNodeConnection();
	}

Exit:
	if (ipcServer.listenSocket != INVALID_SOCKET) {
		closesocket(ipcServer.listenSocket);
	}
	unlink(SOCKET_PATH);
	WSACleanup();
	return NULL;
}
void mxcInitializeInterprocessServer()
{
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	printf("Min size of shared memory. Allocation granularity: %lu\n", systemInfo.dwAllocationGranularity);

	ipcServer.listenSocket = INVALID_SOCKET;
	CHECK(pthread_create(&ipcServer.thread, NULL, RunInterProcessServer, NULL), "IPC server pipe creation Fail!");
}
void mxcShutdownInterprocessServer()
{
	if (ipcServer.listenSocket != INVALID_SOCKET) {
		closesocket(ipcServer.listenSocket);
	}
	unlink(SOCKET_PATH);
	WSACleanup();
}

void mxcConnectInterprocessNode(bool createTestNode)
{
	if (pImportedExternalMemory != NULL && importedExternalMemoryHandle != NULL) {
		printf("IPC already connected, skipping.\n");
		return;
	}

	printf("Connecting on: '%s'\n", SOCKET_PATH);
	MxcNodeContext*        pNodeContext = NULL;
	MxcImportParam*        pImportParam = NULL;
	MxcNodeShared*         pNodeShared = NULL;
	MxcExternalNodeMemory* pExternalNodeMemory = NULL;
	SOCKET                 clientSocket = INVALID_SOCKET;
	HANDLE                 externalNodeMemoryHandle = INVALID_HANDLE_VALUE;

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

		pImportParam = &pExternalNodeMemory->importParam;
		pNodeShared = &pExternalNodeMemory->shared;
	}

	if (!createTestNode)
		goto ExitSuccess;

	// Request and setup handle data
	{
		NodeHandle handle = RequestExternalNodeHandle(pNodeShared);
		pNodeContext = &nodeContexts[handle];
		*pNodeContext = (MxcNodeContext){};
		pNodeContext->type = MXC_NODE_TYPE_INTERPROCESS_VULKAN_IMPORTED;
		pNodeContext->pNodeShared = pNodeShared;
		printf("Importing node handle %d\n", handle);
	}

	// Create node data
	{
		MidVkSemaphoreCreateInfo compTimelineCreateInfo = {
			.debugName = "CompositorTimelineSemaphoreImport",
			.locality = VK_LOCALITY_INTERPROCESS_IMPORTED_READONLY,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			.importHandle = pImportParam->compositorTimelineHandle,
		};
		midVkCreateSemaphore(&compTimelineCreateInfo, &pNodeContext->compositorTimeline);
		MidVkSemaphoreCreateInfo nodeTimelineCreateInfo = {
			.debugName = "NodeTimelineSemaphoreImport",
			.locality = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			.importHandle = pImportParam->nodeTimelineHandle,
		};
		midVkCreateSemaphore(&nodeTimelineCreateInfo, &pNodeContext->nodeTimeline);

		VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = vk.context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
		};
		VK_CHECK(vkCreateCommandPool(vk.context.device, &graphicsCommandPoolCreateInfo, VK_ALLOC, &pNodeContext->pool));
		VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = pNodeContext->pool,
			.commandBufferCount = 1,
		};
		VK_CHECK(vkAllocateCommandBuffers(vk.context.device, &commandBufferAllocateInfo, &pNodeContext->cmd));
		vkSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)pNodeContext->cmd, "TestNode");

		MxcSwap* pFramebufferTextures = pNodeContext->swaps;
		for (int i = 0; i < VK_SWAP_COUNT; ++i) {
			VkTextureCreateInfo colorCreateInfo = {
				.debugName = "ImportedColorFramebuffer",
				.pImageCreateInfo = &(VkImageCreateInfo) {
					.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.pNext = VK_EXTERNAL_IMAGE_CREATE_INFO_PLATFORM,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = VK_BASIC_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_COLOR_INDEX],
					.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = VK_BASIC_PASS_USAGES[MIDVK_PASS_ATTACHMENT_COLOR_INDEX],
				},
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.locality = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
				.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
				.importHandle = pImportParam->framebufferHandles[i].color,
			};
			vkCreateTexture(&colorCreateInfo, &pFramebufferTextures[i].color);
			VkTextureCreateInfo normalCreateInfo = {
				.debugName = "ImportedNormalFramebuffer",
				.pImageCreateInfo = &(VkImageCreateInfo) {
					.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.pNext = VK_EXTERNAL_IMAGE_CREATE_INFO_PLATFORM,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = VK_BASIC_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_NORMAL_INDEX],
					.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = VK_BASIC_PASS_USAGES[MIDVK_PASS_ATTACHMENT_NORMAL_INDEX],
				},
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.locality = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
				.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
				.importHandle = pImportParam->framebufferHandles[i].normal,
			};
			vkCreateTexture(&normalCreateInfo, &pFramebufferTextures[i].normal);
			VkTextureCreateInfo gbufferCreateInfo = {
				.debugName = "ImportedGBufferFramebuffer",
				.pImageCreateInfo = &(VkImageCreateInfo) {
					.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.pNext = VK_EXTERNAL_IMAGE_CREATE_INFO_PLATFORM,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = MXC_NODE_GBUFFER_FORMAT,
					.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
					.mipLevels = MXC_NODE_GBUFFER_LEVELS,
					.arrayLayers = 1,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = MXC_NODE_GBUFFER_USAGE,
				},
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.locality = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
				.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
				.importHandle = pImportParam->framebufferHandles[i].gbuffer,
			};
			vkCreateTexture(&gbufferCreateInfo, &pFramebufferTextures[i].gbuffer);
			// Depth is not shared over IPC.
			VkTextureCreateInfo depthCreateInfo = {
				.debugName = "ImportedDepthFramebuffer",
				.pImageCreateInfo = &(VkImageCreateInfo) {
					.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = VK_BASIC_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_DEPTH_INDEX],
					.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = VK_BASIC_PASS_USAGES[MIDVK_PASS_ATTACHMENT_DEPTH_INDEX],
				},
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.locality = VK_LOCALITY_CONTEXT,
			};
			vkCreateTexture(&depthCreateInfo, &pFramebufferTextures[i].depth);
		}
	}

	// Start node thread
	{
		__atomic_thread_fence(__ATOMIC_RELEASE);
		CHECK(pthread_create(&pNodeContext->threadId, NULL, (void* (*)(void*))mxcTestNodeThread, pNodeContext), "Node Process Import thread creation failed!");
		printf("Node Request Process Import Success.\n");
		goto ExitSuccess;
	}

Exit:
	// do cleanup
	// need a NodeContext cleanup method
ExitSuccess:
	if (clientSocket != INVALID_SOCKET)
		closesocket(clientSocket);
	WSACleanup();
}
void mxcShutdownInterprocessNode()
{
	for (int i = 0; i < nodeCount; ++i) {
		// make another queue to evade ptr?
		mxcIpcFuncEnqueue(&activeNodesShared[i]->ipcFuncQueue, MXC_INTERPROCESS_TARGET_NODE_CLOSED);
	}
}

//
//// IPC

int mxcIpcFuncEnqueue(MxcRingBuffer* pBuffer, const MxcIpcFunc target)
{
	__atomic_thread_fence(__ATOMIC_ACQUIRE);
	const MxcRingBufferHandle head = pBuffer->head;
	const MxcRingBufferHandle tail = pBuffer->tail;
	if (head + 1 == tail) {
		fprintf(stderr, "Ring buffer wrapped!");
		return 1;
	}
	pBuffer->targets[head] = target;
	pBuffer->head = (head + 1) % MXC_RING_BUFFER_CAPACITY;
	__atomic_thread_fence(__ATOMIC_RELEASE);
	return 0;
}

int mxcIpcDequeue(MxcRingBuffer* pBuffer, const NodeHandle nodeHandle)
{
	__atomic_thread_fence(__ATOMIC_ACQUIRE);
	const MxcRingBufferHandle head = pBuffer->head;
	const MxcRingBufferHandle tail = pBuffer->tail;
	if (head == tail)
		return 1;

	printf("IPC Polling %d %d...\n", head, tail);
	MxcIpcFunc target = (MxcIpcFunc)(pBuffer->targets[tail]);
	pBuffer->tail = (tail + 1) % MXC_RING_BUFFER_CAPACITY;
	__atomic_thread_fence(__ATOMIC_RELEASE);

	printf("Calling IPC Target %d...\n", target);
	MXC_IPC_FUNCS[target](nodeHandle);
	return 0;
}

static void ipcFuncNodeClosed(const NodeHandle handle)
{
	printf("Closing %d\n", handle);
	CleanupNode(handle);
}
static void ipcFuncClaimSwap(const NodeHandle handle)
{
	printf("Claiming swap for %d\n", handle);

	MxcNodeContext* pNodeContext = &nodeContexts[handle];
	MxcNodeShared* pNodeShared = activeNodesShared[handle];

	MxcSwapInfo info = {
		.yScale = MXC_SWAP_SCALE_FULL,
		.xScale = MXC_SWAP_SCALE_FULL,
		.type = MXC_SWAP_TYPE_SINGLE,
	};
	int newSwap = mxcClaimSwap(&info);
//	if (newSwap != -1) {
//		pNodeContext->pExportedExternalMemory->importParam.
//
//	}
}
const MxcIpcFuncPtr MXC_IPC_FUNCS[] = {
	[MXC_INTERPROCESS_TARGET_NODE_CLOSED] = (MxcIpcFuncPtr const)ipcFuncNodeClosed,
	[MXC_INTERPROCESS_TARGET_CLAIM_SWAP] = (MxcIpcFuncPtr const)ipcFuncClaimSwap,
};

