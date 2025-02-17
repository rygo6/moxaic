#pragma once

#include "node.h"
#include "mid_vulkan.h"

typedef enum MxcCompositorMode {
  MXC_COMP_MODE_BASIC,
  MXC_COMP_MODE_TESS,
  MXC_COMP_MODE_TASK_MESH,
  MXC_COMP_MODE_COMPUTE,
  MXC_COMP_MODE_COUNT,
} MxcCompositorMode;

typedef struct MxcCompositorCreateInfo {
	MxcCompositorMode mode;
} MxcCompositorCreateInfo;

typedef struct MxcCompositor {
	MxcCompositorMode compMode;

  // device should go in context
  // todo no shjould just access it from context directly
  VkDevice        device;

  VkRenderPass compRenderPass;

  VkDescriptorSetLayout compositorNodeSetLayout;
  VkPipelineLayout      compositorPipeLayout;
  VkPipeline            compNodePipe;

  VkDescriptorSetLayout gbufferProcessSetLayout;
  VkPipelineLayout      gbufferProcessPipeLayout;
  VkPipeline            gbufferProcessBlitUpPipe;

  VkQueryPool timeQueryPool;

  VkGlobalSet globalSet;
//  VkDescriptorSet compNodeSet;

//  MidVkSharedMemory compNodeSetSharedMemory;
//  VkBuffer          compNodeSetBuffer;

  VkMesh quadMesh;

  VkFramebuffer framebuffer;
  VkBasicFramebufferTexture framebuffers[VK_SWAP_COUNT];

} MxcCompositor;

void  mxcCreateCompositor(const MxcCompositorCreateInfo* pInfo, MxcCompositor* pNode);
void* mxcCompNodeThread(MxcCompositorContext* pNodeContext);
void  mxcBindUpdateCompositor(const MxcCompositorCreateInfo* pInfo, MxcCompositor* pNode);
