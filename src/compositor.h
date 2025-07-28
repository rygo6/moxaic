#pragma once

#include "node.h"
#include "mid_vulkan.h"

#define LINE_BUFFER_CAPACITY 128

typedef struct VkSharedLineBuffer {
	VkSharedBuffer buffer;
	int            count;
	vec3*          pMapped;
	vec3           state[LINE_BUFFER_CAPACITY];
} VkSharedLineBuffer;

typedef struct MxcCompositorCreateInfo {
	bool pEnabledCompositorModes[MXC_COMPOSITOR_MODE_COUNT];
} MxcCompositorCreateInfo;

typedef struct MxcCompositor {

  VkDescriptorSetLayout nodeSetLayout;
  VkPipelineLayout      nodePipeLayout;
  VkPipeline            nodeQuadPipe;
  VkPipeline            nodeTessPipe;
  VkPipeline            nodeTaskMeshPipe;

  VkDescriptorSetLayout compOutputSetLayout;
  VkPipelineLayout      nodeCompPipeLayout;
  VkPipeline            nodeCompPipe;
  VkPipeline            nodePostCompPipe;

  VkDescriptorSetLayout gbufProcessSetLayout;
  VkPipelineLayout      gbufProcessPipeLayout;
  VkPipeline            gbufProcessBlitUpPipe;

  VkDescriptorSetLayout finalBlitSetLayout;
  VkPipelineLayout      finalBlitPipeLayout;
  VkPipeline            finalBlitPipe;

  MxcProcessState*      pProcessStateMapped;
  VkSharedBuffer        processSetBuffer;

  VkSharedBuffer  globalBuffer;
  VkDescriptorSet globalSet;

  VkMesh quadMesh;
  VkSharedMesh quadPatchMesh;

  VkDepthFramebufferTexture framebufferTexture;

  VkDedicatedTexture compFrameAtomicTex;
  VkDedicatedTexture compFrameColorTex;
  VkDescriptorSet    compOutSet;

  VkQueryPool timeQryPool;

  VkSharedLineBuffer lineBuf;

} MxcCompositor;

void* mxcCompNodeThread(MxcCompositorContext* pCtx);
