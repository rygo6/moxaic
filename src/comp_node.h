#pragma once

#include "mid_vulkan.h"
#include "node.h"

typedef enum MxcCompMode {
  MXC_COMP_MODE_BASIC,
  MXC_COMP_MODE_TESS,
  MXC_COMP_MODE_TASK_MESH,
  MXC_COMP_MODE_COMPUTE,
  MXC_COMP_MODE_COUNT,
} MxcCompMode;

typedef struct MxcCompNodeCreateInfo {
  MxcCompMode  compMode;
  VkSurfaceKHR surface;
} MxcCompNodeCreateInfo;

typedef struct MxcCompNode {
  MxcCompMode compMode;

  VkDevice        device;
  VkCommandBuffer cmd;

  VkRenderPass compRenderPass;

  VkDescriptorSetLayout compNodeSetLayout;
  VkPipelineLayout      compNodePipeLayout;
  VkPipeline            compNodePipe;

  VkQueryPool timeQueryPool;

  VkmGlobalSet    globalSet;
  VkDescriptorSet compNodeSet;

  VkmSharedMemory compNodeSetMemory;
  VkBuffer        compNodeSetBuffer;

  VkmMesh quadMesh;

  VkFramebuffer framebuffer;
  VkmFramebuffer framebuffers[VKM_SWAP_COUNT];
  VkmSwap        swap;
  VkSemaphore    timeline;

} MxcCompNode;


void  mxcCreateCompNode(const MxcCompNodeCreateInfo* pInfo, MxcCompNode* pNode);
void* mxcCompNodeThread(const MxcNode* pNodeContext);
void  mxcBindUpdateCompNode(const MxcCompNodeCreateInfo* pInfo, MxcCompNode* pNode);
