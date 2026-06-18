#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#undef UNICODE
#include <windows.h>
#include <winsock2.h>
#include <afunix.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <errno.h>
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR   (-1)
#define closesocket    close
#define SHM_NAME       "/moxaic_node_mem"
#endif

#include "mid_vulkan.h"

#include "node.h"
#include "compositor.h"

// State imported into Node Process
platform_handle_t      importedExternalMemoryHandle = INVALID_PLATFORM_HANDLE;
MxcExternalNodeMemory* pImportedExternalMemory = NULL;

struct Node node;

////
//// Swap Pool
////
void mxcNodeGBufferProcessDepth(VkCommandBuffer gfxCmd, ProcessState* pProcessState, MxcNodeSwap* pDepthSwap, MxcNodeGBuffer* pGBuffer, ivec2 swapExtent)
{
	EXTRACT_FIELD(&node, gbufferProcessDownPipe);
	EXTRACT_FIELD(&node, gbufferProcessUpPipe);
	EXTRACT_FIELD(&node, gbufferProcessPipeLayout);

	vk.CmdBindPipeline(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, gbufferProcessDownPipe);
	ProcessState emptyProcessState = {
		.depthFarZ = 1,
		.depthNearZ = 0,
		.cameraFarZ = 1,
		.cameraNearZ = 0,
	};
	vk.CmdPushConstants(gfxCmd, gbufferProcessPipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ProcessState), &emptyProcessState);

	{
		CMD_PUSH_SETS(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, gbufferProcessPipeLayout,	PIPE_SET_INDEX_GBUFFER_PROCESS_INOUT,
			BIND_WRITE_GBUFFER_PROCESS_SRC_DEPTH(pDepthSwap->view),
			BIND_WRITE_GBUFFER_PROCESS_DST_GBUFFER(pGBuffer->mipViews[1]));

		ivec2 groupCount = iVec2Min(iVec2CeiDivide(swapExtent.vec >> 1, 32), 1);
		vk.CmdDispatch(gfxCmd, groupCount.x, groupCount.y, 1);
	}

	/* Blit Down Depth Mips */
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

		ivec2 groupCount = iVec2Min(iVec2CeiDivide(swapExtent.vec >> iMip, 32), 1);
		vk.CmdDispatch(gfxCmd, groupCount.x, groupCount.y, 1);
	}

	/* Blit Up Depth Mips */
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

		ivec2 groupCount = iVec2Min(iVec2CeiDivide(swapExtent.vec >> iMip, 32), 1);
		vk.CmdDispatch(gfxCmd, groupCount.x, groupCount.y, 1);
	}

	/* Final Depth Up Blit */
	{
		CMD_IMAGE_BARRIERS(gfxCmd, {
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

		vk.CmdPushConstants(gfxCmd, gbufferProcessPipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ProcessState), pProcessState);
		CMD_PUSH_SETS(gfxCmd, VK_PIPELINE_BIND_POINT_COMPUTE, gbufferProcessPipeLayout, PIPE_SET_INDEX_GBUFFER_PROCESS_INOUT,
			BIND_WRITE_GBUFFER_PROCESS_SRC_DEPTH(pGBuffer->mipViews[1]),
			BIND_WRITE_GBUFFER_PROCESS_SRC_GBUFFER(pDepthSwap->view),
			BIND_WRITE_GBUFFER_PROCESS_DST_GBUFFER(pGBuffer->mipViews[0]));

		ivec2 groupCount = iVec2Min(iVec2CeiDivide(swapExtent, 32), 1);
		vk.CmdDispatch(gfxCmd, groupCount.x, groupCount.y, 1);
	}
}


static void CreateColorSwapTexture(const XrSwapInfo* pInfo, VkExternalTexture* pSwapTexture)
{
	VkImageCreateInfo info = {
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		&(VkExternalMemoryImageCreateInfo){
			VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
			.handleTypes = MXC_EXTERNAL_FRAMEBUFFER_HANDLE_TYPE,
		},
		.imageType   = VK_IMAGE_TYPE_2D,
		.format      = VK_FORMAT_R8G8B8A8_UNORM,
		.extent      = {pInfo->windowWidth, pInfo->windowHeight, 1},
		.mipLevels   = 1,
		.arrayLayers = 1,
		.samples     = VK_SAMPLE_COUNT_1_BIT,
		.usage       = VK_RENDER_PASS_USAGES[VK_RENDER_PASS_ATTACHMENT_INDEX_COLOR] | pInfo->usageFlags,
	};
#ifdef _WIN32
	vkCreateExternalPlatformTexture(&info, &pSwapTexture->platform);
	VkDedicatedTextureCreateInfo textureInfo = {
		.pImageCreateInfo = &info,
		.aspectMask       = VK_IMAGE_ASPECT_COLOR_BIT,
		.importHandle     = pSwapTexture->platform.handle,
		.handleType       = MXC_EXTERNAL_FRAMEBUFFER_HANDLE_TYPE,
		.locality         = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
	};
#else
	VkDedicatedTextureCreateInfo textureInfo = {
		.pImageCreateInfo = &info,
		.aspectMask       = VK_IMAGE_ASPECT_COLOR_BIT,
		.importHandle     = INVALID_PLATFORM_HANDLE,
		.handleType       = MXC_EXTERNAL_FRAMEBUFFER_HANDLE_TYPE,
		.locality         = VK_LOCALITY_CONTEXT,
	};
#endif
	vkCreateDedicatedTexture(&textureInfo, &pSwapTexture->texture);

	VK_IMMEDIATE_COMMAND_BUFFER_CONTEXT(VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS) {
		CMD_IMAGE_BARRIERS(cmd, {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.image = pSwapTexture->texture.image,
			VK_IMAGE_BARRIER_SRC_UNDEFINED,
			.dstStageMask = VK_PIPELINE_STAGE_2_NONE,
			.dstAccessMask = VK_ACCESS_2_NONE,
			.newLayout = VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
		});
	}
}

