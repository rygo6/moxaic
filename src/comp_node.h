#pragma once

#include "node.h"
#include "mid_vulkan.h"

typedef enum MxcCompositorMode {
	MXC_COMP_MODE_BASIC,
	MXC_COMP_MODE_TESSELATION,
	MXC_COMP_MODE_TASK_MESH,
	MXC_COMP_MODE_COMPUTE,
	MXC_COMP_MODE_COUNT,
} MxcCompositorMode;

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

  VkDescriptorSetLayout compositorNodeSetLayout;
  VkPipelineLayout      compositorPipeLayout;
  VkPipeline            compNodePipe;

  VkDescriptorSetLayout gbufferProcessSetLayout;
  VkPipelineLayout      gbufferProcessPipeLayout;
  VkPipeline            gbufferProcessBlitUpPipe;

  GBufferProcessState*  pGBufferProcessMapped;
  VkBuffer              gBufferProcessSetBuffer;
  VkSharedMemory        gBufferProcessSetSharedMemory;

  VkGlobalSet globalSet;

  VkMesh quadMesh;

  VkFramebuffer framebuffer;
  VkBasicFramebufferTexture framebuffers[VK_SWAP_COUNT];

  VkQueryPool timeQueryPool;

} MxcCompositor;

void  mxcCreateCompositor(const MxcCompositorCreateInfo* pInfo, MxcCompositor* pComp);
void* mxcCompNodeThread(MxcCompositorContext* pContext);
void  mxcBindUpdateCompositor(const MxcCompositorCreateInfo* pInfo, MxcCompositor* pComp);
