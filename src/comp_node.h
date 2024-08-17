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
} MxcCompNodeCreateInfo;

typedef struct MxcCompNode {
  MxcCompMode compMode;

  // device should go in context
  VkDevice        device;

  VkRenderPass compRenderPass;

  VkDescriptorSetLayout compNodeSetLayout;
  VkPipelineLayout      compNodePipeLayout;
  VkPipeline            compNodePipe;

  VkQueryPool timeQueryPool;

  VkmGlobalSet    globalSet;
  VkDescriptorSet compNodeSet;

  MidVkSharedMemory compNodeSetMemory;
  VkBuffer        compNodeSetBuffer;

  VkmMesh quadMesh;

  VkFramebuffer framebuffer;
  MidVkFramebufferTexture framebuffers[MIDVK_SWAP_COUNT];

} MxcCompNode;


void  mxcCreateCompNode(const MxcCompNodeCreateInfo* pInfo, MxcCompNode* pNode);
void* mxcCompNodeThread(const MxcCompNodeContext* pNodeContext);
void  mxcBindUpdateCompNode(const MxcCompNodeCreateInfo* pInfo, MxcCompNode* pNode);
