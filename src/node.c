#define WIN32_LEAN_AND_MEAN
#undef UNICODE
#include <winsock2.h>
#include <afunix.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

#include "mid_vulkan.h"
#include "pipe_gbuffer_process.h"

#include "node.h"
#include "test_node.h"

// Compositor state in Compositor Process
// There is one compositor context in the Compositor Process and it can be accessed directly
MxcCompositorContext compositorContext = {};

// State imported into Node Process
// TODO this should be CompositeSharedMemory and deal with all nodes from another process
HANDLE                 importedExternalMemoryHandle = NULL;
MxcExternalNodeMemory* pImportedExternalMemory = NULL;

// this should become a midvk construct
size_t                     submitNodeQueueStart = 0;
size_t                     submitNodeQueueEnd = 0;
MxcQueuedNodeCommandBuffer submitNodeQueue[MXC_NODE_CAPACITY] = {};

struct Node node;

////
//// Swap Pool
////
void mxcNodeGBufferProcessDepth(VkCommandBuffer gfxCmd, VkBuffer stateBuffer, MxcNodeSwap* pDepthSwap, MxcNodeGBuffer* pGBuffer, ivec2 nodeSwapExtent)
{
	EXTRACT_FIELD(&node, gbufferProcessDownPipe);
	EXTRACT_FIELD(&node, gbufferProcessUpPipe);
	EXTRACT_FIELD(&node, gbufferProcessPipeLayout);

	vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, gbufferProcessDownPipe);

	{
		CMD_PUSH_SETS(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, gbufferProcessPipeLayout,	PIPE_SET_INDEX_GBUFFER_PROCESS_INOUT,
			BIND_WRITE_GBUFFER_PROCESS_STATE(stateBuffer),
			BIND_WRITE_GBUFFER_PROCESS_SRC_DEPTH(pDepthSwap->view),
			BIND_WRITE_GBUFFER_PROCESS_DST_GBUFFER(pGBuffer->mipViews[1]));

		ivec2 groupCount = iVec2Min(iVec2CeiDivide(nodeSwapExtent.vec >> 1, 32), 1);
		vk.CmdDispatch(gfxCmd, groupCount.x, groupCount.y, 1);
	}

	/// Blit Down Depth Mips
	for (int iMip = 2; iMip < pGBuffer->mipViewCount; ++iMip) {
		CMD_IMAGE_BARRIERS(gfxCmd,
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = pGBuffer->image,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = iMip - 1,
					.levelCount = 1,
					.layerCount = VK_REMAINING_ARRAY_LAYERS
				},
				VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
				VK_IMAGE_BARRIER_DST_COMPUTE_READ,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			},
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = pGBuffer->image,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = iMip,
					.levelCount = 1,
					.layerCount = VK_REMAINING_ARRAY_LAYERS
				},
				VK_IMAGE_BARRIER_SRC_COMPUTE_READ,
				VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			});

		CMD_PUSH_SETS(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, gbufferProcessPipeLayout, PIPE_SET_INDEX_GBUFFER_PROCESS_INOUT,
			BIND_WRITE_GBUFFER_PROCESS_SRC_DEPTH(pGBuffer->mipViews[iMip - 1]),
			BIND_WRITE_GBUFFER_PROCESS_DST_GBUFFER(pGBuffer->mipViews[iMip]));

		ivec2 groupCount = iVec2Min(iVec2CeiDivide(nodeSwapExtent.vec >> iMip, 32), 1);
		vk.CmdDispatch(gfxCmd, groupCount.x, groupCount.y, 1);
	}

	/// Blit Up Depth Mips
	vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, gbufferProcessUpPipe);

	for (int iMip = pGBuffer->mipViewCount - 2; iMip >= 1; --iMip) {
		CMD_IMAGE_BARRIERS(gfxCmd,
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = pGBuffer->image,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = iMip + 1,
					.levelCount = 1,
					.layerCount = VK_REMAINING_ARRAY_LAYERS
				},
				VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
				VK_IMAGE_BARRIER_DST_COMPUTE_READ,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			},
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = pGBuffer->image,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = iMip,
					.levelCount = 1,
					.layerCount = VK_REMAINING_ARRAY_LAYERS
				},
				VK_IMAGE_BARRIER_SRC_COMPUTE_READ,
				VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			});

		CMD_PUSH_SETS(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, gbufferProcessPipeLayout, PIPE_SET_INDEX_GBUFFER_PROCESS_INOUT,
			BIND_WRITE_GBUFFER_PROCESS_SRC_DEPTH(pGBuffer->mipViews[iMip + 1]),
			BIND_WRITE_GBUFFER_PROCESS_SRC_GBUFFER(pGBuffer->mipViews[iMip]),
			BIND_WRITE_GBUFFER_PROCESS_DST_GBUFFER(pGBuffer->mipViews[iMip]));

		ivec2 groupCount = iVec2Min(iVec2CeiDivide(nodeSwapExtent.vec >> iMip, 32), 1);
		vk.CmdDispatch(gfxCmd, groupCount.x, groupCount.y, 1);
	}

	/// Final Depth Up Blit
	{
		CMD_IMAGE_BARRIERS(gfxCmd,
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = pGBuffer->image,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 1,
					.levelCount = 1,
					.layerCount = VK_REMAINING_ARRAY_LAYERS
				},
				VK_IMAGE_BARRIER_SRC_COMPUTE_WRITE,
				VK_IMAGE_BARRIER_DST_COMPUTE_READ,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			},
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = pGBuffer->image,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.layerCount = VK_REMAINING_ARRAY_LAYERS
				},
				VK_IMAGE_BARRIER_SRC_COMPUTE_READ,
				VK_IMAGE_BARRIER_DST_COMPUTE_WRITE,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			});

		CMD_PUSH_SETS(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, gbufferProcessPipeLayout, PIPE_SET_INDEX_GBUFFER_PROCESS_INOUT,
			BIND_WRITE_GBUFFER_PROCESS_SRC_DEPTH(pGBuffer->mipViews[1]),
			BIND_WRITE_GBUFFER_PROCESS_SRC_GBUFFER(pDepthSwap->view),
			BIND_WRITE_GBUFFER_PROCESS_DST_GBUFFER(pGBuffer->mipViews[0]));

		ivec2 groupCount = iVec2Min(iVec2CeiDivide(nodeSwapExtent, 32), 1);
		vk.CmdDispatch(gfxCmd, groupCount.x, groupCount.y, 1);
	}
}


