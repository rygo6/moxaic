#pragma once

#include "globals.h"
#include "renderer.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#include <windows.h>

typedef enum MxcNodeType {
  MXC_NODE_TYPE_THREAD,
  MXC_NODE_TYPE_INTERPROCESS,
  MXC_NODE_TYPE_COUNT
} MxcNodeType;

typedef struct MxcImportParam {
  uint32_t framebufferWidth;
  uint32_t framebufferHeight;
  HANDLE   colorFramebuffer0ExternalHandle;
  HANDLE   colorFramebuffer1ExternalHandle;
  HANDLE   normalFramebuffer0ExternalHandle;
  HANDLE   normalFramebuffer1ExternalHandle;
  HANDLE   gBufferFramebuffer0ExternalHandle;
  HANDLE   gBufferFramebuffer1ExternalHandle;
  HANDLE   depthFramebuffer0ExternalHandle;
  HANDLE   depthFramebuffer1ExternalHandle;
  HANDLE   compositorSemaphoreExternalHandle;
  HANDLE   nodeSemaphoreExternalHandle;
} MxcImportParam;

#define MXC_NODE_CLEAR_COLOR (VkClearColorValue) { 0.0f, 0.0f, 0.0f, 0.0f }

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

typedef struct MxcNodeContext {
  MxcNodeType nodeType;
  int         compCycleSkip;

  const void* pNode;
  void (*runFunc)(const struct MxcNodeContext* pNode);

  // need ref to send over IPC. Can't get from compNodeShared. Or can I? I can map parts of array...
  VkSemaphore compTimeline;
  VkSemaphore nodeTimeline;

  VkmNodeFramebuffer framebuffers[VKM_SWAP_COUNT];

  MxcRingBuffer consumer;
  MxcRingBuffer producer;

  // local thread
  pthread_t threadId;

  // interprocess
  STARTUPINFO           startupInfo;
  PROCESS_INFORMATION   processInformation;
  MxcInterProcessBuffer interProcessConsumer;
  MxcInterProcessBuffer interProcessProducer;

} MxcNodeContext;

CACHE_ALIGN typedef struct MxcCompNodeContextShared {
  // shared
  volatile uint64_t        pendingTimelineSignal;
  volatile uint64_t        currentTimelineSignal;
  volatile VkCommandBuffer cmd;
  volatile uint64_t        lastTimelineSignal;
  volatile VkSemaphore     compTimeline;
  volatile uint32_t        swapIndex;
  VkSwapchainKHR           chain;
  VkSemaphore              acquireSemaphore;
  VkSemaphore              renderCompleteSemaphore;
} MxcCompNodeContextShared;

extern MxcCompNodeContextShared compNodeShared;

typedef struct MxcNodeSetState {

  mat4 model;

  mat4 view;
  mat4 proj;
  mat4 viewProj;

  mat4 invView;
  mat4 invProj;
  mat4 invViewProj;

  // subsequent vulkan ubo values must be aligned to what prior was
  ALIGN(16)
  ivec2 framebufferSize;
  ALIGN(8)
  vec2 ulUV;
  ALIGN(8)
  vec2 lrUV;

} MxcNodeSetState;

CACHE_ALIGN typedef struct MxcNodeContextShared {
  // shared
  volatile uint64_t          pendingTimelineSignal;
  volatile uint64_t          currentTimelineSignal;
  volatile vec2              ulUV;
  volatile vec2              lrUV;
  volatile float             radius;
  volatile VkmGlobalSetState globalSetState;
  volatile VkCommandBuffer   cmd;
  volatile uint64_t          lastTimelineSignal;
  volatile VkSemaphore       nodeTimeline;
  volatile bool              active;

  // unshared
  uint64_t        lastTimelineSwap;
  MxcNodeType     type;
  VkmTransform    transform;
  MxcNodeSetState nodeSetState;
  VkImageView     framebufferColorImageViews[VKM_SWAP_COUNT];
  VkImageView     framebufferNormalImageViews[VKM_SWAP_COUNT];
  VkImageView     framebufferGBufferImageViews[VKM_SWAP_COUNT];
  VkImage         framebufferColorImages[VKM_SWAP_COUNT];
  VkImage         framebufferNormalImages[VKM_SWAP_COUNT];
  VkImage         framebufferGBufferImages[VKM_SWAP_COUNT];
} MxcNodeContextShared;

#define MXC_NODE_CAPACITY 256
typedef uint8_t             NodeHandle;
extern size_t               nodeCount;
extern MxcNodeContext       nodes[MXC_NODE_CAPACITY];
extern MxcNodeContextShared nodesShared[MXC_NODE_CAPACITY];

static inline void mxcSubmitNodeQueues(const VkQueue graphicsQueue) {
  for (int i = 0; i < nodeCount; ++i) {
    {  // submit commands
      uint64_t value = nodesShared[i].pendingTimelineSignal;
      __atomic_thread_fence(__ATOMIC_ACQUIRE);
      if (value > nodesShared[i].lastTimelineSignal) {
        nodesShared[i].lastTimelineSignal = value;
        vkmSubmitCommandBuffer(nodesShared[i].cmd, graphicsQueue, nodesShared[i].nodeTimeline, value);
      }
    }
  }
}

void mxcRequestNodeContextThread(const VkSemaphore compTimeline, void (*runFunc)(const struct MxcNodeContext*), const void* pNode, NodeHandle* pNodeHandle);
void mxcRegisterNodeContextThread(NodeHandle handle, VkCommandBuffer cmd);
void mxcRunNodeContext(const MxcNodeContext* pNodeContext);

// Renderpass with layout transitions setup for use in node
void mxcCreateNodeRenderPass();
void mxcCreateNodeFramebufferImport(const VkmLocality locality, const VkmNodeFramebuffer* pNodeFramebuffers, VkmFramebuffer* pFrameBuffers);
void mxcCreateNodeFramebufferExport(const VkmLocality locality, VkmNodeFramebuffer* pNodeFramebuffers);

// Process IPC
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