static void CreateDepthSwapTexture(const XrSwapInfo* pInfo, VkExternalTexture* pSwapTexture)
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
		.usage       = VK_IMAGE_USAGE_STORAGE_BIT |
	                   VK_IMAGE_USAGE_SAMPLED_BIT |
					   VK_IMAGE_USAGE_TRANSFER_DST_BIT,
	};
#ifdef _WIN32
	vkCreateExternalPlatformTexture(&imageCreateInfo, &pSwapTexture->platform);
	VkDedicatedTextureCreateInfo textureInfo = {
		.pImageCreateInfo = &imageCreateInfo,
		.aspectMask       = VK_IMAGE_ASPECT_COLOR_BIT,
		.importHandle     = pSwapTexture->platform.handle,
		.handleType       = MXC_EXTERNAL_FRAMEBUFFER_HANDLE_TYPE,
		.locality         = VK_LOCALITY_INTERPROCESS_IMPORTED_READWRITE,
	};
#else
	VkDedicatedTextureCreateInfo textureInfo = {
		.pImageCreateInfo = &imageCreateInfo,
		.aspectMask       = VK_IMAGE_ASPECT_COLOR_BIT,
		.importHandle     = INVALID_PLATFORM_HANDLE,
		.handleType       = MXC_EXTERNAL_FRAMEBUFFER_HANDLE_TYPE,
		.locality         = VK_LOCALITY_CONTEXT,
	};
#endif
	vkCreateDedicatedTexture(&textureInfo, &pSwapTexture->texture);

	VK_IMMEDIATE_COMMAND_BUFFER_CONTEXT(VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS)	{
		CMD_IMAGE_BARRIERS(cmd,	{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.image = pSwapTexture->texture.image,
			VK_IMAGE_BARRIER_SRC_UNDEFINED,
			.dstStageMask = VK_PIPELINE_STAGE_2_NONE,
			.dstAccessMask = VK_ACCESS_2_NONE,
			.newLayout = VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			VK_IMAGE_BARRIER_COLOR_SUBRESOURCE_RANGE,
		});
	}
}

