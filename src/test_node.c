#include <assert.h>

#include "mid_shape.h"

#include "test_node.h"

enum SetBindNodeProcessIndices {
	SET_BIND_NODE_PROCESS_SRC_INDEX,
	SET_BIND_NODE_PROCESS_DST_INDEX,
	SET_BIND_NODE_PROCESS_COUNT
};
#define SET_WRITE_NODE_PROCESS_SRC(src_image_view)                   \
	(VkWriteDescriptorSet)                                     \
	{                                                                \
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,             \
		.dstBinding = SET_BIND_NODE_PROCESS_SRC_INDEX,               \
		.descriptorCount = 1,                                        \
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
		.pImageInfo = &(VkDescriptorImageInfo){                \
			.imageView = src_image_view,                             \
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,                  \
		},                                                           \
	}
#define SET_WRITE_NODE_PROCESS_DST(dst_image_view)          \
	(VkWriteDescriptorSet)                            \
	{                                                       \
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,    \
		.dstBinding = SET_BIND_NODE_PROCESS_DST_INDEX,      \
		.descriptorCount = 1,                               \
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, \
		.pImageInfo = &(VkDescriptorImageInfo){       \
			.imageView = dst_image_view,                    \
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,         \
		},                                                  \
	}
enum PipeSetNodeProcessIndices {
	PIPE_SET_NODE_PROCESS_INDEX,
	PIPE_SET_NODE_PROCESS_COUNT,
};

#define IMAGE_BARRIER_SRC_NODE_FINISH_RENDERPASS                         \
	.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, \
	.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,         \
	.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
#define IMAGE_BARRIER_DST_NODE_RELEASE                    \
	.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, \
	.dstAccessMask = VK_ACCESS_2_NONE,                    \
	.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL

