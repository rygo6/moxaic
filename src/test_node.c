#include "test_node.h"
#include "mid_shape.h"
#include <assert.h>

enum SetBindNodeProcessIndices {
	SET_BIND_NODE_PROCESS_SRC_INDEX,
	SET_BIND_NODE_PROCESS_DST_INDEX,
	SET_BIND_NODE_PROCESS_COUNT
};
#define SET_WRITE_NODE_PROCESS_SRC(src_image_view)                   \
	(const VkWriteDescriptorSet)                                     \
	{                                                                \
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,             \
		.dstBinding = SET_BIND_NODE_PROCESS_SRC_INDEX,               \
		.descriptorCount = 1,                                        \
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
		.pImageInfo = &(const VkDescriptorImageInfo){                \
			.imageView = src_image_view,                             \
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,                  \
		},                                                           \
	}
#define SET_WRITE_NODE_PROCESS_DST(dst_image_view)          \
	(const VkWriteDescriptorSet)                            \
	{                                                       \
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,    \
		.dstBinding = SET_BIND_NODE_PROCESS_DST_INDEX,      \
		.descriptorCount = 1,                               \
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, \
		.pImageInfo = &(const VkDescriptorImageInfo){       \
			.imageView = dst_image_view,                    \
			.imageLayout = VK_IMAGE_LAYOUT_GENERAL,         \
		},                                                  \
	}
enum PipeSetNodeProcessIndices {
	PIPE_SET_NODE_PROCESS_INDEX,
	PIPE_SET_NODE_PROCESS_COUNT,
};

static const MidVkSrcDstImageBarrier* VKM_IMG_BARRIER_NODE_FINISH_RENDERPASS = &(const MidVkSrcDstImageBarrier){
	.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
	.accessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
	.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
};
#define IMAGE_BARRIER_SRC_NODE_FINISH_RENDERPASS                         \
	.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, \
	.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,         \
	.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
#define IMAGE_BARRIER_DST_NODE_RELEASE                    \
	.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, \
	.dstAccessMask = VK_ACCESS_2_NONE,                    \
	.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL

