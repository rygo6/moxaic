#pragma once

#include "globals.h"
#include "mid_vulkan.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#include <windows.h>

//
/// Constants
typedef enum MxcNodeType {
  MXC_NODE_TYPE_THREAD,
  MXC_NODE_TYPE_INTERPROCESS,
  MXC_NODE_TYPE_COUNT
} MxcNodeType;

#define MXC_NODE_GBUFFER_FORMAT  VK_FORMAT_R32_SFLOAT
#define MXC_NODE_GBUFFER_USAGE   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
#define MXC_NODE_GBUFFER_LEVELS  10
#define MXC_NODE_CLEAR_COLOR (VkClearColorValue) { 0.0f, 0.0f, 0.0f, 0.0f }

//
/// IPC Types
#define MXC_RING_BUFFER_COUNT       256
#define MXC_RING_BUFFER_SIZE        MXC_RING_BUFFER_COUNT * sizeof(uint8_t)
#define MXC_RING_BUFFER_HEADER_SIZE 1
typedef struct MxcRingBuffer {
  volatile uint8_t head;
  volatile uint8_t tail;
  volatile uint8_t ringBuffer[MXC_RING_BUFFER_SIZE];
} MxcRingBuffer;
typedef struct MxcInterProcessBuffer {
  volatile LPVOID sharedBuffer;
  HANDLE          mapFileHandle;
} MxcInterProcessBuffer;

