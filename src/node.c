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
	MidVkSemaphoreCreateInfo semaphoreCreateInfo = {
		.debugName = "CompTimelineSemaphore",
		.locality = VK_LOCALITY_INTERPROCESS_EXPORTED_READONLY,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE};
	midVkCreateSemaphore(&semaphoreCreateInfo, &compositorNodeContext.compTimeline);
	midVkCreateSwap(surface, VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS, &compositorNodeContext.swap);

	VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = vk.context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
	};
	VK_CHECK(vkCreateCommandPool(vk.context.device, &graphicsCommandPoolCreateInfo, MIDVK_ALLOC, &compositorNodeContext.pool));
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = compositorNodeContext.pool,
		.commandBufferCount = 1,
	};
	VK_CHECK(vkAllocateCommandBuffers(vk.context.device, &commandBufferAllocateInfo, &compositorNodeContext.cmd));
	midVkSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)compositorNodeContext.cmd, "CompCmd");

	CHECK(pthread_create(&compositorNodeContext.threadId, NULL, (void* (*)(void*))runFunc, &compositorNodeContext), "Comp Node thread creation failed!");
	printf("Request and Run CompNode Thread Success.\n");
}

static NodeHandle RequestLocalNodeHandle()
{
	NodeHandle handle = __atomic_fetch_add(&nodeCount, 1, __ATOMIC_RELEASE);
	nodesShared[handle] = (MxcNodeShared){};
	activeNodesShared[handle] = &nodesShared[handle];
	return handle;
}
NodeHandle RequestExternalNodeHandle(MxcNodeShared* const pNodeShared)
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
			vkFreeCommandBuffers(vk.context.device, pNodeContext->pool, 1, &pNodeContext->cmd);
			vkDestroyCommandPool(vk.context.device, pNodeContext->pool, MIDVK_ALLOC);
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
			CLOSE_HANDLE(vkGetSemaphoreExternalHandle(pNodeContext->vkNodeTimeline));
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
	vkDestroySemaphore(vk.context.device, pNodeContext->vkNodeTimeline, MIDVK_ALLOC);

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
	for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
		vkDestroyImageView(vk.context.device, pNodeContext->vkNodeFramebufferTextures[i].color.view, MIDVK_ALLOC);
		vkDestroyImage(vk.context.device, pNodeContext->vkNodeFramebufferTextures[i].color.image, MIDVK_ALLOC);
		vkFreeMemory(vk.context.device, pNodeContext->vkNodeFramebufferTextures[i].color.memory, MIDVK_ALLOC);

		vkDestroyImageView(vk.context.device, pNodeContext->vkNodeFramebufferTextures[i].normal.view, MIDVK_ALLOC);
		vkDestroyImage(vk.context.device, pNodeContext->vkNodeFramebufferTextures[i].normal.image, MIDVK_ALLOC);
		vkFreeMemory(vk.context.device, pNodeContext->vkNodeFramebufferTextures[i].normal.memory, MIDVK_ALLOC);

		vkDestroyImageView(vk.context.device, pNodeContext->vkNodeFramebufferTextures[i].depth.view, MIDVK_ALLOC);
		vkDestroyImage(vk.context.device, pNodeContext->vkNodeFramebufferTextures[i].depth.image, MIDVK_ALLOC);
		vkFreeMemory(vk.context.device, pNodeContext->vkNodeFramebufferTextures[i].depth.memory, MIDVK_ALLOC);

		vkDestroyImageView(vk.context.device, pNodeContext->vkNodeFramebufferTextures[i].gbuffer.view, MIDVK_ALLOC);
		vkDestroyImage(vk.context.device, pNodeContext->vkNodeFramebufferTextures[i].gbuffer.image, MIDVK_ALLOC);
		vkFreeMemory(vk.context.device, pNodeContext->vkNodeFramebufferTextures[i].gbuffer.memory, MIDVK_ALLOC);
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
	pNodeShared->cameraPos.rotation = QuatFromEuler(pNodeShared->cameraPos.euler);
	pNodeShared->camera.yFOV = 45.0f;
	pNodeShared->camera.zNear = 0.1f;
	pNodeShared->camera.zFar = 100.0f;
	pNodeShared->compositorRadius = 0.5;
	pNodeShared->compositorCycleSkip = 16;

	MidVkFenceCreateInfo nodeFenceCreateInfo = {
		.debugName = "NodeFence",
		.locality = VK_LOCALITY_CONTEXT,
	};
	midVkCreateFence(&nodeFenceCreateInfo, &pNodeContext->vkNodeFence);
	MidVkSemaphoreCreateInfo semaphoreCreateInfo = {
		.debugName = "NodeTimelineSemaphore",
		.locality = VK_LOCALITY_CONTEXT,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
	};
	midVkCreateSemaphore(&semaphoreCreateInfo, &pNodeContext->vkNodeTimeline);

	VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = vk.context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
	};
	VK_CHECK(vkCreateCommandPool(vk.context.device, &graphicsCommandPoolCreateInfo, MIDVK_ALLOC, &pNodeContext->pool));
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = pNodeContext->pool,
		.commandBufferCount = 1,
	};
	VK_CHECK(vkAllocateCommandBuffers(vk.context.device, &commandBufferAllocateInfo, &pNodeContext->cmd));
	midVkSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)pNodeContext->cmd, "TestNode");

	mxcCreateNodeFramebuffer(VK_LOCALITY_CONTEXT, pNodeContext->vkNodeFramebufferTextures);

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
	VK_CHECK(vkCreateRenderPass2(vk.context.device, &renderPassCreateInfo2, MIDVK_ALLOC, &vk.context.nodeRenderPass));
	midVkSetDebugName(VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)vk.context.nodeRenderPass, "NodeRenderPass");
}

