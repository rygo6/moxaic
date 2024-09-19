#pragma once

#include "mid_vulkan.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#include <windows.h>
#include <assert.h>

//
/// Constants
typedef enum MxcNodeType {
	MXC_NODE_TYPE_THREAD,
	MXC_NODE_TYPE_INTERPROCESS_EXPORTED,
	MXC_NODE_TYPE_INTERPROCESS_IMPORTED,
	MXC_NODE_TYPE_COUNT
} MxcNodeType;

#define MXC_NODE_GBUFFER_FORMAT VK_FORMAT_R32_SFLOAT
#define MXC_NODE_GBUFFER_USAGE  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
#define MXC_NODE_GBUFFER_LEVELS 10
#define MXC_NODE_CLEAR_COLOR \
	(VkClearColorValue) { 0.0f, 0.0f, 0.0f, 0.0f }

//
/// IPC Types
typedef uint8_t MxcRingBufferHandle;
#define MXC_RING_BUFFER_CAPACITY 256
//#define MXC_RING_BUFFER_SIZE         MXC_RING_BUFFER_CAPACITY * sizeof(MxcRingBufferHandle)
#define MXC_RING_BUFFER_HANDLE_CAPACITY (1 << (sizeof(MxcRingBufferHandle) * CHAR_BIT))
_Static_assert(MXC_RING_BUFFER_CAPACITY <= MXC_RING_BUFFER_HANDLE_CAPACITY, "RingBufferHandle type can't store capacity.");
typedef struct MxcRingBuffer {
	MxcRingBufferHandle head;
	MxcRingBufferHandle tail;
	MxcRingBufferHandle targets[MXC_RING_BUFFER_CAPACITY];
} MxcRingBuffer;

typedef struct MxcNodeShared {
	// read/write every cycle
	VkmGlobalSetState nodeGlobalSetState;
	vec2              compositorULScreenUV;
	vec2              compositorLRScreenUV;
	uint64_t          nodeCurrentTimelineSignal;
	MidPose           rootPose;

	// read every cycle, occasional write
	float compositorRadius;
	int   compositorCycleSkip;

	MxcRingBuffer targetQueue;

} MxcNodeShared;
typedef struct MxcImportParam {
	struct {
		HANDLE color;
		HANDLE normal;
		HANDLE gbuffer;
	} framebufferHandles[MIDVK_SWAP_COUNT];
	HANDLE nodeTimelineHandle;
	HANDLE compTimelineHandle;
} MxcImportParam;
typedef struct CACHE_ALIGN MxcExternalNodeMemory {
	MxcNodeShared shared;
	CACHE_ALIGN
	MxcImportParam importParam;
} MxcExternalNodeMemory;

