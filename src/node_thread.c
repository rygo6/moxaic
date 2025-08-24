#if defined(MOXAIC_COMPOSITOR)

#include <assert.h>
#include <stdatomic.h>

#include "mid_shape.h"
#include "mid_openxr_runtime.h"
#include "node_thread.h"

////
//// Loop
////
void mxcTestNodeRun(NodeHandle hNode, MxcNodeThread* pNode)
{
	LOG("Running Thread Node %d\n", hNode);

	auto pNodeCtx = &node.context[hNode];
	auto pNodeShr = node.pShared[hNode];

	auto nodeType = pNodeCtx->interprocessMode;
	auto gfxCmd = pNodeCtx->thread.gfxCmd;

	EXTRACT_FIELD(&vk.context, device);
	EXTRACT_FIELD(&vk.context, depthRenderPass);
	EXTRACT_FIELD(&vk.context, depthFramebuffer);

	EXTRACT_FIELD(pNode, pProcessStateMapped);
	auto processStateBuf = pNode->processStateBuffer.buffer;

	EXTRACT_FIELD(pNode, framebufferTexture);

	EXTRACT_FIELD(pNode, pGlobalSetMapped);
	EXTRACT_FIELD(pNode, globalSet);

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

	VkGlobalSetState globSetState = (VkGlobalSetState){};
	vkUpdateGlobalSetViewProj(pNodeShr->camera, pNodeShr->cameraPose, &globSetState);
	memcpy(pGlobalSetMapped, &globSetState, sizeof(VkGlobalSetState));

	ASSERT(cstTimeline != NULL, "Compositor Timeline Handle is nulL!");
	ASSERT(nodeTimeline != NULL, "Node Timeline Handle is nulL!");

	struct {
		VkImageView colorView;
		VkImage     colorImage;
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
			swaps[iImg].colorImage = pColorSwap->externalTexture[iImg].texture.image;
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

NodeLoop:
	vkTimelineWait(device, pNodeShr->compositorBaseCycleValue + MXC_CYCLE_UPDATE_WINDOW_STATE, cstTimeline);

	////
	//// MXC_CYCLE_UPDATE_WINDOW_STATE
	if (UNLIKELY(nodeTimelineValue == 2)) {
		// TODO this cannot be here it must be a RPC
		ReleaseCompositorNodeActive(hNode);
		pNodeShr->compositorMode = MXC_COMPOSITOR_MODE_COMPUTE;
		SetCompositorNodeActive(hNode);
	}

	////
	//// MXC_CYCLE_PROCESS_INPUT

	////
	//// MXC_CYCLE_UPDATE_NODE_STATES

	// Must wait until after node states are updated to render

	////
	//// MXC_CYCLE_COMPOSITOR_RECORD
	vkTimelineWait(device, pNodeShr->compositorBaseCycleValue + MXC_CYCLE_COMPOSITOR_RECORD, cstTimeline);

	atomic_thread_fence(memory_order_acquire);
	vkUpdateGlobalSetView((pose){
		.pos = (vec3)(pNodeShr->cameraPose.pos.vec - pNodeShr->rootPose.pos.vec),
		.euler = pNodeShr->cameraPose.euler,
		.rot = pNodeShr->cameraPose.rot,
	}, &globSetState);
	memcpy(pGlobalSetMapped, &globSetState, sizeof(VkGlobalSetState));

	vk.ResetCommandBuffer(gfxCmd, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
	vk.BeginCommandBuffer(gfxCmd, &(VkCommandBufferBeginInfo){VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT});

	int iSwapImg = nodeTimelineValue % VK_SWAP_COUNT;

	/// Acquire Swap Barrier
	CMD_IMAGE_BARRIERS2(gfxCmd, {
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.image = swaps[iSwapImg].colorImage,
			VK_IMAGE_BARRIER_SRC_UNDEFINED,
			VK_IMAGE_BARRIER_DST_COLOR_ATTACHMENT_WRITE,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
		},
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.image = swaps[iSwapImg].depthImage,
			VK_IMAGE_BARRIER_SRC_UNDEFINED,
			VK_IMAGE_BARRIER_DST_GENERAL_TRANSFER_WRITE,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
		},
	});

	// Clipped Viewport
//	VkViewport viewport = {.x = -pNodeShr->clip.ulUV.x * DEFAULT_WIDTH,	.y = -pNodeShr->clip.ulUV.y * DEFAULT_HEIGHT, .width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT, .maxDepth = 1.0f};
//	VkRect2D scissor = {.offset = {.x = 0, .y = 0}, .extent = {.width = pNodeShr->globalSetState.framebufferSize.x, .height = pNodeShr->globalSetState.framebufferSize.y}};
	VkViewport viewport = {.width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT, .maxDepth = 1.0f};
	VkRect2D   scissor = {.extent = {.width = DEFAULT_WIDTH, .height = DEFAULT_HEIGHT}};
	vk.CmdSetViewport(gfxCmd, 0, 1, &viewport);
	vk.CmdSetScissor(gfxCmd, 0, 1, &scissor);

	VkClearColorValue clearColor = (VkClearColorValue){{0, 0, 0.1f, 0}};
	CmdBeginDepthRenderPass(gfxCmd, depthRenderPass, depthFramebuffer, clearColor, swaps[iSwapImg].colorView, framebufferTexture.depth.view);

	/// Draw
	{
		vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
		vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout, VK_PIPE_SET_INDEX_GLOBAL, 1, &globalSet, 0, NULL);
		vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout, VK_PIPE_SET_INDEX_MATERIAL, 1, &checkerMaterialSet, 0, NULL);
		vk.CmdBindDescriptorSets(gfxCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout, VK_PIPE_SET_INDEX_OBJECT, 1, &sphereObjectSet, 0, NULL);

		vk.CmdBindVertexBuffers(gfxCmd, 0, 1, (VkBuffer[]){sphereBuffer}, (VkDeviceSize[]){sphereVertexOffset});
		vk.CmdBindIndexBuffer(gfxCmd, sphereBuffer, sphereIndexOffset, VK_INDEX_TYPE_UINT16);
		vk.CmdDrawIndexed(gfxCmd, sphereIndexCount, 1, 0, 0, 0);
	}

	vk.CmdEndRenderPass(gfxCmd);

	CMD_IMAGE_BARRIERS2(gfxCmd, {
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.image = framebufferTexture.depth.image,
			VK_IMAGE_BARRIER_SRC_DEPTH_ATTACHMENT_WRITE,
			VK_IMAGE_BARRIER_DST_GENERAL_TRANSFER_READ,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			VK_IMAGE_BARRIER_DEPTH_SUBRESOURCE_RANGE,
		}
	});

	vkCmdCopyImage2(gfxCmd, &(VkCopyImageInfo2){
		VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2,
		.srcImage = framebufferTexture.depth.image,
		.srcImageLayout = VK_IMAGE_LAYOUT_GENERAL,
		.dstImage = swaps[iSwapImg].depthImage,
		.dstImageLayout = VK_IMAGE_LAYOUT_GENERAL,
		.regionCount = 1,
		.pRegions = &(VkImageCopy2){
			VK_STRUCTURE_TYPE_IMAGE_COPY_2,
			.srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .layerCount = 1},
			.dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
			.extent = {viewport.width, viewport.height, 1}
		}
	});

	// Release Swap Barrier
	CMD_IMAGE_BARRIERS2(gfxCmd, {
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.image = swaps[iSwapImg].colorImage,
			VK_IMAGE_BARRIER_SRC_COLOR_ATTACHMENT_WRITE,
			VK_IMAGE_BARRIER_DST_COMPUTE_RELEASE,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
		},
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.image = swaps[iSwapImg].depthImage,
			VK_IMAGE_BARRIER_SRC_GENERAL_TRANSFER_WRITE,
			VK_IMAGE_BARRIER_DST_COMPUTE_RELEASE,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
		},
	});

	vk.EndCommandBuffer(gfxCmd);

	// Submit Node to Render
	nodeTimelineValue++;
	mxcQueueNodeCommandBuffer((MxcQueuedNodeCommandBuffer){
		.cmd = gfxCmd,
		.nodeTimeline = nodeTimeline,
		.nodeTimelineSignalValue = nodeTimelineValue,
	});
	vkTimelineWait(device, nodeTimelineValue, nodeTimeline);

	// Update Camera Z
	pNodeShr->processState.depthNearZ = pNodeShr->camera.zFar; // reverse Z
	pNodeShr->processState.depthFarZ = pNodeShr->camera.zNear;

	// Signal Updated to Compositor
	pNodeShr->timelineValue = nodeTimelineValue;
	pNodeShr->compositorBaseCycleValue += MXC_CYCLE_COUNT * pNodeShr->compositorCycleSkip;
	atomic_thread_fence(memory_order_release);

	//    _Thread_local static int count;
	//    if (count++ > 10)
	//      return;

	CHECK_RUNNING
	goto NodeLoop;
}