void mxcTestNodeRun(const MxcNodeContext* pNodeContext, const MxcTestNode* pNode)
{
	MxcNodeShared* pNodeShared = pNodeContext->pNodeShared;

	const VkCommandBuffer cmd = pNodeContext->cmd;
	const VkRenderPass    nodeRenderPass = pNode->nodeRenderPass;
	const VkFramebuffer   framebuffer = pNode->framebuffer;

	VkmGlobalSetState*    pGlobalSetMapped = pNode->globalSet.pMapped;
	const VkDescriptorSet globalSet = pNode->globalSet.set;

	const VkDescriptorSet  checkerMaterialSet = pNode->checkerMaterialSet;
	const VkDescriptorSet  sphereObjectSet = pNode->sphereObjectSet;
	const VkPipelineLayout pipeLayout = pNode->pipeLayout;
	const VkPipeline       pipe = pNode->basicPipe;
	const VkPipelineLayout nodeProcessPipeLayout = pNode->nodeProcessPipeLayout;
	const VkPipeline       nodeProcessBlitMipAveragePipe = pNode->nodeProcessBlitMipAveragePipe;
	const VkPipeline       nodeProcessBlitDownPipe = pNode->nodeProcessBlitDownPipe;

	const uint32_t graphicsQueueIndex = midVk.context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index;

	struct {
		VkImage     color;
		VkImage     normal;
		VkImage     gBuffer;
		VkImage     depth;
		VkImageView colorView;
		VkImageView normalView;
		VkImageView depthView;
		VkImageView gBufferView;
	} framebufferImages[MIDVK_SWAP_COUNT];
	for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
		framebufferImages[i].color = pNodeContext->vkNodeFramebufferTextures[i].color.image;
		framebufferImages[i].normal = pNodeContext->vkNodeFramebufferTextures[i].normal.image;
		framebufferImages[i].gBuffer = pNodeContext->vkNodeFramebufferTextures[i].gbuffer.image;
		framebufferImages[i].depth = pNodeContext->vkNodeFramebufferTextures[i].depth.image;
		framebufferImages[i].colorView = pNodeContext->vkNodeFramebufferTextures[i].color.view;
		framebufferImages[i].normalView = pNodeContext->vkNodeFramebufferTextures[i].normal.view;
		framebufferImages[i].depthView = pNodeContext->vkNodeFramebufferTextures[i].depth.view;
		framebufferImages[i].gBufferView = pNodeContext->vkNodeFramebufferTextures[i].gbuffer.view;
	}
	VkImageView gBufferMipViews[MIDVK_SWAP_COUNT][MXC_NODE_GBUFFER_LEVELS];
	memcpy(&gBufferMipViews, &pNode->gBufferMipViews, sizeof(gBufferMipViews));

	const uint32_t     sphereIndexCount = pNode->sphereMesh.indexCount;
	const VkBuffer     sphereBuffer = pNode->sphereMesh.buffer;
	const VkDeviceSize sphereIndexOffset = pNode->sphereMesh.indexOffset;
	const VkDeviceSize sphereVertexOffset = pNode->sphereMesh.vertexOffset;

	const VkDevice device = pNode->device;

	const VkSemaphore compTimeline = pNodeContext->vkCompositorTimeline;
	const VkSemaphore nodeTimeline = pNodeContext->vkNodeTimeline;
	uint64_t nodeTimelineValue = 0;

	{
		uint64_t compositorTimelineValue;
		MIDVK_REQUIRE(vkGetSemaphoreCounterValue(device, compTimeline, &compositorTimelineValue));
		REQUIRE(compositorTimelineValue != 0xffffffffffffffff, "compositorTimelineValue imported as max value!");
		pNodeShared->compositorBaseCycleValue = compositorTimelineValue - (compositorTimelineValue % MXC_CYCLE_COUNT);
	}

	assert(__atomic_always_lock_free(sizeof(pNodeShared->nodeCurrentTimelineSignal), &pNodeShared->nodeCurrentTimelineSignal));

	VkImageMemoryBarrier2 acquireBarriers[MIDVK_SWAP_COUNT][3];
	uint32_t              acquireBarrierCount = 0;
	VkImageMemoryBarrier2 releaseBarriers[MIDVK_SWAP_COUNT][3];
	uint32_t              releaseBarrierCount = 0;
	for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
		switch (pNodeContext->type) {
			case MXC_NODE_TYPE_THREAD:
				// Acquire not needed from thread.
				releaseBarrierCount = 1;
				releaseBarriers[i][0] = (VkImageMemoryBarrier2){
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					MIDVK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
					IMAGE_BARRIER_DST_NODE_RELEASE,
					MIDVK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
					.image = framebufferImages[i].gBuffer,
					.subresourceRange = MIDVK_COLOR_SUBRESOURCE_RANGE,
				};
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
					.image = framebufferImages[i].color,
					.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
				};
				acquireBarriers[i][1] = (VkImageMemoryBarrier2){
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
					.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
					.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
					.dstQueueFamilyIndex = graphicsQueueIndex,
					.image = framebufferImages[i].normal,
					.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
				};
				acquireBarriers[i][2] = (VkImageMemoryBarrier2){
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
					.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
					.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_GENERAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
					.dstQueueFamilyIndex = graphicsQueueIndex,
					.image = framebufferImages[i].gBuffer,
					.subresourceRange = MIDVK_COLOR_SUBRESOURCE_RANGE,
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
					.image = framebufferImages[i].color,
					.subresourceRange = MIDVK_COLOR_SUBRESOURCE_RANGE,
				};
				releaseBarriers[i][1] = (VkImageMemoryBarrier2){
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
					.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
					.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = graphicsQueueIndex,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
					.image = framebufferImages[i].normal,
					.subresourceRange = MIDVK_COLOR_SUBRESOURCE_RANGE,
				};
				releaseBarriers[i][2] = (VkImageMemoryBarrier2){
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
					.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = graphicsQueueIndex,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL,
					.image = framebufferImages[i].gBuffer,
					.subresourceRange = MIDVK_COLOR_SUBRESOURCE_RANGE,
				};
				break;
			case MXC_NODE_TYPE_INTERPROCESS_VULKAN_EXPORTED: PANIC("Shouldn't be rendering an exported node from this process.");
			default:                                         PANIC("nodeType not supported");
		}
	}

	// I probably want to be able to define this into a struct
	// and optionally have it be a static struct
	MIDVK_DEVICE_FUNC(ResetCommandBuffer);
	MIDVK_DEVICE_FUNC(BeginCommandBuffer);
	MIDVK_DEVICE_FUNC(CmdSetViewport);
	MIDVK_DEVICE_FUNC(CmdBeginRenderPass);
	MIDVK_DEVICE_FUNC(CmdSetScissor);
	MIDVK_DEVICE_FUNC(CmdBindPipeline);
	MIDVK_DEVICE_FUNC(CmdDispatch);
	MIDVK_DEVICE_FUNC(CmdBindDescriptorSets);
	MIDVK_DEVICE_FUNC(CmdBindVertexBuffers);
	MIDVK_DEVICE_FUNC(CmdBindIndexBuffer);
	MIDVK_DEVICE_FUNC(CmdDrawIndexed);
	MIDVK_DEVICE_FUNC(CmdEndRenderPass);
	MIDVK_DEVICE_FUNC(EndCommandBuffer);
	MIDVK_DEVICE_FUNC(CmdPipelineBarrier2);
	MIDVK_DEVICE_FUNC(CmdPushDescriptorSetKHR);
	MIDVK_DEVICE_FUNC(CmdClearColorImage);