//
/// Compositor Types
typedef struct MxcCompositorNodeContext {
	// read/write multiple threads
	uint32_t swapIndex;

	// read by multiple threads
	VkCommandBuffer cmd;
	VkSemaphore     compTimeline;
	uint64_t        compBaseCycleValue;
	MidVkSwap       swap;

	// cold data
	VkCommandPool pool;
	pthread_t     threadId;

} MxcCompositorNodeContext;
typedef struct MxcNodeCompositorSetState {
	mat4 model;

	// Laid out same as VkmGlobalSetState for memcpy
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
typedef struct CACHE_ALIGN MxcNodeCompositorData {
	MxcNodeCompositorSetState nodeSetState;
	MidPose                   rootPose;
	uint64_t                  lastTimelineSwap;
	VkDescriptorSet            set;
	MxcNodeCompositorSetState* pSetMapped;
	// should make them share buffer? probably
	VkBuffer                   SetBuffer;
	MidVkSharedMemory          SetSharedMemory;

	CACHE_ALIGN
	struct {
		// this will get pulled out into a shared pool of some sort
		VkImage               color;
		VkImage               normal;
		VkImage               gBuffer;
		VkImageView           colorView;
		VkImageView           normalView;
		VkImageView           gBufferView;
		VkImageMemoryBarrier2 acquireBarriers[3];
	} framebuffers[MIDVK_SWAP_COUNT];
} MxcNodeCompositorData;

//
/// Node Types
typedef struct MxcNodeFramebufferTexture {
	MidVkTexture color;
	MidVkTexture normal;
	MidVkTexture depth;
	MidVkTexture gbuffer;
} MxcNodeFramebufferTexture;
typedef struct MxcNodeContext {
	// cold data
	MxcNodeType               type;

	VkSemaphore               nodeTimeline;
	// not convinced I need this...
	VkSemaphore               compTimeline;
	// ya this will get pulled out into a shared pool of some sort
	MxcNodeFramebufferTexture framebufferTextures[MIDVK_SWAP_COUNT];
	MxcNodeShared*            pNodeShared;

	// MXC_NODE_TYPE_THREAD
	pthread_t       threadId;
	VkCommandPool   pool;
	VkCommandBuffer cmd;

	// MXC_NODE_TYPE_INTERPROCESS
	DWORD                  processId;
	HANDLE                 processHandle;
	HANDLE                 externalMemoryHandle;
	MxcExternalNodeMemory* pExternalMemory;

} MxcNodeContext;

//
/// Global State
#define MXC_NODE_CAPACITY 64
typedef uint8_t NodeHandle;
// move these into struct
extern size_t nodeCount;
// Cold storage for all node data
extern MxcNodeContext        nodeContexts[MXC_NODE_CAPACITY];
// Could be missing if node is external process
extern MxcNodeShared nodesShared[MXC_NODE_CAPACITY];
// Holds pointer to either local or external process shared memory
extern MxcNodeShared* activeNodesShared[MXC_NODE_CAPACITY];

// technically this should go into a comp node thread local....
extern MxcNodeCompositorData nodeCompositorData[MXC_NODE_CAPACITY];
extern MxcCompositorNodeContext compositorNodeContext;

//
/// Node Queue
// This could be a MidVK construct
typedef struct MxcQueuedNodeCommandBuffer {
	VkCommandBuffer cmd;
	VkSemaphore     nodeTimeline;
	uint64_t        nodeTimelineSignalValue;
} MxcQueuedNodeCommandBuffer;
extern size_t                     submitNodeQueueStart;
extern size_t                     submitNodeQueueEnd;
extern MxcQueuedNodeCommandBuffer submitNodeQueue[MXC_NODE_CAPACITY];
static inline void                mxcQueueNodeCommandBuffer(MxcQueuedNodeCommandBuffer handle)
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

void mxcRequestAndRunCompNodeThread(const VkSurfaceKHR surface, void* (*runFunc)(const struct MxcCompositorNodeContext*));
void mxcRequestNodeThread(void* (*runFunc)(const struct MxcNodeContext*), NodeHandle* pNodeHandle);

void mxcCreateNodeRenderPass();
void mxcCreateNodeFramebuffer(const MidLocality locality, MxcNodeFramebufferTexture* pNodeFramebufferTextures);

//
/// Process IPC
void mxcInitializeInterprocessServer();
void mxcShutdownInterprocessServer();
void mxcConnectInterprocessNode();
void mxcShutdownInterprocessNode();

//
/// Process IPC Targets

// Do we need this for threads!? Ya lets do it
void mxcInterprocessTargetNodeClosed(const NodeHandle handle);

typedef void (*MxcInterProcessFuncPtr)(const NodeHandle);
typedef enum MxcInterprocessTarget {
	MXC_INTERPROCESS_TARGET_NODE_CLOSED,
	MXC_INTERPROCESS_TARGET_COUNT,
} MxcInterprocessTarget;
_Static_assert(MXC_INTERPROCESS_TARGET_COUNT <= MXC_RING_BUFFER_HANDLE_CAPACITY, "IPC targets larger than ring buffer size.");
extern const MxcInterProcessFuncPtr MXC_INTERPROCESS_TARGET_FUNC[];

static inline int mxcInterprocessEnqueue(MxcRingBuffer* pBuffer, const MxcInterprocessTarget target)
{
	__atomic_thread_fence(__ATOMIC_ACQUIRE);
	const MxcRingBufferHandle head = pBuffer->head;
	const MxcRingBufferHandle tail = pBuffer->tail;
	if (head + 1 == tail) {
		fprintf(stderr, "Ring buffer wrapped!");
		return 1;
	}
	pBuffer->targets[head] = target;
	pBuffer->head = (head + 1) % MXC_RING_BUFFER_CAPACITY;
	__atomic_thread_fence(__ATOMIC_RELEASE);
	return 0;
}
static inline int mxcInterprocessDequeue(MxcRingBuffer* pBuffer, const NodeHandle nodeHandle)
{
	__atomic_thread_fence(__ATOMIC_ACQUIRE);
	const MxcRingBufferHandle head = pBuffer->head;
	const MxcRingBufferHandle tail = pBuffer->tail;
	if (head == tail)
		return 1;

	printf("IPC Polling %d %d...\n", head, tail);
	MxcInterprocessTarget target = (MxcInterprocessTarget)(pBuffer->targets[tail]);
	pBuffer->tail = (tail + 1) % MXC_RING_BUFFER_CAPACITY;
	__atomic_thread_fence(__ATOMIC_RELEASE);

	printf("Calling IPC Target %d...\n", target);
	MXC_INTERPROCESS_TARGET_FUNC[target](nodeHandle);
	return 0;
}
static inline void mxcInterprocessQueuePoll()
{
	for (int i = 0; i < nodeCount; ++i) {
		mxcInterprocessDequeue(&activeNodesShared[i]->targetQueue, i);
	}
}