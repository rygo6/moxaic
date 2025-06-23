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

//////////////
//// Constants
////

// The number of mip levels flattened to one image by compositor_gbuffer_blit_mip_step.comp
#define MXC_NODE_GBUFFER_FLATTENED_MIP_COUNT 6
#define MXC_NODE_GBUFFER_FORMAT VK_FORMAT_R16G16B16A16_SFLOAT
#define MXC_NODE_GBUFFER_USAGE  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
#define MXC_NODE_CLEAR_COLOR (VkClearColorValue) { 0.0f, 0.0f, 0.0f, 0.0f }
#define MXC_EXTERNAL_FRAMEBUFFER_HANDLE_TYPE VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT

//////////////
//// IPC Types
////
typedef uint8_t MxcRingBufferHandle;
#define MXC_RING_BUFFER_CAPACITY 256
#define MXC_RING_BUFFER_HANDLE_CAPACITY (1 << (sizeof(MxcRingBufferHandle) * CHAR_BIT))
static_assert(MXC_RING_BUFFER_CAPACITY <= MXC_RING_BUFFER_HANDLE_CAPACITY, "RingBufferHandle type can't store capacity.");
typedef struct MxcRingBuffer {
	MxcRingBufferHandle head;
	MxcRingBufferHandle tail;
	MxcRingBufferHandle targets[MXC_RING_BUFFER_CAPACITY];
} MxcRingBuffer;

/////////////////
//// Shared Types
////
typedef enum MxcNodeType {
	MXC_NODE_TYPE_NONE,
	MXC_NODE_TYPE_THREAD,
	// don't think api matters anymore?
	MXC_NODE_TYPE_INTERPROCESS_VULKAN_EXPORTED,
	MXC_NODE_TYPE_INTERPROCESS_VULKAN_IMPORTED,
	MXC_NODE_TYPE_INTERPROCESS_OPENGL_EXPORTED,
	MXC_NODE_TYPE_INTERPROCESS_OPENGL_IMPORTED,
	MXC_NODE_TYPE_INTERPROCESS_EXPORTED,
	MXC_NODE_TYPE_INTERPROCESS_IMPORTED,
	MXC_NODE_TYPE_COUNT
} MxcNodeType;

typedef enum MxcSwapScale : u8{
	MXC_SWAP_SCALE_FULL,
	MXC_SWAP_SCALE_HALF,
	MXC_SWAP_SCALE_QUARTER,
	MXC_SWAP_SCALE_COUNT,
} MxcSwapScale;

typedef struct MxcDepthState {
	float minDepth;
	float maxDepth;
	float nearZ;
	float farZ;
} MxcDepthState;

typedef enum MxcCompositorMode : u8 {
	MXC_COMPOSITOR_MODE_NONE,
	MXC_COMPOSITOR_MODE_QUAD,
	MXC_COMPOSITOR_MODE_TESSELATION,
	MXC_COMPOSITOR_MODE_TASK_MESH,
	MXC_COMPOSITOR_MODE_COMPUTE,
	MXC_COMPOSITOR_MODE_COUNT,
} MxcCompositorMode;

typedef struct MxcController {
	bool    active;
	pose    gripPose;
	pose    aimPose;
	bool    selectClick;
	bool    menuClick;
	float   triggerValue;
} MxcController;

typedef struct MxcProcessState {
	MxcDepthState depth;
	float cameraNearZ;
	float cameraFarZ;
} MxcProcessState;

typedef struct MxcClip {
	vec2 ulUV;
	vec2 lrUV;
} MxcClip;

typedef struct MxcNodeShared {
	// read/write every cycle
	u64 timelineValue;

	VkGlobalSetState globalSetState;
	MxcClip          clip;

	MxcProcessState processState;

	// I don't think I need this either?
	pose rootPose;

	// I don't think I need this?
	// should maybe in context?
	pose              cameraPose;
	cam               camera;

	MxcController left;
	MxcController right;

	// read every cycle, occasional write
	f32               compositorRadius;
	u32               compositorCycleSkip;
	uint64_t          compositorBaseCycleValue;
	MxcCompositorMode compositorMode;

	// Interprocess
	MxcRingBuffer nodeInterprocessFuncQueue;

	// Swap
	XrSwapType  swapType;
	u16 swapWidth;
	u16 swapHeight;

} MxcNodeShared;

