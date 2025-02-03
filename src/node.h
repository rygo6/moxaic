#pragma once

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "mid_vulkan.h"
#include "mid_bit.h"

//////////////
//// Constants
////

#define MXC_NODE_GBUFFER_FORMAT VK_FORMAT_R32_SFLOAT
#define MXC_NODE_GBUFFER_USAGE  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
#define MXC_NODE_GBUFFER_LEVELS 10
#define MXC_NODE_CLEAR_COLOR (VkClearColorValue) { 0.0f, 0.0f, 0.0f, 0.0f }

#if defined(D3D11)
#define MXC_EXTERNAL_FRAMEBUFFER_HANDLE_TYPE VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT
#elif defined(D3D12)
#define MXC_EXTERNAL_FRAMEBUFFER_HANDLE_TYPE VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT
#endif

//////////////
//// IPC Types
////
typedef uint8_t MxcRingBufferHandle;
#define MXC_RING_BUFFER_CAPACITY 256
#define MXC_RING_BUFFER_HANDLE_CAPACITY (1 << (sizeof(MxcRingBufferHandle) * CHAR_BIT))
_Static_assert(MXC_RING_BUFFER_CAPACITY <= MXC_RING_BUFFER_HANDLE_CAPACITY, "RingBufferHandle type can't store capacity.");
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

typedef enum MxcSwapType {
	MXC_SWAP_TYPE_UNKNOWN,
	MXC_SWAP_TYPE_SINGLE,
	MXC_SWAP_TYPE_STEREO_SINGLE,
	MXC_SWAP_TYPE_STEREO_TEXTURE_ARRAY,
	MXC_SWAP_TYPE_STEREO_DOUBLE_WIDE,
	MXC_SWAP_TYPE_COUNT,
} MxcSwapType;

constexpr int MXC_SWAP_TYPE_COUNTS[] = {
	[MXC_SWAP_TYPE_UNKNOWN] = 0,
	[MXC_SWAP_TYPE_SINGLE] = 1,
	[MXC_SWAP_TYPE_STEREO_SINGLE] = 2,
	[MXC_SWAP_TYPE_STEREO_TEXTURE_ARRAY] = 1,
	[MXC_SWAP_TYPE_STEREO_DOUBLE_WIDE] = 1,
};

typedef enum MxcSwapUsage {
	MXC_SWAP_USAGE_UNKNOWN,
	MXC_SWAP_USAGE_COLOR,
	MXC_SWAP_USAGE_COLOR_AND_DEPTH,
	MXC_SWAP_USAGE_COUNT,
} MxcSwapUsage;

typedef enum MxcSwapScale {
	MXC_SWAP_SCALE_FULL,
	MXC_SWAP_SCALE_HALF,
	MXC_SWAP_SCALE_QUARTER,
	MXC_SWAP_SCALE_COUNT,
} MxcSwapScale;

typedef struct MxcNodeShared {
	// read/write every cycle
	VkGlobalSetState  globalSetState;
	vec2              ulScreenUV;
	vec2              lrScreenUV;
	uint64_t          timelineValue;

	// I don't think I need this either?
	MidPose           rootPose;

	// I don't think I need this?
	// should maybe in context?
	MidPose           cameraPose;
	MidCamera         camera;

	// read every cycle, occasional write
	f32      compositorRadius;
	u32      compositorCycleSkip;
	uint64_t compositorBaseCycleValue;

	// Interprocess
	MxcRingBuffer nodeInterprocessFuncQueue;

	// Swap
	MxcSwapType  swapType;
	MxcSwapUsage swapUsage;

} MxcNodeShared;

typedef struct MxcNodeImports {

	HANDLE swapsSyncedHandle;

	// We need to do * 2 in case we are mult-pass framebuffer which needs a framebuffer for each eye
	HANDLE colorSwapHandles[VK_SWAP_COUNT * 2];
	HANDLE gbufferSwapHandles[VK_SWAP_COUNT * 2];
	HANDLE depthSwapHandles[VK_SWAP_COUNT * 2];

	HANDLE nodeTimelineHandle;
	HANDLE compositorTimelineHandle;

} MxcNodeImports;

