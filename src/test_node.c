#if defined(MOXAIC_COMPOSITOR)

#include <assert.h>
#include <stdatomic.h>

#include "mid_shape.h"
#include "mid_openxr_runtime.h"
#include "test_node.h"


#define IMAGE_BARRIER_SRC_NODE_FINISH_RENDERPASS                         \
	.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, \
	.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,         \
	.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
#define IMAGE_BARRIER_DST_NODE_RELEASE                    \
	.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, \
	.dstAccessMask = VK_ACCESS_2_NONE,                    \
	.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL

//----------------------------------------------------------------------------------
// Loop
//----------------------------------------------------------------------------------
void mxcTestNodeRun(MxcNodeContext* pNodeCtx ,MxcNodeShared* pNodeShr, MxcTestNode* pNode)
{
	NodeHandle hNode = pNodeCtx->hNode;

	auto nodeType = pNodeCtx->interprocessMode;

	EXTRACT_FIELD(&vk.context, device);
	EXTRACT_FIELD(&vk.context, depthRenderPass);
	EXTRACT_FIELD(&vk.context, depthFramebuffer);

	EXTRACT_FIELD(pNode, framebufferTexture);

	VkCommandBuffer gfxCmd = pNodeCtx->thread.gfxCmd;

	auto              pGlobalSetMapped = vkSharedMemoryPtr(pNode->globalBuffer.memory);
	VkDescriptorSet   globalSet = pNode->globalSet;

	VkDescriptorSet  checkerMaterialSet = pNode->checkerMaterialSet;
	VkDescriptorSet  sphereObjectSet = pNode->sphereObjectSet;
	VkPipelineLayout pipeLayout = vk.context.trianglePipeLayout;
	VkPipeline       pipe = vk.context.trianglePipe;

	u32 gfxQueueIndex = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index;

	u32          sphereIndexCount = pNode->sphereMesh.offsets.indexCount;
	VkBuffer     sphereBuffer = pNode->sphereMesh.buf;
	VkDeviceSize sphereIndexOffset = pNode->sphereMesh.offsets.indexOffset;
	VkDeviceSize sphereVertexOffset = pNode->sphereMesh.offsets.vertexOffset;

	VkSemaphore cstTimeline = compositorContext.timeline;
	VkSemaphore nodeTimeline = pNodeCtx->thread.nodeTimeline;
	u64 nodeTimelineValue = 0;

	ASSERT(cstTimeline != NULL, "Compositor Timeline Handle is nulL!");
	ASSERT(nodeTimeline != NULL, "Node Timeline Handle is nulL!");

	struct {
		VkImageView colorView;
		VkImageView depthView;
		VkImage     depthImage;
	} swaps[XR_SWAPCHAIN_IMAGE_COUNT] ;

	{
		const int colorSwapId = 0;
		XrSwapchainInfo colorInfo = {
			.createFlags = 0,
			.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
			.windowWidth = DEFAULT_WIDTH,
			.windowHeight = DEFAULT_HEIGHT,
			.format = VK_FORMAT_R8G8B8A8_UNORM,
			.sampleCount = 1,
			.faceCount = 1,
			.arraySize = 1,
			.mipCount = 1,
		};
		// I think I want this to be a thread_node and not go through openxr constructs at all
		xrCreateSwapchainImages(hNode, &colorInfo, colorSwapId);
		pNodeShr->viewSwaps[XR_VIEW_ID_CENTER_MONO].colorId = colorSwapId;

		const int depthSwapId = 1;
		XrSwapchainInfo depthInfo = {
			.createFlags = 0,
			.usageFlags = XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			.windowWidth = DEFAULT_WIDTH,
			.windowHeight = DEFAULT_HEIGHT,
			.format = VK_FORMAT_D16_UNORM,
			.sampleCount = 1,
			.faceCount = 1,
			.arraySize = 1,
			.mipCount = 1,
		};
		xrCreateSwapchainImages(hNode, &depthInfo, depthSwapId);
		pNodeShr->viewSwaps[XR_VIEW_ID_CENTER_MONO].depthId = depthSwapId;

		assert(pNodeShr->swapStates[colorSwapId] == XR_SWAP_STATE_CREATED && "Color swap not created!");
		assert(pNodeShr->swapStates[depthSwapId] == XR_SWAP_STATE_CREATED && "Depth swap not created!");

		swap_h hColorSwap = pNodeCtx->hSwaps[colorSwapId];
		auto pColorSwap = BLOCK_PTR(node.cst.block.swap, hColorSwap);\
		swap_h hDepthSwap = pNodeCtx->hSwaps[depthSwapId];
		auto pDepthSwap = BLOCK_PTR(node.cst.block.swap, hDepthSwap);
		for (int iImg = 0; iImg < XR_SWAPCHAIN_IMAGE_COUNT; ++iImg) {
			swaps[iImg].colorView = pColorSwap->externalTexture[iImg].texture.view;
			swaps[iImg].depthView = pDepthSwap->externalTexture[iImg].texture.view;
			swaps[iImg].depthImage = pDepthSwap->externalTexture[iImg].texture.image;
		}
	}

	{
		uint64_t compositorTimelineValue;
		VK_CHECK(vk.GetSemaphoreCounterValue(device, cstTimeline, &compositorTimelineValue));
		REQUIRE(compositorTimelineValue != 0xffffffffffffffff, "compositorTimelineValue imported as max value!");
		u64 timelineCycleStartValue = compositorTimelineValue - (compositorTimelineValue % MXC_CYCLE_COUNT);
		pNodeShr->compositorBaseCycleValue = timelineCycleStartValue + MXC_CYCLE_COUNT;
	}

	// I probably want to be able to define this into a struct
	// and optionally have it be a static struct
	VK_DEVICE_FUNC(ResetCommandBuffer);
	VK_DEVICE_FUNC(BeginCommandBuffer);
	VK_DEVICE_FUNC(CmdSetViewport);
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

	vkTimelineWait(device, pNodeShr->compositorBaseCycleValue + MXC_CYCLE_UPDATE_WINDOW_STATE, cstTimeline);
	ReleaseNodeActive(hNode);
	SetNodeActive(hNode, pNodeShr->compositorMode);

run_loop:

	vkTimelineWait(device, pNodeShr->compositorBaseCycleValue + MXC_CYCLE_COMPOSITOR_RECORD, cstTimeline);

	atomic_thread_fence(memory_order_acquire);
	memcpy(pGlobalSetMapped, (void*)&pNodeShr->globalSetState, sizeof(VkGlobalSetState));

	ResetCommandBuffer(gfxCmd, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
	BeginCommandBuffer(gfxCmd, &(VkCommandBufferBeginInfo){VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT});

	int framebufferIndex = nodeTimelineValue % VK_SWAP_COUNT;

	// Clipped Viewport
//	VkViewport viewport = {.x = -pNodeShr->clip.ulUV.x * DEFAULT_WIDTH,	.y = -pNodeShr->clip.ulUV.y * DEFAULT_HEIGHT, .width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT, .maxDepth = 1.0f};
//	VkRect2D scissor = {.offset = {.x = 0, .y = 0}, .extent = {.width = pNodeShr->globalSetState.framebufferSize.x, .height = pNodeShr->globalSetState.framebufferSize.y}};
	VkViewport viewport = {.width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT, .maxDepth = 1.0f};
	VkRect2D   scissor = {.extent = {.width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT}};
	CmdSetViewport(gfxCmd, 0, 1, &viewport);
	CmdSetScissor(gfxCmd, 0, 1, &scissor);
//	CmdPipelineImageBarriers2(gfxCmd, acquireBarrierCount, acquireBarriers[framebufferIndex]);

	VkClearColorValue clearColor = (VkClearColorValue){{0, 0, 0.1f, 0}};
	CmdBeginDepthRenderPass(gfxCmd, depthRenderPass, depthFramebuffer, clearColor, swaps[framebufferIndex].colorView, framebufferTexture.depth.view);

	/// Draw
	{
		CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
		CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout, VK_PIPE_SET_INDEX_GLOBAL, 1, &globalSet, 0, NULL);
		CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout, VK_PIPE_SET_INDEX_MATERIAL, 1, &checkerMaterialSet, 0, NULL);
		CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout, VK_PIPE_SET_INDEX_OBJECT, 1, &sphereObjectSet, 0, NULL);

		CmdBindVertexBuffers(gfxCmd, 0, 1, (VkBuffer[]){sphereBuffer}, (VkDeviceSize[]){sphereVertexOffset});
		CmdBindIndexBuffer(gfxCmd, sphereBuffer, sphereIndexOffset, VK_INDEX_TYPE_UINT16);
		CmdDrawIndexed(gfxCmd, sphereIndexCount, 1, 0, 0, 0);
	}

	CmdEndRenderPass(gfxCmd);

	CmdBlitImageFullScreen(gfxCmd, framebufferTexture.depth.image,  swaps[framebufferIndex].depthImage);

