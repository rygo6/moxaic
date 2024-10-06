#include "node.h"
#include "test_node.h"
#include "mid_vulkan.h"

#define WIN32_LEAN_AND_MEAN
#undef UNICODE
#include <winsock2.h>
#include <afunix.h>
#include <stdio.h>

#include <pthread.h>
#include <assert.h>

size_t                   nodeCount = 0;
MxcNodeContext           nodeContexts[MXC_NODE_CAPACITY] = {};
MxcNodeShared            nodesShared[MXC_NODE_CAPACITY] = {};
MxcNodeCompositorData    nodeCompositorData[MXC_NODE_CAPACITY] = {};
MxcNodeShared*           activeNodesShared[MXC_NODE_CAPACITY] = {};
MxcCompositorNodeContext compositorNodeContext = {};

HANDLE                 importedExternalMemoryHandle = NULL;
MxcExternalNodeMemory* pImportedExternalMemory = NULL;

// this should become a midvk construct
size_t                     submitNodeQueueStart = 0;
size_t                     submitNodeQueueEnd = 0;
MxcQueuedNodeCommandBuffer submitNodeQueue[MXC_NODE_CAPACITY] = {};

void mxcRequestAndRunCompositorNodeThread(const VkSurfaceKHR surface, void* (*runFunc)(const struct MxcCompositorNodeContext*))
{
	const MidVkSemaphoreCreateInfo semaphoreCreateInfo = {
		.debugName = "CompTimelineSemaphore",
		.locality = MID_LOCALITY_INTERPROCESS_EXPORTED_READONLY,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE};
	midVkCreateSemaphore(&semaphoreCreateInfo, &compositorNodeContext.compTimeline);
	midVkCreateSwap(surface, VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS, &compositorNodeContext.swap);

	const VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = midVk.context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
	};
	MIDVK_REQUIRE(vkCreateCommandPool(midVk.context.device, &graphicsCommandPoolCreateInfo, MIDVK_ALLOC, &compositorNodeContext.pool));
	const VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = compositorNodeContext.pool,
		.commandBufferCount = 1,
	};
	MIDVK_REQUIRE(vkAllocateCommandBuffers(midVk.context.device, &commandBufferAllocateInfo, &compositorNodeContext.cmd));
	midVkSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)compositorNodeContext.cmd, "CompCmd");

	int result = pthread_create(&compositorNodeContext.threadId, NULL, (void* (*)(void*))runFunc, &compositorNodeContext);
	REQUIRE(result == 0, "Comp Node thread creation failed!");
	printf("Request and Run CompNode Thread Success.\n");
}

static NodeHandle RequestLocalNodeHandle()
{
	NodeHandle handle = __atomic_fetch_add(&nodeCount, 1, __ATOMIC_RELEASE);
	nodesShared[handle] = (MxcNodeShared){};
	activeNodesShared[handle] = &nodesShared[handle];
	return handle;
}
NodeHandle RequestExternalNodeHandle(MxcNodeShared* pNodeShared)
{
	NodeHandle handle = __atomic_fetch_add(&nodeCount, 1, __ATOMIC_RELEASE);
	activeNodesShared[handle] = pNodeShared;
	return handle;
}

