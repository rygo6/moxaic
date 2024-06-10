#include "node.h"

MxcCompNodeContextShared compNodeShared;

size_t               nodeCount = 0;
MxcNodeContext       nodes[MXC_NODE_CAPACITY] = {};
MxcNodeContextShared nodesShared[MXC_NODE_CAPACITY] = {};

void mxcInterProcessImportNode(void* pParam) {
}

void mxcRegisterNodeContextThread(const NodeHandle handle, const VkCommandBuffer cmd) {
  nodesShared[handle] = (MxcNodeContextShared){};
  nodesShared[handle].cmd = cmd;
  nodesShared[handle].active = true;
  nodesShared[handle].radius = 0.5;
  nodesShared[handle].type = nodes[handle].nodeType;
  nodesShared[handle].nodeTimeline = nodes[handle].nodeTimeline;
  nodesShared[handle].transform.rotation = QuatFromEuler(nodesShared[handle].transform.euler);
  for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
    nodesShared[handle].framebufferColorImageViews[i] = nodes[handle].framebuffers[i].color.view;
    nodesShared[handle].framebufferNormalImageViews[i] = nodes[handle].framebuffers[i].normal.view;
    nodesShared[handle].framebufferGBufferImageViews[i] = nodes[handle].framebuffers[i].gBuffer.view;
    nodesShared[handle].framebufferColorImages[i] = nodes[handle].framebuffers[i].color.img;
    nodesShared[handle].framebufferNormalImages[i] = nodes[handle].framebuffers[i].normal.img;
    nodesShared[handle].framebufferGBufferImages[i] = nodes[handle].framebuffers[i].gBuffer.img;
  }
  nodeCount++;
  __atomic_thread_fence(__ATOMIC_RELEASE);
  mxcRunNodeContext(&nodes[handle]);
}

NodeHandle mxcRequestNodeContextThread(void (*runFunc)(const struct MxcNodeContext*), const void* pNode) {
  NodeHandle      nodeHandle = 0;
  MxcNodeContext* pNodeContext = &nodes[nodeHandle];

  // create every time? or recycle? recycle probnably better to free resource
  vkmCreateNodeFramebufferExport(VKM_LOCALITY_CONTEXT, pNodeContext->framebuffers);
  vkmCreateTimeline(&pNodeContext->nodeTimeline);
  pNodeContext->compTimeline = compNodeShared.compTimeline;
  pNodeContext->nodeType = MXC_NODE_TYPE_THREAD;
  pNodeContext->pNode = pNode;
  pNodeContext->compCycleSkip = 16;
  pNodeContext->runFunc = runFunc;

  return 0;
}

void mxcRunNodeContext(const MxcNodeContext* pNodeContext) {
  switch (pNodeContext->nodeType) {
    case MXC_NODE_TYPE_THREAD:
      int result = pthread_create(&pNodeContext->threadId, NULL, (void* (*)(void*))pNodeContext->runFunc, pNodeContext);
      REQUIRE(result == 0, "Node thread creation failed!");
      break;
    case MXC_NODE_TYPE_INTERPROCESS:
      vkmCreateNodeFramebufferExport(VKM_LOCALITY_CONTEXT, pNodeContext->framebuffers);
      break;
    default: PANIC("Node type not available!");
  }
}