void mxcCreateNodeFramebuffer(VkLocality locality, MxcNodeVkFramebufferTexture* pNodeFramebufferTextures)
{
	for (int swapIndex = 0; swapIndex < MIDVK_SWAP_COUNT; ++swapIndex) {
		{
			VkImageCreateInfo info = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.pNext = &(VkExternalMemoryImageCreateInfo){
					.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
					.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
				},
				.imageType = VK_IMAGE_TYPE_2D,
				.format = VK_BASIC_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_COLOR_INDEX],
				.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
				.mipLevels = 1,
				.arrayLayers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.usage = VK_BASIC_PASS_USAGES[MIDVK_PASS_ATTACHMENT_COLOR_INDEX],
			};
			vkWin32CreateExternalTexture(&info, &pNodeFramebufferTextures[swapIndex].colorExternal);
			VkTextureCreateInfo textureInfo = {
				.debugName = "ExportedColorFramebuffer",
				.pImageCreateInfo = &info,
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.importHandle = pNodeFramebufferTextures[swapIndex].colorExternal.handle,
				.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
				.locality = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
			};
			vkCreateTexture(&textureInfo, &pNodeFramebufferTextures[swapIndex].color);
		}

		{
			VkImageCreateInfo info = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.pNext = &(VkExternalMemoryImageCreateInfo){
					.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
					.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
				},
				.imageType = VK_IMAGE_TYPE_2D,
				.format = VK_BASIC_PASS_FORMATS[MIDVK_PASS_ATTACHMENT_NORMAL_INDEX],
				.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
				.mipLevels = 1,
				.arrayLayers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.usage = VK_BASIC_PASS_USAGES[MIDVK_PASS_ATTACHMENT_NORMAL_INDEX],
			};
			vkWin32CreateExternalTexture(&info, &pNodeFramebufferTextures[swapIndex].normalExternal);
			VkTextureCreateInfo textureInfo = {
				.debugName = "ExportedNormalFramebuffer",
				.pImageCreateInfo = &info,
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.importHandle = pNodeFramebufferTextures[swapIndex].normalExternal.handle,
				.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
				.locality = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
			};
			vkCreateTexture(&textureInfo, &pNodeFramebufferTextures[swapIndex].normal);
		}

		{
			VkImageCreateInfo info = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				.pNext = &(VkExternalMemoryImageCreateInfo){
					.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
					.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
				},
				.imageType = VK_IMAGE_TYPE_2D,
				.format = MXC_NODE_GBUFFER_FORMAT,
				.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
				.mipLevels = MXC_NODE_GBUFFER_LEVELS,
				.arrayLayers = 1,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.usage = MXC_NODE_GBUFFER_USAGE,
			};
			vkWin32CreateExternalTexture(&info, &pNodeFramebufferTextures[swapIndex].gbufferExternal);
			VkTextureCreateInfo textureInfo = {
				.debugName = "ExportedGBufferFramebuffer",
				.pImageCreateInfo = &info,
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.importHandle = pNodeFramebufferTextures[swapIndex].gbufferExternal.handle,
				.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
				.locality = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
			};
			vkCreateTexture(&textureInfo, &pNodeFramebufferTextures[swapIndex].gbuffer);
		}

		// Depth is not shared over IPC.
		if (locality == VK_LOCALITY_INTERPROCESS_EXPORTED_READWRITE || locality == VK_LOCALITY_INTERPROCESS_EXPORTED_READONLY) {
			// we need to transition these out of undefined initially because the transition in the other process won't update layout to avoid initial validation error on transition
			VkCommandBuffer             cmd = midVkBeginImmediateTransferCommandBuffer();
			const VkImageMemoryBarrier2 interProcessBarriers[] = {
				VKM_COLOR_IMAGE_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMG_BARRIER_EXTERNAL_ACQUIRE_SHADER_READ, pNodeFramebufferTextures[swapIndex].color.image),
				VKM_COLOR_IMAGE_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMG_BARRIER_EXTERNAL_ACQUIRE_SHADER_READ, pNodeFramebufferTextures[swapIndex].normal.image),
				VKM_COLOR_IMAGE_BARRIER_MIPS(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMG_BARRIER_EXTERNAL_ACQUIRE_SHADER_READ, pNodeFramebufferTextures[swapIndex].gbuffer.image, 0, MXC_NODE_GBUFFER_LEVELS),
			};
			VK_DEVICE_FUNC(CmdPipelineBarrier2);
			CmdPipelineImageBarriers2(cmd, COUNT(interProcessBarriers), interProcessBarriers);
			midVkEndImmediateTransferCommandBuffer(cmd);
			continue;
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
		vkCreateTexture(&depthCreateInfo, &pNodeFramebufferTextures[swapIndex].depth);
	}

	// test code to ensure it imports into dx11