typedef struct MxcNodeImports {

	HANDLE swapsSyncedHandle;

	// We need to do * 2 in case we are mult-pass framebuffer which needs a framebuffer for each eye
	HANDLE colorSwapHandles[XR_SWAP_COUNT * XR_MAX_VIEW_COUNT];
	HANDLE depthSwapHandles[XR_SWAP_COUNT * XR_MAX_VIEW_COUNT];
	u8     claimedColorSwapCount;
	u8     claimedDepthSwapCount;

	HANDLE nodeTimelineHandle;
	HANDLE compositorTimelineHandle;

} MxcNodeImports;

typedef struct MxcExternalNodeMemory {
	// these needs to turn into array so one process can share chunk of shared memory
	MxcNodeShared shared;
	MxcNodeImports imports;
} MxcExternalNodeMemory;

//////////////
//// Swap Pool
////
typedef uint16_t swap_index_t;
typedef struct MxcSwapInfo {
	XrSwapType        type;
	XrSwapOutputFlags usage;

	// not sure if I will use these
	MxcSwapScale xScale;
	MxcSwapScale yScale;

	u16 width;
	u16 height;

	MxcCompositorMode compositorMode;
} MxcSwapInfo;

typedef struct MxcSwap {
	VkDedicatedTexture color;
	VkDedicatedTexture depth;
	VkDedicatedTexture gbuffer;
	VkDedicatedTexture gbufferMip;
#if _WIN32
	VkWin32ExternalTexture colorExternal;
	VkWin32ExternalTexture depthExternal;
#endif
} MxcSwap;

typedef bitset64_t swap_bitset_t;
typedef struct MxcNodeSwapPool {
	swap_bitset_t occupied;
	swap_bitset_t created;
	MxcSwap       swaps[BITNSIZE(swap_bitset_t)];
} MxcNodeSwapPool;

constexpr int MXC_SWAP_TYPE_POOL_INDEX[] = {
	[XR_SWAP_TYPE_UNKNOWN] = 0,
	[XR_SWAP_TYPE_MONO_SINGLE] = 0,
	[XR_SWAP_TYPE_STEREO_SINGLE] = 0,
	[XR_SWAP_TYPE_STEREO_TEXTURE_ARRAY] = 1,
	[XR_SWAP_TYPE_STEREO_DOUBLE_WIDE] = 2,
};
constexpr int MXC_SWAP_TYPE_POOL_COUNT = 3;

//extern MxcNodeSwapPool nodeSwapPool[MXC_SWAP_SCALE_COUNT][MXC_SWAP_SCALE_COUNT];
extern MxcNodeSwapPool nodeSwapPool[MXC_SWAP_TYPE_POOL_COUNT];

MxcSwap* mxcGetSwap(const MxcSwapInfo* pInfo, swap_index_t index);
int      mxcClaimSwap(const MxcSwapInfo* pInfo);
void     mxcReleaseSwap(const MxcSwapInfo* pInfo, const swap_index_t index);
void     mxcCreateSwap(const MxcSwapInfo* pInfo, const VkBasicFramebufferTextureCreateInfo* pTexInfo, MxcSwap* pSwap);

//////////////////////////////
//// Compositor Types and Data
////
typedef struct MxcCompositorContext {
	// read by multiple threads
	VkCommandBuffer gfxCmd;
	VkSemaphore     timeline;
	VkSwapContext   swapCtx;

	// cold data
	VkCommandPool gfxPool;
	pthread_t     threadId;

} MxcCompositorContext;

// I should do this. remove compositor code from nodes entirely
//#if defined(MOXAIC_COMPOSITOR)
extern MxcCompositorContext compositorContext;