// this couild go in mid vk
static void CreateColorSwapTexture(const XrSwapchainInfo* pInfo, VkExternalTexture* pSwapTexture)
{
	VkImageCreateInfo info = {
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		&(VkExternalMemoryImageCreateInfo){
			VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
			.handleTypes = MXC_EXTERNAL_FRAMEBUFFER_HANDLE_TYPE,
		},
		.imageType   = VK_IMAGE_TYPE_2D,
		.format      = VK_RENDER_PASS_FORMATS[VK_RENDER_PASS_ATTACHMENT_INDEX_COLOR],
		.extent      = {pInfo->windowWidth, pInfo->windowHeight, 1},
		.mipLevels   = 1,
		.arrayLayers = 1,
		.samples     = VK_SAMPLE_COUNT_1_BIT,
		.usage       = VK_RENDER_PASS_USAGES[VK_RENDER_PASS_ATTACHMENT_INDEX_COLOR],
	};
	vkCreateExternalPlatformTexture(&info, &pSwapTexture->platform);
	VkDedicatedTextureCreateInfo textureInfo = {
		.pImageCreateInfo = &info,
		.aspectMask       = VK_IMAGE_ASPECT_COLOR_BIT,
		.importHandle     = pSwapTexture->platform.handle,
		.handleType       = MXC_EXTERNAL_FRAMEBUFFER_HANDLE_TYPE,
		.locality         = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
	};
	vkCreateDedicatedTexture(&textureInfo, &pSwapTexture->texture);

	VK_IMMEDIATE_COMMAND_BUFFER_CONTEXT(VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS)
	{
		CMD_IMAGE_BARRIERS(cmd,	{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.image = pSwapTexture->texture.image,
			VK_IMAGE_BARRIER_SRC_UNDEFINED,
			.dstStageMask = VK_PIPELINE_STAGE_2_NONE,
			.dstAccessMask = VK_ACCESS_2_NONE,
			// could I do this elsewhere?
			//.newLayout = pInfo->compositorMode == MXC_COMPOSITOR_MODE_COMPUTE ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
		});
	}
}

static void CreateDepthSwapTexture(const XrSwapchainInfo* pInfo, VkExternalTexture* pSwapTexture)
{
	VkImageCreateInfo imageCreateInfo = {
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		&(VkExternalMemoryImageCreateInfo){
			VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
			.handleTypes = MXC_EXTERNAL_FRAMEBUFFER_HANDLE_TYPE,
		},
		.imageType   = VK_IMAGE_TYPE_2D,
		.format      = VK_FORMAT_R16_UNORM,
		.extent      = {pInfo->windowWidth, pInfo->windowHeight, 1},
		.mipLevels   = 1,
		.arrayLayers = 1,
		.samples     = VK_SAMPLE_COUNT_1_BIT,
		// You cannot export a depth texture to all platforms.
		.usage       = VK_IMAGE_USAGE_STORAGE_BIT |
		               VK_IMAGE_USAGE_SAMPLED_BIT |
		               VK_IMAGE_USAGE_TRANSFER_DST_BIT,
	};
	vkCreateExternalPlatformTexture(&imageCreateInfo, &pSwapTexture->platform);
	VkDedicatedTextureCreateInfo textureInfo = {
		.pImageCreateInfo = &imageCreateInfo,
		.aspectMask       = VK_IMAGE_ASPECT_COLOR_BIT,
		.importHandle     = pSwapTexture->platform.handle,
		.handleType       = MXC_EXTERNAL_FRAMEBUFFER_HANDLE_TYPE,
		.locality         = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
	};
	vkCreateDedicatedTexture(&textureInfo, &pSwapTexture->texture);

	VK_IMMEDIATE_COMMAND_BUFFER_CONTEXT(VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS)
	{
		CMD_IMAGE_BARRIERS(cmd,	{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.image = pSwapTexture->texture.image,
			VK_IMAGE_BARRIER_SRC_UNDEFINED,
			.dstStageMask = VK_PIPELINE_STAGE_2_NONE,
			.dstAccessMask = VK_ACCESS_2_NONE,
			// could I do this elsewhere?
			//.newLayout = pInfo->compositorMode == MXC_COMPOSITOR_MODE_COMPUTE ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
		});
	}
}

