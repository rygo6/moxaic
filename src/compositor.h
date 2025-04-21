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

//  VkDescriptorSetLayout computeNodeSetLayout;
  VkDescriptorSetLayout computeOutputSetLayout;
  VkPipelineLayout      computeNodePipeLayout;
  VkPipeline            computeNodePipe;
  VkPipeline            computePostNodePipe;

  VkDescriptorSetLayout gbufferProcessSetLayout;
  VkPipelineLayout      gbufferProcessPipeLayout;
  VkPipeline            gbufferProcessBlitUpPipe;

  GBufferProcessState*  pGBufferProcessMapped;
  VkSharedBuffer  		gBufferProcessSetBuffer;

  VkSharedBuffer  globalBuffer;
  VkDescriptorSet globalSet;

  VkMesh quadMesh;
  VkSharedMesh quadPatchMesh;

  VkFramebuffer             graphicsFramebuffer;
  VkBasicFramebufferTexture graphicsFramebufferTexture;

  VkDedicatedTexture computeFramebufferAtomicTexture;
  VkDedicatedTexture computeFramebufferColorTexture;
  VkDescriptorSet    computeOutputSet;

  VkQueryPool timeQueryPool;

} MxcCompositor;

void  mxcCreateCompositor(const MxcCompositorCreateInfo* pInfo, MxcCompositor* pCompositor);
void* mxcCompNodeThread(MxcCompositorContext* pContext);
void  mxcBindUpdateCompositor(const MxcCompositorCreateInfo* pInfo, MxcCompositor* pComp);