static void CreateNodeGBuffer(node_h hNode)
{
// I need to get compositor specific and node specific code into different files
#if defined(MOXAIC_COMPOSITOR)
	MxcNodeContext*        pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcCompositorNodeData* pNodeCpst = ARRAY_PTR_H(cst.nodeData, hNode);
	MxcNodeShared*         pNodeShrd = ARRAY_H(node.pShared, hNode);

	CHECK(pNodeShrd->swapMaxWidth < 1024 || pNodeShrd->swapMaxHeight < 1024, "Swap too small!")

	int mipLevelCount = VK_MIP_LEVEL_COUNT(pNodeShrd->swapMaxWidth, pNodeShrd->swapMaxHeight);
	ASSERT(mipLevelCount < MXC_NODE_GBUFFER_MAX_MIP_COUNT, "Max gbuffer mip count exceeded.");

	for (int iView = 0; iView < XR_MAX_VIEW_COUNT; ++iView) {
		vkCreateDedicatedTexture(&(VkDedicatedTextureCreateInfo){
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
		pNodeCpst->gbuffer[iView].image = pNodeCtxt->gbuffer[iView].image;
		pNodeCpst->gbuffer[iView].mipViewCount = mipLevelCount;

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
			pNodeCpst->gbuffer[iView].mipViews[iMip] = view;
			VK_SET_DEBUG_NAME(pNodeCpst->gbuffer[iView].mipViews[iMip], "NodeGBufferView%d Mip%d", iView, iMip);
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
	for (int iImg = 0; iImg < XR_SWAPCHAIN_IMAGE_COUNT; ++iImg) {
		vkDestroyDedicatedTexture(&pSwap->externalTexture[iImg].texture);
#ifdef _WIN32
		vkDestroyExternalPlatformTexture(&pSwap->externalTexture[iImg].platform);
#endif
	}
	memset(pSwap, 0, sizeof(MxcSwapTexture));
}

/*
 * Node Lifecycle
 */
node_h RequestLocalNodeHandle()
{
	node_h hNode = BLOCK_CLAIM(node.context, 0);
	LOG("Requested Local Node Handle %d.\n", HANDLE_INDEX(hNode));

	MxcNodeContext* pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	ASSERT(IS_P_ZERO(pNodeCtxt), "MxcNodeContext not zeroed!");

	MxcNodeShared** ppNodeShrd = ARRAY_PTR_H(node.pShared, hNode);
	ASSERT(*ppNodeShrd == NULL);
	XMALLOC_ZERO_P(*ppNodeShrd);

#if defined(MOXAIC_COMPOSITOR)
	MxcCompositorNodeData* pNodeCpst = ARRAY_PTR_H(cst.nodeData, hNode);
	ASSERT(IS_P_ZERO(pNodeCpst), "MxcCompositorNodeData not zeroed!");
#endif

	return hNode;
}

MidResult RequestExternalNodeHandle(MxcNodeShared* pNodeShared, node_h* pNode_h)
{
	node_h hNode = BLOCK_CLAIM(node.context, 0);
	if (HANDLE_INVALID(hNode)) return MID_LIMIT_REACHED;
	LOG("Claimed External Node Handle %d.\n", HANDLE_INDEX(hNode));

	MxcNodeContext* pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	ASSERT(IS_P_ZERO(pNodeCtxt), "MxcNodeContext not zeroed!");

	MxcNodeShared** ppNodeShrd = ARRAY_PTR_H(node.pShared, hNode);
	ASSERT(*ppNodeShrd == NULL);
	*ppNodeShrd = pNodeShared;

#if defined(MOXAIC_COMPOSITOR)
	MxcCompositorNodeData* pNodeCpst = ARRAY_PTR_H(cst.nodeData, hNode);
	ASSERT(IS_P_ZERO(pNodeCpst), "MxcCompositorNodeData not zeroed!");
#endif

	*pNode_h = hNode;
	return MID_SUCCESS;
}

void ReleaseNodeHandle(node_h hNode)
{
	MxcNodeContext* pNodeCtxt = BLOCK_RELEASE(node.context, hNode);
	ASSERT(!IS_P_ZERO(pNodeCtxt), "MxcNodeContext zeroed!");
	ZERO(pNodeCtxt);

	MxcNodeShared** ppNodeShrd = ARRAY_PTR_H(node.pShared, hNode);
	ASSERT(*ppNodeShrd != NULL);
	*ppNodeShrd = NULL;

#if defined(MOXAIC_COMPOSITOR)
	MxcCompositorNodeData* pNodeCpst = ARRAY_PTR_H(cst.nodeData, hNode);
	ASSERT(!IS_P_ZERO(pNodeCpst), "MxcCompositorNodeData zeroed!");
	ZERO(pNodeCpst);
#endif

	LOG("Released Node Handle %d.\n", HANDLE_INDEX(hNode));
}

// Register node to MXC_COMPOSITOR_MODE_NONE to start processing
void mxcRegisterActiveNode(node_h hNode)
{
#if defined(MOXAIC_COMPOSITOR)
	MxcNodeContext*        pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*         pNodeShrd = ARRAY_H(node.pShared, hNode);
	MxcCompositorNodeData* pNodeCpst = ARRAY_PTR_H(cst.nodeData, hNode);

	pNodeCpst->activeCompositorMode = MXC_COMPOSITOR_MODE_NONE;
	MxcActiveNodes* pActiveNodes = &node.active[pNodeShrd->compositorMode];
	u32 iActiveNode = atomic_fetch_add(&pActiveNodes->count, 1);
	pActiveNodes->handles[iActiveNode] = hNode;

	LOG("Registered node %d to %s\n", HANDLE_INDEX(hNode), string_MxcCompositorMode(pNodeShrd->compositorMode));
#endif
}

// Syncs node into Active Node List specified by pNodeShrd->compositorMode
void syncActiveNodeMode(node_h hNode)
{
#if defined(MOXAIC_COMPOSITOR)
	MxcNodeContext*        pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*         pNodeShrd = ARRAY_H(node.pShared, hNode);
	MxcCompositorNodeData* pNodeCpst = ARRAY_PTR_H(cst.nodeData, hNode);

	pNodeCpst->activeCompositorMode = pNodeShrd->compositorMode;
	MxcActiveNodes* pActiveNodes = &node.active[pNodeShrd->compositorMode];
	u32 iActiveNode = atomic_fetch_add(&pActiveNodes->count, 1);
	pActiveNodes->handles[iActiveNode] = hNode;

	LOG("Added node %d to %s\n", HANDLE_INDEX(hNode), string_MxcCompositorMode(pNodeShrd->compositorMode));
#endif
}

// Release Node from Active Node List specified by pNodeCpst->activeCompositorMode
void ReleaseCompositorNodeActive(node_h hNode)
{
#if defined(MOXAIC_COMPOSITOR)
	ASSERT((compositorContext.baseCycleValue % MXC_CYCLE_COUNT) == MXC_CYCLE_UPDATE_WINDOW_STATE,
		"Trying to ReleaseCompositorNodeActive not in MXC_CYCLE_UPDATE_WINDOW_STATE");

	MxcNodeContext*        pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*         pNodeShrd = ARRAY_H(node.pShared, hNode);
	MxcCompositorNodeData* pNodeCpst = ARRAY_PTR_H(cst.nodeData, hNode);
	ASSERT(pNodeCtxt->interprocessMode != MXC_NODE_INTERPROCESS_MODE_NONE);

	MxcActiveNodes* pActiveNodes = &node.active[pNodeCpst->activeCompositorMode];

	// compact handles down... TODO this should use memmove
	u16 count = atomic_fetch_sub(&pActiveNodes->count, 1);
	u16 i = 0;
	for (; i < count; ++i)
		if (pActiveNodes->handles[i] == hNode) break;
	for (; i < count - 1; ++i)
		pActiveNodes->handles[i] = pActiveNodes->handles[i + 1];
#endif
}

#ifdef _WIN32
#define CLOSE_HANDLE(_handle)                                                     \
	if (!CloseHandle(_handle)) {                                                  \
		DWORD dwError = GetLastError();                                           \
		LOG("Could not close (%s) object buffer (%lu).\n", #_handle, dwError); \
	}
#else
#define CLOSE_HANDLE(_handle) close(_handle)
#endif
static int CleanupNode(node_h hNode)
{
	u16 iNode = HANDLE_INDEX(hNode);
	MxcNodeContext*        pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*         pNodeShrd = ARRAY_H(node.pShared, hNode);
//	MxcCompositorNodeData* pNodeCpst = ARRAY_PTR_H(cst.nodeData, hNode);

//	int  iSwapBlock = MXC_SWAP_TYPE_BLOCK_INDEX_BY_TYPE[pNodeShared->swapType];

	switch (pNodeCtxt->interprocessMode)
	{
		case MXC_NODE_INTERPROCESS_MODE_THREAD: {
			vkFreeCommandBuffers(vk.context.device, pNodeCtxt->thread.pool, 1, &pNodeCtxt->thread.gfxCmd);
			vkDestroyCommandPool(vk.context.device, pNodeCtxt->thread.pool, VK_ALLOC);
			int result = pthread_join(pNodeCtxt->thread.threadId, NULL);
			if (result != 0) {
				perror("Thread join failed");
			}
			free((void*)pNodeShrd);
			break;
		}
		case MXC_NODE_INTERPROCESS_MODE_EXPORTED: {
#if defined(MOXAIC_COMPOSITOR)
			 mxcClearNodeDescriptorSet(hNode);

			// We are fully destroying swaps and gbuffer but may want to retain them someday
			for (int iNodeSwap = 0; iNodeSwap < MXC_NODE_SWAP_CAPACITY; ++iNodeSwap) {
				swap_h hSwap = pNodeCtxt->hSwaps[iNodeSwap];
				if (!HANDLE_VALID(hSwap)) continue;

				MxcSwapTexture* pSwap = BLOCK_RELEASE(cst.block.swap, hSwap);
				mxcDestroySwapTexture(pSwap);
			}
			for (int iView = 0; iView < XR_MAX_VIEW_COUNT; ++iView) {
				if (pNodeCtxt->gbuffer[iView].view == NULL) continue;
				vkDestroyDedicatedTexture(&pNodeCtxt->gbuffer[iView]);
			}

			CLOSE_HANDLE(pNodeCtxt->exported.nodeTimelineHandle);
			vkDestroySemaphore(vk.context.device, pNodeCtxt->exported.nodeTimeline, VK_ALLOC);

			CLOSE_HANDLE(pNodeCtxt->swapsSyncedHandle);

#ifdef _WIN32
			CHECK_WIN32(UnmapViewOfFile(pNodeCtxt->exported.pExportedMemory));
			CLOSE_HANDLE(pNodeCtxt->exported.exportedMemoryHandle);
			CLOSE_HANDLE(pNodeCtxt->exported.hProcess);
#else
			munmap(pNodeCtxt->exported.pExportedMemory, sizeof(MxcExternalNodeMemory));
			shm_unlink(SHM_NAME);
#endif
#endif
			break;
		}
		case MXC_NODE_INTERPROCESS_MODE_IMPORTED: {
			// The process which imported the handle must close them.
			for (int iSwap = 0; iSwap < XR_SWAPCHAIN_CAPACITY; ++iSwap) {
				for (int iImg = 0; iImg < XR_SWAPCHAIN_IMAGE_COUNT; ++iImg) {
					CLOSE_HANDLE(pImportedExternalMemory->imports.swapImageHandles[iSwap][iImg]);
				}
			}
			CLOSE_HANDLE(pNodeCtxt->swapsSyncedHandle);
			CLOSE_HANDLE(pNodeCtxt->imported.nodeTimelineHandle);
			CLOSE_HANDLE(pNodeCtxt->imported.compositorTimelineHandle);
#ifdef _WIN32
			CHECK_WIN32(UnmapViewOfFile(pImportedExternalMemory));
#else
			munmap(pImportedExternalMemory, sizeof(MxcExternalNodeMemory));
#endif
			CLOSE_HANDLE(importedExternalMemoryHandle);
			break;
		}
		default: PANIC("Node interprocessMode not supported");
	}

	return 0;
}

void mxcRequestNodeThread(void* (*runFunc)(void*), node_h* pNodeHandle)
{
#if defined(MOXAIC_COMPOSITOR)
	LOG("Requesting Node Thread.\n");
	node_h hNode = RequestLocalNodeHandle();
	u16    iNode = HANDLE_INDEX(hNode);
	MxcNodeContext*        pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*         pNodeShrd = ARRAY_H(node.pShared, hNode);
	MxcCompositorNodeData* pNodeCpst = ARRAY_PTR_H(cst.nodeData, hNode);

	pNodeCtxt->interprocessMode = MXC_NODE_INTERPROCESS_MODE_THREAD;
#ifdef _WIN32
	pNodeCtxt->swapsSyncedHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
#else
	pNodeCtxt->swapsSyncedHandle = eventfd(0, 0);
#endif

	pNodeShrd->compositorMode = MXC_COMPOSITOR_MODE_NONE;

	pNodeShrd->rootPose.pos = VEC3(iNode + 1, 0, 0);
	pNodeShrd->rootPose.rot = QuatFromEuler(pNodeShrd->rootPose.euler);

	pNodeShrd->cameraPose.pos = VEC3(0, 0, 0);
	pNodeShrd->cameraPose.rot = QuatFromEuler(pNodeShrd->cameraPose.euler);

	pNodeShrd->camera.yFovRad = RAD_FROM_DEG(45.0f);
	pNodeShrd->camera.zNear = 0.1f;
	pNodeShrd->camera.zFar = 100.0f;
	pNodeShrd->camera.dimension.x = DEFAULT_WIDTH;
	pNodeShrd->camera.dimension.y = DEFAULT_HEIGHT;

	pNodeShrd->compositorRadius = 0.5;
	pNodeShrd->compositorCycleSkip = 16;

	pNodeShrd->swapMaxWidth = DEFAULT_WIDTH;
	pNodeShrd->swapMaxHeight = DEFAULT_HEIGHT;

	for (int i = 0; i < XR_MAX_VIEW_COUNT; ++i) {
		// need better way to determine these invalid
		// and maybe better way to signify frame has been set
		pNodeShrd->viewSwaps[i].iColorSwap = CHAR_MAX;
		pNodeShrd->viewSwaps[i].iDepthSwap = CHAR_MAX;
	}

	vkSemaphoreCreateInfoExt semaphoreCreateInfo = {
		.locality = VK_LOCALITY_CONTEXT,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
	};
	vkCreateSemaphoreExt(&semaphoreCreateInfo, &pNodeCtxt->thread.nodeTimeline);
	VK_SET_DEBUG(pNodeCtxt->thread.nodeTimeline);

	VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
	};
	VK_CHECK(vkCreateCommandPool(vk.context.device, &graphicsCommandPoolCreateInfo, VK_ALLOC, &pNodeCtxt->thread.pool));
	VK_SET_DEBUG(pNodeCtxt->thread.pool);
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = pNodeCtxt->thread.pool,
		.commandBufferCount = 1,
	};
	VK_CHECK(vkAllocateCommandBuffers(vk.context.device, &commandBufferAllocateInfo, &pNodeCtxt->thread.gfxCmd));
	VK_SET_DEBUG(pNodeCtxt->thread.gfxCmd);

	*pNodeHandle = hNode;

	CHECK(pthread_create(&pNodeCtxt->thread.threadId, NULL, (void* (*)(void*))runFunc, (void*)(u64)hNode), "Node thread creation failed!");

	// Add to COMPOSITOR_MODE_NONE initially to start processing
	MID_CHANNEL_SEND(&node.newConnectionQueue, node.queuedNewConnections, &hNode);

	LOG("Request Node Thread Success. Handle: %d\n", HANDLE_INDEX(hNode));
	// todo this needs error handling
#endif
}

/*
 * IPC LifeCycle
 */
#ifdef _WIN32
#define SOCKET_PATH "C:\\temp\\moxaic_socket"
#else
#define SOCKET_PATH "/tmp/moxaic_socket"
#endif

static struct {
	SOCKET    listenSocket;
	pthread_t thread;
} ipcServer;
const char serverIPCAckMessage[] = "CONNECT-MOXAIC-COMPOSITOR-0.0.0";
const char nodeIPCAckMessage[] = "CONNECT-MOXAIC-NODE-0.0.0";

#define SOCK_CHECK(_command, _message)                              \
	{                                                               \
		int _result = (int)(_command);                              \
		if (__builtin_expect(!!(_result < 0), 0)) {                 \
			fprintf(stderr, "%s: %d\n", _message, errno);           \
			goto Error;                                             \
		}                                                           \
	}

#ifdef _WIN32
#define WIN32_CHECK(_command, _message)                             \
	{                                                               \
		int _result = (_command);                                   \
		if (__builtin_expect(!(_result), 0)) {                      \
			fprintf(stderr, "%s: %ld\n", _message, GetLastError()); \
			goto Error;                                             \
		}                                                           \
	}
#define WSA_CHECK(_command, _message)                                 \
	{                                                                 \
		int _result = (_command);                                     \
		if (__builtin_expect(!!(_result), 0)) {                       \
			fprintf(stderr, "%s: %d\n", _message, WSAGetLastError()); \
			goto Error;                                               \
		}                                                             \
	}
#endif

static void ServerInterprocessAcceptNodeConnection()
{
#if defined(MOXAIC_COMPOSITOR)
	LOG("Accepting connections on: '%s'\n", SOCKET_PATH);
	MxcNodeContext*        pNodeCtxt = NULL;
	MxcNodeShared*         pNodeShrd = NULL;
	MxcNodeImports*        pImports  = NULL;
	MxcExternalNodeMemory* pExtNodeMem = NULL;
	platform_pid_t         processId = 0;

	SOCKET clientSocket = accept(ipcServer.listenSocket, NULL, NULL);
	if (clientSocket == INVALID_SOCKET) goto Error;
	LOG("Accepted Connection.\n");

	{
		char buffer[sizeof(nodeIPCAckMessage)] = {};
		int  n = (int)recv(clientSocket, buffer, sizeof(nodeIPCAckMessage), 0);
		if (n <= 0) { fprintf(stderr, "Recv nodeIPCAckMessage failed\n"); goto Error; }
		CHECK(strcmp(buffer, nodeIPCAckMessage), "Unexpected node message");
	}
	{
		int sendResult = (int)send(clientSocket, serverIPCAckMessage, strlen(serverIPCAckMessage), 0);
		if (sendResult <= 0) { fprintf(stderr, "Send server ack failed\n"); goto Error; }
	}
	{
		int n = (int)recv(clientSocket, (char*)&processId, sizeof(processId), 0);
		if (n <= 0) { fprintf(stderr, "Recv processId failed\n"); goto Error; }
		LOG("Received node processId: %d\n", (int)processId);
	}

#ifdef _WIN32
	HANDLE hProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, processId);
	WIN32_CHECK(hProcess != NULL && hProcess != INVALID_HANDLE_VALUE, "Open process failed");
	HANDLE hExtNodeMem = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(MxcExternalNodeMemory), NULL);
	WIN32_CHECK(hExtNodeMem != NULL, "Could not create file mapping object");
	pExtNodeMem = MapViewOfFile(hExtNodeMem, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MxcExternalNodeMemory));
	WIN32_CHECK(pExtNodeMem != NULL, "MapViewOfFile failed");