static void CreateNodeGBuffer(NodeHandle hNode)
{
// I need to get compositor specific and node specific code into different files
#if defined(MOXAIC_COMPOSITOR)
	auto pNodeCtxt = &node.context[hNode];
	auto pNodeCompData = &node.cst.data[hNode];
	auto pNodeShrd = node.pShared[hNode];

	CHECK(pNodeShrd->swapMaxWidth < 1024 || pNodeShrd->swapMaxHeight < 1024, "Swap too small!")

	int mipLevelCount = VK_MIP_LEVEL_COUNT(pNodeShrd->swapMaxWidth, pNodeShrd->swapMaxHeight);
	ASSERT(mipLevelCount < MXC_NODE_GBUFFER_MAX_MIP_COUNT, "Max gbuffer mip count exceeded.");

	for (int iView = 0; iView < XR_MAX_VIEW_COUNT; ++iView) {
		vkCreateDedicatedTexture(
			&(VkDedicatedTextureCreateInfo){
				.pImageCreateInfo = &(VkImageCreateInfo){
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = MXC_NODE_GBUFFER_FORMAT,
					.extent = {
						pNodeShrd->swapMaxWidth,
						pNodeShrd->swapMaxHeight,
						1,
					},
					.mipLevels = mipLevelCount,
					.arrayLayers = 1,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = MXC_NODE_GBUFFER_USAGE,
				},
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.locality = VK_LOCALITY_CONTEXT},
			&pNodeCtxt->gbuffer[iView]);
		VK_SET_DEBUG_NAME(pNodeCtxt->gbuffer[iView].image, "NodeGBufferImage%d", iView);
		VK_SET_DEBUG_NAME(pNodeCtxt->gbuffer[iView].view, "NodeGBufferView%d", iView);
		VK_SET_DEBUG_NAME(pNodeCtxt->gbuffer[iView].image, "NodeGBufferMemory%d", iView);

		// Pack data for compositor hot access
		pNodeCompData->gbuffer[iView].image = pNodeCtxt->gbuffer[iView].image;
		pNodeCompData->gbuffer[iView].mipViewCount = mipLevelCount;

		// Generate views for mip access
		for (int iMip = 0; iMip < mipLevelCount; ++iMip) {
			VkImageViewCreateInfo imageViewCreateInfo = {
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = pNodeCtxt->gbuffer[iView].image,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = MXC_NODE_GBUFFER_FORMAT,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = iMip,
					.levelCount = 1,
					.layerCount = 1,
				},
			};
			VkImageView view;
			VK_CHECK(vkCreateImageView(vk.context.device, &imageViewCreateInfo, VK_ALLOC, &view));
			pNodeCompData->gbuffer[iView].mipViews[iMip] = view;
			VK_SET_DEBUG_NAME(pNodeCompData->gbuffer[iView].mipViews[iMip], "NodeGBufferView%d Mip%d", iView, iMip);
		}

		VK_IMMEDIATE_COMMAND_BUFFER_CONTEXT(VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS) {
			CMD_IMAGE_BARRIERS(cmd,	{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = pNodeCtxt->gbuffer[iView].image,
				.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
				VK_IMAGE_BARRIER_SRC_UNDEFINED,
				VK_IMAGE_BARRIER_DST_COMPUTE_NONE,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			});
		}
	}
#endif
}

static void mxcDestroySwapTexture(MxcSwapTexture* pSwap)
{
	pSwap->state = XR_SWAP_STATE_UNITIALIZED;
	pSwap->info = (XrSwapchainInfo){};
	for (int i = 0; i < XR_SWAPCHAIN_IMAGE_COUNT; ++i) {
		vkDestroyDedicatedTexture(&pSwap->externalTexture[i].texture);
		vkDestroyExternalPlatformTexture(&pSwap->externalTexture[i].platform);
	}
}

////
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

////
//// Node Lifecycle
////
NodeHandle RequestLocalNodeHandle()
{
	// TODO this claim/release handle needs to be a pooling logic. Switch to use mid block
	NodeHandle hNode = atomic_fetch_add(&node.count, 1);
	// malloc to reflect this being shared memory malloc over IPC when not local thread.
	node.pShared[hNode] = malloc(sizeof(MxcNodeShared));
	memset(node.pShared[hNode], 0, sizeof(MxcNodeShared));
	memset(&node.context[hNode], 0, sizeof(MxcNodeContext));
	node.context[hNode].hNode = hNode;
	return hNode;
}

NodeHandle RequestExternalNodeHandle(MxcNodeShared* pNodeShared)
{
	NodeHandle hNode = atomic_fetch_add(&node.count, 1);
	node.pShared[hNode] = pNodeShared;
	memset(&node.context[hNode], 0, sizeof(MxcNodeContext));
	node.context[hNode].hNode = hNode;
	return hNode;
}

void ReleaseNodeHandle(NodeHandle hNode)
{
	// TODO this needs to use block
	int priorCount = atomic_fetch_sub(&node.count, 1);
	LOG("Releasing Node Handle %d. Prior Count %d.\n", hNode, priorCount);
}

void SetNodeActive(NodeHandle hNode, MxcCompositorMode mode)
{
#if defined(MOXAIC_COMPOSITOR)
	auto pNodeCstData = &node.cst.data[hNode];
	auto pActiveNode = &node.active[mode];
	pActiveNode->handles[pActiveNode->count] = hNode;
	pNodeCstData->activeCompositorMode = mode;
	atomic_fetch_add(&pActiveNode->count, 1);
	LOG("Added node %d to %s\n", hNode, string_MxcCompositorMode(mode));
#endif
}

