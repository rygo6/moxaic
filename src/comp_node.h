#pragma once

#include "node.h"
#include "renderer.h"

// we probably want to keep the comp node on its own thread so the main
// thread/context can be used for loading in content
// but the comp node can't make new things from context, main thread has to do that
// but it seems so much simpler if main thread is comp thread, or node process thread
// truth is context loading is side effort... it should be occasional thread

typedef enum MxcCompMode {
  MXC_COMP_MODE_BASIC,
  MXC_COMP_MODE_TESS,
} MxcCompMode;

typedef struct MxcCompNodeCreateInfo {
  MxcCompMode  compMode;
  VkSurfaceKHR surface;
} MxcCompNodeCreateInfo;

typedef struct MxcBasicComp {
  VkCommandBuffer cmd;

  VkRenderPass stdRenderPass;

  VkDescriptorSetLayout nodeSetLayout;
  VkPipelineLayout      nodePipeLayout;
  VkPipeline            nodePipe;

  VkDevice device;

  VkQueryPool timeQueryPool;

  VkmGlobalSet globalSet;

  VkDescriptorSet nodeSet;

  MxcNodeSetState* pNodeSetMapped;
  VkDeviceMemory   nodeSetMemory;
  VkBuffer         nodeSetBuffer;

  VkmMesh quadMesh;

  VkmFramebuffer framebuffers[VKM_SWAP_COUNT];

  VkmSwap swap;

  VkSemaphore timeline;

  VkQueue graphicsQueue;

} MxcCompNode;


void mxcCreateCompNode(const MxcCompNodeCreateInfo* pInfo, MxcCompNode* pComp);
void mxcRunCompNode(const MxcNodeContext* pNodeContext);