typedef struct MxcExternalNodeMemory {
	// these needs to turn into array so one process can share chunk of shared memory
	// is that good for security though?
	MxcNodeShared shared;
	MxcNodeImports imports;
} MxcExternalNodeMemory;

//////////////
//// Swap Pool
////
typedef uint16_t swap_index_t;
typedef struct MxcSwapInfo {
	MxcSwapType  type   : 4;
	MxcSwapUsage usage  : 4;
	// not sure if I will use these
	MxcSwapScale xScale : 4;
	MxcSwapScale yScale : 4;
} MxcSwapInfo;

typedef struct MxcSwap {
	VkDedicatedTexture color;
	VkDedicatedTexture depth;
	VkDedicatedTexture gbuffer;
#if _WIN32
	VkWin32ExternalTexture colorExternal;
	VkWin32ExternalTexture depthExternal;
	VkWin32ExternalTexture gbufferExternal;
#endif
} MxcSwap;

typedef bitset64_t swap_bitset_t;
typedef struct MxcNodeSwapPool {
	swap_bitset_t occupied;
	swap_bitset_t created;
	MxcSwap       swaps[BITNSIZE(swap_bitset_t)];
} MxcNodeSwapPool;

//extern MxcNodeSwapPool nodeSwapPool[MXC_SWAP_SCALE_COUNT][MXC_SWAP_SCALE_COUNT];
extern MxcNodeSwapPool nodeSwapPool[MXC_SWAP_USAGE_COUNT];

MxcSwap* mxcGetSwap(const MxcSwapInfo* pInfo, swap_index_t index);
int      mxcClaimSwap(const MxcSwapInfo* pSwapInfo);
void     mxcReleaseSwap(const MxcSwapInfo* pInfo, const swap_index_t index);
void     mxcCreateSwap(const MxcSwapInfo* pSwapInfo, const VkBasicFramebufferTextureCreateInfo* pTexInfo, MxcSwap* pSwap);

//////////////////////////////
//// Compositor Types and Data
////
typedef struct MxcCompositorContext {
	// read multiple threads, write 1 thread
	uint32_t swapIndex;

	// read by multiple threads
	VkCommandBuffer cmd;
	VkSemaphore     compositorTimeline;
	VkSwapContext   swap;

	// cold data
	VkCommandPool pool;
	pthread_t     threadId;

} MxcCompositorContext;

// I should do this. remove compositor code from nodes entirely
//#if defined(MOXAIC_COMPOSITOR)
extern MxcCompositorContext compositorContext;

///////////////////////////////////
//// Compositor Node Types and Data
////
typedef struct MxcNodeCompositorSetState {
	mat4 model;

	// Laid out same as GlobalSetState for memcpy
	mat4  view;
	mat4  proj;
	mat4  viewProj;
	mat4  invView;
	mat4  invProj;
	mat4  invViewProj;
	ivec2 framebufferSize;

	vec2 ulUV;
	vec2 lrUV;
} MxcNodeCompositorSetState;

// Data compositor needs for each node. Could this go in CompositorContext?
typedef struct CACHE_ALIGN MxcNodeCompositorLocal {

	MidPose                    rootPose;
	uint64_t                   lastTimelineValue;

	// Does it actually make difference to keep local copy?
	// Should keep it anyways in case we do need to start flushing buffers
	MxcNodeCompositorSetState  setState;
	MxcNodeCompositorSetState* pSetMapped;
	VkDescriptorSet            set;

	// should make them share buffer? probably
//	VkSharedBuffer       SetBuffer;
	VkBuffer       SetBuffer;
	VkSharedMemory SetSharedMemory;

	struct CACHE_ALIGN {
		VkImageMemoryBarrier2 acquireBarriers[2];
		VkImageMemoryBarrier2 releaseBarriers[2];
		VkImage               color;
		VkImage               gBuffer;
		VkImageView           colorView;
		VkImageView           gBufferMipViews[MXC_NODE_GBUFFER_LEVELS];
	} swaps[VK_SWAP_COUNT];

} MxcNodeCompositorLocal;