// Release Node from active lists while still retaining node handle.
void ReleaseNodeActive(NodeHandle hNode)
{
#if defined(MOXAIC_COMPOSITOR)
	assert((compositorContext.baseCycleValue % MXC_CYCLE_COUNT) == MXC_CYCLE_UPDATE_WINDOW_STATE && "Trying to ReleaseNodeActive not in MXC_CYCLE_UPDATE_WINDOW_STATE");
	assert(node.context[hNode].interprocessMode != MXC_NODE_INTERPROCESS_MODE_NONE);
	auto pNodeCstData = &node.cst.data[hNode];
	auto pActiveNode = &node.active[pNodeCstData->activeCompositorMode];

	ATOMIC_FENCE_SCOPE
	{
		int i = 0;
		// compact handles down... this should use memmove
		// this needs to be done in MXC_CYCLE_UPDATE_WINDOW_STATE or MXC_CYCLE_PROCESS_INPUT when active node lists aren't use
		// probably want an ipc call to do this
		for (; i < pActiveNode->count; ++i) {
			if (pActiveNode->handles[i] == hNode)
				break;
		}
		for (; i < pActiveNode->count - 1; ++i) {
			pActiveNode->handles[i] = pActiveNode->handles[i + 1];
		}
		pActiveNode->count--;
	}
#endif
}

#define CLOSE_HANDLE(_handle)                                                     \
	if (!CloseHandle(_handle)) {                                                  \
		DWORD dwError = GetLastError();                                           \
		printf("Could not close (%s) object buffer (%lu).\n", #_handle, dwError); \
	}
static int CleanupNode(NodeHandle hNode)
{
	assert((compositorContext.baseCycleValue % MXC_CYCLE_COUNT) == MXC_CYCLE_UPDATE_WINDOW_STATE && "Trying to ReleaseNodeActive not in MXC_CYCLE_UPDATE_WINDOW_STATE");
	auto pNodeCtxt = &node.context[hNode];
	auto pNodeShared = node.pShared[hNode];

//	int  iSwapBlock = MXC_SWAP_TYPE_BLOCK_INDEX_BY_TYPE[pNodeShared->swapType];

	switch (pNodeCtxt->interprocessMode) {
		case MXC_NODE_INTERPROCESS_MODE_THREAD:
			vkFreeCommandBuffers(vk.context.device, pNodeCtxt->thread.pool, 1, &pNodeCtxt->thread.gfxCmd);
			vkDestroyCommandPool(vk.context.device, pNodeCtxt->thread.pool, VK_ALLOC);
			int result = pthread_join(pNodeCtxt->thread.id, NULL);
			if (result != 0) {
				perror("Thread join failed");
			}

			free(pNodeShared);
			node.pShared[hNode] = NULL;

			break;
		case MXC_NODE_INTERPROCESS_MODE_EXPORTED:
#if defined(MOXAIC_COMPOSITOR)
			// really need a different way to do this
//			CMD_WRITE_SINGLE_SETS(vk.context.device,
//				BIND_WRITE_NODE_COLOR(nodeCompositorData[hNode].nodeSet, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL),
//				BIND_WRITE_NODE_GBUFFER(nodeCompositorData[hNode].nodeSet, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL));
			VkWriteDescriptorSet writeSets[] = {
				{
					VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
					.dstSet = node.cst.data[hNode].nodeDesc.set,
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
					.dstSet = node.cst.data[hNode].nodeDesc.set,
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

			// We are fully destroying swaps and gbuffer but may want to retain them someday
			for (int i = 0; i < MXC_NODE_SWAP_CAPACITY; ++i) {
				if (!HANDLE_VALID(pNodeCtxt->hSwaps[i]))
					continue;

				auto pSwap = BLOCK_RELEASE(node.cst.block.swap, pNodeCtxt->hSwaps[i]);
				mxcDestroySwapTexture(pSwap);
			}
			for (int i = 0; i < XR_MAX_VIEW_COUNT; ++i) {
				// We are fully clearing but at some point we want a mechanic to recycle and share node swaps
				vkDestroyDedicatedTexture(&pNodeCtxt->gbuffer[i]);
			}

			CLOSE_HANDLE(pNodeCtxt->swapsSyncedHandle);
			CLOSE_HANDLE(pNodeCtxt->exported.nodeTimelineHandle);
			CHECK_WIN32(UnmapViewOfFile(pNodeCtxt->exported.pExportedMemory));
			CLOSE_HANDLE(pNodeCtxt->exported.exportedMemoryHandle);
			CLOSE_HANDLE(pNodeCtxt->exported.handle);

			node.pShared[hNode] = NULL;
#endif
			break;
		case MXC_NODE_INTERPROCESS_MODE_IMPORTED:
			// The process which imported the handle must close them.
			for (int iSwap = 0; iSwap < XR_SWAPCHAIN_CAPACITY; ++iSwap) {
				for (int iImg = 0; iImg < XR_SWAPCHAIN_IMAGE_COUNT; ++iImg) {
					CLOSE_HANDLE(pImportedExternalMemory->imports.swapImageHandles[iSwap][iImg]);
				}
			}
			CLOSE_HANDLE(pImportedExternalMemory->imports.swapsSyncedHandle);
			CLOSE_HANDLE(pImportedExternalMemory->imports.nodeTimelineHandle);
			CLOSE_HANDLE(pImportedExternalMemory->imports.compositorTimelineHandle);
			CHECK_WIN32(UnmapViewOfFile(pImportedExternalMemory));
			CLOSE_HANDLE(importedExternalMemoryHandle);
			CLOSE_HANDLE(pNodeCtxt->exported.handle);
			break;
		default: PANIC("Node interprocessMode not supported");
	}

	*pNodeCtxt = (MxcNodeContext){};

	return 0;
}

void mxcRequestNodeThread(void* (*runFunc)(MxcNodeContext*), NodeHandle* pNodeHandle)
{
#if defined(MOXAIC_COMPOSITOR)
	LOG("Requesting Node Thread.\n");
	NodeHandle hNode = RequestLocalNodeHandle();

	auto pNodeCtx = &node.context[hNode];
	pNodeCtx->interprocessMode = MXC_NODE_INTERPROCESS_MODE_THREAD;
	pNodeCtx->swapsSyncedHandle = CreateEvent(NULL, FALSE, FALSE, NULL);

	auto pNodeShr = node.pShared[hNode];
	pNodeShr->rootPose.pos = VEC3(hNode, 0, 0);
	pNodeShr->rootPose.rot = QuatFromEuler(pNodeShr->rootPose.euler);

	pNodeShr->camera.yFovRad = RAD_FROM_DEG(45.0f);
	pNodeShr->camera.zNear = 0.1f;
	pNodeShr->camera.zFar = 100.0f;

	pNodeShr->compositorRadius = 0.5;
	pNodeShr->compositorCycleSkip = 16;
	pNodeShr->compositorMode = MXC_COMPOSITOR_MODE_COMPUTE;

	pNodeShr->swapMaxWidth = DEFAULT_WIDTH;
	pNodeShr->swapMaxHeight = DEFAULT_HEIGHT;

	vkSemaphoreCreateInfoExt semaphoreCreateInfo = {
		.debugName = "NodeTimelineSemaphore",
		.locality = VK_LOCALITY_CONTEXT,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
	};
	vkCreateSemaphoreExt(&semaphoreCreateInfo, &pNodeCtx->thread.nodeTimeline);

	VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
	};
	VK_CHECK(vkCreateCommandPool(vk.context.device, &graphicsCommandPoolCreateInfo, VK_ALLOC, &pNodeCtx->thread.pool));
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = pNodeCtx->thread.pool,
		.commandBufferCount = 1,
	};
	VK_CHECK(vkAllocateCommandBuffers(vk.context.device, &commandBufferAllocateInfo, &pNodeCtx->thread.gfxCmd));
	vkSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)pNodeCtx->thread.gfxCmd, "TestNode");

	auto pCstNodeData = &node.cst.data[hNode];
	pCstNodeData = &node.cst.data[hNode];

	// we want to get nodedesc out of this and into a descriptor set array that goes only by index so we don't need to retian any state in CstNodeData
	VkSharedDescriptor nodeDesc = pCstNodeData->nodeDesc;
	memset(pCstNodeData, 0, sizeof(MxcNodeCompositeData));
	pCstNodeData->nodeDesc = nodeDesc;

	CreateNodeGBuffer(hNode);

	*pNodeHandle = hNode;

	CHECK(pthread_create(&node.context[hNode].thread.id, NULL, (void* (*)(void*))runFunc, pNodeCtx), "Node thread creation failed!");

	SetNodeActive(hNode, MXC_COMPOSITOR_MODE_NONE);

	LOG("Request Node Thread Success. Handle: %d\n", hNode);
	// todo this needs error handling