run_loop:

	midVkTimelineWait(device, pNodeShared->compositorBaseCycleValue + MXC_CYCLE_COMPOSITOR_RECORD, compTimeline);

	__atomic_thread_fence(__ATOMIC_ACQUIRE);
	memcpy(pGlobalSetMapped, (void*)&pNodeShared->nodeGlobalSetState, sizeof(VkmGlobalSetState));

	ResetCommandBuffer(cmd, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
	BeginCommandBuffer(cmd, &(const VkCommandBufferBeginInfo){.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT});

	const int framebufferIndex = nodeTimelineValue % MIDVK_SWAP_COUNT;

	{
		const VkViewport viewport = {
			.x = -pNodeShared->compositorULScreenUV.x * DEFAULT_WIDTH,
			.y = -pNodeShared->compositorULScreenUV.y * DEFAULT_HEIGHT,
			.width = DEFAULT_WIDTH,
			.height = DEFAULT_HEIGHT,
			.maxDepth = 1.0f,
		};
		const VkRect2D scissor = {
			.offset = {
				.x = 0,
				.y = 0,
			},
			.extent = {
				.width = pNodeShared->nodeGlobalSetState.framebufferSize.x,
				.height = pNodeShared->nodeGlobalSetState.framebufferSize.y,
			},
		};
		CmdSetViewport(cmd, 0, 1, &viewport);
		CmdSetScissor(cmd, 0, 1, &scissor);
		CmdPipelineImageBarriers2(cmd, acquireBarrierCount, acquireBarriers[framebufferIndex]);

		const VkClearColorValue clearColor = (VkClearColorValue){0, 0, 0.1, 0};
		CmdBeginRenderPass(cmd, nodeRenderPass, framebuffer, clearColor, framebufferImages[framebufferIndex].colorView, framebufferImages[framebufferIndex].normalView, framebufferImages[framebufferIndex].depthView);

		// this is really all that'd be user exposed....
		CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
		CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout, MIDVK_PIPE_SET_INDEX_GLOBAL, 1, &globalSet, 0, NULL);
		CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout, MIDVK_PIPE_SET_INDEX_MATERIAL, 1, &checkerMaterialSet, 0, NULL);
		CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout, MIDVK_PIPE_SET_OBJECT_INDEX, 1, &sphereObjectSet, 0, NULL);

		CmdBindVertexBuffers(cmd, 0, 1, (const VkBuffer[]){sphereBuffer}, (const VkDeviceSize[]){sphereVertexOffset});
		CmdBindIndexBuffer(cmd, sphereBuffer, sphereIndexOffset, VK_INDEX_TYPE_UINT16);
		CmdDrawIndexed(cmd, sphereIndexCount, 1, 0, 0, 0);

		CmdEndRenderPass(cmd);
	}

	{  // Blit GBuffer
		const ivec2 extent = {pNodeShared->nodeGlobalSetState.framebufferSize.x, pNodeShared->nodeGlobalSetState.framebufferSize.y};
		const ivec2 groupCount = iVec2Min(iVec2CeiDivide(extent, 32), 1);
		{
			const VkImageMemoryBarrier2 clearBarrier = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				MIDVK_IMAGE_BARRIER_SRC_UNDEFINED,
				MIDVK_IMAGE_BARRIER_DST_TRANSFER_WRITE,
				MIDVK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
				.image = framebufferImages[framebufferIndex].gBuffer,
				.subresourceRange = MIDVK_COLOR_SUBRESOURCE_RANGE,
			};
			CmdPipelineImageBarrier2(cmd, &clearBarrier);
			CmdClearColorImage(cmd, framebufferImages[framebufferIndex].gBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &MXC_NODE_CLEAR_COLOR, 1, &MIDVK_COLOR_SUBRESOURCE_RANGE);
			const VkImageMemoryBarrier2 beginComputeBarriers[] = {
				// could technically alter shader to take VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL and not need this, can different mips be in different layouts?
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					.image = framebufferImages[framebufferIndex].depth,
					IMAGE_BARRIER_SRC_NODE_FINISH_RENDERPASS,
					MIDVK_IMAGE_BARRIER_DST_COMPUTE_READ,
					MIDVK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
					.subresourceRange = MIDVK_DEPTH_SUBRESOURCE_RANGE,
				},
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					MIDVK_IMAGE_BARRIER_SRC_TRANSFER_WRITE,
					MIDVK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
					MIDVK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
					.image = framebufferImages[framebufferIndex].gBuffer,
					.subresourceRange = MIDVK_COLOR_SUBRESOURCE_RANGE,
				},
			};
			CmdPipelineImageBarriers2(cmd, COUNT(beginComputeBarriers), beginComputeBarriers);
			const VkWriteDescriptorSet initialPushSet[] = {
				SET_WRITE_NODE_PROCESS_SRC(framebufferImages[framebufferIndex].depthView),
				SET_WRITE_NODE_PROCESS_DST(framebufferImages[framebufferIndex].gBufferView),
			};
			CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeProcessBlitMipAveragePipe);
			CmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeProcessPipeLayout, PIPE_SET_NODE_PROCESS_INDEX, COUNT(initialPushSet), initialPushSet);
			CmdDispatch(cmd, groupCount.x, groupCount.y, 1);
		}

		for (uint32_t i = 1; i < MXC_NODE_GBUFFER_LEVELS; ++i) {
			const VkImageMemoryBarrier2 barriers[] = {
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					MIDVK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
					MIDVK_IMAGE_BARRIER_DST_COMPUTE_READ,
					MIDVK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
					.image = framebufferImages[framebufferIndex].gBuffer,
					.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = i - 1, .levelCount = 1, .layerCount = VK_REMAINING_ARRAY_LAYERS},
				},
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					MIDVK_IMAGE_BARRIER_SRC_COMPUTE_READ,
					MIDVK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
					MIDVK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
					.image = framebufferImages[framebufferIndex].gBuffer,
					.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 1, .levelCount = 1, .layerCount = VK_REMAINING_ARRAY_LAYERS},
				},
			};
			CmdPipelineImageBarriers2(cmd, COUNT(barriers), barriers);
			const VkWriteDescriptorSet pushSet[] = {
				SET_WRITE_NODE_PROCESS_SRC(gBufferMipViews[framebufferIndex][i - 1]),
				SET_WRITE_NODE_PROCESS_DST(gBufferMipViews[framebufferIndex][i]),
			};
			CmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeProcessPipeLayout, PIPE_SET_NODE_PROCESS_INDEX, COUNT(pushSet), pushSet);
			const ivec2 mipGroupCount = iVec2Min(iVec2CeiDivide((ivec2){extent.vec >> 1}, 32), 1);
			CmdDispatch(cmd, mipGroupCount.x, mipGroupCount.y, 1);
		}

		CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeProcessBlitDownPipe);
		for (uint32_t i = MXC_NODE_GBUFFER_LEVELS - 1; i > 0; --i) {
			const VkImageMemoryBarrier2 barriers[] = {
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					MIDVK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
					MIDVK_IMAGE_BARRIER_DST_COMPUTE_READ,
					MIDVK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
					.image = framebufferImages[framebufferIndex].gBuffer,
					.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = i, .levelCount = 1, .layerCount = VK_REMAINING_ARRAY_LAYERS},
				},
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
					MIDVK_IMAGE_BARRIER_SRC_COMPUTE_READ,
					MIDVK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
					MIDVK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
					.image = framebufferImages[framebufferIndex].gBuffer,
					.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = i - 1, .levelCount = 1, .layerCount = VK_REMAINING_ARRAY_LAYERS},
				},
			};
			CmdPipelineImageBarriers2(cmd, COUNT(barriers), barriers);
			const VkWriteDescriptorSet pushSet[] = {
				SET_WRITE_NODE_PROCESS_SRC(gBufferMipViews[framebufferIndex][i]),
				SET_WRITE_NODE_PROCESS_DST(gBufferMipViews[framebufferIndex][i - 1]),
			};
			CmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeProcessPipeLayout, PIPE_SET_NODE_PROCESS_INDEX, COUNT(pushSet), pushSet);
			const ivec2 mipGroupCount = iVec2Min(iVec2CeiDivide((ivec2){extent.vec >> (1 - 1)}, 32), 1);
			CmdDispatch(cmd, mipGroupCount.x, mipGroupCount.y, 1);
		}
	}

	CmdPipelineImageBarriers2(cmd, releaseBarrierCount, releaseBarriers[framebufferIndex]);
	EndCommandBuffer(cmd);

	nodeTimelineValue++;
	mxcQueueNodeCommandBuffer((MxcQueuedNodeCommandBuffer){.cmd = cmd, .nodeTimeline = nodeTimeline, .nodeTimelineSignalValue = nodeTimelineValue});
	midVkTimelineWait(device, nodeTimelineValue, nodeTimeline);

	// tests show reading from shared memory is 500~ x faster than vkGetSemaphoreCounterValue
	// shared: 569 - semaphore: 315416 ratio: 554.333919
	pNodeShared->nodeCurrentTimelineSignal = nodeTimelineValue;
	__atomic_thread_fence(__ATOMIC_RELEASE);

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
	const VkDescriptorSetLayoutCreateInfo nodeSetLayoutCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
		.bindingCount = SET_BIND_NODE_PROCESS_COUNT,
		.pBindings = (const VkDescriptorSetLayoutBinding[]){
			[SET_BIND_NODE_PROCESS_SRC_INDEX] = {
				.binding = SET_BIND_NODE_PROCESS_SRC_INDEX,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
				.pImmutableSamplers = &midVk.context.linearSampler,
			},
			[SET_BIND_NODE_PROCESS_DST_INDEX] = {
				.binding = SET_BIND_NODE_PROCESS_DST_INDEX,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			},
		},
	};
	MIDVK_REQUIRE(vkCreateDescriptorSetLayout(midVk.context.device, &nodeSetLayoutCreateInfo, MIDVK_ALLOC, pLayout));
}

