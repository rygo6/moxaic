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
#include "mid_qring.h"

#include "pipe_gbuffer_process.h"

////
//// Constants
////

// The number of mip levels flattened to one image by compositor_gbuffer_blit_mip_step.comp
#define MXC_NODE_GBUFFER_MAX_MIP_COUNT 12
#define MXC_NODE_GBUFFER_FORMAT VK_FORMAT_R16G16B16A16_SFLOAT
#define MXC_NODE_GBUFFER_USAGE  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
#define MXC_NODE_CLEAR_COLOR (VkClearColorValue) { 0.0f, 0.0f, 0.0f, 0.0f }
#define MXC_EXTERNAL_FRAMEBUFFER_HANDLE_TYPE VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT

////
//// Shared Types
////
typedef u8 NodeHandle;

typedef enum MxcNodeInterprocessMode : u8 {
	MXC_NODE_INTERPROCESS_MODE_NONE,
	MXC_NODE_INTERPROCESS_MODE_THREAD,
	MXC_NODE_INTERPROCESS_MODE_EXPORTED,
	MXC_NODE_INTERPROCESS_MODE_IMPORTED,
	MXC_NODE_INTERPROCESS_MODE_COUNT,
} MxcNodeInterprocessMode;

typedef enum MxcSwapScale : u8{
	MXC_SWAP_SCALE_FULL,
	MXC_SWAP_SCALE_HALF,
	MXC_SWAP_SCALE_QUARTER,
	MXC_SWAP_SCALE_COUNT,
} MxcSwapScale;

typedef enum MxcCompositorMode : u8 {
	MXC_COMPOSITOR_MODE_NONE, // Used to process messages with no compositing.
	MXC_COMPOSITOR_MODE_QUAD,
	MXC_COMPOSITOR_MODE_TESSELATION,
	MXC_COMPOSITOR_MODE_TASK_MESH,
	MXC_COMPOSITOR_MODE_COMPUTE,
	MXC_COMPOSITOR_MODE_COUNT,
} MxcCompositorMode;

static const char* string_MxcCompositorMode(MxcCompositorMode mode) {
	switch(mode){
		case MXC_COMPOSITOR_MODE_NONE:        return "MXC_COMPOSITOR_MODE_NONE";
		case MXC_COMPOSITOR_MODE_QUAD:        return "MXC_COMPOSITOR_MODE_QUAD";
		case MXC_COMPOSITOR_MODE_TESSELATION: return "MXC_COMPOSITOR_MODE_TESSELATION";
		case MXC_COMPOSITOR_MODE_TASK_MESH:   return "MXC_COMPOSITOR_MODE_TASK_MESH";
		case MXC_COMPOSITOR_MODE_COMPUTE:     return "MXC_COMPOSITOR_MODE_COMPUTE";
		default:                              return "N/A";
	}
}

typedef enum MxcIpcFunc {
	MXC_INTERPROCESS_TARGET_NODE_OPENED,
	MXC_INTERPROCESS_TARGET_NODE_CLOSED,
	MXC_INTERPROCESS_TARGET_SYNC_SWAPS,
	MXC_INTERPROCESS_TARGET_COUNT,
} MxcIpcFunc;
#define MXC_IPC_FUNC_QUEUE_CAPACITY MID_QRING_CAPACITY

typedef struct MxcController {
	bool    active;
	pose    gripPose;
	pose    aimPose;
	bool    selectClick;
	bool    menuClick;
	float   triggerValue;
} MxcController;

typedef struct MxcClip {
	vec2 ulUV;
	vec2 lrUV;
} MxcClip;

typedef struct MxcNodeShared {

	// Read/Write every cycle
	u64          timelineValue;
	MxcClip      clip;
	ProcessState processState;
	pose         rootPose;
	pose         cameraPose;
	camera       camera;

	struct {
		u8 colorId;
		u8 depthId;
	} viewSwaps[XR_MAX_VIEW_COUNT];

	MxcController left;
	MxcController right;

	// Read every cycle. Occasional write.
	f32               compositorRadius;
	u32               compositorCycleSkip;
	uint64_t          compositorBaseCycleValue;
	MxcCompositorMode compositorMode;

	// Interprocess
	MidQRing   ipcFuncQueue;
	MxcIpcFunc queuedIpcFuncs[MID_QRING_CAPACITY];

	// Swap
	u16             swapMaxWidth;
	u16             swapMaxHeight;
	XrSwapState     swapStates[XR_SWAPCHAIN_CAPACITY];
	XrSwapchainInfo swapInfos[XR_SWAPCHAIN_CAPACITY];

} MxcNodeShared;

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