#endif
}

////
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
static void ServerInterprocessAcceptNodeConnection()
{
#if defined(MOXAIC_COMPOSITOR) // we need to break this out in a Compositor Node file
	printf("Accepting connections on: '%s'\n", SOCKET_PATH);
	MxcNodeContext*         pNodeCtx = NULL;
	MxcNodeShared*          pNodeShrd = NULL;
	MxcNodeImports*         pImports = NULL;
	MxcNodeCompositeData*  pCstNodeData = NULL;
	HANDLE                  hNodeProc = INVALID_HANDLE_VALUE;
	HANDLE                  hExtNodeMem = INVALID_HANDLE_VALUE;
	MxcExternalNodeMemory*  pExtNodeMem = NULL;
	DWORD                   nodeProcId = 0;

	SOCKET clientSocket = accept(ipcServer.listenSocket, NULL, NULL);
	WSA_CHECK(clientSocket == INVALID_SOCKET, "Accept failed");
	LOG("Accepted Connection.\n");

	/// Receive Node Ack Message
	{
		char buffer[sizeof(nodeIPCAckMessage)] = {};
		int  receiveLength = recv(clientSocket, buffer, sizeof(nodeIPCAckMessage), 0);
		WSA_CHECK(receiveLength == SOCKET_ERROR || receiveLength == 0, "Recv nodeIPCAckMessage failed");
		LOG("Received node ack: %s Size: %d\n", buffer, receiveLength);
		CHECK(strcmp(buffer, nodeIPCAckMessage), "Unexpected node message");
	}

	/// Send Server Ack message
	{
		LOG("Sending server ack: %s size: %llu\n", serverIPCAckMessage, strlen(serverIPCAckMessage));
		int sendResult = send(clientSocket, serverIPCAckMessage, strlen(serverIPCAckMessage), 0);
		WSA_CHECK(sendResult == SOCKET_ERROR || sendResult == 0, "Send server ack failed");
	}

	/// Receive Node Process Handle
	{
		int receiveLength = recv(clientSocket, (char*)&nodeProcId, sizeof(DWORD), 0);
		WSA_CHECK(receiveLength == SOCKET_ERROR || receiveLength == 0, "Recv node exported id failed");
		LOG("Received node exported id: %lu Size: %d\n", nodeProcId, receiveLength);
		CHECK(nodeProcId == 0, "Invalid node exported id");
	}

	/// Create Shared Memory
	{
		hNodeProc = OpenProcess(PROCESS_DUP_HANDLE, FALSE, nodeProcId);
		WIN32_CHECK(hNodeProc != NULL && hNodeProc != INVALID_HANDLE_VALUE, "Duplicate exported buffer failed");
		hExtNodeMem = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(MxcExternalNodeMemory), NULL);
		WIN32_CHECK(hExtNodeMem != NULL, "Could not create file mapping object");
		pExtNodeMem = MapViewOfFile(hExtNodeMem, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MxcExternalNodeMemory));
		WIN32_CHECK(pExtNodeMem != NULL, "Could not map view of file");
		memset(pExtNodeMem, 0, sizeof(MxcExternalNodeMemory));
		pImports = &pExtNodeMem->imports;
		pNodeShrd = &pExtNodeMem->shared;
	}

	/// Claim Node Handle
	NodeHandle hNode = RequestExternalNodeHandle(pNodeShrd);

	/// Initialize Context
	{
		// Init Node Shared
		pNodeShrd->rootPose.rot = QuatFromEuler(pNodeShrd->rootPose.euler);
		pNodeShrd->camera.yFovRad = RAD_FROM_DEG(45.0f);
		pNodeShrd->camera.zNear = 0.1f;
		pNodeShrd->camera.zFar = 100.0f;
		pNodeShrd->compositorRadius = 0.5;
		pNodeShrd->compositorCycleSkip = 8;
		pNodeShrd->swapMaxWidth = DEFAULT_WIDTH;
		pNodeShrd->swapMaxHeight = DEFAULT_HEIGHT;
//		pNodeShrd->compositorMode = MXC_COMPOSITOR_MODE_QUAD;
//		pNodeShrd->compositorMode = MXC_COMPOSITOR_MODE_TESSELATION;
		pNodeShrd->compositorMode = MXC_COMPOSITOR_MODE_COMPUTE;

		vkSemaphoreCreateInfoExt semaphoreCreateInfo = {
			.debugName = "NodeTimelineSemaphoreExport",
			.locality = VK_LOCALITY_INTERPROCESS_EXPORTED_READWRITE,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		};
		VkSemaphore nodeTimeline; vkCreateSemaphoreExt(&semaphoreCreateInfo, &nodeTimeline);

		// Init Node Context
		// TODO should more of this setup go in RequestExternalNodeHandle ?
		pNodeCtx = &node.context[hNode];
		pNodeCtx->interprocessMode = MXC_NODE_INTERPROCESS_MODE_EXPORTED;

		pNodeCtx->swapsSyncedHandle = CreateEvent(NULL, FALSE, FALSE, NULL);

		pNodeCtx->exported.id = nodeProcId;
		pNodeCtx->exported.handle = hNodeProc;

		pNodeCtx->exported.exportedMemoryHandle = hExtNodeMem;
		pNodeCtx->exported.pExportedMemory = pExtNodeMem;

		pNodeCtx->exported.nodeTimeline = nodeTimeline;
		pNodeCtx->exported.nodeTimelineHandle = vkGetSemaphoreExternalHandle(nodeTimeline);

		pNodeCtx->exported.compositorTimeline = compositorContext.timeline;
		pNodeCtx->exported.compositorTimelineHandle = compositorContext.timelineHandle;

		pCstNodeData = &node.cst.data[hNode];
		VkSharedDescriptor nodeDesc = pCstNodeData->nodeDesc;
		memset(pCstNodeData, 0, sizeof(MxcNodeCompositeData));
		// we want to get nodedesc out of this and into a descriptor set array that goes only by index so we don't need to retian any state in CstNodeData
		pCstNodeData->nodeDesc = nodeDesc;


		// Duplicate Handles
		HANDLE currentHandle = GetCurrentProcess();
		WIN32_CHECK(DuplicateHandle(
						currentHandle, pNodeCtx->swapsSyncedHandle,
						hNodeProc, &pImports->swapsSyncedHandle,
						0, false, DUPLICATE_SAME_ACCESS),
			"Duplicate nodeFenceHandle buffer fail.");
		WIN32_CHECK(DuplicateHandle(
						currentHandle, pNodeCtx->exported.nodeTimelineHandle,
						hNodeProc, &pImports->nodeTimelineHandle,
						0, false, DUPLICATE_SAME_ACCESS),
			"Duplicate nodeTimeline buffer fail.");
		WIN32_CHECK(DuplicateHandle(
						currentHandle, pNodeCtx->exported.compositorTimelineHandle,
						hNodeProc, &pImports->compositorTimelineHandle,
						0, false, DUPLICATE_SAME_ACCESS),
			"Duplicate compositor timeline buffer fail.");
	}

	/// Send Shared Memory
	{
		HANDLE duplicatedExternalNodeMemoryHandle;
		WIN32_CHECK(DuplicateHandle(
						GetCurrentProcess(), pNodeCtx->exported.exportedMemoryHandle,
						hNodeProc, &duplicatedExternalNodeMemoryHandle,
						0, false, DUPLICATE_SAME_ACCESS),
					"Duplicate sharedMemory buffer fail.");
		printf("Sending duplicatedExternalNodeMemoryHandle: %p Size: %llu\n", duplicatedExternalNodeMemoryHandle, sizeof(HANDLE));
		int sendResult = send(clientSocket, (const char*)&duplicatedExternalNodeMemoryHandle, sizeof(HANDLE), 0);
		WSA_CHECK(sendResult == SOCKET_ERROR || sendResult == 0, "Send shared memory buffer failed");
		LOG("Process Node Export Success.\n");

	}

	/// Add Active node
	{
		// Add to COMPOSITOR_MODE_NONE initially to start processing
		SetNodeActive(hNode, MXC_COMPOSITOR_MODE_NONE);
		goto ExitSuccess;
	}