void mxcTestNodeRun(MxcNodeContext* pNodeContext, MxcTestNode* pNode)
{
	auto nodeType = pNodeContext->type;
	auto needsImport = nodeType != MXC_NODE_TYPE_THREAD;

	MxcNodeShared*  pNodeShared = pNodeContext->pNodeShared;
	MxcNodeImports* pNodeImports = pNodeContext->pNodeImports;
	MxcSwap*        pSwap = pNodeContext->swap;

	VkDevice        device = pNode->device;
	VkCommandBuffer cmd = pNodeContext->cmd;
	VkRenderPass    nodeRenderPass = pNode->nodeRenderPass;
	VkFramebuffer   framebuffer = pNode->framebuffer;

	VkGlobalSetState* pGlobalSetMapped = pNode->globalSet.pMapped;
	VkDescriptorSet   globalSet = pNode->globalSet.set;

	VkDescriptorSet  checkerMaterialSet = pNode->checkerMaterialSet;
	VkDescriptorSet  sphereObjectSet = pNode->sphereObjectSet;
	VkPipelineLayout pipeLayout = pNode->pipeLayout;
	VkPipeline       pipe = pNode->basicPipe;
	VkPipelineLayout nodeProcessPipeLayout = pNode->nodeProcessPipeLayout;
	VkPipeline       nodeProcessBlitMipAveragePipe = pNode->nodeProcessBlitMipAveragePipe;
	VkPipeline       nodeProcessBlitDownPipe = pNode->nodeProcessBlitDownPipe;

	uint32_t graphicsQueueIndex = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index;

//	VkImageView gBufferMipViews[VK_SWAP_COUNT][MXC_NODE_GBUFFER_LEVELS];
//	memcpy(&gBufferMipViews, &pNode->gBufferMipViews, sizeof(gBufferMipViews));

	uint32_t     sphereIndexCount = pNode->sphereMesh.indexCount;
	VkBuffer     sphereBuffer = pNode->sphereMesh.buffer;
	VkDeviceSize sphereIndexOffset = pNode->sphereMesh.indexOffset;
	VkDeviceSize sphereVertexOffset = pNode->sphereMesh.vertexOffset;

	VkSemaphore compTimeline = pNodeContext->compositorTimeline;
	VkSemaphore nodeTimeline = pNodeContext->nodeTimeline;

	uint64_t nodeTimelineValue = 0;

	{
		uint64_t compositorTimelineValue;
		VK_CHECK(vkGetSemaphoreCounterValue(device, compTimeline, &compositorTimelineValue));
		CHECK(compositorTimelineValue == 0xffffffffffffffff, "compositorTimelineValue imported as max value!");
		pNodeShared->compositorBaseCycleValue = compositorTimelineValue - (compositorTimelineValue % MXC_CYCLE_COUNT);
	}

	assert(__atomic_always_lock_free(sizeof(pNodeShared->timelineValue), &pNodeShared->timelineValue));

	VkImageMemoryBarrier2 acquireBarriers[VK_SWAP_COUNT][2];
	uint32_t              acquireBarrierCount = 0;
	VkImageMemoryBarrier2 releaseBarriers[VK_SWAP_COUNT][2];
	uint32_t              releaseBarrierCount = 0;
	for (int i = 0; i < VK_SWAP_COUNT; ++i) {
		switch (pNodeContext->type) {
			case MXC_NODE_TYPE_THREAD:
				// Acquire not needed from thread.
				releaseBarrierCount = 0;
//				releaseBarriers[i][0] = (VkImageMemoryBarrier2){
//					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
//					VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
//					IMAGE_BARRIER_DST_NODE_RELEASE,
//					VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
////					.image = framebufferImages[i].gBuffer,
//					.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
//				};

				break;
			case MXC_NODE_TYPE_INTERPROCESS_VULKAN_IMPORTED:

				acquireBarrierCount = 3;
				acquireBarriers[i][0] = (VkImageMemoryBarrier2){
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
					.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
					.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
					.dstQueueFamilyIndex = graphicsQueueIndex,
//					.image = framebufferImages[i].color,
					.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
				};
				acquireBarriers[i][1] = (VkImageMemoryBarrier2){
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
					.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
					.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_GENERAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
					.dstQueueFamilyIndex = graphicsQueueIndex,
//					.image = framebufferImages[i].gBuffer,
					.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
				};

				releaseBarrierCount = 3;
				releaseBarriers[i][0] = (VkImageMemoryBarrier2){
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
					.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
					.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = graphicsQueueIndex,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
//					.image = framebufferImages[i].color,
					.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
				};
				releaseBarriers[i][1] = (VkImageMemoryBarrier2){
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
					.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = graphicsQueueIndex,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
//					.image = framebufferImages[i].gBuffer,
					.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
				};

				break;
			case MXC_NODE_TYPE_INTERPROCESS_VULKAN_EXPORTED:
				PANIC("Shouldn't be rendering an exported node from this process.");
			default:
				PANIC("nodeType not supported");
		}
	}

	bool claimSwaps = true;

	// I probably want to be able to define this into a struct
	// and optionally have it be a static struct
	VK_DEVICE_FUNC(ResetCommandBuffer);
	VK_DEVICE_FUNC(BeginCommandBuffer);
	VK_DEVICE_FUNC(CmdSetViewport);
	VK_DEVICE_FUNC(CmdBeginRenderPass);
	VK_DEVICE_FUNC(CmdSetScissor);
	VK_DEVICE_FUNC(CmdBindPipeline);
	VK_DEVICE_FUNC(CmdDispatch);
	VK_DEVICE_FUNC(CmdBindDescriptorSets);
	VK_DEVICE_FUNC(CmdBindVertexBuffers);
	VK_DEVICE_FUNC(CmdBindIndexBuffer);
	VK_DEVICE_FUNC(CmdDrawIndexed);
	VK_DEVICE_FUNC(CmdEndRenderPass);
	VK_DEVICE_FUNC(EndCommandBuffer);
	VK_DEVICE_FUNC(CmdPipelineBarrier2);
	VK_DEVICE_FUNC(CmdPushDescriptorSetKHR);
	VK_DEVICE_FUNC(CmdClearColorImage);

run_loop:

	midVkTimelineWait(device, pNodeShared->compositorBaseCycleValue + MXC_CYCLE_COMPOSITOR_RECORD, compTimeline);

	__atomic_thread_fence(__ATOMIC_ACQUIRE);
	memcpy(pGlobalSetMapped, (void*)&pNodeShared->globalSetState, sizeof(VkGlobalSetState));

	if (claimSwaps) {
		claimSwaps = false;

		bool hasDepth = pNodeShared->swapUsage == MXC_SWAP_USAGE_COLOR_AND_DEPTH;
		int  swapCount = MXC_SWAP_TYPE_COUNTS[pNodeShared->swapType] * VK_SWAP_COUNT;

		for (int i = 0; i < swapCount; ++i) {
			if (needsImport) {
				if (pNodeImports->colorSwapHandles[i] != NULL)
					CloseHandle(pNodeContext->pNodeImports->colorSwapHandles[i]);
				if (hasDepth && pNodeImports->depthSwapHandles[i] != NULL)
					CloseHandle(pNodeImports->depthSwapHandles[i]);
			}

			if (pSwap[i].color.image != VK_NULL_HANDLE) {
				vkDestroyImage(device, pSwap[i].color.image, VK_ALLOC);
				vkDestroyImageView(device, pSwap[i].color.view, VK_ALLOC);
			}

			if (hasDepth && pSwap[i].depth.image != VK_NULL_HANDLE) {
				vkDestroyImage(device, pSwap[i].depth.image, VK_ALLOC);
				vkDestroyImageView(device, pSwap[i].depth.view, VK_ALLOC);
			}
		}

		mxcIpcFuncEnqueue(&pNodeShared->nodeInterprocessFuncQueue, MXC_INTERPROCESS_TARGET_SYNC_SWAPS);
		printf("Waiting on swap claim.");
		WaitForSingleObject(pNodeContext->swapsSyncedHandle, INFINITE);
		printf("Swaps claimed.");

		if (needsImport) {
			for (int i = 0; i < swapCount; ++i) {

				VkTextureCreateInfo colorCreateInfo = {
					.debugName = "ImportedColorFramebuffer",
					.pImageCreateInfo = &(VkImageCreateInfo){
						.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
						.pNext = &(VkExternalMemoryImageCreateInfo){
							.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
							.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_PLATFORM,
						},
						.imageType = VK_IMAGE_TYPE_2D,
						.format = VK_BASIC_PASS_FORMATS[VK_BASIC_PASS_ATTACHMENT_COLOR_INDEX],
						.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
						.mipLevels = 1,
						.arrayLayers = 1,
						.samples = VK_SAMPLE_COUNT_1_BIT,
						.usage = VK_BASIC_PASS_USAGES[VK_BASIC_PASS_ATTACHMENT_COLOR_INDEX],
					},
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.locality = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
					.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
					.importHandle = pNodeContext->pNodeImports->colorSwapHandles[i],
				};
				vkCreateTexture(&colorCreateInfo, &pSwap[i].color);

				if (hasDepth) {
					VkTextureCreateInfo depthCreateInfo = {
						.debugName = "ImportedDepthFramebuffer",
						.pImageCreateInfo = &(VkImageCreateInfo){
							.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
							.pNext = &(VkExternalMemoryImageCreateInfo){
								.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
								.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_PLATFORM,
							},
							.imageType = VK_IMAGE_TYPE_2D,
							.format = VK_BASIC_PASS_FORMATS[VK_BASIC_PASS_ATTACHMENT_DEPTH_INDEX],
							.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
							.mipLevels = 1,
							.arrayLayers = 1,
							.samples = VK_SAMPLE_COUNT_1_BIT,
							.usage = VK_BASIC_PASS_USAGES[VK_BASIC_PASS_ATTACHMENT_DEPTH_INDEX],
						},
						.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
						.locality = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
						.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
						.importHandle = pNodeContext->pNodeImports->depthSwapHandles[i],
					};
					vkCreateTexture(&depthCreateInfo, &pSwap[i].depth);
				}

			}
		}
	}

	ResetCommandBuffer(cmd, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
	BeginCommandBuffer(cmd, &(VkCommandBufferBeginInfo){.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT});

	int framebufferIndex = nodeTimelineValue % VK_SWAP_COUNT;

	{
		VkViewport viewport = {
			.x = -pNodeShared->ulScreenUV.x * DEFAULT_WIDTH,
			.y = -pNodeShared->ulScreenUV.y * DEFAULT_HEIGHT,
			.width = DEFAULT_WIDTH,
			.height = DEFAULT_HEIGHT,
			.maxDepth = 1.0f,
		};
		VkRect2D scissor = {
			.offset = {
				.x = 0,
				.y = 0,
			},
			.extent = {
				.width = pNodeShared->globalSetState.framebufferSize.x,
				.height = pNodeShared->globalSetState.framebufferSize.y,
			},
		};
		CmdSetViewport(cmd, 0, 1, &viewport);
		CmdSetScissor(cmd, 0, 1, &scissor);
//		CmdPipelineImageBarriers2(cmd, acquireBarrierCount, acquireBarriers[framebufferIndex]);

		VkClearColorValue clearColor = (VkClearColorValue){0, 0, 0.1, 0};
		CmdBeginRenderPass(cmd, nodeRenderPass, framebuffer, clearColor, pSwap[framebufferIndex].color.view, pNodeContext->normalFramebuffers[framebufferIndex].view, pSwap[framebufferIndex].depth.view);

		// this is really all that'd be user exposed....
		CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
		CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout, VK_PIPE_SET_INDEX_GLOBAL, 1, &globalSet, 0, NULL);
		CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout, VK_PIPE_SET_INDEX_MATERIAL, 1, &checkerMaterialSet, 0, NULL);
		CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout, VK_PIPE_SET_OBJECT_INDEX, 1, &sphereObjectSet, 0, NULL);

		CmdBindVertexBuffers(cmd, 0, 1, (VkBuffer[]){sphereBuffer}, (VkDeviceSize[]){sphereVertexOffset});
		CmdBindIndexBuffer(cmd, sphereBuffer, sphereIndexOffset, VK_INDEX_TYPE_UINT16);
		CmdDrawIndexed(cmd, sphereIndexCount, 1, 0, 0, 0);

		CmdEndRenderPass(cmd);
	}

