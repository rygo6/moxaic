#pragma once

#include "node.h"
#include "mid_vulkan.h"

typedef struct GBufferProcessState {
	MxcDepthState depth;
	float cameraNearZ;
	float cameraFarZ;
} GBufferProcessState;

typedef struct MxcCompositorCreateInfo {
	MxcCompositorMode mode;
} MxcCompositorCreateInfo;

typedef struct MxcCompositor {
  // device should go in context
  // todo no should just access it from context directly
  VkDevice        device;

  VkRenderPass compRenderPass;

  VkDescriptorSetLayout nodeSetLayout;
  VkPipelineLayout      nodePipeLayout;
  VkPipeline            nodePipe;

  VkDescriptorSetLayout computeNodeSetLayout;
  VkDescriptorSetLayout computeOutputSetLayout;
  VkPipelineLayout      computeNodePipeLayout;
  VkPipeline            computeNodePipe;

  VkDescriptorSetLayout gbufferProcessSetLayout;
  VkPipelineLayout      gbufferProcessPipeLayout;
  VkPipeline            gbufferProcessBlitUpPipe;

  GBufferProcessState*  pGBufferProcessMapped;
  VkSharedBuffer  		gBufferProcessSetBuffer;

  VkSharedDescriptorSet globalSet;

  VkMesh quadMesh;
  VkSharedMesh quadPatchMesh;

  VkFramebuffer framebuffer;
  VkBasicFramebufferTexture framebuffers[VK_SWAP_COUNT];

  VkQueryPool timeQueryPool;

} MxcCompositor;

void  mxcCreateCompositor(const MxcCompositorCreateInfo* pInfo, MxcCompositor* pComp);
void* mxcCompNodeThread(MxcCompositorContext* pContext);
void  mxcBindUpdateCompositor(const MxcCompositorCreateInfo* pInfo, MxcCompositor* pComp);