#define CLOSE_HANDLE(_handle)                                                     \
	if (!CloseHandle(_handle)) {                                                  \
		DWORD dwError = GetLastError();                                           \
		printf("Could not close (%s) object handle (%lu).\n", #_handle, dwError); \
	}
static int ReleaseNode(NodeHandle handle)
{
	MxcNodeContext* pNodeContext = &nodeContexts[handle];
	int newCount = __atomic_sub_fetch(&nodeCount, 1, __ATOMIC_RELEASE);
	printf("Releasing Node %d. Count %d.\n", handle, newCount);

//	{ as long as mxcInterprocessQueuePoll is called after waiting on MXC_CYCLE_PROCESS_INPUT we don't have to wait here
//		// we need to wait one cycle to get the compositor cmd buffer cleared
//		uint64_t compBaseCycleValue = compositorNodeContext.compBaseCycleValue - (compositorNodeContext.compBaseCycleValue % MXC_CYCLE_COUNT);
//		vkmTimelineWait(midVk.context.device, compBaseCycleValue + MXC_CYCLE_COUNT, compositorNodeContext.compTimeline);
//	}

	// Close external handles
	switch (pNodeContext->type) {
		case MXC_NODE_TYPE_THREAD:
			vkFreeCommandBuffers(midVk.context.device, pNodeContext->pool, 1, &pNodeContext->cmd);
			vkDestroyCommandPool(midVk.context.device, pNodeContext->pool, MIDVK_ALLOC);
			int result = pthread_join(pNodeContext->threadId, NULL);
			if (result != 0) {
				perror("Thread join failed");
			}
			break;
		case MXC_NODE_TYPE_INTERPROCESS_VULKAN_EXPORTED:
			for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
				CLOSE_HANDLE(midVkGetMemoryExternalHandle(pNodeContext->vkNodeFramebufferTextures[i].color.memory));
				CLOSE_HANDLE(midVkGetMemoryExternalHandle(pNodeContext->vkNodeFramebufferTextures[i].normal.memory));
				CLOSE_HANDLE(midVkGetMemoryExternalHandle(pNodeContext->vkNodeFramebufferTextures[i].gbuffer.memory));
			}
			CLOSE_HANDLE(midVkGetSemaphoreExternalHandle(pNodeContext->vkNodeTimeline));
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
	vkDestroySemaphore(midVk.context.device, pNodeContext->vkNodeTimeline, MIDVK_ALLOC);

	// Clear compositor data
	const VkWriteDescriptorSet writeSets[] = {
		(VkWriteDescriptorSet) {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = nodeCompositorData[handle].set,
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &(const VkDescriptorImageInfo){
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
			.pImageInfo = &(const VkDescriptorImageInfo){
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
			.pImageInfo = &(const VkDescriptorImageInfo){
				.imageView = VK_NULL_HANDLE,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			},
		}
	};
	vkUpdateDescriptorSets(midVk.context.device, COUNT(writeSets), writeSets, 0, NULL);

	// destroy images... this will eventually just return back to some pool
	for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
		vkDestroyImageView(midVk.context.device, pNodeContext->vkNodeFramebufferTextures[i].color.view, MIDVK_ALLOC);
		vkDestroyImage(midVk.context.device, pNodeContext->vkNodeFramebufferTextures[i].color.image, MIDVK_ALLOC);
		vkFreeMemory(midVk.context.device, pNodeContext->vkNodeFramebufferTextures[i].color.memory, MIDVK_ALLOC);

		vkDestroyImageView(midVk.context.device, pNodeContext->vkNodeFramebufferTextures[i].normal.view, MIDVK_ALLOC);
		vkDestroyImage(midVk.context.device, pNodeContext->vkNodeFramebufferTextures[i].normal.image, MIDVK_ALLOC);
		vkFreeMemory(midVk.context.device, pNodeContext->vkNodeFramebufferTextures[i].normal.memory, MIDVK_ALLOC);

		vkDestroyImageView(midVk.context.device, pNodeContext->vkNodeFramebufferTextures[i].depth.view, MIDVK_ALLOC);
		vkDestroyImage(midVk.context.device, pNodeContext->vkNodeFramebufferTextures[i].depth.image, MIDVK_ALLOC);
		vkFreeMemory(midVk.context.device, pNodeContext->vkNodeFramebufferTextures[i].depth.memory, MIDVK_ALLOC);

		vkDestroyImageView(midVk.context.device, pNodeContext->vkNodeFramebufferTextures[i].gbuffer.view, MIDVK_ALLOC);
		vkDestroyImage(midVk.context.device, pNodeContext->vkNodeFramebufferTextures[i].gbuffer.image, MIDVK_ALLOC);
		vkFreeMemory(midVk.context.device, pNodeContext->vkNodeFramebufferTextures[i].gbuffer.memory, MIDVK_ALLOC);
	}

	return 0;
}

void mxcRequestNodeThread(void* (*runFunc)(const struct MxcNodeContext*), NodeHandle* pNodeHandle)
{
	printf("Requesting Node Thread.\n");
	const NodeHandle handle = RequestLocalNodeHandle();

	MxcNodeContext* pNodeContext = &nodeContexts[handle];
	*pNodeContext = (MxcNodeContext){};
	pNodeContext->vkCompositorTimeline = compositorNodeContext.compTimeline;
	pNodeContext->type = MXC_NODE_TYPE_THREAD;

	// maybe have the thread allocate this and submit it once ready to get rid of < 1 check
	// then could also get rid of pNodeContext->pNodeShared?
	// no how would something on another process do that
	MxcNodeShared* pNodeShared = &nodesShared[handle];
	*pNodeShared = (MxcNodeShared){};
	pNodeContext->pNodeShared = pNodeShared;
	pNodeShared->rootPose.rotation = QuatFromEuler(pNodeShared->rootPose.euler);
	pNodeShared->compositorRadius = 0.5;
	pNodeShared->compositorCycleSkip = 16;

	const MidVkFenceCreateInfo nodeFenceCreateInfo = {
		.debugName = "NodeFence",
		.locality = MID_LOCALITY_CONTEXT,
	};
	midVkCreateFence(&nodeFenceCreateInfo, &pNodeContext->vkNodeFence);
	const MidVkSemaphoreCreateInfo semaphoreCreateInfo = {
		.debugName = "NodeTimelineSemaphore",
		.locality = MID_LOCALITY_CONTEXT,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
	};
	midVkCreateSemaphore(&semaphoreCreateInfo, &pNodeContext->vkNodeTimeline);

	const VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = midVk.context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
	};
	MIDVK_REQUIRE(vkCreateCommandPool(midVk.context.device, &graphicsCommandPoolCreateInfo, MIDVK_ALLOC, &pNodeContext->pool));
	const VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = pNodeContext->pool,
		.commandBufferCount = 1,
	};
	MIDVK_REQUIRE(vkAllocateCommandBuffers(midVk.context.device, &commandBufferAllocateInfo, &pNodeContext->cmd));
	midVkSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)pNodeContext->cmd, "TestNode");

	mxcCreateNodeFramebuffer(MID_LOCALITY_CONTEXT, pNodeContext->vkNodeFramebufferTextures);

	MxcNodeCompositorData* pNodeCompositorData = &nodeCompositorData[handle];
	// do not clear since it is set data is prealloced
