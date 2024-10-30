#pragma once

#include "node.h"
#include "mid_vulkan.h"

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
  // todo no shjould just access it from context directly
  VkDevice        device;

  VkRenderPass compRenderPass;

  VkDescriptorSetLayout compNodeSetLayout;
  VkPipelineLayout      compNodePipeLayout;
  VkPipeline            compNodePipe;

  VkQueryPool timeQueryPool;

  VkmGlobalSet    globalSet;
//  VkDescriptorSet compNodeSet;

//  MidVkSharedMemory compNodeSetSharedMemory;
//  VkBuffer          compNodeSetBuffer;

  VkMesh quadMesh;

  VkFramebuffer framebuffer;
  VkFramebufferTexture framebuffers[MIDVK_SWAP_COUNT];

} MxcCompNode;

void  mxcCreateCompNode(const MxcCompNodeCreateInfo* pInfo, MxcCompNode* pNode);
void* mxcCompNodeThread(const MxcCompositorNodeContext* pNodeContext);
void  mxcBindUpdateCompNode(const MxcCompNodeCreateInfo* pInfo, MxcCompNode* pNode);