ExitError:
//	if (pImports != NULL) {
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
//		if (pImports->nodeTimelineHandle != INVALID_HANDLE_VALUE)
//			CloseHandle(pImports->nodeTimelineHandle);
//		if (pImports->compositorTimelineHandle != INVALID_HANDLE_VALUE)
//			CloseHandle(pImports->compositorTimelineHandle);
//	}
//	if (pExtNodeMem != NULL)
//		UnmapViewOfFile(pExtNodeMem);
//	if (hExtNodeMem != INVALID_HANDLE_VALUE)
//		CloseHandle(hExtNodeMem);
//	if (hNodeProc != INVALID_HANDLE_VALUE)
//		CloseHandle(hNodeProc);
ExitSuccess:
	if (clientSocket != INVALID_SOCKET)
		closesocket(clientSocket);
#endif
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
		ServerInterprocessAcceptNodeConnection();

ExitError:
	if (ipcServer.listenSocket != INVALID_SOCKET)
		closesocket(ipcServer.listenSocket);

	unlink(SOCKET_PATH);
	WSACleanup();
	return NULL;
}

/// Start Server Compositor
void mxcServerInitializeInterprocess()
{
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	printf("Min size of shared memory. Allocation granularity: %lu\n", systemInfo.dwAllocationGranularity);

	ipcServer.listenSocket = INVALID_SOCKET;
	CHECK(pthread_create(&ipcServer.thread, NULL, RunInterProcessServer, NULL), "IPC server pipe creation Fail!");
}

