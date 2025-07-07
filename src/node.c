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

struct Node node;

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
		.format      = VK_BASIC_PASS_FORMATS[VK_PASS_ATTACHMENT_INDEX_BASIC_COLOR],
		.extent      = {pInfo->windowWidth, pInfo->windowHeight, 1},
		.mipLevels   = 1,
		.arrayLayers = 1,
		.samples     = VK_SAMPLE_COUNT_1_BIT,
		.usage       = VK_BASIC_PASS_USAGES[VK_PASS_ATTACHMENT_INDEX_BASIC_COLOR],
	};
	vkCreateExternalPlatformTexture(&info, &pSwapTexture->platform);
	VkDedicatedTextureCreateInfo textureInfo = {
		.debugName        = "ExportedColorFramebuffer",
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
	VkImageCreateInfo info = {
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
		.usage       = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
	};
	vkCreateExternalPlatformTexture(&info, &pSwapTexture->platform);
	VkDedicatedTextureCreateInfo textureInfo = {
		.debugName        = "ExportedDepthFramebuffer",
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

static void CreateNodeGBuffer(NodeHandle hNode)
{
// I need to get compositor specific and node specific code into different files
#if defined(MOXAIC_COMPOSITOR)
	auto pNodeCtxt = &node.ctxt[hNode];
	auto pNodeCompData = &node.cst.data[hNode];
	auto pNodeShrd = node.pShared[hNode];

	CHECK(pNodeShrd->swapMaxWidth < 1024 || pNodeShrd->swapMaxHeight < 1024, "Swap too small!")

	for (int i = 0; i < XR_MAX_VIEW_COUNT; ++i) {
		vkCreateDedicatedTexture(
			&(VkDedicatedTextureCreateInfo){
				.debugName = "GBufferFramebuffer",
				.pImageCreateInfo = &(VkImageCreateInfo){
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = MXC_NODE_GBUFFER_FORMAT,
					.extent = {
						pNodeShrd->swapMaxWidth,
						pNodeShrd->swapMaxHeight,
						1,
					},
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = MXC_NODE_GBUFFER_USAGE,
				},
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.locality = VK_LOCALITY_CONTEXT},
			&pNodeCtxt->gbuffer[i].texture);

		vkCreateDedicatedTexture(
			&(VkDedicatedTextureCreateInfo){
				.debugName = "GBufferFramebufferMip",
				.pImageCreateInfo = &(VkImageCreateInfo){
					VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
					.imageType = VK_IMAGE_TYPE_2D,
					.format = MXC_NODE_GBUFFER_FORMAT,
					.extent = {
						pNodeShrd->swapMaxWidth  >> MXC_NODE_GBUFFER_FLATTENED_MIP_COUNT,
						pNodeShrd->swapMaxHeight >> MXC_NODE_GBUFFER_FLATTENED_MIP_COUNT,
						1,
					},
					.mipLevels = 1,
					.arrayLayers = 1,
					.samples = VK_SAMPLE_COUNT_1_BIT,
					.usage = MXC_NODE_GBUFFER_USAGE,
				},
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.locality = VK_LOCALITY_CONTEXT},
			&pNodeCtxt->gbuffer[i].mipTexture);

		// Pack data for compositor hot access
		pNodeCompData->gbuffer[i].image = pNodeCtxt->gbuffer[i].texture.image;
		pNodeCompData->gbuffer[i].view = pNodeCtxt->gbuffer[i].texture.view;
		pNodeCompData->gbuffer[i].mipImage = pNodeCtxt->gbuffer[i].mipTexture.image;
		pNodeCompData->gbuffer[i].mipView = pNodeCtxt->gbuffer[i].mipTexture.view;

		VK_IMMEDIATE_COMMAND_BUFFER_CONTEXT(VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS) {
			CMD_IMAGE_BARRIERS(cmd,	{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = pNodeCtxt->gbuffer[i].texture.image,
				.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
				VK_IMAGE_BARRIER_SRC_UNDEFINED,
				VK_IMAGE_BARRIER_DST_COMPUTE_NONE,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			},
			{
				VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.image = pNodeCtxt->gbuffer[i].mipTexture.image,
				.subresourceRange = VK_COLOR_SUBRESOURCE_RANGE,
				VK_IMAGE_BARRIER_SRC_UNDEFINED,  // only ever used in compute blit
				VK_IMAGE_BARRIER_DST_COMPUTE_NONE,
				VK_IMAGE_BARRIER_QUEUE_FAMILY_IGNORED,
			});
		}
	}
#endif
}

//static void mxcDestroySwap(MxcSwap* pSwap)
//{
//	pSwap->type = XR_SWAP_TYPE_UNKNOWN;
//	vkDestroyDedicatedTexture(&pSwap->color);
//	vkDestroyDedicatedTexture(&pSwap->depth);
//	vkDestroyDedicatedTexture(&pSwap->gbuffer);
//	vkDestroyDedicatedTexture(&pSwap->gbufferMip);
//	vkDestroyExternalPlatformTexture(&pSwap->colorExternal);
//	vkDestroyExternalPlatformTexture(&pSwap->depthExternal);
//}

/////////////////////////
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

///////////////////
//// Node Lifecycle
////
NodeHandle RequestLocalNodeHandle()
{
	// TODO this claim/release handle needs to be a pooling logic
	NodeHandle hNode = atomic_fetch_add(&node.ct, 1);
	node.pShared[hNode] = malloc(sizeof(MxcNodeShared));
	memset(node.pShared[hNode], 0, sizeof(MxcNodeShared));
	memset(&node.ctxt[hNode], 0, sizeof(MxcNodeContext));
	return hNode;
}

NodeHandle RequestExternalNodeHandle(MxcNodeShared* pNodeShared)
{
	NodeHandle hNode = atomic_fetch_add(&node.ct, 1);
	node.pShared[hNode] = pNodeShared;
	memset(&node.ctxt[hNode], 0, sizeof(MxcNodeContext));
	return hNode;
}

void SetNodeActive(NodeHandle hNode, MxcCompositorMode mode)
{
#if defined(MOXAIC_COMPOSITOR)
	auto pNodeCstData = &node.cst.data[hNode];
	auto pActiveNode = &node.active[mode];
	pActiveNode->handles[pActiveNode->ct] = hNode;
	pNodeCstData->activeCompositorMode = mode;
	atomic_fetch_add(&pActiveNode->ct, 1);
	LOG("Added node %d to %s\n", hNode, string_MxcCompositorMode(mode));
#endif
}

void ReleaseNodeActive(NodeHandle hNode)
{
#if defined(MOXAIC_COMPOSITOR)
	assert((compositorContext.baseCycleValue % MXC_CYCLE_COUNT) == MXC_CYCLE_UPDATE_WINDOW_STATE && "Trying to ReleaseNodeActive not in MXC_CYCLE_UPDATE_WINDOW_STATE");
	assert(node.ctxt[hNode].interprocessMode != MXC_NODE_INTERPROCESS_MODE_NONE);
	auto pNodeCstData = &node.cst.data[hNode];
	auto pActiveNode = &node.active[pNodeCstData->activeCompositorMode];

	ATOMIC_FENCE_BLOCK {
		int i = 0;
		for (; i < pActiveNode->ct; ++i) {
			if (pActiveNode->handles[i] == hNode)
				break;
		}
		for (; i < pActiveNode->ct - 1; ++i) {
			pActiveNode->handles[i] = pActiveNode->handles[i + 1];
		}
		pActiveNode->ct--;
	}

	int newCount = atomic_fetch_sub(&node.ct, 1) - 1;
	LOG("Releasing Node %d. Count %d.\n", hNode, newCount);
#endif
}

#define CLOSE_HANDLE(_handle)                                                     \
	if (!CloseHandle(_handle)) {                                                  \
		DWORD dwError = GetLastError();                                           \
		printf("Could not close (%s) object handle (%lu).\n", #_handle, dwError); \
	}
static int CleanupNode(NodeHandle hNode)
{
	assert((compositorContext.baseCycleValue % MXC_CYCLE_COUNT) == MXC_CYCLE_UPDATE_WINDOW_STATE && "Trying to ReleaseNodeActive not in MXC_CYCLE_UPDATE_WINDOW_STATE");
	auto pNodeCtxt = &node.ctxt[hNode];
	auto pNodeShared = node.pShared[hNode];

//	int  iSwapBlock = MXC_SWAP_TYPE_BLOCK_INDEX_BY_TYPE[pNodeShared->swapType];

	switch (pNodeCtxt->interprocessMode) {
		case MXC_NODE_INTERPROCESS_MODE_THREAD:
			vkFreeCommandBuffers(vk.context.device, pNodeCtxt->thread.pool, 1, &pNodeCtxt->thread.cmd);
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
					.dstSet = node.cst.data[hNode].nodeSet,
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
					.dstSet = node.cst.data[hNode].nodeSet,
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

//			for (int i = 0; i < MXC_NODE_SWAP_CAPACITY; ++i) {
//				if (!HANDLE_VALID(pNodeCtxt->hSwaps[i]))
//					continue;

				// We are fully clearing but at some point we want a mechanic to recycle and share node swaps
//				auto pSwap = BLOCK_RELEASE(node.cst.block.swap[iSwapBlock], pNodeCtxt->hSwaps[i]);
//				mxcDestroySwap(pSwap);
//			}
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
//			for (int i = 0; i < MXC_NODE_SWAP_CAPACITY; ++i) {
//				CLOSE_HANDLE(pImportedExternalMemory->imports.colorSwapHandles[i]);
//				CLOSE_HANDLE(pImportedExternalMemory->imports.depthSwapHandles[i]);
//			}
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

void mxcRequestNodeThread(void* (*runFunc)(struct MxcNodeContext*), NodeHandle* pNodeHandle)
{
#if defined(MOXAIC_COMPOSITOR)
	printf("Requesting Node Thread.\n");
	auto handle = RequestLocalNodeHandle();

	auto pNodeCtxt = &node.ctxt[handle];
	*pNodeCtxt = (MxcNodeContext){};
	pNodeCtxt->interprocessMode = MXC_NODE_INTERPROCESS_MODE_THREAD;
	pNodeCtxt-> swapsSyncedHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
	pNodeCtxt->exported.compositorTimeline = compositorContext.timeline;

	auto pNodeShrd = node.pShared[handle];
	*pNodeShrd = (MxcNodeShared){};
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
	vkCreateSemaphoreExt(&semaphoreCreateInfo, &pNodeCtxt->exported.nodeTimeline);

	VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = vk.context.queueFamilies[VK_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
	};
	VK_CHECK(vkCreateCommandPool(vk.context.device, &graphicsCommandPoolCreateInfo, VK_ALLOC, &pNodeCtxt->thread.pool));
	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = pNodeCtxt->thread.pool,
		.commandBufferCount = 1,
	};
	VK_CHECK(vkAllocateCommandBuffers(vk.context.device, &commandBufferAllocateInfo, &pNodeCtxt->thread.cmd));
	vkSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)pNodeCtxt->thread.cmd, "TestNode");

	MxcCompositorNodeHot* pNodeCompositorData = &node.cst.data[handle];
	// do not clear since set data is preallocated
//	*pNodeCompositorData = (MxcNodeCompositorData){};
//	pNodeCompositorData->rootPose.rotation = QuatFromEuler(pNodeCompositorData->rootPose.euler);

	*pNodeHandle = handle;

	CHECK(pthread_create(&node.ctxt[handle].thread.id, NULL, (void* (*)(void*))runFunc, pNodeCtxt), "Node thread creation failed!");

	printf("Request Node Thread Success. Handle: %d\n", handle);
	// todo this needs error handling
#endif
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

// Called when compositor accepts connection
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

	/// Receive Node Ack Message
	{
		char buffer[sizeof(nodeIPCAckMessage)] = {};
		int  receiveLength = recv(clientSocket, buffer, sizeof(nodeIPCAckMessage), 0);
		WSA_CHECK(receiveLength == SOCKET_ERROR || receiveLength == 0, "Recv nodeIPCAckMessage failed");
		printf("Received node ack: %s Size: %d\n", buffer, receiveLength);
		CHECK(strcmp(buffer, nodeIPCAckMessage), "Unexpected node message");
	}

	/// Send Server Ack message
	{
		printf("Sending server ack: %s size: %llu\n", serverIPCAckMessage, strlen(serverIPCAckMessage));
		int sendResult = send(clientSocket, serverIPCAckMessage, strlen(serverIPCAckMessage), 0);
		WSA_CHECK(sendResult == SOCKET_ERROR || sendResult == 0, "Send server ack failed");
	}

	/// Receive Node Process Handle
	{
		int receiveLength = recv(clientSocket, (char*)&nodeProcId, sizeof(DWORD), 0);
		WSA_CHECK(receiveLength == SOCKET_ERROR || receiveLength == 0, "Recv node exported id failed");
		printf("Received node exported id: %lu Size: %d\n", nodeProcId, receiveLength);
		CHECK(nodeProcId == 0, "Invalid node exported id");
	}

	/// Create Shared Memory
	{
		hNodeProc = OpenProcess(PROCESS_DUP_HANDLE, FALSE, nodeProcId);
		WIN32_CHECK(hNodeProc != NULL && hNodeProc != INVALID_HANDLE_VALUE, "Duplicate exported handle failed");
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
		pNodeCtxt = &node.ctxt[hNode];
		pNodeCtxt->interprocessMode = MXC_NODE_INTERPROCESS_MODE_EXPORTED;

		pNodeCtxt->swapsSyncedHandle = CreateEvent(NULL, FALSE, FALSE, NULL);

		pNodeCtxt->exported.id = nodeProcId;
		pNodeCtxt->exported.handle = hNodeProc;

		pNodeCtxt->exported.exportedMemoryHandle = hExtNodeMem;
		pNodeCtxt->exported.pExportedMemory = pExtNodeMem;

		pNodeCtxt->exported.nodeTimeline = nodeTimeline;
		pNodeCtxt->exported.nodeTimelineHandle = vkGetSemaphoreExternalHandle(nodeTimeline);

		pNodeCtxt->exported.compositorTimeline = compositorContext.timeline;
		pNodeCtxt->exported.compositorTimelineHandle = compositorContext.timelineHandle;


		// Duplicate Handles
		HANDLE currentHandle = GetCurrentProcess();
		WIN32_CHECK(DuplicateHandle(
						currentHandle, pNodeCtxt->swapsSyncedHandle,
						hNodeProc, &pImports->swapsSyncedHandle,
						0, false, DUPLICATE_SAME_ACCESS),
			"Duplicate nodeFenceHandle handle fail.");
		WIN32_CHECK(DuplicateHandle(
						currentHandle, pNodeCtxt->exported.nodeTimelineHandle,
						hNodeProc, &pImports->nodeTimelineHandle,
						0, false, DUPLICATE_SAME_ACCESS),
			"Duplicate nodeTimeline handle fail.");
		WIN32_CHECK(DuplicateHandle(
						currentHandle, pNodeCtxt->exported.compositorTimelineHandle,
						hNodeProc, &pImports->compositorTimelineHandle,
						0, false, DUPLICATE_SAME_ACCESS),
			"Duplicate compositor timeline handle fail.");
	}

	/// Send Shared Memory
	{
		HANDLE duplicatedExternalNodeMemoryHandle;
		WIN32_CHECK(DuplicateHandle(
						GetCurrentProcess(), pNodeCtxt->exported.exportedMemoryHandle,
						hNodeProc, &duplicatedExternalNodeMemoryHandle,
						0, false, DUPLICATE_SAME_ACCESS),
					"Duplicate sharedMemory handle fail.");
		printf("Sending duplicatedExternalNodeMemoryHandle: %p Size: %llu\n", duplicatedExternalNodeMemoryHandle, sizeof(HANDLE));
		int sendResult = send(clientSocket, (const char*)&duplicatedExternalNodeMemoryHandle, sizeof(HANDLE), 0);
		WSA_CHECK(sendResult == SOCKET_ERROR || sendResult == 0, "Send shared mem handle failed");
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
		printf("Sending exported handle: %lu size: %llu\n", currentProcessId, sizeof(DWORD));
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
		pNodeContext = &node.ctxt[hNode];
		pNodeContext->interprocessMode = MXC_NODE_INTERPROCESS_MODE_IMPORTED;

		pNodeContext->swapsSyncedHandle = pImportedExternalMemory->imports.swapsSyncedHandle;
		assert(pNodeContext->swapsSyncedHandle != NULL);

		printf("Importing node handle %d\n", hNode);
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
		VK_CHECK(vkAllocateCommandBuffers(vk.context.device, &commandBufferAllocateInfo, &pNodeContext->thread.cmd));
		vkSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)pNodeContext->thread.cmd, "TestNode");
	}

	// Start node thread
	{
		atomic_thread_fence(memory_order_release);
		CHECK(pthread_create(&pNodeContext->thread.id, NULL, (void* (*)(void*))mxcTestNodeThread, pNodeContext), "Node Process Import thread creation failed!");
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
int midRingEnqueue(MxcRingBuffer* pBuffer, MxcRingBufferHandle target)
{
	ATOMIC_FENCE_BLOCK {
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
	ATOMIC_FENCE_BLOCK {
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
}

static void ipcFuncClaimSwap(NodeHandle hNode)
{
#if defined(MOXAIC_COMPOSITOR)
	LOG("Claiming Swap for Node %d\n", hNode);

	auto pNodeShrd = node.pShared[hNode];
	auto pNodeCstData = &node.cst.data[hNode];
	auto pNodeCtxt = &node.ctxt[hNode];
	auto pImports = &pNodeCtxt->exported.pExportedMemory->imports;
	bool needsExport = pNodeCtxt->interprocessMode != MXC_NODE_INTERPROCESS_MODE_THREAD;

//	if (pNodeShrd->swapWidth != DEFAULT_WIDTH || pNodeShrd->swapHeight != DEFAULT_HEIGHT) {
//		LOG_ERROR("Requested swapCtx size not available! %d %d\n", pNodeShrd->swapWidth, pNodeShrd->swapHeight);
//		pNodeShrd->swapType = XR_SWAP_TYPE_ERROR;
//		return;
//	}

	for (int iNodeSwap = 0; iNodeSwap < XR_SWAPCHAIN_CAPACITY; ++iNodeSwap) {
		if (pNodeShrd->swapStates[iNodeSwap] != XR_SWAP_STATE_REQUESTED)
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
		pSwap->info = pNodeShrd->swapInfos[iNodeSwap];
		bool isColor = pSwap->info.usageFlags & XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
		bool isDepth = pSwap->info.usageFlags & XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		for (int iImg = 0; iImg < XR_SWAPCHAIN_IMAGE_COUNT; ++iImg) {
			// We could determine color in CreateColorSwapTexture. Really should just make a VkExternalTexture.
			if      (isColor) CreateColorSwapTexture(&pSwap->info, &pSwap->externalTexture[iImg]);
			else if (isDepth) CreateDepthSwapTexture(&pSwap->info, &pSwap->externalTexture[iImg]);
			else    assert(false && "SwapImage is neither color nor depth!");

			if (needsExport) {
				WIN32_CHECK(DuplicateHandle(GetCurrentProcess(),
						pSwap->externalTexture[iImg].platform.handle,
						pNodeCtxt->exported.handle,
						&pImports->swapImageHandles[iNodeSwap][iImg],
						0, false, DUPLICATE_SAME_ACCESS),
					"Duplicate localTexture handle fail");
			}

			pNodeCstData->swaps[iNodeSwap][iImg].image = pSwap->externalTexture[iImg].texture.image;
			pNodeCstData->swaps[iNodeSwap][iImg].view = pSwap->externalTexture[iImg].texture.view;
		}

		pNodeCtxt->hSwaps[iNodeSwap] = hSwap;
		pNodeShrd->swapStates[iNodeSwap] = XR_SWAP_STATE_CREATED;
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
	[MXC_INTERPROCESS_TARGET_SYNC_SWAPS] = (MxcIpcFuncPtr const)ipcFuncClaimSwap,
};