//	*pNodeCompositorData = (MxcNodeCompositorData){};
	pNodeCompositorData->rootPose.rotation = QuatFromEuler(pNodeCompositorData->rootPose.euler);
	for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
		pNodeCompositorData->framebuffers[i].color = nodeContexts[handle].vkNodeFramebufferTextures[i].color.image;
		pNodeCompositorData->framebuffers[i].normal = nodeContexts[handle].vkNodeFramebufferTextures[i].normal.image;
		pNodeCompositorData->framebuffers[i].gBuffer = nodeContexts[handle].vkNodeFramebufferTextures[i].gbuffer.image;
		pNodeCompositorData->framebuffers[i].colorView = nodeContexts[handle].vkNodeFramebufferTextures[i].color.view;
		pNodeCompositorData->framebuffers[i].normalView = nodeContexts[handle].vkNodeFramebufferTextures[i].normal.view;
		pNodeCompositorData->framebuffers[i].gBufferView = nodeContexts[handle].vkNodeFramebufferTextures[i].gbuffer.view;
		pNodeCompositorData->framebuffers[i].acquireBarriers[0] = (VkImageMemoryBarrier2){
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT | VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			MIDVK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			.image = pNodeCompositorData->framebuffers[i].color,
			MIDVK_COLOR_SUBRESOURCE_RANGE,
		};
		pNodeCompositorData->framebuffers[i].acquireBarriers[1] = (VkImageMemoryBarrier2){
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT | VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			MIDVK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			.image = pNodeCompositorData->framebuffers[i].normal,
			MIDVK_COLOR_SUBRESOURCE_RANGE,
		};
		pNodeCompositorData->framebuffers[i].acquireBarriers[2] = (VkImageMemoryBarrier2){
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT | VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			MIDVK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			.image = pNodeCompositorData->framebuffers[i].gBuffer,
			MIDVK_COLOR_SUBRESOURCE_RANGE,
		};
	}

	*pNodeHandle = handle;

	int result = pthread_create(&nodeContexts[handle].threadId, NULL, (void* (*)(void*))runFunc, pNodeContext);
	REQUIRE(result == 0, "Node thread creation failed!");

	printf("Request Node Thread Success. Handle: %d\n", handle);
	// todo this needs error handling
}