/// Shutdown Server Compositor
void mxcServerShutdownInterprocess()
{
	if (ipcServer.listenSocket != INVALID_SOCKET)
		closesocket(ipcServer.listenSocket);

	unlink(SOCKET_PATH);
	WSACleanup();
}

//// Connect Node to Server Compositor over IPC
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
		printf("Sending exported buffer: %lu size: %llu\n", currentProcessId, sizeof(DWORD));
		int sendResult = send(clientSocket, (const char*)&currentProcessId, sizeof(DWORD), 0);
		WSA_CHECK(sendResult == SOCKET_ERROR, "Send exported id failed");
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
		NodeHandle hNode = RequestExternalNodeHandle(pNodeShared);
		pNodeContext = &node.context[hNode];
		pNodeContext->interprocessMode = MXC_NODE_INTERPROCESS_MODE_IMPORTED;

		pNodeContext->swapsSyncedHandle = pImportedExternalMemory->imports.swapsSyncedHandle;
		assert(pNodeContext->swapsSyncedHandle != NULL);

		printf("Importing node buffer %d\n", hNode);
	}

	// Create node data
	{
		vkSemaphoreCreateInfoExt compTimelineCreateInfo = {
			.debugName = "CompositorTimelineSemaphoreImport",
			.locality = VK_LOCALITY_INTERPROCESS_IMPORTED_READONLY,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			.importHandle = pNodeImports->compositorTimelineHandle,
		};
		vkCreateSemaphoreExt(&compTimelineCreateInfo, &pNodeContext->exported.compositorTimeline);
		vkSemaphoreCreateInfoExt nodeTimelineCreateInfo = {
			.debugName = "NodeTimelineSemaphoreImport",
			.locality = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			.importHandle = pNodeImports->nodeTimelineHandle,
		};
		vkCreateSemaphoreExt(&nodeTimelineCreateInfo, &pNodeContext->exported.nodeTimeline);

		VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
			VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
		};
		VK_CHECK(vkCreateCommandPool(vk.context.device, &graphicsCommandPoolCreateInfo, VK_ALLOC, &pNodeContext->thread.pool));
		VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = pNodeContext->thread.pool,
			.commandBufferCount = 1,
		};
		VK_CHECK(vkAllocateCommandBuffers(vk.context.device, &commandBufferAllocateInfo, &pNodeContext->thread.gfxCmd));
		vkSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)pNodeContext->thread.gfxCmd, "TestNode");
	}

	// Start node thread
	{
#if defined(MOXAIC_COMPOSITOR)
		PANIC("We need a new test scene...");
		atomic_thread_fence(memory_order_release);
		CHECK(pthread_create(&pNodeContext->thread.id, NULL, (void* (*)(void*))mxcTestNodeThread, pNodeContext), "Node Process Import thread creation failed!");
		printf("Node Request Process Import Success.\n");
#endif
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

////
//// IPC Func Queue
////
int midRingEnqueue(MxcRingBuffer* pBuffer, MxcRingBufferHandle target)
{
	ATOMIC_FENCE_SCOPE
	{
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
	ATOMIC_FENCE_SCOPE
	{
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

static void ipcFuncNodeOpened(NodeHandle hNode)
{
	LOG("Opened %d\n", hNode);
	auto pNodeShrd = node.pShared[hNode];

	CreateNodeGBuffer(hNode);

	// Move out of COMPOSITOR_MODE_NONE and set to real COMPOSITOR_MODE to begin compositing
	ReleaseNodeActive(hNode);
	SetNodeActive(hNode, pNodeShrd->compositorMode);
}

static void ipcFuncNodeClosed(NodeHandle hNode)
{
	LOG("Closing %d\n", hNode);
	ReleaseNodeActive(hNode);
	CleanupNode(hNode);
	ReleaseNodeHandle(hNode);
}

/// Scan Node Swapchains for requests and create them if needed.
static void ipcFuncClaimSwap(NodeHandle hNode)
{
#if defined(MOXAIC_COMPOSITOR)
	LOG("Claiming Swap for Node %d\n", hNode);

	auto pNodeShr = node.pShared[hNode];
	auto pNodeCstData = &node.cst.data[hNode];
	auto pNodeCtxt = &node.context[hNode];
	bool needsExport = pNodeCtxt->interprocessMode != MXC_NODE_INTERPROCESS_MODE_THREAD;

	for (int iNodeSwap = 0; iNodeSwap < XR_SWAPCHAIN_CAPACITY; ++iNodeSwap) {
		if (pNodeShr->swapStates[iNodeSwap] != XR_SWAP_STATE_REQUESTED)
			continue;

		block_h hSwap = BLOCK_CLAIM(node.cst.block.swap, 0); // key should be hash of swap info to find recycled swaps
		if (!HANDLE_VALID(hSwap)) {
			LOG_ERROR("Fail to claim SwapImage!\n");
			goto ExitError;
		}
		LOG("Claimed Swap %d for Node %d\n", HANDLE_INDEX(hSwap), hNode);

		auto pSwap = BLOCK_PTR(node.cst.block.swap, hSwap);
		assert(pSwap->state == XR_SWAP_STATE_UNITIALIZED && "Trying to create already created SwapImage!");
		pSwap->state = XR_SWAP_STATE_REQUESTED;
		pSwap->info = pNodeShr->swapInfos[iNodeSwap];
		bool isColor = pSwap->info.usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
		bool isDepth = pSwap->info.usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		for (int iImg = 0; iImg < XR_SWAPCHAIN_IMAGE_COUNT; ++iImg) {
			// We could determine color in CreateColorSwapTexture. Really should just make a VkExternalTexture.
			if (isColor) {
				CreateColorSwapTexture(&pSwap->info, &pSwap->externalTexture[iImg]);
				VK_SET_DEBUG_NAME(pSwap->externalTexture[iImg].texture.image, "ExportedColorSwapImage%d", iImg);
				VK_SET_DEBUG_NAME(pSwap->externalTexture[iImg].texture.view, "ExportedColorSwapView%d", iImg);
				VK_SET_DEBUG_NAME(pSwap->externalTexture[iImg].texture.memory, "ExportedColorSwapMemory%d", iImg);
			} else if (isDepth) {
				CreateDepthSwapTexture(&pSwap->info, &pSwap->externalTexture[iImg]);
				VK_SET_DEBUG_NAME(pSwap->externalTexture[iImg].texture.image, "ExportedDepthSwapImage%d", iImg);
				VK_SET_DEBUG_NAME(pSwap->externalTexture[iImg].texture.view, "ExportedDepthSwapView%d", iImg);
				VK_SET_DEBUG_NAME(pSwap->externalTexture[iImg].texture.memory, "ExportedDepthSwapMemory%d", iImg);
			} else {
				ASSERT(false, "SwapImage is neither color nor depth!");
			}

			if (needsExport) {
				auto pImports = &pNodeCtxt->exported.pExportedMemory->imports;
				WIN32_CHECK(DuplicateHandle(GetCurrentProcess(),
						pSwap->externalTexture[iImg].platform.handle,
						pNodeCtxt->exported.handle,
						&pImports->swapImageHandles[iNodeSwap][iImg],
						0, false, DUPLICATE_SAME_ACCESS),
					"Duplicate localTexture buffer fail");
			}

			pNodeCstData->swaps[iNodeSwap][iImg].image = pSwap->externalTexture[iImg].texture.image;
			pNodeCstData->swaps[iNodeSwap][iImg].view = pSwap->externalTexture[iImg].texture.view;
		}

		pNodeCtxt->hSwaps[iNodeSwap] = hSwap;
		pNodeShr->swapStates[iNodeSwap] = XR_SWAP_STATE_CREATED;
		pSwap->state = XR_SWAP_STATE_CREATED;
	}

	SetEvent(pNodeCtxt->swapsSyncedHandle);

ExitError:
	// TODO
#endif
}
const MxcIpcFuncPtr MXC_IPC_FUNCS[] = {
	[MXC_INTERPROCESS_TARGET_NODE_OPENED] = (MxcIpcFuncPtr const)ipcFuncNodeOpened,
	[MXC_INTERPROCESS_TARGET_NODE_CLOSED] = (MxcIpcFuncPtr const)ipcFuncNodeClosed,
	[MXC_INTERPROCESS_TARGET_SYNC_SWAPS] =  (MxcIpcFuncPtr const)ipcFuncClaimSwap,
};

void mxcInitializeNode() {
	CreateGBufferProcessSetLayout(&node.gbufferProcessSetLayout);
	CreateGBufferProcessPipeLayout(node.gbufferProcessSetLayout, &node.gbufferProcessPipeLayout);
	vkCreateComputePipe("./shaders/compositor_gbuffer_process_down.comp.spv",    node.gbufferProcessPipeLayout, &node.gbufferProcessDownPipe);
	vkCreateComputePipe("./shaders/compositor_gbuffer_process_up.comp.spv",      node.gbufferProcessPipeLayout, &node.gbufferProcessUpPipe);
	VK_SET_DEBUG(node.gbufferProcessDownPipe);
	VK_SET_DEBUG(node.gbufferProcessUpPipe);
}