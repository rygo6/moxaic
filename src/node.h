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
	MXC_NODE_TYPE_INTERPROCESS_VULKAN_EXPORTED,
	MXC_NODE_TYPE_INTERPROCESS_VULKAN_IMPORTED,
	MXC_NODE_TYPE_INTERPROCESS_OPENGL_EXPORTED,
	MXC_NODE_TYPE_INTERPROCESS_OPENGL_IMPORTED,
	MXC_NODE_TYPE_INTERPROCESS_EXPORTED,
	MXC_NODE_TYPE_INTERPROCESS_IMPORTED,
	MXC_NODE_TYPE_COUNT
} MxcNodeType;

#define MXC_NODE_GBUFFER_FORMAT VK_FORMAT_R32_SFLOAT
#define MXC_NODE_GBUFFER_USAGE  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
#define MXC_NODE_GBUFFER_LEVELS 10
#define MXC_NODE_CLEAR_COLOR (VkClearColorValue) { 0.0f, 0.0f, 0.0f, 0.0f }

//
/// IPC Types
typedef uint8_t MxcRingBufferHandle;
#define MXC_RING_BUFFER_CAPACITY 256
#define MXC_RING_BUFFER_HANDLE_CAPACITY (1 << (sizeof(MxcRingBufferHandle) * CHAR_BIT))
_Static_assert(MXC_RING_BUFFER_CAPACITY <= MXC_RING_BUFFER_HANDLE_CAPACITY, "RingBufferHandle type can't store capacity.");
typedef struct MxcRingBuffer {
	MxcRingBufferHandle head;
	MxcRingBufferHandle tail;
	MxcRingBufferHandle targets[MXC_RING_BUFFER_CAPACITY];
} MxcRingBuffer;

//
/// Shared Types
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
	int   compositorBaseCycleValue;

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
	// read multiple threads, write 1 thread
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
typedef struct MxcNodeVkFramebufferTexture {
	MidVkTexture color;
	MidVkTexture normal;
	MidVkTexture depth;
	MidVkTexture gbuffer;
} MxcNodeVkFramebufferTexture;
// Cold Node date
typedef struct MxcNodeContext {
	MxcNodeType type;

	VkCommandPool   pool;
	VkCommandBuffer cmd;

	// shared data
	MxcNodeShared*            pNodeShared;

	// vulkan shared data
	MxcNodeVkFramebufferTexture nodeFramebuffer[MIDVK_SWAP_COUNT];

	// I'm not actually 100% sure if I want this
	VkFence     nodeFence;

	VkSemaphore nodeTimeline;
	VkSemaphore compositorTimeline;

	// debating if this should be an option
	// opengl shared data
//	GLuint glFramebufferTextures;
//	GLuint glCompTimeline;
//	GLuint glNodeTimeline;

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

//
//// Node State

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
extern MxcNodeShared nodesShared[MXC_NODE_CAPACITY];
// Holds pointer to either local or external process shared memory
extern MxcNodeShared* activeNodesShared[MXC_NODE_CAPACITY];



//
//// Compositor State

extern HANDLE                 importedExternalMemoryHandle;
extern MxcExternalNodeMemory* pImportedExternalMemory;

// I should do this. remove compositor code from nodes entirely
//#if defined(MOXAIC_COMPOSITOR)
// technically this should go into a comp node thread local....
extern MxcNodeCompositorData nodeCompositorData[MXC_NODE_CAPACITY];
extern MxcCompositorNodeContext compositorNodeContext;
//#endif

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

void mxcRequestAndRunCompositorNodeThread(const VkSurfaceKHR surface, void* (*runFunc)(const struct MxcCompositorNodeContext*));
void mxcRequestNodeThread(void* (*runFunc)(const struct MxcNodeContext*), NodeHandle* pNodeHandle);

void mxcCreateNodeRenderPass();
void mxcCreateNodeFramebuffer(const MidLocality locality, MxcNodeVkFramebufferTexture* pNodeFramebufferTextures);

NodeHandle RequestExternalNodeHandle(MxcNodeShared* pNodeShared);

//
/// Process IPC
void mxcInitializeInterprocessServer();
void mxcShutdownInterprocessServer();
void mxcConnectInterprocessNode(bool createTestNode);
void mxcShutdownInterprocessNode();

//
/// Process IPC Targets
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