#else
	shm_unlink(SHM_NAME);
	int shmFd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600);
	SOCK_CHECK(shmFd, "shm_open failed");
	SOCK_CHECK(ftruncate(shmFd, sizeof(MxcExternalNodeMemory)), "ftruncate failed");
	pExtNodeMem = mmap(NULL, sizeof(MxcExternalNodeMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
	SOCK_CHECK(pExtNodeMem == MAP_FAILED ? -1 : 0, "mmap failed");
	if (pExtNodeMem == MAP_FAILED) goto Error;
	platform_handle_t hExtNodeMem = shmFd;
#endif

	memset(pExtNodeMem, 0, sizeof(MxcExternalNodeMemory));
	pImports = &pExtNodeMem->imports;
	pNodeShrd = &pExtNodeMem->shared;

	node_h hNode;
	RequestExternalNodeHandle(pNodeShrd, &hNode);

	{
		pNodeShrd->rootPose.pos = VEC3(0, 0, 0);
		pNodeShrd->rootPose.rot = QuatFromEuler(pNodeShrd->rootPose.euler);
		pNodeShrd->cameraPose.pos = VEC3(0, 0, 0);
		pNodeShrd->cameraPose.rot = QuatFromEuler(pNodeShrd->cameraPose.euler);
		pNodeShrd->camera.yFovRad = RAD_FROM_DEG(45.0f);
		pNodeShrd->camera.zNear = 0.1f;
		pNodeShrd->camera.zFar = 100.0f;
		pNodeShrd->camera.dimension.x = DEFAULT_WIDTH;
		pNodeShrd->camera.dimension.y = DEFAULT_HEIGHT;
		pNodeShrd->compositorRadius = 0.5;
		pNodeShrd->compositorCycleSkip = 8;
		pNodeShrd->swapMaxWidth = DEFAULT_WIDTH;
		pNodeShrd->swapMaxHeight = DEFAULT_HEIGHT;
		for (int i = 0; i < XR_MAX_VIEW_COUNT; ++i) {
			pNodeShrd->viewSwaps[i].iColorSwap = CHAR_MAX;
			pNodeShrd->viewSwaps[i].iDepthSwap = CHAR_MAX;
		}

		vkSemaphoreCreateInfoExt semaphoreCreateInfo = {
			.locality = VK_LOCALITY_INTERPROCESS_EXPORTED_READWRITE,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		};
		VkSemaphore nodeTimeline;
		vkCreateSemaphoreExt(&semaphoreCreateInfo, &nodeTimeline);
		VK_SET_DEBUG_NAME(nodeTimeline, "Export Node Timeline");

		pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
		pNodeCtxt->interprocessMode = MXC_NODE_INTERPROCESS_MODE_EXPORTED;
		pNodeCtxt->exported.processId = processId;
		pNodeCtxt->exported.exportedMemoryHandle = hExtNodeMem;
		pNodeCtxt->exported.pExportedMemory = pExtNodeMem;
		pNodeCtxt->exported.nodeTimeline = nodeTimeline;
		pNodeCtxt->exported.nodeTimelineHandle = vkGetSemaphoreExternalHandle(nodeTimeline);
		pNodeCtxt->exported.compositorTimeline = compositorContext.timeline;
		pNodeCtxt->exported.compositorTimelineHandle = compositorContext.timelineHandle;

#ifdef _WIN32
		pNodeCtxt->swapsSyncedHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
		pNodeCtxt->exported.hProcess = hProcess;

		HANDLE currentHandle = GetCurrentProcess();
		WIN32_CHECK(DuplicateHandle(currentHandle, pNodeCtxt->swapsSyncedHandle,
				hProcess, &pImports->swapsSyncedHandle, 0, false, DUPLICATE_SAME_ACCESS),
			"Duplicate swapsSyncedHandle fail.");
		WIN32_CHECK(DuplicateHandle(currentHandle, pNodeCtxt->exported.nodeTimelineHandle,
				hProcess, &pImports->nodeTimelineHandle, 0, false, DUPLICATE_SAME_ACCESS),
			"Duplicate nodeTimeline fail.");
		WIN32_CHECK(DuplicateHandle(currentHandle, pNodeCtxt->exported.compositorTimelineHandle,
				hProcess, &pImports->compositorTimelineHandle, 0, false, DUPLICATE_SAME_ACCESS),
			"Duplicate compositor timeline fail.");
#else
		pNodeCtxt->swapsSyncedHandle = eventfd(0, 0);
		pImports->swapsSyncedHandle = INVALID_PLATFORM_HANDLE;
		pImports->nodeTimelineHandle = INVALID_PLATFORM_HANDLE;
		pImports->compositorTimelineHandle = INVALID_PLATFORM_HANDLE;
#endif
	}

	{
#ifdef _WIN32
		HANDLE duplicatedExtNodeMemHandle;
		WIN32_CHECK(DuplicateHandle(GetCurrentProcess(), pNodeCtxt->exported.exportedMemoryHandle,
				hProcess, &duplicatedExtNodeMemHandle, 0, false, DUPLICATE_SAME_ACCESS),
			"Duplicate sharedMemory fail.");
		int sendResult = (int)send(clientSocket, (const char*)&duplicatedExtNodeMemHandle, sizeof(HANDLE), 0);
		if (sendResult <= 0) { fprintf(stderr, "Send shared memory failed\n"); goto Error; }
#else
		/* On Linux, send the shm name and node index; node opens it independently */
		uint32_t nodeIdx = (uint32_t)HANDLE_INDEX(hNode);
		int n = (int)send(clientSocket, (char*)&nodeIdx, sizeof(nodeIdx), 0);
		if (n <= 0) { fprintf(stderr, "Send nodeIdx failed\n"); goto Error; }
#endif
		LOG("Process Node Export Success.\n");
	}

	MID_CHANNEL_SEND(&node.newConnectionQueue, node.queuedNewConnections, &hNode);
	goto ExitSuccess;

Error:
	fprintf(stderr, "ServerInterprocessAcceptNodeConnection error\n");
ExitSuccess:
	if (clientSocket != INVALID_SOCKET)
		closesocket(clientSocket);
#endif
}

static void* RunInterProcessServer(void* arg)
{
	(void)arg;
	struct sockaddr_un address;
	memset(&address, 0, sizeof(address));
	address.sun_family = AF_UNIX;

	unlink(SOCKET_PATH);
	strncpy(address.sun_path, SOCKET_PATH, sizeof(address.sun_path) - 1);

#ifdef _WIN32
	WSADATA wsaData = {};
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) goto Error;
#endif

	ipcServer.listenSocket = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ipcServer.listenSocket == INVALID_SOCKET) goto Error;
	if (bind(ipcServer.listenSocket, (struct sockaddr*)&address, sizeof(address))) goto Error;
	if (listen(ipcServer.listenSocket, SOMAXCONN)) goto Error;

	while (ATOMIC_GET(mxc.lifecycle) != MXC_LIFECYCLE_EXITING)
		ServerInterprocessAcceptNodeConnection();