//
/// Node Render
void mxcCreateNodeRenderPass()
{
	const VkRenderPassCreateInfo2 renderPassCreateInfo2 = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
		.attachmentCount = 3,
		.pAttachments = (const VkAttachmentDescription2[]){
			[MIDVK_PASS_ATTACHMENT_COLOR_INDEX] = {
				.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
				.format = MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_COLOR_INDEX],
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
				.format = MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_NORMAL_INDEX],
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
				.format = MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_DEPTH_INDEX],
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
		.pSubpasses = (const VkSubpassDescription2[]){
			{
				.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.colorAttachmentCount = 2,
				.pColorAttachments = (const VkAttachmentReference2[]){
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
				.pDepthStencilAttachment = &(const VkAttachmentReference2){
					.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
					.attachment = MIDVK_PASS_ATTACHMENT_DEPTH_INDEX,
					.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				},
			},
		},
		.dependencyCount = 2,
		.pDependencies = (const VkSubpassDependency2[]){
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
	MIDVK_REQUIRE(vkCreateRenderPass2(midVk.context.device, &renderPassCreateInfo2, MIDVK_ALLOC, &midVk.context.nodeRenderPass));
	midVkSetDebugName(VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)midVk.context.nodeRenderPass, "NodeRenderPass");
}
void mxcCreateNodeFramebuffer(const MidLocality locality, MxcNodeVkFramebufferTexture* pNodeFramebufferTextures)
{
	for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
		const VkmTextureCreateInfo colorCreateInfo = {
			.debugName = "ExportedColorFramebuffer",
			.imageCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.pNext = MID_LOCALITY_INTERPROCESS(locality) ? MIDVK_EXTERNAL_IMAGE_CREATE_INFO : NULL,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_COLOR_INDEX],
				.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
				.mipLevels = 1,
				.arrayLayers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.usage = MIDVK_PASS_USAGES[MIDVK_PASS_ATTACHMENT_COLOR_INDEX],
			},
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.locality = locality,
		};
		midvkCreateTexture(&colorCreateInfo, &pNodeFramebufferTextures[i].color);
		const VkmTextureCreateInfo normalCreateInfo = {
			.debugName = "ExportedNormalFramebuffer",
			.imageCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.pNext = MID_LOCALITY_INTERPROCESS(locality) ? MIDVK_EXTERNAL_IMAGE_CREATE_INFO : NULL,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_NORMAL_INDEX],
				.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
				.mipLevels = 1,
				.arrayLayers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.usage = MIDVK_PASS_USAGES[MIDVK_PASS_ATTACHMENT_NORMAL_INDEX],
			},
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.locality = locality,
		};
		midvkCreateTexture(&normalCreateInfo, &pNodeFramebufferTextures[i].normal);
		const VkmTextureCreateInfo gbufferCreateInfo = {
			.debugName = "ExportedGBufferFramebuffer",
			.imageCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.pNext = MID_LOCALITY_INTERPROCESS(locality) ? MIDVK_EXTERNAL_IMAGE_CREATE_INFO : NULL,
				.imageType = VK_IMAGE_TYPE_2D,
				.format = MXC_NODE_GBUFFER_FORMAT,
				.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
				.mipLevels = MXC_NODE_GBUFFER_LEVELS,
				.arrayLayers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.usage = MXC_NODE_GBUFFER_USAGE,
			},
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.locality = locality,
		};
		midvkCreateTexture(&gbufferCreateInfo, &pNodeFramebufferTextures[i].gbuffer);

		// Depth is not shared over IPC.
		if (locality == MID_LOCALITY_INTERPROCESS_EXPORTED_READWRITE || locality == MID_LOCALITY_INTERPROCESS_EXPORTED_READONLY) {
			// we need to transition these out of undefined initially because the transition in the other process won't update layout to avoid initial validation error on transition
			VkCommandBuffer             cmd = midVkBeginImmediateTransferCommandBuffer();
			const VkImageMemoryBarrier2 interProcessBarriers[] = {
				VKM_COLOR_IMAGE_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMG_BARRIER_EXTERNAL_ACQUIRE_SHADER_READ, pNodeFramebufferTextures[i].color.image),
				VKM_COLOR_IMAGE_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMG_BARRIER_EXTERNAL_ACQUIRE_SHADER_READ, pNodeFramebufferTextures[i].normal.image),
				VKM_COLOR_IMAGE_BARRIER_MIPS(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMG_BARRIER_EXTERNAL_ACQUIRE_SHADER_READ, pNodeFramebufferTextures[i].gbuffer.image, 0, MXC_NODE_GBUFFER_LEVELS),
			};
			MIDVK_DEVICE_FUNC(CmdPipelineBarrier2);
			CmdPipelineImageBarriers2(cmd, COUNT(interProcessBarriers), interProcessBarriers);
			midVkEndImmediateTransferCommandBuffer(cmd);
			continue;
		}
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
		midvkCreateTexture(&depthCreateInfo, &pNodeFramebufferTextures[i].depth);
	}
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

// Checks error code. Expects 0 for success.
#define ERR_CHECK(_command, _message)                               \
	{                                                               \
		int _result = (_command);                                   \
		if (__builtin_expect(!!(_result), 0)) {                     \
			fprintf(stderr, "%s: %ld\n", _message, GetLastError()); \
			goto Exit;                                              \
		}                                                           \
	}