typedef struct CACHE_ALIGN MxcNodeShared {
  // read/write every cycle
  volatile VkmGlobalSetState globalSetState;
  volatile vec2              ulUV;
  volatile vec2              lrUV;
  volatile uint64_t          pendingTimelineSignal;
  volatile uint64_t          currentTimelineSignal;

  // read every cycle, occasional write
  volatile int               compCycleSkip;
  volatile float             radius;
  volatile bool              active;

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
typedef struct MxcExternalNodeMemory {
  volatile MxcNodeShared nodeShared;
  volatile MxcImportParam importParam;
} MxcExternalNodeMemory;

//
/// Node Comp Types
typedef struct CACHE_ALIGN MxcCompNodeContext {
  // read/write multiple threads
  volatile uint32_t swapIndex;

  // read by multiple threads
  VkCommandBuffer cmd;
  VkSemaphore     compTimeline;
  VkmSwap         swap;

  // read by comp thread
  VkCommandPool pool;
  pthread_t threadId;

} MxcCompNodeContext;
typedef struct MxcNodeSetState {
  mat4 model;

  // Laid out same as VkmGlobalSetState for memcpy
  mat4 view;
  mat4 proj;
  mat4 viewProj;
  mat4 invView;
  mat4 invProj;
  mat4 invViewProj;
  ivec2 framebufferSize;

  vec2 ulUV;
  vec2 lrUV;
} MxcNodeSetState;
// node hot data for compositor
typedef struct CACHE_ALIGN MxcNodeCompData {
  MxcNodeType             type;
  VkCommandBuffer         cmd;
  VkSemaphore             nodeTimeline;
  uint64_t                lastTimelineSignal;
  uint64_t                lastTimelineSwap;
  MxcNodeSetState         nodeSetState;
  VkmTransform            transform;
  struct {
    VkImage     color;
    VkImage     normal;
    VkImage     gBuffer;
    VkImageView colorView;
    VkImageView normalView;
    VkImageView gBufferView;
  } framebufferImages[MIDVK_SWAP_COUNT];
} MxcNodeCompData;

//
/// Node Types
typedef struct MxcNodeFramebufferTexture {
  MidVkTexture color;
  MidVkTexture normal;
  MidVkTexture depth;
  MidVkTexture gbuffer;
} MxcNodeFramebufferTexture;
typedef struct MxcNodeContext {
  MxcNodeType nodeType;

  VkCommandPool   pool;
  VkCommandBuffer cmd;
  VkSemaphore nodeTimeline;
  VkSemaphore compTimeline;

  MxcNodeFramebufferTexture framebufferTextures[MIDVK_SWAP_COUNT];

  // MXC_NODE_TYPE_THREAD
  pthread_t threadId;

  // MXC_NODE_TYPE_INTERPROCESS
  HANDLE processHandle;
  HANDLE compTimelineHandle;
  HANDLE nodeTimelineHandle;
  HANDLE externalNodeMemoryHandle;

  MxcExternalNodeMemory* pExternalNodeMemory;

} MxcNodeContext;

//
/// Global state
#define MXC_NODE_CAPACITY 256
typedef uint8_t           NodeHandle;
extern size_t             nodeCount;
extern NodeHandle         nodesAvailable[MXC_NODE_CAPACITY];
extern MxcNodeContext     nodeContexts[MXC_NODE_CAPACITY];
extern MxcNodeShared      nodesShared[MXC_NODE_CAPACITY];
extern MxcNodeCompData    nodeCompData[MXC_NODE_CAPACITY];
extern MxcCompNodeContext compNodeContext;

//
/// Methods

static inline void mxcSubmitNodeThreadQueues(const VkQueue graphicsQueue) {
  for (int i = 0; i < nodeCount; ++i) {
    uint64_t value = nodesShared[i].pendingTimelineSignal;
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    if (value > nodeCompData[i].lastTimelineSignal) {
      nodeCompData[i].lastTimelineSignal = value;
      vkmSubmitCommandBuffer(nodeCompData[i].cmd, graphicsQueue, nodeCompData[i].nodeTimeline, value);
    }
  }
}

void mxcRequestAndRunCompNodeThread(const VkSurfaceKHR surface, void* (*runFunc)(const struct MxcCompNodeContext*));
void mxcRequestNodeThread(NodeHandle* pHandle);
void mxcRunNodeThread(void* (*runFunc)(const struct MxcNodeContext*), NodeHandle handle);
void mxcRequestNodeProcess(const VkSemaphore compTimeline, NodeHandle* pNodeHandle);

// Renderpass with layout transitions setup for use in node
void mxcCreateNodeRenderPass();
void mxcCreateNodeFramebufferImport(const MidLocality locality, const MxcNodeFramebufferTexture* pNodeFramebuffers, MxcNodeFramebufferTexture* pFramebufferTextures);
void mxcCreateNodeFramebufferExport(const MidLocality locality, MxcNodeFramebufferTexture* pNodeFramebufferTextures);

// Process IPC
#include <pthread.h>
void mxcInitializeIPCServer();
void mxcShutdownIPCServer();
void mxcConnectNodeIPC();
void mxcShutdownNodeIPC();

void mxcCreateSharedBuffer();
void mxcCreateProcess();

typedef void (*MxcInterProcessFuncPtr)(void*);
void mxcInterProcessImportNode(void* pParam);
typedef enum MxcRingBufferTarget {
  MXC_INTERPROCESS_TARGET_IMPORT
} MxcRingBufferTarget;
static const size_t MXC_INTERPROCESS_TARGET_SIZE[] = {
    [MXC_INTERPROCESS_TARGET_IMPORT] = sizeof(MxcImportParam),
};
static const MxcInterProcessFuncPtr MXC_INTERPROCESS_TARGET_FUNC[] = {
    [MXC_INTERPROCESS_TARGET_IMPORT] = mxcInterProcessImportNode,
};


// to use ring buffer?
static inline void mxcRingBufferEnqeue(MxcRingBuffer* pBuffer, uint8_t target, void* pParam) {
  pBuffer->ringBuffer[pBuffer->head] = target;
  memcpy((void*)(pBuffer->ringBuffer + pBuffer->head + MXC_RING_BUFFER_HEADER_SIZE), pParam, MXC_INTERPROCESS_TARGET_SIZE[target]);
  pBuffer->head = pBuffer->head + MXC_RING_BUFFER_HEADER_SIZE + MXC_INTERPROCESS_TARGET_SIZE[target];
}
static inline int mxcRingBufferDeque(MxcRingBuffer* pBuffer) {
  // TODO this needs to actually cycle around the ring buffer, this is only half done

  if (pBuffer->head == pBuffer->tail)
    return 1;

  printf("IPC Polling %d %d...\n", pBuffer->head, pBuffer->tail);

  MxcRingBufferTarget target = (MxcRingBufferTarget)(pBuffer->ringBuffer[pBuffer->tail]);

  // TODO do you copy it out of the IPC or just send that chunk of shared memory on through?
  // If consumer consumes too slow then producer might run out of data in a stream?
  // From trusted parent app sending shared memory through is probably fine
  //    void *param = malloc(fbrIPCTargetParamSize(target));
  //    memcpy(param, pRingBuffer->pRingBuffer + pRingBuffer->tail + FBR_IPC_RING_HEADER_SIZE, fbrIPCTargetParamSize(target));
  void* pParam = (void*)(pBuffer->ringBuffer + pBuffer->tail + MXC_RING_BUFFER_HEADER_SIZE);

  if (pBuffer->tail + MXC_RING_BUFFER_HEADER_SIZE + MXC_INTERPROCESS_TARGET_SIZE[target] > MXC_RING_BUFFER_SIZE) {
    // TODO this needs to actually cycle around the ring buffer, this is only half done
    PANIC("IPC BYTE ARRAY REACHED END!!!");
  }

  printf("Calling IPC Target %d... ", target);
  MXC_INTERPROCESS_TARGET_FUNC[target](pParam);

  pBuffer->tail = pBuffer->tail + MXC_RING_BUFFER_HEADER_SIZE + MXC_INTERPROCESS_TARGET_SIZE[target];

  return 0;
}