Error:
	if (ipcServer.listenSocket != INVALID_SOCKET)
		closesocket(ipcServer.listenSocket);
	unlink(SOCKET_PATH);
#ifdef _WIN32
	WSACleanup();
#endif
	return NULL;
}

void mxcServerInitializeInterprocess()
{
#ifdef _WIN32
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	LOG("Allocation granularity: %lu\n", systemInfo.dwAllocationGranularity);
#endif
	ipcServer.listenSocket = INVALID_SOCKET;
	CHECK(pthread_create(&ipcServer.thread, NULL, RunInterProcessServer, NULL), "IPC server thread creation failed!");
}

void mxcServerShutdownInterprocess()
{
	if (ipcServer.listenSocket != INVALID_SOCKET)
		closesocket(ipcServer.listenSocket);
	unlink(SOCKET_PATH);
#ifdef _WIN32
	WSACleanup();
#endif
}

void mxcConnectInterprocessNode(bool createTestNode)
{
	(void)createTestNode;
	if (pImportedExternalMemory != NULL && PLATFORM_HANDLE_VALID(importedExternalMemoryHandle)) {
		LOG("IPC already connected, skipping.\n");
		return;
	}

	LOG("Connecting on: '%s'\n", SOCKET_PATH);
	MxcExternalNodeMemory* pExternalNodeMemory = NULL;
	SOCKET clientSocket = INVALID_SOCKET;
	platform_handle_t externalNodeMemoryHandle = INVALID_PLATFORM_HANDLE;

	{
		struct sockaddr_un address;
		memset(&address, 0, sizeof(address));
		address.sun_family = AF_UNIX;
		strncpy(address.sun_path, SOCKET_PATH, sizeof(address.sun_path) - 1);
#ifdef _WIN32
		WSADATA wsaData = {};
		if (WSAStartup(MAKEWORD(2, 2), &wsaData)) goto Error;
#endif
		clientSocket = socket(AF_UNIX, SOCK_STREAM, 0);
		if (clientSocket == INVALID_SOCKET) { fprintf(stderr, "Socket failed\n"); goto Error; }
		if (connect(clientSocket, (struct sockaddr*)&address, sizeof(address))) { fprintf(stderr, "Connect failed\n"); goto Error; }
		LOG("Connected to server.\n");
	}

	{
		int n = (int)send(clientSocket, nodeIPCAckMessage, strlen(nodeIPCAckMessage), 0);
		if (n <= 0) { fprintf(stderr, "Send node ack failed\n"); goto Error; }
	}
	{
		char buffer[sizeof(serverIPCAckMessage)] = {};
		int n = (int)recv(clientSocket, buffer, sizeof(serverIPCAckMessage), 0);
		if (n <= 0 || strcmp(buffer, serverIPCAckMessage)) { fprintf(stderr, "Recv server ack failed\n"); goto Error; }
	}

#ifdef _WIN32
	{
		DWORD pid = GetCurrentProcessId();
		int n = (int)send(clientSocket, (char*)&pid, sizeof(pid), 0);
		if (n <= 0) { fprintf(stderr, "Send processId failed\n"); goto Error; }
	}
	{
		int n = (int)recv(clientSocket, (char*)&externalNodeMemoryHandle, sizeof(HANDLE), 0);
		if (n <= 0) { fprintf(stderr, "Recv sharedMem handle failed\n"); goto Error; }
		pExternalNodeMemory = MapViewOfFile(externalNodeMemoryHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MxcExternalNodeMemory));
		WIN32_CHECK(pExternalNodeMemory != NULL, "MapViewOfFile failed");
	}