///////////////////////////////////
//// Compositor Node Types and Data
////

// State uploaded to node descriptor set
typedef struct MxcNodeCompositorSetState {

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

} MxcNodeCompositorSetState;

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

// Hot data used by compositor for each node
typedef struct CACHE_ALIGN MxcNodeCompositorData {

	MxcNodeInteractionState interactionState;

	pose                       rootPose;
	uint64_t                   lastTimelineValue;

	// This should go in one big shared buffer for all nodes
	MxcNodeCompositorSetState  nodeSetState;
	MxcNodeCompositorSetState* pSetMapped;
	VkSharedBuffer             nodeSetBuffer;
	VkDescriptorSet            nodeSet;

	u8 swapIndex;
	struct CACHE_ALIGN {
		VkImage               color;
		VkImage               depth;
		VkImage               gBuffer;
		VkImage               gBufferMip;
		VkImageView           colorView;
		VkImageView           depthView;
		VkImageView           gBufferView;
		VkImageView           gBufferMipView;
	} swaps[VK_SWAP_COUNT];

	vec3 worldSegments[MXC_CUBE_SEGMENT_COUNT];
	vec3 worldCorners[CORNER_COUNT];
	vec2 uvCorners[CORNER_COUNT];

} MxcNodeCompositorData;

/////////////////
//// Node Context
////

// All data related to node
typedef struct MxcNodeContext {

	MxcNodeType type;

	// Compositor Data

	// Node/Compositor Duplicated
	// If thread the node/compositor both directly access this.
	// If IPC it is replicated via duplicated handles from NodeImports.
	HANDLE      swapsSyncedHandle;
	MxcSwap     swap[VK_SWAP_COUNT * 2];
	VkSemaphore nodeTimeline;
	VkSemaphore compositorTimeline;


	// Node/Compositor Shared
	MxcNodeShared* pNodeShared;
	MxcNodeImports* pNodeImports;


	// these could be a union too
	// MXC_NODE_TYPE_THREAD
	pthread_t       threadId;
	VkCommandPool   pool;
	VkCommandBuffer cmd;


	// MXC_NODE_TYPE_INTERPROCESS
	DWORD                  processId;
	HANDLE                 processHandle;
	HANDLE                 exportedExternalMemoryHandle;
	MxcExternalNodeMemory* pExportedExternalMemory;

} MxcNodeContext;

// I probably want to get rid of this and just use typical basic pass and put more barriers
typedef struct MxcVulkanNodeContext {
	VkPipeline  basicPipe;
	VkRenderPass basicPass;
} MxcVulkanNodeContext;

// I am presuming a node simply won't need to have as many thread nodes within it
#if defined(MOXAIC_COMPOSITOR)
#define MXC_NODE_CAPACITY 64
#elif defined(MOXAIC_NODE)
#define MXC_NODE_CAPACITY 4
#endif

typedef u8 NodeHandle;
// move these into struct
extern u16 nodeCount;

// Cold storage for all node data
extern MxcNodeContext nodeContext[MXC_NODE_CAPACITY];

// Could be missing if node is an external process... maybe I should malloc these?
extern MxcNodeShared localNodeShared[MXC_NODE_CAPACITY];
// Holds pointer to either local or external process shared memory
extern MxcNodeShared* pDuplicatedNodeShared[MXC_NODE_CAPACITY];

// Holds pointer to nodes in each compositor mode
typedef struct MxcActiveNodes {
	u16        ct;
	NodeHandle handles[MXC_NODE_CAPACITY];
} MxcActiveNodes;
extern MxcActiveNodes activeNodes[MXC_COMPOSITOR_MODE_COUNT];

// Only one import into a node from a compositor?
extern HANDLE                 importedExternalMemoryHandle;
extern MxcExternalNodeMemory* pImportedExternalMemory;

// Move into a comp node thread local...
extern MxcNodeCompositorData nodeCompositorData[MXC_NODE_CAPACITY];

