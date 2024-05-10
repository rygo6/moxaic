#pragma once

#include "globals.h"
#include "renderer.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <windows.h>

typedef struct VkmNodeFramebuffer {
  VkmTexture    color;
  VkmTexture    gBuffer;
} VkmNodeFramebuffer;

typedef enum MxcNodeType {
  MXC_NODE_TYPE_LOCAL_THREAD,
  MXC_NODE_TYPE_PROCESS
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

  const VkmContext* pContext;
  void*             pInitArg;
  void (*createFunc)(void*);
  void (*updateFunc)();

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

// One thread can only access one node
extern _Thread_local const MxcNodeContext* pNodeContext;

void* mxcUpdateNode(void* pArg);

static inline void mxcCreateNodeContext(MxcNodeContext* pNode) {
  switch (pNode->nodeType) {
    case MXC_NODE_TYPE_LOCAL_THREAD: {
      int result = pthread_create(&pNode->threadId, NULL, mxcUpdateNode, pNode);
      REQUIRE(result == 0, "Node thread creation failed!");
      break;
    }
    case MXC_NODE_TYPE_PROCESS:


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