#else
	{
		platform_pid_t pid = getpid();
		int n = (int)send(clientSocket, (char*)&pid, sizeof(pid), 0);
		if (n <= 0) { fprintf(stderr, "Send processId failed\n"); goto Error; }
	}
	{
		uint32_t nodeIdx = 0;
		int n = (int)recv(clientSocket, (char*)&nodeIdx, sizeof(nodeIdx), 0);
		if (n <= 0) { fprintf(stderr, "Recv nodeIdx failed\n"); goto Error; }
		int shmFd = shm_open(SHM_NAME, O_RDWR, 0);
		if (shmFd < 0) { fprintf(stderr, "shm_open failed\n"); goto Error; }
		pExternalNodeMemory = mmap(NULL, sizeof(MxcExternalNodeMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
		if (pExternalNodeMemory == MAP_FAILED) { fprintf(stderr, "mmap failed\n"); close(shmFd); goto Error; }
		externalNodeMemoryHandle = shmFd;
	}
#endif

	pImportedExternalMemory = pExternalNodeMemory;
	importedExternalMemoryHandle = externalNodeMemoryHandle;
	goto ExitSuccess;

Error:
	fprintf(stderr, "mxcConnectInterprocessNode error\n");
ExitSuccess:
	if (clientSocket != INVALID_SOCKET)
		closesocket(clientSocket);
#ifdef _WIN32
	WSACleanup();
#endif
}

void mxcShutdownInterprocessNode()
{
	if (PLATFORM_HANDLE_VALID(importedExternalMemoryHandle)) {
#ifdef _WIN32
		UnmapViewOfFile(pImportedExternalMemory);
		CloseHandle(importedExternalMemoryHandle);
#else
		munmap(pImportedExternalMemory, sizeof(MxcExternalNodeMemory));
		close(importedExternalMemoryHandle);
#endif
		pImportedExternalMemory = NULL;
		importedExternalMemoryHandle = INVALID_PLATFORM_HANDLE;
	}
}

/*
 * IPC Func Queue
 */
static void ipcFuncNodeOpened(node_h hNode)
{
	LOG("Node Opened %d\n", HANDLE_INDEX(hNode));
	CreateNodeGBuffer(hNode);

	// Move out of COMPOSITOR_MODE_NONE and set to current COMPOSITOR_MODE to begin compositing
	ReleaseCompositorNodeActive(hNode);
	syncActiveNodeMode(hNode);
}

static void ipcFuncNodeClosed(node_h hNode)
{
	LOG("Node Closing %d\n", HANDLE_INDEX(hNode));
	ReleaseCompositorNodeActive(hNode);

	CleanupNode(hNode);
	ReleaseNodeHandle(hNode);
}

static void ipcFuncNodeBounds(node_h hNode)
{
	LOG("Node Bounds %d\n", HANDLE_INDEX(hNode));
}

static void ipcFuncClaimSwap(node_h hNode)
{
#if defined(MOXAIC_COMPOSITOR)
	MxcNodeContext*        pNodeCtxt = BLOCK_PTR_H(node.context, hNode);
	MxcNodeShared*         pNodeShrd = ARRAY_H(node.pShared, hNode);
	MxcCompositorNodeData* pNodeCpst = ARRAY_PTR_H(cst.nodeData, hNode);
	bool needsExport = pNodeCtxt->interprocessMode != MXC_NODE_INTERPROCESS_MODE_THREAD;

	// Scan Node Swapchains for requests and create them if needed.
	for (int iNodeSwap = 0; iNodeSwap < XR_SWAPCHAIN_CAPACITY; ++iNodeSwap) {

		switch (pNodeShrd->nodeSwapStates[iNodeSwap])
		{
			case XR_SWAP_STATE_REQUESTED: {
				block_h hSwap = BLOCK_CLAIM(cst.block.swap, 0); // key should be hash of swap info to find recycled swaps
				if (!HANDLE_VALID(hSwap)) {
					LOG_ERROR("Fail to claim SwapImage!\n");
					pNodeShrd->nodeSwapStates[iNodeSwap] = XR_SWAP_STATE_ERROR;
					goto Out;
				}
				LOG("Claimed Swap %d for Node %d\n", HANDLE_INDEX(hSwap), HANDLE_INDEX(hNode));

				MxcSwapTexture* pSwap = BLOCK_PTR_H(cst.block.swap, hSwap);
				if (pSwap->externalTexture->texture.image != NULL) {
					LOG_ERROR("Trying to claim Swap Image which is already initialized!\n");
					pNodeShrd->nodeSwapStates[iNodeSwap] = XR_SWAP_STATE_ERROR;
					goto Out;
				}

				pSwap->info = pNodeShrd->nodeSwapInfos[iNodeSwap];
				bool isColor = pSwap->info.usageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
				bool isDepth = pSwap->info.usageFlags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

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
						LOG_ERROR("SwapImage is neither color nor depth!\n");
						pNodeShrd->nodeSwapStates[iNodeSwap] = XR_SWAP_STATE_ERROR;
						goto Out;
					}

					// TODO Should always export to enable a reusable pool of swaps
#ifdef _WIN32
					if (needsExport) {
						MxcNodeImports* pImports = &pNodeCtxt->exported.pExportedMemory->imports;
						WIN32_CHECK(DuplicateHandle(GetCurrentProcess(),
						                            pSwap->externalTexture[iImg].platform.handle,
						                            pNodeCtxt->exported.hProcess,
						                            &pImports->swapImageHandles[iNodeSwap][iImg],
						                            0, false, DUPLICATE_SAME_ACCESS),
						            "Duplicate localTexture buffer fail");
					}
#endif

					pNodeCpst->swaps[iNodeSwap][iImg].image = pSwap->externalTexture[iImg].texture.image;
					pNodeCpst->swaps[iNodeSwap][iImg].view = pSwap->externalTexture[iImg].texture.view;
				}

				pNodeCtxt->hSwaps[iNodeSwap] = hSwap;
				pNodeShrd->nodeSwapStates[iNodeSwap] = XR_SWAP_STATE_READY;
				break;
			}

			case XR_SWAP_STATE_DESTROYED: {
				swap_h hSwap = pNodeCtxt->hSwaps[iNodeSwap];
				if (!HANDLE_VALID(hSwap)) continue;

				MxcSwapTexture* pSwap = BLOCK_RELEASE(cst.block.swap, hSwap);
				mxcDestroySwapTexture(pSwap);

				pNodeCtxt->hSwaps[iNodeSwap] = HANDLE_DEFAULT;
				pNodeShrd->nodeSwapStates[iNodeSwap] = XR_SWAP_STATE_UNITIALIZED;
				ZERO(&pNodeShrd->nodeSwapInfos[iNodeSwap]);

				break;
			}

			default:
			case XR_SWAP_STATE_UNITIALIZED:
			case XR_SWAP_STATE_READY:
			case XR_SWAP_STATE_AVAILABLE:
			case XR_SWAP_STATE_ACQUIRED:
			case XR_SWAP_STATE_WAITED:
			case XR_SWAP_STATE_ERROR:
				break;
		}
	}