//	{  // Blit GBuffer
//		ivec2 extent = {pNodeShared->globalSetState.framebufferSize.x, pNodeShared->globalSetState.framebufferSize.y};
//		ivec2 groupCount = iVec2Min(iVec2CeiDivide(extent, 32), 1);
//		{
//			VkImageMemoryBarrier2 clearBarrier = {
//				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
//				VK_IMAGE_BARRIER_SRC_UNDEFINED,
//				VK_IMAGE_BARRIER_DST_TRANSFER_WRITE,
//				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
//				.image = framebufferImages[framebufferIndex].gBuffer,
//				.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
//			};
//			CmdPipelineImageBarrier2(cmd, &clearBarrier);
//			CmdClearColorImage(cmd, framebufferImages[framebufferIndex].gBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &MXC_NODE_CLEAR_COLOR, 1, &VK_COLOR_SUBRESOURCE_RANGE);
//			VkImageMemoryBarrier2 beginComputeBarriers[] = {
//				// could technically alter shader to take VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL and not need this, can different mips be in different layouts?
//				{
//					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
//					.image = framebufferImages[framebufferIndex].depth,
//					IMAGE_BARRIER_SRC_NODE_FINISH_RENDERPASS,
//					VK_IMAGE_BARRIER_DST_COMPUTE_READ,
//					VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
//					.subresourceRange = VK_DEPTH_SUBRESOURCE_RANGE,
//				},
//				{
//					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
//					VK_IMAGE_BARRIER_SRC_TRANSFER_WRITE,
//					VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
//					VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
//					.image = framebufferImages[framebufferIndex].gBuffer,
//					.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
//				},
//			};
//			CmdPipelineImageBarriers2(cmd, COUNT(beginComputeBarriers), beginComputeBarriers);
//			VkWriteDescriptorSet initialPushSet[] = {
//				SET_WRITE_NODE_PROCESS_SRC(framebufferImages[framebufferIndex].depthView),
//				SET_WRITE_NODE_PROCESS_DST(framebufferImages[framebufferIndex].gBufferView),
//			};
//			CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeProcessBlitMipAveragePipe);
//			CmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeProcessPipeLayout, PIPE_SET_NODE_PROCESS_INDEX, COUNT(initialPushSet), initialPushSet);
//			CmdDispatch(cmd, groupCount.x, groupCount.y, 1);
//		}
//
//		for (uint32_t i = 1; i < MXC_NODE_GBUFFER_LEVELS; ++i) {
//			VkImageMemoryBarrier2 barriers[] = {
//				{
//					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
//					VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
//					VK_IMAGE_BARRIER_DST_COMPUTE_READ,
//					VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
//					.image = framebufferImages[framebufferIndex].gBuffer,
//					.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = i - 1, .levelCount = 1, .layerCount = VK_REMAINING_ARRAY_LAYERS},
//				},
//				{
//					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
//					VK_IMAGE_BARRIER_SRC_COMPUTE_READ,
//					VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
//					VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
//					.image = framebufferImages[framebufferIndex].gBuffer,
//					.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 1, .levelCount = 1, .layerCount = VK_REMAINING_ARRAY_LAYERS},
//				},
//			};
//			CmdPipelineImageBarriers2(cmd, COUNT(barriers), barriers);
//			VkWriteDescriptorSet pushSet[] = {
//				SET_WRITE_NODE_PROCESS_SRC(gBufferMipViews[framebufferIndex][i - 1]),
//				SET_WRITE_NODE_PROCESS_DST(gBufferMipViews[framebufferIndex][i]),
//			};
//			CmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeProcessPipeLayout, PIPE_SET_NODE_PROCESS_INDEX, COUNT(pushSet), pushSet);
//			ivec2 mipGroupCount = iVec2Min(iVec2CeiDivide((ivec2){extent.vec >> 1}, 32), 1);
//			CmdDispatch(cmd, mipGroupCount.x, mipGroupCount.y, 1);
//		}
//
//		CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeProcessBlitDownPipe);
//		for (uint32_t i = MXC_NODE_GBUFFER_LEVELS - 1; i > 0; --i) {
//			VkImageMemoryBarrier2 barriers[] = {
//				{
//					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
//					VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
//					VK_IMAGE_BARRIER_DST_COMPUTE_READ,
//					VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
//					.image = framebufferImages[framebufferIndex].gBuffer,
//					.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = i, .levelCount = 1, .layerCount = VK_REMAINING_ARRAY_LAYERS},
//				},
//				{
//					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
//					VK_IMAGE_BARRIER_SRC_COMPUTE_READ,
//					VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
//					VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
//					.image = framebufferImages[framebufferIndex].gBuffer,
//					.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = i - 1, .levelCount = 1, .layerCount = VK_REMAINING_ARRAY_LAYERS},
//				},
//			};
//			CmdPipelineImageBarriers2(cmd, COUNT(barriers), barriers);
//			VkWriteDescriptorSet pushSet[] = {
//				SET_WRITE_NODE_PROCESS_SRC(gBufferMipViews[framebufferIndex][i]),
//				SET_WRITE_NODE_PROCESS_DST(gBufferMipViews[framebufferIndex][i - 1]),
//			};
//			CmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeProcessPipeLayout, PIPE_SET_NODE_PROCESS_INDEX, COUNT(pushSet), pushSet);
//			ivec2 mipGroupCount = iVec2Min(iVec2CeiDivide((ivec2){extent.vec >> (1 - 1)}, 32), 1);
//			CmdDispatch(cmd, mipGroupCount.x, mipGroupCount.y, 1);
//		}
//	}

