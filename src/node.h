#pragma once

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "mid_vulkan.h"
#include "mid_bit.h"
#include "mid_openxr_runtime.h"
#include "mid_channel.h"

#include "pipe_gbuffer_process.h"

/*
 * Constants
 */

// The number of mip levels flattened to one image by compositor_gbuffer_blit_mip_step.comp
#define MXC_NODE_GBUFFER_MAX_MIP_COUNT 12
#define MXC_NODE_GBUFFER_FORMAT VK_FORMAT_R16G16B16A16_SFLOAT
#define MXC_NODE_GBUFFER_USAGE  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
#define MXC_NODE_CLEAR_COLOR (VkClearColorValue) { 0.0f, 0.0f, 0.0f, 0.0f }
#define MXC_EXTERNAL_FRAMEBUFFER_HANDLE_TYPE VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT

/*
 * Shared Types
 */
typedef block_handle node_h;

typedef enum PACKED MxcNodeInterprocessMode {
	MXC_NODE_INTERPROCESS_MODE_NONE,
	MXC_NODE_INTERPROCESS_MODE_THREAD,
	MXC_NODE_INTERPROCESS_MODE_EXPORTED,
	MXC_NODE_INTERPROCESS_MODE_IMPORTED,
	MXC_NODE_INTERPROCESS_MODE_COUNT,
} MxcNodeInterprocessMode;

typedef enum PACKED MxcSwapScale {
	MXC_SWAP_SCALE_FULL,
	MXC_SWAP_SCALE_HALF,
	MXC_SWAP_SCALE_QUARTER,
	MXC_SWAP_SCALE_COUNT,
} MxcSwapScale;

typedef enum PACKED MxcCompositorMode {
	MXC_COMPOSITOR_MODE_NONE, // Used to process messages with no compositing.
	MXC_COMPOSITOR_MODE_QUAD,
	MXC_COMPOSITOR_MODE_TESSELATION,
	MXC_COMPOSITOR_MODE_TASK_MESH,
	MXC_COMPOSITOR_MODE_COMPUTE,
	MXC_COMPOSITOR_MODE_COUNT,
} MxcCompositorMode;

static const char* string_MxcCompositorMode(MxcCompositorMode mode) {
	switch (mode) {
		STRING_CASE(MXC_COMPOSITOR_MODE_NONE);
		STRING_CASE(MXC_COMPOSITOR_MODE_QUAD);
		STRING_CASE(MXC_COMPOSITOR_MODE_TESSELATION);
		STRING_CASE(MXC_COMPOSITOR_MODE_TASK_MESH);
		STRING_CASE(MXC_COMPOSITOR_MODE_COMPUTE);
		default: return "N/A";
	}
}

typedef enum PACKED MxcIpcFunc {
	MXC_INTERPROCESS_TARGET_NODE_OPENED,
	MXC_INTERPROCESS_TARGET_NODE_CLOSED,
	MXC_INTERPROCESS_TARGET_NODE_BOUNDS,
	MXC_INTERPROCESS_TARGET_SYNC_SWAPS,
	MXC_INTERPROCESS_TARGET_COUNT,
} MxcIpcFunc;

typedef struct MxcController {
	bool    active;
	MidPose gripPose;
	MidPose aimPose;
	bool    selectClick;
	bool    menuClick;
	float   triggerValue;
} MxcController;

typedef struct MxcClip {
	vec2 ulUV;
	vec2 lrUV;
} MxcClip;

typedef volatile struct MxcNodeShared {
	// TODO these all should be volatile and use barriers
	// Read/Write every cycle
	u64     timelineValue;
	MxcClip clip;
	ProcessState processState;
	MidPose   rootPose;
	MidPose   cameraPose;
	camera camera;

	MxcController left;
	MxcController right;

	// Swap
	struct {
		swap_i iColorSwap;
		u32    iColorImg;
		swap_i iDepthSwap;
		u32    iDepthImg;
	} viewSwaps[XR_MAX_VIEW_COUNT];

	XrSwapState     nodeSwapStates[XR_SWAPCHAIN_CAPACITY];
	XrSwapInfo      nodeSwapInfos[XR_SWAPCHAIN_CAPACITY];
	u16             swapMaxWidth;
	u16             swapMaxHeight;

	// Read every cycle. Occasional write.
	f32               compositorRadius;
	u32               compositorCycleSkip;
	uint64_t          compositorBaseCycleValue;
	MxcCompositorMode compositorMode;

	// Interprocess
	MidChannelRing ipcFuncQueue;
	MxcIpcFunc queuedIpcFuncs[MID_QRING_CAPACITY];

	/* Events */
	MidChannelRing eventDataQueue;
	XrEventDataUnion queuedEventDataBuffers[MID_QRING_CAPACITY];

} MxcNodeShared; //MxcSharedNodeData to reflect MxcCompositorNodeData?