////
//// Create
////
static void Create(NodeHandle hNode, MxcNodeThread* pNode)
{
	LOG("Creating Thread Node %d\n", hNode);

	auto pNodeCtx = &node.context[hNode];
	auto pNodeShr = node.pShared[hNode];

	// This is way too many descriptors... optimize this
	VK_CHECK(vkCreateDescriptorPool(vk.context.device, &(VkDescriptorPoolCreateInfo){
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
		.maxSets = 30,
		.poolSizeCount = 3,
		.pPoolSizes = (VkDescriptorPoolSize[]){
			{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 10},
			{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 10},
			{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 10},
		},
	}, VK_ALLOC, &threadContext.descriptorPool));

	vkAllocateDescriptorSet(threadContext.descriptorPool, &vk.context.globalSetLayout, &pNode->globalSet);\
	vkCreateSharedBuffer(&(VkRequestAllocationInfo){
		.memoryPropertyFlags = VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
		.size = sizeof(VkGlobalSetState),
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
	}, &pNode->globalBuffer);

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
	vkUpdateDescriptorSets(vk.context.device, COUNT(writeSets), writeSets, 0, NULL);

	pNode->sphereTransform = (pose){.pos = VEC3(0, 0, 0)};
	vkUpdateObjectSet(&pNode->sphereTransform, &pNode->sphereObjectState, pNode->pSphereObjectSetMapped);

	vkCreateSphereMesh(0.5, 32, 32, &pNode->sphereMesh);

	vkCreateDepthFramebufferTextures(&(VkDepthFramebufferTextureCreateInfo){
		.locality = VK_LOCALITY_CONTEXT,
		.extent   = {DEFAULT_WIDTH, DEFAULT_HEIGHT, 1},
	}, &pNode->framebufferTexture);

	vkCreateSharedBuffer(&(VkRequestAllocationInfo){
		.memoryPropertyFlags = VK_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
		.size                = sizeof(MxcProcessState),
		.usage               = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
	}, &pNode->processStateBuffer);
}