Error: __attribute__((unused));
	// TODO
Out:
#ifdef _WIN32
	SetEvent(pNodeCtxt->swapsSyncedHandle);
#else
	{ uint64_t v = 1; write(pNodeCtxt->swapsSyncedHandle, &v, sizeof(v)); }
#endif
#endif
}
const MxcIpcFuncPtr MXC_IPC_FUNCS[] = {
	[MXC_INTERPROCESS_TARGET_NODE_OPENED] = (MxcIpcFuncPtr const)ipcFuncNodeOpened,
	[MXC_INTERPROCESS_TARGET_NODE_CLOSED] = (MxcIpcFuncPtr const)ipcFuncNodeClosed,
	[MXC_INTERPROCESS_TARGET_NODE_BOUNDS] = (MxcIpcFuncPtr const)ipcFuncNodeBounds,
	[MXC_INTERPROCESS_TARGET_SYNC_SWAPS] =  (MxcIpcFuncPtr const)ipcFuncClaimSwap,
};

int mxcIpcFuncEnqueue(node_h hNode, MxcIpcFunc target) {
	MxcNodeShared* pNodeShrd = ARRAY_H(node.pShared, hNode);
	return MID_CHANNEL_SEND(&pNodeShrd->ipcFuncQueue, pNodeShrd->queuedIpcFuncs, &target);
}

void mxcIpcFuncDequeue(node_h hNode)
{
	MxcNodeShared* pNodeShrd = ARRAY_H(node.pShared, hNode);
	MxcIpcFunc target;
	if (MID_CHANNEL_RECV(&pNodeShrd->ipcFuncQueue, pNodeShrd->queuedIpcFuncs, &target) == MID_SUCCESS)
      MXC_IPC_FUNCS[target](hNode);
}

void mxcInitializeNode() {
	CreateGBufferProcessSetLayout(&node.gbufferProcessSetLayout);
	CreateGBufferProcessPipeLayout(node.gbufferProcessSetLayout, &node.gbufferProcessPipeLayout);
	vkCreateComputePipe("./shaders/compositor_gbuffer_process_down.comp.spv", node.gbufferProcessPipeLayout, &node.gbufferProcessDownPipe);
	vkCreateComputePipe("./shaders/compositor_gbuffer_process_up.comp.spv",   node.gbufferProcessPipeLayout, &node.gbufferProcessUpPipe);
	VK_SET_DEBUG(node.gbufferProcessDownPipe);
	VK_SET_DEBUG(node.gbufferProcessUpPipe);
}