typedef struct MxcNodeImports {

	// We could do sync handle per swap but it's also not an issue if nodes wait a little.
	HANDLE swapsSyncedHandle;
	HANDLE swapImageHandles[XR_SWAPCHAIN_CAPACITY][XR_SWAPCHAIN_IMAGE_COUNT];

	HANDLE nodeTimelineHandle;
	HANDLE compositorTimelineHandle;

} MxcNodeImports;

typedef struct MxcExternalNodeMemory {
	// these needs to turn into array so one process can share chunk of shared memory
	MxcNodeShared shared;
	MxcNodeImports imports;
} MxcExternalNodeMemory;

/*
 * Compositor Node Types and Data
 */

typedef struct MxcCompositorNodeSetState {

	mat4 model;

	// Laid out flat to align with std140
	// GlobalSetState
	mat4  view;
	mat4  proj;
	mat4  viewProj;
	mat4  invView;
	mat4  invProj;
	mat4  invViewProj;
	ivec2 framebufferSize;

	// MxcClip
	vec2 ulUV;
	vec2 lrUV;

} MxcCompositorNodeSetState;

typedef enum PACKED MxcCubeCorners {
	CORNER_LUB,
	CORNER_LUF,
	CORNER_LDB,
	CORNER_LDF,
	CORNER_RUB,
	CORNER_RUF,
	CORNER_RDB,
	CORNER_RDF,
	CORNER_COUNT,
} MxcCubeCorners;

#define MXC_CUBE_SEGMENT_COUNT 24
static const u8 MXC_CUBE_SEGMENTS[MXC_CUBE_SEGMENT_COUNT] = {
	CORNER_LUF, CORNER_LUB,
	CORNER_LUB, CORNER_RUB,
	CORNER_RUB, CORNER_RUF,
	CORNER_RUF, CORNER_LUF,

	CORNER_RDF, CORNER_RDB,
	CORNER_RDB, CORNER_LDB,
	CORNER_LDB, CORNER_LDF,
	CORNER_LDF, CORNER_RDF,

	CORNER_LUF, CORNER_LDF,
	CORNER_LUB, CORNER_LDB,
	CORNER_RUF, CORNER_RDF,
	CORNER_RUB, CORNER_RDB,
};

typedef enum PACKED MxcNodeInteractionState {
	NODE_INTERACTION_STATE_NONE,
	NODE_INTERACTION_STATE_HOVER,
	NODE_INTERACTION_STATE_SELECT,
	NODE_INTERACTION_STATE_COUNT,
} MxcNodeInteractionState;

typedef struct MxcNodeGBuffer {
	VkImage     image;
	int         mipViewCount;
	VkImageView mipViews[MXC_NODE_GBUFFER_MAX_MIP_COUNT];
} MxcNodeGBuffer;

typedef struct MxcNodeSwap {
	VkImage     image;
	VkImageView view;
} MxcNodeSwap;

/*
 * Node Context
 */
#define MXC_NODE_SWAP_CAPACITY (VK_SWAP_COUNT * 2)

typedef struct MxcNodeContext {
	MxcNodeInterprocessMode interprocessMode;

	HANDLE swapsSyncedHandle;
	swap_h hSwaps[MXC_NODE_SWAP_CAPACITY];

	VkDedicatedTexture gbuffer[XR_MAX_VIEW_COUNT];

	union {
		// MXC_NODE_INTERPROCESS_MODE_THREAD
		struct {
			pthread_t threadId;

			VkCommandPool   pool;
			VkCommandBuffer gfxCmd;

			// TODO use these pNode should be entirely for custom node rendering
//			VkGlobalSetState* pGlobalSetMapped;
//			VkSharedBuffer  globalBuffer;
//			VkDescriptorSet globalSet;

			VkSemaphore nodeTimeline;
		} thread;

		// MXC_NODE_INTERPROCESS_MODE_EXPORTED
		struct {
			DWORD  processId;
			HANDLE hProcess;

			HANDLE                 exportedMemoryHandle;
			MxcExternalNodeMemory* pExportedMemory;

			VkSemaphore  compositorTimeline;
			VkSemaphore  nodeTimeline;

			HANDLE nodeTimelineHandle;
			HANDLE compositorTimelineHandle;
		} exported;

		// MXC_NODE_INTERPROCESS_MODE_IMPORTED
		struct {
			// Use this. even if it points to same shared memory. Although its not a tragedy if each has their own shared memory
//			HANDLE                 importedExternalMemoryHandle;
//			MxcExternalNodeMemory* pImportedExternalMemory;

			HANDLE nodeTimelineHandle;
			HANDLE compositorTimelineHandle;
		} imported;
	};

} MxcNodeContext;