//	CmdPipelineImageBarriers2(cmd, releaseBarrierCount, releaseBarriers[framebufferIndex]);
	EndCommandBuffer(cmd);

	nodeTimelineValue++;
	mxcQueueNodeCommandBuffer((MxcQueuedNodeCommandBuffer){
		.cmd = cmd,
		.nodeTimeline = nodeTimeline,
		.nodeTimelineSignalValue = nodeTimelineValue,
	});
	midVkTimelineWait(device, nodeTimelineValue, nodeTimeline);

	// tests show reading from shared memory is 500~ x faster than vkGetSemaphoreCounterValue
	// shared: 569 - semaphore: 315416 ratio: 554.333919
	__atomic_store_n(&pNodeShared->timelineValue, nodeTimelineValue, __ATOMIC_RELEASE);

	pNodeShared->compositorBaseCycleValue += MXC_CYCLE_COUNT * pNodeShared->compositorCycleSkip;

	//    _Thread_local static int count;
	//    if (count++ > 10)
	//      return;

	CHECK_RUNNING
	goto run_loop;
}

//
///Create
static void CreateNodeProcessSetLayout(VkDescriptorSetLayout* pLayout)
{
	VkDescriptorSetLayoutCreateInfo nodeSetLayoutCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
		.bindingCount = SET_BIND_NODE_PROCESS_COUNT,
		.pBindings = (VkDescriptorSetLayoutBinding[]){
			[SET_BIND_NODE_PROCESS_SRC_INDEX] = {
				.binding = SET_BIND_NODE_PROCESS_SRC_INDEX,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
				.pImmutableSamplers = &vk.context.linearSampler,
			},
			[SET_BIND_NODE_PROCESS_DST_INDEX] = {
				.binding = SET_BIND_NODE_PROCESS_DST_INDEX,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			},
		},
	};
	VK_CHECK(vkCreateDescriptorSetLayout(vk.context.device, &nodeSetLayoutCreateInfo, VK_ALLOC, pLayout));
}