////
//// Swap
////
typedef struct MxcSwapTexture {
	VkExternalTexture externalTexture[XR_SWAPCHAIN_IMAGE_COUNT];
	XrSwapState       state;
	XrSwapchainInfo   info;
} MxcSwapTexture;

////
//// Compositor Types and Data
////
// this should merge int MxcCompositor
typedef struct MxcCompositorContext {
	// read by multiple threads
	VkCommandBuffer gfxCmd;
	u64             baseCycleValue;
	VkSemaphore     timeline;
	VkSwapContext   swapCtx;

	// cold data
	VkCommandPool gfxPool;
	pthread_t     threadId;

	HANDLE timelineHandle;

	bool isReady;

} MxcCompositorContext;

// I should do this. remove compositor code from nodes entirely
//#if defined(MOXAIC_COMPOSITOR)
extern MxcCompositorContext compositorContext;

////
//// Compositor Node Types and Data
////

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

typedef enum MxcCubeCorners: u8 {
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
constexpr u8 MXC_CUBE_SEGMENTS[MXC_CUBE_SEGMENT_COUNT] = {
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

typedef enum MxcNodeInteractionState{
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
	VkImage               image;
	VkImageView           view;
} MxcNodeSwap;

// Hot data used by the compositor for each node
// should I call it Hot or Data?
// Context = Cold. Data = Hot?
typedef struct MxcNodeCompositeData {

	MxcNodeInteractionState interactionState;
	MxcCompositorMode       activeCompositorMode;
	MxcNodeInterprocessMode activeInterprocessMode;

	//	pose rootPose;
	u64  lastTimelineValue;

	// NodeSet which node is actively using to render
	MxcCompositorNodeSetState renderingNodeSetState;
	MxcCompositorNodeSetState compositingNodeSetState;
	// This should go in one big shared buffer for all nodes
	// VkSharedDescriptor         compositingNodeSet;
	// MxcCompositorNodeSetState* pCompositingNodeSetMapped;

	MxcNodeSwap swaps[XR_SWAPCHAIN_CAPACITY][XR_SWAPCHAIN_IMAGE_COUNT];

	MxcNodeGBuffer gbuffer[XR_MAX_VIEW_COUNT];
	//	} gbuffer[XR_MAX_VIEW_COUNT][XR_SWAPCHAIN_IMAGE_COUNT];
	// If I ever need to retain gbuffers, I will need more gbuffers.
	// However, as long as gbuffer process is on the compositor thread
	// it can sync a single gbuffer with composite render loop

  	// this should go a UI thread node
	VkLineVert worldLineSegments[MXC_CUBE_SEGMENT_COUNT];
	vec3 worldCorners[CORNER_COUNT];
	vec2 uvCorners[CORNER_COUNT];

} MxcNodeCompositeData;

////
//// Node Context
////

constexpr int MXC_NODE_SWAP_CAPACITY = VK_SWAP_COUNT * 2;

typedef struct MxcNodeContext {

	MxcNodeInterprocessMode interprocessMode;

	HANDLE       swapsSyncedHandle;
	block_handle hSwaps[XR_SWAPCHAIN_CAPACITY];

	VkDedicatedTexture gbuffer[XR_MAX_VIEW_COUNT];

	union {
		// MXC_NODE_INTERPROCESS_MODE_THREAD
		struct {
			pthread_t id;

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
			DWORD  id;
			HANDLE handle;

			HANDLE                 exportedMemoryHandle;
			MxcExternalNodeMemory* pExportedMemory;

			VkSemaphore  compositorTimeline;
			VkSemaphore  nodeTimeline;

			HANDLE nodeTimelineHandle;
			HANDLE compositorTimelineHandle;
		} exported;

		// MXC_NODE_INTERPROCESS_MODE_IMPORTED
		struct {
			// Use this. even if it points ot same shared memory. Although its not a tragedy if each has their own shared memory
//			HANDLE                 importedExternalMemoryHandle;
//			MxcExternalNodeMemory* pImportedExternalMemory;

			HANDLE nodeTimelineHandle;
			HANDLE compositorTimelineHandle;
		} imported;
	};

} MxcNodeContext;

// I am presuming a node simply won't need to have as many thread nodes within it
#if defined(MOXAIC_COMPOSITOR)
#define MXC_NODE_CAPACITY 64
#elif defined(MOXAIC_NODE)
#define MXC_NODE_CAPACITY 4
#endif

typedef struct MxcActiveNodes {
	u16        count;
	NodeHandle handles[MXC_NODE_CAPACITY];
} MxcActiveNodes;

// Only one import into a node from a compositor? No move into MxcNodeContext. Duplicat is probably fine
extern HANDLE                 importedExternalMemoryHandle;
extern MxcExternalNodeMemory* pImportedExternalMemory;

extern struct Node {
	MxcActiveNodes active[MXC_COMPOSITOR_MODE_COUNT];

	u16            count;
	MxcNodeContext context[MXC_NODE_CAPACITY];
	MxcNodeShared* pShared[MXC_NODE_CAPACITY];

	VkDescriptorSetLayout gbufferProcessSetLayout;
	VkPipelineLayout      gbufferProcessPipeLayout;
	VkPipeline            gbufferProcessDownPipe;
	VkPipeline            gbufferProcessUpPipe;

#if defined(MOXAIC_COMPOSITOR)

	// this should be in a cst anon struct?!? no move this to compositor.c
	// node. equal node data then cst.node equal node data for cst?
	struct {
		MxcNodeCompositeData data[MXC_NODE_CAPACITY];

		// TODO use this as one big array desc set
		// MxcCompositorNodeSetState  nodeSetState[MXC_NODE_CAPACITY];
		MxcCompositorNodeSetState* pNodeSetMapped;
		VkSharedBuffer             nodeSetBuffer;
		VkDescriptorSet            nodeSet;

		struct {
			BLOCK_DECL(MxcSwapTexture, MXC_NODE_CAPACITY) swap;
		} block;

	} cst;

#endif

#if defined(MOXAIC_NODE)

	HANDLE                 importedExternalMemoryHandle;
	MxcExternalNodeMemory* pImportedExternalMemory;

#endif

} node;

/*
 * Node Functions
 */

void mxcInitializeNode();

NodeHandle RequestLocalNodeHandle();
NodeHandle RequestExternalNodeHandle(MxcNodeShared* const pNodeShared);

void mxcRequestNodeThread(void* (*runFunc)(void*), NodeHandle* pNodeHandle);

void mxcNodeGBufferProcessDepth(VkCommandBuffer gfxCmd, ProcessState* pProcessState, MxcNodeSwap* pDepthSwap, MxcNodeGBuffer* pGBuffer, ivec2 nodeSwapExtent);

////
//// Process Connection
////
void mxcServerInitializeInterprocess();
void mxcServerShutdownInterprocess();
void mxcConnectInterprocessNode(bool createTestNode);
void mxcShutdownInterprocessNode();

////
//// Process IPC Funcs
////
typedef void (*MxcIpcFuncPtr)(const NodeHandle);
static_assert(MXC_INTERPROCESS_TARGET_COUNT <= MID_QRING_CAPACITY, "IPC targets larger than ring buffer size.");
extern const MxcIpcFuncPtr MXC_IPC_FUNCS[];

int mxcIpcFuncEnqueue(NodeHandle hNode, MxcIpcFunc target);
int mxcIpcFuncDequeue(NodeHandle hNode);

static inline void mxcNodeInterprocessPoll()
{
	// We still want to poll MXC_COMPOSITOR_MODE_NONE so it can send events when not being composited.
	for (u32 iCstMode = MXC_COMPOSITOR_MODE_NONE; iCstMode < MXC_COMPOSITOR_MODE_COUNT; ++iCstMode) {
		atomic_thread_fence(memory_order_acquire);
		int activeNodeCt = node.active[iCstMode].count;
		if (activeNodeCt == 0)
			continue;

		auto pActiveNodes = &node.active[iCstMode];
		for (int iNode = 0; iNode < activeNodeCt; ++iNode) {
			auto hNode = pActiveNodes->handles[iNode];
			mxcIpcFuncDequeue(hNode);
		}
	}
}