#define WSA_ERR_CHECK(_command, _message)                             \
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
	WSA_ERR_CHECK(clientSocket == INVALID_SOCKET, "Accept failed");
	printf("Accepted Connection.\n");

	// Receive Node Ack Message
	{
		char buffer[sizeof(nodeIPCAckMessage)] = {};
		int  receiveLength = recv(clientSocket, buffer, sizeof(nodeIPCAckMessage), 0);
		WSA_ERR_CHECK(receiveLength == SOCKET_ERROR || receiveLength == 0, "Recv nodeIPCAckMessage failed");
		printf("Received node ack: %s Size: %d\n", buffer, receiveLength);
		ERR_CHECK(strcmp(buffer, nodeIPCAckMessage), "Unexpected node message");
	}

	// Send Server Ack message
	{
		printf("Sending server ack: %s size: %llu\n", serverIPCAckMessage, strlen(serverIPCAckMessage));
		WSA_ERR_CHECK(send(clientSocket, serverIPCAckMessage, strlen(serverIPCAckMessage), 0) == SOCKET_ERROR, "Send failed");
	}

	// Receive Node Process Handle
	{
		int receiveLength = recv(clientSocket, (char*)&nodeProcessId, sizeof(DWORD), 0);
		WSA_ERR_CHECK(receiveLength == SOCKET_ERROR || receiveLength == 0, "Recv node process id failed");
		printf("Received node process id: %lu Size: %d\n", nodeProcessId, receiveLength);
		ERR_CHECK(nodeProcessId == 0, "Invalid node process id");
	}

	// Create shared memory
	{
		nodeProcessHandle = OpenProcess(PROCESS_DUP_HANDLE, FALSE, nodeProcessId);
		ERR_CHECK(nodeProcessHandle == INVALID_HANDLE_VALUE, "Duplicate process handle failed");
		externalNodeMemoryHandle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(MxcExternalNodeMemory), NULL);
		ERR_CHECK(externalNodeMemoryHandle == NULL, "Could not create file mapping object");
		pExternalNodeMemory = MapViewOfFile(externalNodeMemoryHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MxcExternalNodeMemory));
		ERR_CHECK(pExternalNodeMemory == NULL, "Could not map view of file");
		*pExternalNodeMemory = (MxcExternalNodeMemory){};
	}

	// Request and setup handle data
	{
		pImportParam = &pExternalNodeMemory->importParam;
		pNodeShared = &pExternalNodeMemory->shared;
		pNodeShared->rootPose.rotation = QuatFromEuler(pNodeShared->rootPose.euler);
		pNodeShared->compositorRadius = 0.5;
		pNodeShared->compositorCycleSkip = 16;

		const NodeHandle handle = RequestExternalNodeHandle(pNodeShared);
		pNodeCompositorData = &nodeCompositorData[handle];
		// do not clear since it is set data is prealloced
//		*pNodeCompositorData = (MxcNodeCompositorData){};
		pNodeContext = &nodeContexts[handle];
		*pNodeContext = (MxcNodeContext){};
		pNodeContext->vkCompositorTimeline = compositorNodeContext.compTimeline;
		pNodeContext->type = MXC_NODE_TYPE_INTERPROCESS_VULKAN_EXPORTED;
		pNodeContext->processId = nodeProcessId;
		pNodeContext->processHandle = nodeProcessHandle;
		pNodeContext->exportedExternalMemoryHandle = externalNodeMemoryHandle;
		pNodeContext->pExportedExternalMemory = pExternalNodeMemory;
		pNodeContext->pNodeShared = pNodeShared;

		printf("Exporting node handle %d\n", handle);
	}

	// Create node data
	{
		const MidVkFenceCreateInfo nodeFenceCreateInfo = {
			.debugName = "NodeFenceExport",
			.locality = MID_LOCALITY_INTERPROCESS_EXPORTED_READWRITE,
		};
		midVkCreateFence(&nodeFenceCreateInfo, &pNodeContext->vkNodeFence);
		const MidVkSemaphoreCreateInfo semaphoreCreateInfo = {
			.debugName = "NodeTimelineSemaphoreExport",
			.locality = MID_LOCALITY_INTERPROCESS_EXPORTED_READWRITE,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		};
		midVkCreateSemaphore(&semaphoreCreateInfo, &pNodeContext->vkNodeTimeline);

		mxcCreateNodeFramebuffer(MID_LOCALITY_INTERPROCESS_EXPORTED_READWRITE, pNodeContext->vkNodeFramebufferTextures);

		const HANDLE currentHandle = GetCurrentProcess();
		for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
			ERR_CHECK(!DuplicateHandle(currentHandle, midVkGetMemoryExternalHandle(pNodeContext->vkNodeFramebufferTextures[i].color.memory), nodeProcessHandle, &pImportParam->framebufferHandles[i].color, 0, false, DUPLICATE_SAME_ACCESS), "Duplicate color handle fail");
			ERR_CHECK(!DuplicateHandle(currentHandle, midVkGetMemoryExternalHandle(pNodeContext->vkNodeFramebufferTextures[i].normal.memory), nodeProcessHandle, &pImportParam->framebufferHandles[i].normal, 0, false, DUPLICATE_SAME_ACCESS), "Duplicate normal handle fail.");
			ERR_CHECK(!DuplicateHandle(currentHandle, midVkGetMemoryExternalHandle(pNodeContext->vkNodeFramebufferTextures[i].gbuffer.memory), nodeProcessHandle, &pImportParam->framebufferHandles[i].gbuffer, 0, false, DUPLICATE_SAME_ACCESS), "Duplicate gbuffer handle fail.");
		}
		ERR_CHECK(!DuplicateHandle(currentHandle, midVkGetFenceExternalHandle(pNodeContext->vkNodeFence), nodeProcessHandle, &pImportParam->nodeFenceHandle, 0, false, DUPLICATE_SAME_ACCESS), "Duplicate nodeFenceHandle handle fail.");
		ERR_CHECK(!DuplicateHandle(currentHandle, midVkGetSemaphoreExternalHandle(pNodeContext->vkNodeTimeline), nodeProcessHandle, &pImportParam->nodeTimelineHandle, 0, false, DUPLICATE_SAME_ACCESS), "Duplicate vkNodeTimeline handle fail.");
		ERR_CHECK(!DuplicateHandle(currentHandle, midVkGetSemaphoreExternalHandle(compositorNodeContext.compTimeline), nodeProcessHandle, &pImportParam->compTimelineHandle, 0, false, DUPLICATE_SAME_ACCESS), "Duplicate compeTimeline handle fail.");

		const uint32_t graphicsQueueIndex = midVk.context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index;
		pNodeCompositorData->rootPose.rotation = QuatFromEuler(pNodeCompositorData->rootPose.euler);
		for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
			pNodeCompositorData->framebuffers[i].color = pNodeContext->vkNodeFramebufferTextures[i].color.image;
			pNodeCompositorData->framebuffers[i].normal = pNodeContext->vkNodeFramebufferTextures[i].normal.image;
			pNodeCompositorData->framebuffers[i].gBuffer = pNodeContext->vkNodeFramebufferTextures[i].gbuffer.image;
			pNodeCompositorData->framebuffers[i].colorView = pNodeContext->vkNodeFramebufferTextures[i].color.view;
			pNodeCompositorData->framebuffers[i].normalView = pNodeContext->vkNodeFramebufferTextures[i].normal.view;
			pNodeCompositorData->framebuffers[i].gBufferView = pNodeContext->vkNodeFramebufferTextures[i].gbuffer.view;
			pNodeCompositorData->framebuffers[i].acquireBarriers[0] = (VkImageMemoryBarrier2){
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
				.dstQueueFamilyIndex = graphicsQueueIndex,
				.image = pNodeCompositorData->framebuffers[i].color,
				MIDVK_COLOR_SUBRESOURCE_RANGE};
			pNodeCompositorData->framebuffers[i].acquireBarriers[1] = (VkImageMemoryBarrier2){
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
				.dstQueueFamilyIndex = graphicsQueueIndex,
				.image = pNodeCompositorData->framebuffers[i].normal,
				MIDVK_COLOR_SUBRESOURCE_RANGE};
			pNodeCompositorData->framebuffers[i].acquireBarriers[2] = (VkImageMemoryBarrier2){
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
				.dstQueueFamilyIndex = graphicsQueueIndex,
				.image = pNodeCompositorData->framebuffers[i].gBuffer,
				MIDVK_COLOR_SUBRESOURCE_RANGE};
		}
	}

	// Send shared memory handle
	{
		HANDLE duplicatedExternalNodeMemoryHandle;
		ERR_CHECK(!DuplicateHandle(GetCurrentProcess(), pNodeContext->exportedExternalMemoryHandle, nodeProcessHandle, &duplicatedExternalNodeMemoryHandle, 0, false, DUPLICATE_SAME_ACCESS), "Duplicate sharedMemory handle fail.");
		printf("Sending duplicatedExternalNodeMemoryHandle: %p Size: %llu\n", duplicatedExternalNodeMemoryHandle, sizeof(HANDLE));
		WSA_ERR_CHECK(send(clientSocket, (const char*)&duplicatedExternalNodeMemoryHandle, sizeof(HANDLE), 0) == SOCKET_ERROR, "Send failed");
		printf("Process Node Export Success.\n");
		goto ExitSuccess;
	}