//	{
//		IDXGIFactory1* factory1 = NULL;
//		DX_CHECK(CreateDXGIFactory1(&IID_IDXGIFactory, (void**)&factory1));
//
//		IDXGIAdapter* adapter = NULL;
//		for (UINT i = 0; IDXGIFactory1_EnumAdapters(factory1, i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
//			DXGI_ADAPTER_DESC desc;
//			DX_CHECK(IDXGIAdapter_GetDesc(adapter, &desc));
//			wprintf(L"DX11 Adapter %d Name: %ls LUID: %ld:%lu\n",
//					i, desc.Description, desc.AdapterLuid.HighPart, desc.AdapterLuid.LowPart);
//
//			ID3D11Device*        device;
//			ID3D11DeviceContext* context;
//			D3D_FEATURE_LEVEL    featureLevel;
//			DX_CHECK(D3D11CreateDevice(
//				adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, D3D11_CREATE_DEVICE_DEBUG,
//				(D3D_FEATURE_LEVEL[]){D3D_FEATURE_LEVEL_11_1}, 1, D3D11_SDK_VERSION,
//				&device, &featureLevel, &context));
//			printf("XR D3D11 Device: %p\n", device);
//			if (device == NULL) {
//				fprintf(stderr, "Mid D3D11 Device Invalid.\n");
//			}
//
//			ID3D11Device1* device1;
//			DX_CHECK(ID3D11Device_QueryInterface(device, &IID_ID3D11Device1, (void**)&device1));
//			printf("XR D3D11 Device1: %p\n", device1);
//			if (device1 == NULL) {
//				fprintf(stderr, "XR D3D11 Device Invalid.\n");
//			}
//
//			printf("got shared handle %p\n", pNodeFramebufferTextures[0].colorExternal.handle);
//			ID3D11Texture2D* importedResource = NULL;
//			DX_CHECK(ID3D11Device1_OpenSharedResource1(device1, pNodeFramebufferTextures[0].colorExternal.handle, &IID_ID3D11Texture2D, (void**)&importedResource));
//			printf("imported texture %p\n", importedResource);
//
//			break;
//		}
//
//		IDXGIAdapter_Release(adapter);
//		IDXGIFactory1_Release(factory1);
//	}
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
		pNodeShared->cameraPos.rotation = QuatFromEuler(pNodeShared->cameraPos.euler);
		pNodeShared->camera.yFOV = 45.0f;
		pNodeShared->camera.zNear = 0.1f;
		pNodeShared->camera.zFar = 100.0f;
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
			.locality = VK_LOCALITY_INTERPROCESS_EXPORTED_READWRITE,
		};
		midVkCreateFence(&nodeFenceCreateInfo, &pNodeContext->vkNodeFence);
		const MidVkSemaphoreCreateInfo semaphoreCreateInfo = {
			.debugName = "NodeTimelineSemaphoreExport",
			.locality = VK_LOCALITY_INTERPROCESS_EXPORTED_READWRITE,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		};
		midVkCreateSemaphore(&semaphoreCreateInfo, &pNodeContext->vkNodeTimeline);

		mxcCreateNodeFramebuffer(VK_LOCALITY_INTERPROCESS_EXPORTED_READWRITE, pNodeContext->vkNodeFramebufferTextures);

		const HANDLE currentHandle = GetCurrentProcess();
		for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
			WIN32_CHECK(DuplicateHandle(
						  currentHandle, pNodeContext->vkNodeFramebufferTextures[i].colorExternal.handle,
						  nodeProcessHandle, &pImportParam->framebufferHandles[i].color,
						  0, false, DUPLICATE_SAME_ACCESS),
					  "Duplicate glColor handle fail");
			WIN32_CHECK(DuplicateHandle(
						  currentHandle, pNodeContext->vkNodeFramebufferTextures[i].normalExternal.handle,
						  nodeProcessHandle, &pImportParam->framebufferHandles[i].normal,
						  0, false, DUPLICATE_SAME_ACCESS),
					  "Duplicate normal handle fail.");
			WIN32_CHECK(DuplicateHandle(
						  currentHandle, pNodeContext->vkNodeFramebufferTextures[i].gbufferExternal.handle,
						  nodeProcessHandle, &pImportParam->framebufferHandles[i].gbuffer,
						  0, false, DUPLICATE_SAME_ACCESS),
					  "Duplicate gbuffer handle fail.");
		}
		WIN32_CHECK(DuplicateHandle(
					  currentHandle, vkGetFenceExternalHandle(pNodeContext->vkNodeFence),
					  nodeProcessHandle, &pImportParam->nodeFenceHandle,
					  0, false, DUPLICATE_SAME_ACCESS),
				  "Duplicate nodeFenceHandle handle fail.");
		WIN32_CHECK(DuplicateHandle(
					  currentHandle, vkGetSemaphoreExternalHandle(pNodeContext->vkNodeTimeline),
					  nodeProcessHandle, &pImportParam->nodeTimelineHandle,
					  0, false, DUPLICATE_SAME_ACCESS),
				  "Duplicate vkNodeTimeline handle fail.");
		WIN32_CHECK(DuplicateHandle(
					  currentHandle, vkGetSemaphoreExternalHandle(compositorNodeContext.compTimeline),
					  nodeProcessHandle, &pImportParam->compTimelineHandle,
					  0, false, DUPLICATE_SAME_ACCESS),
				  "Duplicate compeTimeline handle fail.");

		const uint32_t graphicsQueueIndex = vk.context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index;
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
				MIDVK_COLOR_SUBRESOURCE_RANGE,
			};
			pNodeCompositorData->framebuffers[i].acquireBarriers[1] = (VkImageMemoryBarrier2){
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
				.dstQueueFamilyIndex = graphicsQueueIndex,
				.image = pNodeCompositorData->framebuffers[i].normal,
				MIDVK_COLOR_SUBRESOURCE_RANGE,
			};
			pNodeCompositorData->framebuffers[i].acquireBarriers[2] = (VkImageMemoryBarrier2){
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
				.dstQueueFamilyIndex = graphicsQueueIndex,
				.image = pNodeCompositorData->framebuffers[i].gBuffer,
				MIDVK_COLOR_SUBRESOURCE_RANGE,
			};
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
			.importHandle = pImportParam->compTimelineHandle,
		};
		midVkCreateSemaphore(&compTimelineCreateInfo, &pNodeContext->vkCompositorTimeline);
		MidVkSemaphoreCreateInfo nodeTimelineCreateInfo = {
			.debugName = "NodeTimelineSemaphoreImport",
			.locality = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			.importHandle = pImportParam->nodeTimelineHandle,
		};
		midVkCreateSemaphore(&nodeTimelineCreateInfo, &pNodeContext->vkNodeTimeline);

		VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = vk.context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
		};
		VK_CHECK(vkCreateCommandPool(vk.context.device, &graphicsCommandPoolCreateInfo, MIDVK_ALLOC, &pNodeContext->pool));
		VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = pNodeContext->pool,
			.commandBufferCount = 1,
		};
		VK_CHECK(vkAllocateCommandBuffers(vk.context.device, &commandBufferAllocateInfo, &pNodeContext->cmd));
		midVkSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)pNodeContext->cmd, "TestNode");

		MxcNodeVkFramebufferTexture* pFramebufferTextures = pNodeContext->vkNodeFramebufferTextures;
		for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
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