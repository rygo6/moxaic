#pragma once

#include "globals.h"
#include "renderer.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>

typedef enum MxcNodeType {
  MXC_NODE_TYPE_CONTEXT_THREAD,
  MXC_NODE_TYPE_INTERCONTEXT_PROCESS,
  MXC_NODE_TYPE_INTERPROCESS
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

typedef struct MxcNodeContextHot {
  VkmTimeline     nodeTimeline;
  VkImageView     framebufferColorImageViews[VKM_SWAP_COUNT];
  VkImage         framebufferColorImages[VKM_SWAP_COUNT];
  VkCommandBuffer cmd;
  bool            cmdReady;
} MxcNodeContextHot;

#define MXC_NODE_CAPACITY 256
typedef uint8_t          mxc_node_handle;
extern size_t            MXC_NODE_HANDLE_COUNT;
extern MxcNodeContext    MXC_NODE_CONTEXTS[MXC_NODE_CAPACITY];
extern MxcNodeContextHot MXC_HOT_NODE_CONTEXTS[MXC_NODE_CAPACITY];


static inline void mxcCopyHotNodeContext(mxc_node_handle handle) {
  MXC_HOT_NODE_CONTEXTS[handle].nodeTimeline.semaphore = MXC_NODE_CONTEXTS[handle].nodeTimeline;
  for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
    MXC_HOT_NODE_CONTEXTS[handle].framebufferColorImageViews[i] = MXC_NODE_CONTEXTS[handle].framebuffers[i].color.imageView;
    MXC_HOT_NODE_CONTEXTS[handle].framebufferColorImages[i] = MXC_NODE_CONTEXTS[handle].framebuffers[i].color.image;
  }
}

static inline void mxcCreateNodeContext(MxcNodeContext* pNodeContext) {

  switch (pNodeContext->nodeType) {
    case MXC_NODE_TYPE_CONTEXT_THREAD: {
      int result = pthread_create(&pNodeContext->threadId, NULL, (void* (*)(void*))pNodeContext->runFunc, pNodeContext);
      REQUIRE(result == 0, "Node thread creation failed!");
      break;
    }
    case MXC_NODE_TYPE_INTERCONTEXT_PROCESS: break;
    case MXC_NODE_TYPE_INTERPROCESS:
      vkmCreateNodeFramebufferExport(VKM_LOCALITY_CONTEXT, pNodeContext->framebuffers);

      break;
  }
}

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