//	CmdPipelineImageBarriers2(gfxCmd, releaseBarrierCount, releaseBarriers[framebufferIndex]);
	EndCommandBuffer(gfxCmd);

	// Submit Node to Render
	nodeTimelineValue++;
	mxcQueueNodeCommandBuffer((MxcQueuedNodeCommandBuffer){
		.cmd = gfxCmd,
		.nodeTimeline = nodeTimeline,
		.nodeTimelineSignalValue = nodeTimelineValue,
	});
	vkTimelineWait(device, nodeTimelineValue, nodeTimeline);

	// Signal Updated to Compositor
	pNodeShr->timelineValue = nodeTimelineValue;
	atomic_thread_fence(memory_order_release);

	pNodeShr->compositorBaseCycleValue += MXC_CYCLE_COUNT * pNodeShr->compositorCycleSkip;

	//    _Thread_local static int count;
	//    if (count++ > 10)
	//      return;

	CHECK_RUNNING
	goto run_loop;
}

//----------------------------------------------------------------------------------
// Create
//----------------------------------------------------------------------------------
static void Create(MxcNodeContext* pNodeContext, MxcNodeShared* pNodeShr, MxcTestNode* pNode)
{
	LOG("Creating Thread Node\n");

	// This is way too many descriptors... optimize this
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
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

	vkAllocateDescriptorSet(threadContext.descriptorPool, &vk.context.globalSetLayout, &pNode->globalSet);
	VkRequestAllocationInfo requestInfo = {
		.memoryPropertyFlags = VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
		.size = sizeof(VkGlobalSetState),
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
	};
	vkCreateSharedBuffer(&requestInfo, &pNode->globalBuffer);

	vkAllocateDescriptorSet(threadContext.descriptorPool, &vk.context.materialSetLayout, &pNode->checkerMaterialSet);
	vkCreateDedicatedTextureFromFile("textures/uvgrid.jpg", &pNode->checkerTexture);

	vkAllocateDescriptorSet(threadContext.descriptorPool, &vk.context.objectSetLayout, &pNode->sphereObjectSet);
	vkCreateAllocateBindMapBuffer(
		VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
		sizeof(VkObjectSetState),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_LOCALITY_CONTEXT,
		&pNode->sphereObjectSetMemory,
		&pNode->sphereObjectSetBuffer,
		(void**)&pNode->pSphereObjectSetMapped);

	VkWriteDescriptorSet writeSets[] = {
		VK_BIND_WRITE_MATERIAL_IMAGE(pNode->checkerMaterialSet, pNode->checkerTexture.view),
		VK_BIND_WRITE_OBJECT_BUFFER(pNode->sphereObjectSet, pNode->sphereObjectSetBuffer),
	};
	vkUpdateDescriptorSets(vk.context.device, _countof(writeSets), writeSets, 0, NULL);

	pNode->sphereTransform = (pose){.pos = VEC3(0, 0, -4)};
	vkUpdateObjectSet(&pNode->sphereTransform, &pNode->sphereObjectState, pNode->pSphereObjectSetMapped);

	vkCreateSphereMesh(0.5, 32, 32, &pNode->sphereMesh);

	VkDepthFramebufferTextureCreateInfo framebufferInfo = {
		.locality = VK_LOCALITY_CONTEXT,
		.extent   = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1},
	};
	vkCreateDepthFramebufferTextures(&framebufferInfo, &pNode->framebufferTexture);
}

static void Bind(MxcNodeContext* pNodeCtx, MxcNodeShared* pNodeShr, MxcTestNode* pNode)
{
	LOG("Binding Thread Node\n");

	vkBindSharedBuffer(&pNode->globalBuffer);
	VK_UPDATE_DESCRIPTOR_SETS(VK_BIND_WRITE_GLOBAL_BUFFER(pNode->globalSet, pNode->globalBuffer.buffer));
}

void* mxcTestNodeThread(MxcNodeContext* pNodeCtx) // we should pass in hNode
{
	LOG("Initializing Thread Node\n");

	NodeHandle hNode = pNodeCtx->hNode;
//	auto pNodeCtx = &node.context[hNode];
	auto pNodeShr = node.pShared[hNode];
	MxcTestNode testNode;
	memset(&testNode, 0, sizeof(MxcTestNode));

	vkBeginAllocationRequests();
	Create(pNodeCtx, pNodeShr, &testNode);
	vkEndAllocationRequests();

	Bind(pNodeCtx, pNodeShr, &testNode);

	mxcTestNodeRun(pNodeCtx, pNodeShr, &testNode);

	return NULL;
}




   // Blit GBuffer. It may still be smarter to do this in the other other command buffer / process
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

#endif