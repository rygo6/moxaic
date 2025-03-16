#pragma once

#include "node.h"
#include "mid_vulkan.h"

typedef enum MxcCompositorMode {
	MXC_COMPOSITOR_MODE_BASIC,
	MXC_COMPOSITOR_MODE_TESSELATION,
	MXC_COMPOSITOR_MODE_TASK_MESH,
	MXC_COMPOSITOR_MODE_COMPUTE,
	MXC_COMPOSITOR_MODE_COUNT,
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

  VkDescriptorSetLayout nodeSetLayout;
  VkPipelineLayout      nodePipeLayout;
  VkPipeline            nodePipe;
  VkPipeline            computeNodePipe;

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
