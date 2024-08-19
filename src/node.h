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
  uint8_t head;
  uint8_t tail;
  uint8_t ringBuffer[MXC_RING_BUFFER_SIZE];
} MxcRingBuffer;
typedef struct MxcInterProcessBuffer {
  LPVOID sharedBuffer;
  HANDLE          mapFileHandle;
} MxcInterProcessBuffer;

typedef struct CACHE_ALIGN MxcNodeShared {
  // read/write every cycle
  VkmGlobalSetState globalSetState;
  vec2              ulUV;
  vec2              lrUV;
  uint64_t          pendingTimelineSignal;
  uint64_t          currentTimelineSignal;

  // read every cycle, occasional write
  int               compCycleSkip;
  float             radius;
//  bool              active;

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
  MxcNodeShared nodeShared;
  MxcImportParam importParam;
} MxcExternalNodeMemory;

//
/// Node Comp Types
typedef struct CACHE_ALIGN MxcCompNodeContext {
  // read/write multiple threads
  uint32_t swapIndex;

  // read by multiple threads
  VkCommandBuffer cmd;
  VkSemaphore     compTimeline;
  MidVkSwap       swap;

  // cold data
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
typedef struct CACHE_ALIGN MxcNodeProcessCompData { // pretty sure we want this
  MxcNodeSetState nodeSetState;
  MidTransform    transform;
  VkSemaphore     nodeTimeline;
  uint64_t        lastTimelineSwap;
  struct {
    VkImage     color;
    VkImage     normal;
    VkImage     gBuffer;
    VkImageView colorView;
    VkImageView normalView;
    VkImageView gBufferView;
  } framebufferImages[MIDVK_SWAP_COUNT];
} MxcNodeProcessCompData;
typedef struct CACHE_ALIGN MxcNodeThreadCompData {
  // node hot data for compositor
  MxcNodeType             type; // get rid of this, we need thread and process arrays
  MxcNodeSetState         nodeSetState;
  MidTransform            transform;
  VkCommandBuffer         cmd;
  VkSemaphore             nodeTimeline;
  uint64_t                lastTimelineSignal;
  uint64_t                lastTimelineSwap;
  struct {
    VkImage     color;
    VkImage     normal;
    VkImage     gBuffer;
    VkImageView colorView;
    VkImageView normalView;
    VkImageView gBufferView;
  } framebufferImages[MIDVK_SWAP_COUNT];
} MxcNodeThreadCompData;

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
  MxcNodeType nodeType;
  VkCommandPool   pool;
  VkCommandBuffer cmd;
  VkSemaphore nodeTimeline;
  VkSemaphore compTimeline; // not convinced I need this...
  MxcNodeFramebufferTexture framebufferTextures[MIDVK_SWAP_COUNT];

  // MXC_NODE_TYPE_THREAD
  pthread_t threadId;

  // MXC_NODE_TYPE_INTERPROCESS
  DWORD                  processId;
  HANDLE                 processHandle;
  HANDLE                 externalMemoryHandle;
  MxcExternalNodeMemory* pExternalMemory;

} MxcNodeContext;

// probably going to want to do this.. although it could make it harder to pass this to the node thread
// mayube dont want this? it is cold data
typedef struct MxcNodeProcessExportContext {
  VkSemaphore nodeTimeline;
  MxcNodeFramebufferTexture framebufferTextures[MIDVK_SWAP_COUNT];
  DWORD                  processId;
  HANDLE                 processHandle;
  HANDLE                 externalMemoryHandle;
  MxcExternalNodeMemory* pExternalMemory;
} MxcNodeProcessContext;
typedef struct MxcNodeProcessImportContext {
  VkCommandPool             pool;
  VkCommandBuffer           cmd;
  VkSemaphore               nodeTimeline;
  VkSemaphore               compTimeline;  // probably need this
  MxcNodeFramebufferTexture framebufferTextures[MIDVK_SWAP_COUNT];
  pthread_t                 threadId;
  HANDLE                    externalMemoryHandle;
  MxcExternalNodeMemory*    pExternalMemory;
} MxcNodeProcessImportContext;

//
/// Global state
extern MxcCompNodeContext compNodeContext;

#define MXC_NODE_CAPACITY 256
typedef uint8_t           NodeHandle;

extern size_t             nodeCount;
extern NodeHandle         nodesAvailable[MXC_NODE_CAPACITY];
extern MxcNodeContext     nodeContexts[MXC_NODE_CAPACITY];
extern MxcNodeShared      nodesShared[MXC_NODE_CAPACITY];
extern MxcNodeThreadCompData nodeCompData[MXC_NODE_CAPACITY];

// do this?
extern size_t             nodeProcessCount;
extern NodeHandle         availableNodeProcess[MXC_NODE_CAPACITY];
extern MxcNodeContext     nodeProcessContext[MXC_NODE_CAPACITY];
extern MxcNodeShared      nodeProcessShared[MXC_NODE_CAPACITY];
extern MxcNodeThreadCompData nodeProcessCompData[MXC_NODE_CAPACITY];

//
/// Methods

// todo change to use this
extern size_t      submitNodeQueueCount;
extern NodeHandle  submitNodeQueue[MXC_NODE_CAPACITY];
static inline void mxcQueueNodeCommandBuffer(NodeHandle handle) {


}

static inline void mxcSubmitNodeCommandBuffers(const VkQueue graphicsQueue) {
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
void mxcRequestNodeThread(NodeHandle* pNodeHandle);
void mxcRunNodeThread(void* (*runFunc)(const struct MxcNodeContext*), NodeHandle handle);
void mxcRequestNodeProcessExport(const VkSemaphore compTimeline, NodeHandle* pNodeHandle);

void mxcCreateNodeRenderPass();
void mxcCreateNodeFramebuffer(const MidLocality locality, MxcNodeFramebufferTexture* pNodeFramebufferTextures);

// Process IPC
#include <pthread.h>
void mxcInitializeIPCServer();
void mxcShutdownIPCServer();
void mxcConnectNodeIPC();
void mxcShutdownNodeIPC();

void mxcRequestNodeProcessImport(const HANDLE externalMemoryHandle, MxcExternalNodeMemory* pExternalMemory, const MxcImportParam* pImportParam, NodeHandle* pNodeHandle);
void mxcCreateNodeFramebufferImport(const MidLocality locality, const MxcNodeFramebufferTexture* pNodeFramebuffers, MxcNodeFramebufferTexture* pFramebufferTextures);

typedef void (*MxcInterProcessFuncPtr)(const void*);
typedef enum MxcRingBufferTarget {
  MXC_INTERPROCESS_TARGET_IMPORT
} MxcRingBufferTarget;
static const size_t MXC_INTERPROCESS_TARGET_SIZE[] = {
    [MXC_INTERPROCESS_TARGET_IMPORT] = sizeof(MxcImportParam),
};
static const MxcInterProcessFuncPtr MXC_INTERPROCESS_TARGET_FUNC[] = {
    [MXC_INTERPROCESS_TARGET_IMPORT] = (MxcInterProcessFuncPtr const)mxcRequestNodeProcessImport,
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