static void CreateNodeProcessPipeLayout(const VkDescriptorSetLayout nodeProcessSetLayout, VkPipelineLayout* pPipeLayout)
{
	const VkPipelineLayoutCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = (const VkDescriptorSetLayout[]){
			[PIPE_SET_NODE_PROCESS_INDEX] = nodeProcessSetLayout,
		},
	};
	MIDVK_REQUIRE(vkCreatePipelineLayout(midVk.context.device, &createInfo, MIDVK_ALLOC, pPipeLayout));
}
static void CreateNodeProcessPipe(const char* shaderPath, const VkPipelineLayout layout, VkPipeline* pPipe)
{
	VkShaderModule shader;
	midVkCreateShaderModule(shaderPath, &shader);
	const VkComputePipelineCreateInfo pipelineInfo = {
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.stage = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_COMPUTE_BIT,
			.module = shader,
			.pName = "main",
		},
		.layout = layout,
	};
	MIDVK_REQUIRE(vkCreateComputePipelines(midVk.context.device, VK_NULL_HANDLE, 1, &pipelineInfo, MIDVK_ALLOC, pPipe));
	vkDestroyShaderModule(midVk.context.device, shader, MIDVK_ALLOC);
}
static void mxcCreateTestNode(const MxcNodeContext* pTestNodeContext, MxcTestNode* pTestNode)
{
	{  // Create
		CreateNodeProcessSetLayout(&pTestNode->nodeProcessSetLayout);
		CreateNodeProcessPipeLayout(pTestNode->nodeProcessSetLayout, &pTestNode->nodeProcessPipeLayout);
		CreateNodeProcessPipe("./shaders/node_process_blit_slope_average_up.comp.spv", pTestNode->nodeProcessPipeLayout, &pTestNode->nodeProcessBlitMipAveragePipe);
		CreateNodeProcessPipe("./shaders/node_process_blit_down_alpha_omit.comp.spv", pTestNode->nodeProcessPipeLayout, &pTestNode->nodeProcessBlitDownPipe);

		midVkCreateFramebuffer(midVk.context.nodeRenderPass, &pTestNode->framebuffer);
		midVkSetDebugName(VK_OBJECT_TYPE_FRAMEBUFFER, (uint64_t)pTestNode->framebuffer, "TestNodeFramebuffer");

		for (uint32_t bufferIndex = 0; bufferIndex < MIDVK_SWAP_COUNT; ++bufferIndex) {
			for (uint32_t mipIndex = 0; mipIndex < MXC_NODE_GBUFFER_LEVELS; ++mipIndex) {
				const VkImageViewCreateInfo imageViewCreateInfo = {
					.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
					.image = pTestNodeContext->vkNodeFramebufferTextures[bufferIndex].gbuffer.image,
					.viewType = VK_IMAGE_VIEW_TYPE_2D,
					.format = MXC_NODE_GBUFFER_FORMAT,
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = mipIndex,
						.levelCount = 1,
						.layerCount = 1,
					},
				};
				MIDVK_REQUIRE(vkCreateImageView(midVk.context.device, &imageViewCreateInfo, MIDVK_ALLOC, &pTestNode->gBufferMipViews[bufferIndex][mipIndex]));
			}
		}

		// This is way too many descriptors... optimize this
		const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
			.maxSets = 30,
			.poolSizeCount = 3,
			.pPoolSizes = (const VkDescriptorPoolSize[]){
				{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 10},
				{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 10},
				{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 10},
			},
		};
		MIDVK_REQUIRE(vkCreateDescriptorPool(midVk.context.device, &descriptorPoolCreateInfo, MIDVK_ALLOC, &threadContext.descriptorPool));

		midVkCreateGlobalSet(&pTestNode->globalSet);

		midVkAllocateDescriptorSet(threadContext.descriptorPool, &midVk.context.basicPipeLayout.materialSetLayout, &pTestNode->checkerMaterialSet);
		midVkCreateTextureFromFile("textures/uvgrid.jpg", &pTestNode->checkerTexture);

		midVkAllocateDescriptorSet(threadContext.descriptorPool, &midVk.context.basicPipeLayout.objectSetLayout, &pTestNode->sphereObjectSet);
		midVkCreateAllocBindMapBuffer(VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(VkmStdObjectSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, MID_LOCALITY_CONTEXT, &pTestNode->sphereObjectSetMemory, &pTestNode->sphereObjectSetBuffer, (void**)&pTestNode->pSphereObjectSetMapped);

		const VkWriteDescriptorSet writeSets[] = {
			VKM_SET_WRITE_STD_MATERIAL_IMAGE(pTestNode->checkerMaterialSet, pTestNode->checkerTexture.view),
			VKM_SET_WRITE_STD_OBJECT_BUFFER(pTestNode->sphereObjectSet, pTestNode->sphereObjectSetBuffer),
		};
		vkUpdateDescriptorSets(midVk.context.device, _countof(writeSets), writeSets, 0, NULL);

		pTestNode->sphereTransform = (MidPose){.position = {0, 0, 0}};
		vkmUpdateObjectSet(&pTestNode->sphereTransform, &pTestNode->sphereObjectState, pTestNode->pSphereObjectSetMapped);

		CreateSphereMesh(0.5, 32, 32, &pTestNode->sphereMesh);
	}

	{  // Copy needed state
		// context is available to all now so don't need to do this
		pTestNode->device = midVk.context.device;
		pTestNode->nodeRenderPass = midVk.context.nodeRenderPass;
		pTestNode->pipeLayout = midVk.context.basicPipeLayout.pipeLayout;
		pTestNode->basicPipe = midVk.context.basicPipe;
	}
}

void* mxcTestNodeThread(const MxcNodeContext* pNodeContext)
{
	MxcTestNode testNode;
	mxcCreateTestNode(pNodeContext, &testNode);
	mxcTestNodeRun(pNodeContext, &testNode);
	return NULL;
}