static void CreateNodeProcessPipeLayout(VkDescriptorSetLayout nodeProcessSetLayout, VkPipelineLayout* pPipeLayout)
{
	VkPipelineLayoutCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = (VkDescriptorSetLayout[]){
			[PIPE_SET_NODE_PROCESS_INDEX] = nodeProcessSetLayout,
		},
	};
	VK_CHECK(vkCreatePipelineLayout(vk.context.device, &createInfo, VK_ALLOC, pPipeLayout));
}
static void CreateNodeProcessPipe(const char* shaderPath, VkPipelineLayout layout, VkPipeline* pPipe)
{
	VkShaderModule shader;
	vkCreateShaderModuleFromPath(shaderPath, &shader);
	VkComputePipelineCreateInfo pipelineInfo = {
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.stage = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_COMPUTE_BIT,
			.module = shader,
			.pName = "main",
		},
		.layout = layout,
	};
	VK_CHECK(vkCreateComputePipelines(vk.context.device, VK_NULL_HANDLE, 1, &pipelineInfo, VK_ALLOC, pPipe));
	vkDestroyShaderModule(vk.context.device, shader, VK_ALLOC);
}
static void mxcCreateTestNode(MxcNodeContext* pNodeContext, MxcTestNode* pTestNode)
{
	auto pNodeShared = pNodeContext->pNodeShared;
	int  swapCount = MXC_SWAP_TYPE_COUNTS[pNodeShared->swapType] * VK_SWAP_COUNT;

	{	// Config
		pNodeShared->swapType = MXC_SWAP_TYPE_SINGLE;
		pNodeShared->swapUsage = MXC_SWAP_USAGE_COLOR_AND_DEPTH;
	}

	{	// Create
		CreateNodeProcessSetLayout(&pTestNode->nodeProcessSetLayout);
		CreateNodeProcessPipeLayout(pTestNode->nodeProcessSetLayout, &pTestNode->nodeProcessPipeLayout);
		CreateNodeProcessPipe("./shaders/node_process_blit_slope_average_up.comp.spv", pTestNode->nodeProcessPipeLayout, &pTestNode->nodeProcessBlitMipAveragePipe);
		CreateNodeProcessPipe("./shaders/node_process_blit_down_alpha_omit.comp.spv", pTestNode->nodeProcessPipeLayout, &pTestNode->nodeProcessBlitDownPipe);

		vkCreateBasicFramebuffer(vk.context.nodeRenderPass, &pTestNode->framebuffer);
		vkSetDebugName(VK_OBJECT_TYPE_FRAMEBUFFER, (uint64_t)pTestNode->framebuffer, "TestNodeFramebuffer");

		for (int i = 0; i < swapCount; ++i) {

			VkTextureCreateInfo colorCreateInfo = {
				.debugName = "ImportedColorFramebuffer",
				.pImageCreateInfo = &(VkImageCreateInfo){
					.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.pNext = VK_EXTERNAL_IMAGE_CREATE_INFO_PLATFORM,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = VK_BASIC_PASS_FORMATS[VK_BASIC_PASS_ATTACHMENT_COLOR_INDEX],
					.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = VK_BASIC_PASS_USAGES[VK_BASIC_PASS_ATTACHMENT_COLOR_INDEX],
				},
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.locality = VK_LOCALITY_CONTEXT,
			};
			vkCreateTexture(&colorCreateInfo, &pNodeContext->colorFramebuffers[i]);

			VkTextureCreateInfo normalCreateInfo = {
				.debugName = "ImportedNormalFramebuffer",
				.pImageCreateInfo = &(VkImageCreateInfo){
					.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.pNext = VK_EXTERNAL_IMAGE_CREATE_INFO_PLATFORM,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = VK_BASIC_PASS_FORMATS[VK_BASIC_PASS_ATTACHMENT_NORMAL_INDEX],
					.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = VK_BASIC_PASS_USAGES[VK_BASIC_PASS_ATTACHMENT_NORMAL_INDEX],
				},
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.locality = VK_LOCALITY_CONTEXT,
			};
			vkCreateTexture(&normalCreateInfo, &pNodeContext->normalFramebuffers[i]);

			VkTextureCreateInfo depthCreateInfo = {
				.debugName = "ImportedDepthFramebuffer",
				.pImageCreateInfo = &(VkImageCreateInfo){
					.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.pNext = VK_EXTERNAL_IMAGE_CREATE_INFO_PLATFORM,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = VK_BASIC_PASS_FORMATS[VK_BASIC_PASS_ATTACHMENT_DEPTH_INDEX],
					.extent = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1.0f},
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = VK_BASIC_PASS_USAGES[VK_BASIC_PASS_ATTACHMENT_DEPTH_INDEX],
				},
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.locality = VK_LOCALITY_CONTEXT,
			};
			vkCreateTexture(&depthCreateInfo, &pNodeContext->depthFramebuffers[i]);

		}

		// this gbuffer manipulation needs to happen outside of the node in the compositor to prepare the data
//		for (uint32_t bufferIndex = 0; bufferIndex < VK_SWAP_COUNT; ++bufferIndex) {
//			for (uint32_t mipIndex = 0; mipIndex < MXC_NODE_GBUFFER_LEVELS; ++mipIndex) {
//				VkImageViewCreateInfo imageViewCreateInfo = {
//					.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
//					.image = pNodeContext->gbufferFramebuffers[bufferIndex].image,
//					.viewType = VK_IMAGE_VIEW_TYPE_2D,
//					.format = MXC_NODE_GBUFFER_FORMAT,
//					.subresourceRange = {
//						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
//						.baseMipLevel = mipIndex,
//						.levelCount = 1,
//						.layerCount = 1,
//					},
//				};
//				VK_CHECK(vkCreateImageView(vk.context.device, &imageViewCreateInfo, VK_ALLOC, &pTestNode->gBufferMipViews[bufferIndex][mipIndex]));
//			}
//		}

		// This is way too many descriptors... optimize this
		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
			.maxSets = 30,
			.poolSizeCount = 3,
			.pPoolSizes = (VkDescriptorPoolSize[]){
				{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 10},
				{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 10},
				{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 10},
			},
		};
		VK_CHECK(vkCreateDescriptorPool(vk.context.device, &descriptorPoolCreateInfo, VK_ALLOC, &threadContext.descriptorPool));

		vkCreateGlobalSet(&pTestNode->globalSet);

		vkAllocateDescriptorSet(threadContext.descriptorPool, &vk.context.basicPipeLayout.materialSetLayout, &pTestNode->checkerMaterialSet);
		vkCreateTextureFromFile("textures/uvgrid.jpg", &pTestNode->checkerTexture);

		vkAllocateDescriptorSet(threadContext.descriptorPool, &vk.context.basicPipeLayout.objectSetLayout, &pTestNode->sphereObjectSet);
		vkCreateAllocateBindMapBuffer(
			VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
			sizeof(VkObjectSetState),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_LOCALITY_CONTEXT,
			&pTestNode->sphereObjectSetMemory,
			&pTestNode->sphereObjectSetBuffer,
			(void**)&pTestNode->pSphereObjectSetMapped);

		VkWriteDescriptorSet writeSets[] = {
			KM_SET_WRITE_MATERIAL_IMAGE(pTestNode->checkerMaterialSet, pTestNode->checkerTexture.view),
			VK_SET_WRITE_OBJECT_BUFFER(pTestNode->sphereObjectSet, pTestNode->sphereObjectSetBuffer),
		};
		vkUpdateDescriptorSets(vk.context.device, _countof(writeSets), writeSets, 0, NULL);

		pTestNode->sphereTransform = (MidPose){.position = {0, 0, -4}};
		vkUpdateObjectSet(&pTestNode->sphereTransform, &pTestNode->sphereObjectState, pTestNode->pSphereObjectSetMapped);

		CreateSphereMesh(0.5, 32, 32, &pTestNode->sphereMesh);
	}

	{  // Copy needed state
		// context is available to all now so don't need to do this
		pTestNode->device = vk.context.device;
		pTestNode->nodeRenderPass = vk.context.nodeRenderPass;
		pTestNode->pipeLayout = vk.context.basicPipeLayout.pipeLayout;
		pTestNode->basicPipe = vk.context.basicPipe;
	}
}

void* mxcTestNodeThread(MxcNodeContext* pNodeContext)
{
	auto pNodeShared = pNodeContext->pNodeShared;
	pNodeShared->swapType = MXC_SWAP_TYPE_SINGLE;
	pNodeShared->swapUsage = MXC_SWAP_USAGE_COLOR_AND_DEPTH;

	MxcTestNode testNode;
	mxcCreateTestNode(pNodeContext, &testNode);
	mxcTestNodeRun(pNodeContext, &testNode);

	return NULL;
}