#if defined(MOXAIC_COMPOSITOR)
#define MXC_NODE_CAPACITY 64
#elif defined(MOXAIC_NODE)
#define MXC_NODE_CAPACITY 8
#endif

typedef struct MxcActiveNodes {
	_Atomic u16 count;
	node_h  handles[MXC_NODE_CAPACITY];
} MxcActiveNodes;

// Only one import into a node from a compositor? No move into MxcNodeContext. Duplicate is probably fine
extern HANDLE                 importedExternalMemoryHandle;
extern MxcExternalNodeMemory* pImportedExternalMemory;

extern struct Node {
	MidChannelRing newConnectionQueue;
	node_h 	 queuedNewConnections[MID_QRING_CAPACITY];

	MxcActiveNodes active[MXC_COMPOSITOR_MODE_COUNT];

	BLOCK_T_N(MxcNodeContext, MXC_NODE_CAPACITY) context;
	MxcNodeShared* pShared[MXC_NODE_CAPACITY];

	VkDescriptorSetLayout gbufferProcessSetLayout;
	VkPipelineLayout      gbufferProcessPipeLayout;
	VkPipeline            gbufferProcessDownPipe;
	VkPipeline            gbufferProcessUpPipe;

#if defined(MOXAIC_NODE)

	HANDLE                 importedExternalMemoryHandle;
	MxcExternalNodeMemory* pImportedExternalMemory;

#endif

} node;

/*
 * Node Functions
 */

void mxcInitializeNode();

node_h RequestLocalNodeHandle();
MidResult RequestExternalNodeHandle(MxcNodeShared* pNodeShared, node_h* pNode_h);
void ReleaseNodeHandle(node_h hNode);

void mxcRequestNodeThread(void* (*runFunc)(void*), node_h* pNodeHandle);
void mxcNodeGBufferProcessDepth(VkCommandBuffer gfxCmd, ProcessState* pProcessState, MxcNodeSwap* pDepthSwap, MxcNodeGBuffer* pGBuffer, ivec2 nodeSwapExtent);
void mxcRegisterActiveNode(node_h hNode);

/*
 * Process Connection
 */
void mxcServerInitializeInterprocess();
void mxcServerShutdownInterprocess();
void mxcConnectInterprocessNode(bool createTestNode);
void mxcShutdownInterprocessNode();

/*
 * Process IPC Funcs
 */
typedef void (*MxcIpcFuncPtr)(const node_h);
static_assert(MXC_INTERPROCESS_TARGET_COUNT <= MID_QRING_CAPACITY, "IPC targets larger than ring buffer size.");
extern const MxcIpcFuncPtr MXC_IPC_FUNCS[];

int mxcIpcFuncEnqueue(node_h hNode, MxcIpcFunc target);
void mxcIpcFuncDequeue(node_h hNode);

static inline void mxcNodeInterprocessPoll()
{
	node_h hNewNode;
	while (MID_CHANNEL_RECV(&node.newConnectionQueue, node.queuedNewConnections, &hNewNode) == MID_SUCCESS)
		mxcRegisterActiveNode(hNewNode);

	// We still want to poll MXC_COMPOSITOR_MODE_NONE so it can send events when not being composited.
	for (u32 iCpstMode = MXC_COMPOSITOR_MODE_NONE; iCpstMode < MXC_COMPOSITOR_MODE_COUNT; ++iCpstMode) {
		u16 activeNodeCt = atomic_load_explicit(&node.active[iCpstMode].count, memory_order_acquire);
		if (activeNodeCt == 0)
			continue;

		MxcActiveNodes* pActiveNodes = &node.active[iCpstMode];
		for (int iNode = 0; iNode < activeNodeCt; ++iNode) {
			node_h hNode = pActiveNodes->handles[iNode];
			mxcIpcFuncDequeue(hNode);
		}
	}
}