Exit:
	if (pImportParam != NULL) {
		for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
			if (pImportParam->framebufferHandles[i].color != INVALID_HANDLE_VALUE)
				CloseHandle(pImportParam->framebufferHandles[i].color);
			if (pImportParam->framebufferHandles[i].normal != INVALID_HANDLE_VALUE)
				CloseHandle(pImportParam->framebufferHandles[i].normal);
			if (pImportParam->framebufferHandles[i].gbuffer != INVALID_HANDLE_VALUE)
				CloseHandle(pImportParam->framebufferHandles[i].gbuffer);
		}
		if (pImportParam->nodeFenceHandle != INVALID_HANDLE_VALUE)
			CloseHandle(pImportParam->nodeFenceHandle);
		if (pImportParam->nodeTimelineHandle != INVALID_HANDLE_VALUE)
			CloseHandle(pImportParam->nodeTimelineHandle);
		if (pImportParam->compTimelineHandle != INVALID_HANDLE_VALUE)
			CloseHandle(pImportParam->compTimelineHandle);
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

	ERR_CHECK(strncpy_s(address.sun_path, sizeof address.sun_path, SOCKET_PATH, (sizeof SOCKET_PATH) - 1), "Address copy failed");
	ERR_CHECK(WSAStartup(MAKEWORD(2, 2), &wsaData), "WSAStartup failed");

	ipcServer.listenSocket = socket(AF_UNIX, SOCK_STREAM, 0);
	WSA_ERR_CHECK(ipcServer.listenSocket == INVALID_SOCKET, "Socket failed");
	WSA_ERR_CHECK(bind(ipcServer.listenSocket, (struct sockaddr*)&address, sizeof(address)), "Bind failed");
	WSA_ERR_CHECK(listen(ipcServer.listenSocket, SOMAXCONN), "Listen failed");

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
	REQUIRE_ERR(pthread_create(&ipcServer.thread, NULL, RunInterProcessServer, NULL), "IPC server pipe creation Fail!");
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
		ERR_CHECK(strncpy_s(address.sun_path, sizeof address.sun_path, SOCKET_PATH, (sizeof SOCKET_PATH) - 1), "Address copy failed");
		WSADATA wsaData = {};
		ERR_CHECK(WSAStartup(MAKEWORD(2, 2), &wsaData), "WSAStartup failed");
		clientSocket = socket(AF_UNIX, SOCK_STREAM, 0);
		WSA_ERR_CHECK(clientSocket == INVALID_SOCKET, "Socket creation failed");
		WSA_ERR_CHECK(connect(clientSocket, (struct sockaddr*)&address, sizeof(address)), "Connect failed");
		printf("Connected to server.\n");
	}

	// Send ack
	{
		printf("Sending node ack: %s size: %llu\n", nodeIPCAckMessage, strlen(nodeIPCAckMessage));
		int sendResult = send(clientSocket, nodeIPCAckMessage, (int)strlen(nodeIPCAckMessage), 0);
		WSA_ERR_CHECK(sendResult == SOCKET_ERROR, "Send node ack failed");
	}

	// Receive and check ack
	{
		char buffer[sizeof(serverIPCAckMessage)] = {};
		int  receiveLength = recv(clientSocket, buffer, sizeof(serverIPCAckMessage), 0);
		WSA_ERR_CHECK(receiveLength == SOCKET_ERROR || receiveLength == 0, "Recv compositor ack failed");
		printf("Received from server: %s size: %d\n", buffer, receiveLength);
		WSA_ERR_CHECK(strcmp(buffer, serverIPCAckMessage), "Unexpected compositor ack");
	}

	// Send process id
	{
		const DWORD currentProcessId = GetCurrentProcessId();
		printf("Sending process handle: %lu size: %llu\n", currentProcessId, sizeof(DWORD));
		int sendResult = send(clientSocket, (const char*)&currentProcessId, sizeof(DWORD), 0);
		WSA_ERR_CHECK(sendResult == SOCKET_ERROR, "Send process id failed");
	}

	// Receive shared memory
	{
		printf("Waiting to receive externalNodeMemoryHandle.\n");
		int receiveLength = recv(clientSocket, (char*)&externalNodeMemoryHandle, sizeof(HANDLE), 0);
		WSA_ERR_CHECK(receiveLength == SOCKET_ERROR || receiveLength == 0, "Recv externalNodeMemoryHandle failed");
		printf("Received externalNodeMemoryHandle: %p Size: %d\n", externalNodeMemoryHandle, receiveLength);

		pExternalNodeMemory = MapViewOfFile(externalNodeMemoryHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MxcExternalNodeMemory));
		ERR_CHECK(pExternalNodeMemory == NULL, "Map pExternalNodeMemory failed");
		pImportedExternalMemory = pExternalNodeMemory;
		importedExternalMemoryHandle = externalNodeMemoryHandle;

		pImportParam = &pExternalNodeMemory->importParam;
		pNodeShared = &pExternalNodeMemory->shared;
	}

	if (!createTestNode)
		goto ExitSuccess;

	// Request and setup handle data
	{
		const NodeHandle handle = RequestExternalNodeHandle(pNodeShared);
		pNodeContext = &nodeContexts[handle];
		*pNodeContext = (MxcNodeContext){};
		pNodeContext->type = MXC_NODE_TYPE_INTERPROCESS_VULKAN_IMPORTED;
		pNodeContext->pNodeShared = pNodeShared;
		printf("Importing node handle %d\n", handle);
	}

	// Create node data
	{
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

		const VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = midVk.context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
		};
		MIDVK_REQUIRE(vkCreateCommandPool(midVk.context.device, &graphicsCommandPoolCreateInfo, MIDVK_ALLOC, &pNodeContext->pool));
		const VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = pNodeContext->pool,
			.commandBufferCount = 1,
		};
		MIDVK_REQUIRE(vkAllocateCommandBuffers(midVk.context.device, &commandBufferAllocateInfo, &pNodeContext->cmd));
		midVkSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)pNodeContext->cmd, "TestNode");

		MxcNodeVkFramebufferTexture* pFramebufferTextures = pNodeContext->vkNodeFramebufferTextures;
		for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
			const VkmTextureCreateInfo colorCreateInfo = {
				.debugName = "ImportedColorFramebuffer",
				.imageCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.pNext = MIDVK_EXTERNAL_IMAGE_CREATE_INFO,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_COLOR_INDEX],
					.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = MIDVK_PASS_USAGES[MIDVK_PASS_ATTACHMENT_COLOR_INDEX],
				},
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.locality = MID_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
				.externalHandle = pImportParam->framebufferHandles[i].color,
			};
			midvkCreateTexture(&colorCreateInfo, &pFramebufferTextures[i].color);
			const VkmTextureCreateInfo normalCreateInfo = {
				.debugName = "ImportedNormalFramebuffer",
				.imageCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.pNext = MIDVK_EXTERNAL_IMAGE_CREATE_INFO,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = MIDVK_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_NORMAL_INDEX],
					.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = MIDVK_PASS_USAGES[MIDVK_PASS_ATTACHMENT_NORMAL_INDEX],
				},
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.locality = MID_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
				.externalHandle = pImportParam->framebufferHandles[i].normal,
			};
			midvkCreateTexture(&normalCreateInfo, &pFramebufferTextures[i].normal);
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
			midvkCreateTexture(&gbufferCreateInfo, &pFramebufferTextures[i].gbuffer);
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
			midvkCreateTexture(&depthCreateInfo, &pFramebufferTextures[i].depth);
		}
	}

	// Start node thread
	{
		__atomic_thread_fence(__ATOMIC_RELEASE);
		int result = pthread_create(&pNodeContext->threadId, NULL, (void* (*)(void*))mxcTestNodeThread, pNodeContext);
		REQUIRE(result == 0, "Node Process Import thread creation failed!");
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
		mxcInterprocessEnqueue(&activeNodesShared[i]->targetQueue, MXC_INTERPROCESS_TARGET_NODE_CLOSED);
	}
}

void mxcInterprocessTargetNodeClosed(const NodeHandle handle)
{
	printf("Closing %d\n", handle);
	ReleaseNode(handle);
}

const MxcInterProcessFuncPtr MXC_INTERPROCESS_TARGET_FUNC[] = {
	[MXC_INTERPROCESS_TARGET_NODE_CLOSED] = (MxcInterProcessFuncPtr const)mxcInterprocessTargetNodeClosed,
};