///////////////
//// Node Context
////
typedef struct MxcNodeContext {
	MxcNodeType type;

	// these could be a union if truly only one or the other
	// Node Data
	VkCommandPool   pool;
	VkCommandBuffer cmd;

	// Compositor Data
//	VkDedicatedTexture colorFramebuffers[VK_SWAP_COUNT * 2];
//	VkDedicatedTexture normalFramebuffers[VK_SWAP_COUNT * 2];
//	VkDedicatedTexture depthFramebuffers[VK_SWAP_COUNT * 2];
//	VkDedicatedTexture gbufferFramebuffers[VK_SWAP_COUNT * 2];


	// Node/Compositor Duplicated
	// If thread the node/compositor both directly access this.
	// If IPC it is replicate via duplicated handles from NodeImports.
	// Maybe these should be their own struct? MxcNodeDuplicated
	HANDLE      swapsSyncedHandle;
	// Should be a handle? Maybe. Although when replicated to a node it won't have a handle.
	MxcSwap     swap[VK_SWAP_COUNT * 2];

	VkSemaphore nodeTimeline;
	VkSemaphore compositorTimeline;

	// Node/Compositor Shared
	MxcNodeShared* pNodeShared;
	MxcNodeImports* pNodeImports;


	// these could be a union too
	// MXC_NODE_TYPE_THREAD
	pthread_t       threadId;

	// really this can be shared by multiple node thread contexts on a node
	// it should go into generic ipc struct
	// MXC_NODE_TYPE_INTERPROCESS
	DWORD                  processId;
	HANDLE                 processHandle;
	HANDLE                 exportedExternalMemoryHandle;
	MxcExternalNodeMemory* pExportedExternalMemory;

} MxcNodeContext;

// I am presuming a node simply won't need to have as many thread nodes within it
#if defined(MOXAIC_COMPOSITOR)
#define MXC_NODE_CAPACITY 64
#elif defined(MOXAIC_NODE)
#define MXC_NODE_CAPACITY 4
#endif

typedef uint8_t NodeHandle;
// move these into struct
extern size_t nodeCount;
// Cold storage for all node data
extern MxcNodeContext        nodeContexts[MXC_NODE_CAPACITY];
// Could be missing if node is external process
extern MxcNodeShared localNodesShared[MXC_NODE_CAPACITY];
// Holds pointer to either local or external process shared memory
extern MxcNodeShared* activeNodesShared[MXC_NODE_CAPACITY];

extern HANDLE                 importedExternalMemoryHandle;
extern MxcExternalNodeMemory* pImportedExternalMemory;

// technically this should go into a comp node thread local....
extern MxcNodeCompositorLocal nodeCompositorData[MXC_NODE_CAPACITY];

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
		vkmSubmitCommandBuffer(submitNodeQueue[submitNodeQueueStart].cmd, graphicsQueue, submitNodeQueue[submitNodeQueueStart].nodeTimeline, submitNodeQueue[submitNodeQueueStart].nodeTimelineSignalValue);

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
_Static_assert(MXC_INTERPROCESS_TARGET_COUNT <= MXC_RING_BUFFER_HANDLE_CAPACITY, "IPC targets larger than ring buffer size.");
extern const MxcIpcFuncPtr MXC_IPC_FUNCS[];

// I could get rid of this and just do a comparison polling on every node state like OXR does
// or I could move this queue mechanic into OXR and make that better?
int mxcIpcFuncEnqueue(MxcRingBuffer* pBuffer, const MxcIpcFunc target);
int mxcIpcDequeue(MxcRingBuffer* pBuffer, const NodeHandle nodeHandle);

static inline void mxcNodeInterprocessPoll()
{
	for (int i = 0; i < nodeCount; ++i)
		mxcIpcDequeue(&activeNodesShared[i]->nodeInterprocessFuncQueue, i);
}