static void Bind(NodeHandle hNode, MxcNodeThread* pNode)
{
	LOG("Binding Thread Node %d\n", hNode);

	auto pNodeCtx = &node.context[hNode];
	auto pNodeShr = node.pShared[hNode];

	vkBindSharedBuffer(&pNode->globalBuffer);
	VK_UPDATE_DESCRIPTOR_SETS(VK_BIND_WRITE_GLOBAL_BUFFER(pNode->globalSet, pNode->globalBuffer.buffer));
	pNode->pGlobalSetMapped = vkSharedMemoryPtr(pNode->globalBuffer.memory);

	vkBindSharedBuffer(&pNode->processStateBuffer);
	pNode->pProcessStateMapped = vkSharedBufferPtr(pNode->processStateBuffer);
	memset(pNode->pProcessStateMapped, 0, sizeof(MxcProcessState));
}

void* mxcRunNodeThread(void* nodeHandle)
{
	NodeHandle hNode = (NodeHandle)(u64)nodeHandle;
	LOG("Initializing Thread Node: %d\n", hNode);

	MxcNodeThread testNode;
	memset(&testNode, 0, sizeof(MxcNodeThread));

	vkBeginAllocationRequests();
	Create(hNode, &testNode);
	vkEndAllocationRequests();

	Bind(hNode, &testNode);

	mxcTestNodeRun(hNode, &testNode);

	return NULL;
}

#endif