extern MxcVulkanNodeContext vkNode;

///////////////
//// Node Queue
////
// This could be a MidVK construct
typedef struct MxcQueuedNodeCommandBuffer {
	VkCommandBuffer cmd;
	VkSemaphore     nodeTimeline;
	uint64_t        nodeTimelineSignalValue;
} MxcQueuedNodeCommandBuffer;
extern size_t                     submitNodeQueueStart;
extern size_t                     submitNodeQueueEnd;
extern MxcQueuedNodeCommandBuffer submitNodeQueue[MXC_NODE_CAPACITY];

static inline void mxcQueueNodeCommandBuffer(MxcQueuedNodeCommandBuffer handle)
{
	__atomic_thread_fence(__ATOMIC_ACQUIRE);
	submitNodeQueue[submitNodeQueueEnd] = handle;
	submitNodeQueueEnd = (submitNodeQueueEnd + 1) % MXC_NODE_CAPACITY;
	assert(submitNodeQueueEnd != submitNodeQueueStart);
	__atomic_thread_fence(__ATOMIC_RELEASE);
}
static inline void mxcSubmitQueuedNodeCommandBuffers(const VkQueue graphicsQueue)
{
	__atomic_thread_fence(__ATOMIC_ACQUIRE);
	bool pendingBuffer = submitNodeQueueStart != submitNodeQueueEnd;
	while (pendingBuffer) {
		CmdSubmit(submitNodeQueue[submitNodeQueueStart].cmd, graphicsQueue, submitNodeQueue[submitNodeQueueStart].nodeTimeline, submitNodeQueue[submitNodeQueueStart].nodeTimelineSignalValue);

		submitNodeQueueStart = (submitNodeQueueStart + 1) % MXC_NODE_CAPACITY;
		__atomic_thread_fence(__ATOMIC_RELEASE);

		__atomic_thread_fence(__ATOMIC_ACQUIRE);
		pendingBuffer = submitNodeQueueStart < submitNodeQueueEnd;
	}
}

void mxcRequestAndRunCompositorNodeThread(VkSurfaceKHR surface, void* (*runFunc)(struct MxcCompositorContext*));
void mxcRequestNodeThread(void* (*runFunc)(struct MxcNodeContext*), NodeHandle* pNodeHandle);

void mxcCreateNodeRenderPass();

NodeHandle RequestLocalNodeHandle();
NodeHandle RequestExternalNodeHandle(MxcNodeShared* const pNodeShared);
void ReleaseNode(NodeHandle handle);


///////////////////////
//// Process Connection
////
void mxcInitializeInterprocessServer();
void mxcShutdownInterprocessServer();
void mxcConnectInterprocessNode(bool createTestNode);
void mxcShutdownInterprocessNode();

//////////////////////
//// Process IPC Funcs
////
typedef void (*MxcIpcFuncPtr)(const NodeHandle);
typedef enum MxcIpcFunc {
	MXC_INTERPROCESS_TARGET_NODE_CLOSED,
	MXC_INTERPROCESS_TARGET_SYNC_SWAPS,
	MXC_INTERPROCESS_TARGET_COUNT,
} MxcIpcFunc;
static_assert(MXC_INTERPROCESS_TARGET_COUNT <= MXC_RING_BUFFER_HANDLE_CAPACITY, "IPC targets larger than ring buf size.");
extern const MxcIpcFuncPtr MXC_IPC_FUNCS[];

// I could get rid of this and just do a comparison polling on every node state like OXR does
// or I could move this queue mechanic into OXR and make that better?
int mxcIpcFuncEnqueue(MxcRingBuffer* pBuffer, const MxcIpcFunc target);
int mxcIpcDequeue(MxcRingBuffer* pBuffer, const NodeHandle nodeHandle);

static inline void mxcNodeInterprocessPoll()
{
	for (int i = 0; i < nodeCount; ++i)
		mxcIpcDequeue(&pDuplicatedNodeShared[i]->nodeInterprocessFuncQueue, i);
}
