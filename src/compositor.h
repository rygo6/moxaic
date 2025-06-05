#pragma once

#include "node.h"
#include "mid_vulkan.h"

#define LINE_BUFFER_CAPACITY 128

typedef struct VkSharedLine {
	VkSharedBuffer buffer;
	int            count;
	vec3*          pMapped;
	vec3           state[LINE_BUFFER_CAPACITY];
} VkSharedLine;

typedef struct MxcCompositorCreateInfo {
	MxcCompositorMode mode;
} MxcCompositorCreateInfo;

typedef struct MxcCompositor {

  VkDescriptorSetLayout nodeSetLayout;
  VkPipelineLayout      nodePipeLayout;
  VkPipeline            nodeQuadPipe;
  VkPipeline            nodeTessPipe;
  VkPipeline            nodeTaskMeshPipe;

//  VkDescriptorSetLayout computeNodeSetLayout;
  VkDescriptorSetLayout computeOutputSetLayout;
  VkPipelineLayout      computeNodePipeLayout;
  VkPipeline            computeNodePipe;
  VkPipeline            computePostNodePipe;

  VkDescriptorSetLayout gbufferProcessSetLayout;
  VkPipelineLayout      gbufferProcessPipeLayout;
  VkPipeline            gbufferProcessBlitUpPipe;

  VkDescriptorSetLayout finalBlitSetLayout;
  VkPipelineLayout      finalBlitPipeLayout;
  VkPipeline            finalBlitPipe;

  MxcProcessState*      pProcessStateMapped;
  VkSharedBuffer        processSetBuffer;

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

  VkSharedLine line;

} MxcCompositor;

INLINE void vkLineClear(VkSharedLine* pLine)
{
	pLine->count = 0;
}

INLINE void vkLineAdd(VkSharedLine* pLine, vec3 start, vec3 end)
{
	pLine->state[pLine->count++] = start;
	pLine->state[pLine->count++] = end;
}

void  mxcCreateCompositor(const MxcCompositorCreateInfo* pInfo, MxcCompositor* pCompositor);
void* mxcCompNodeThread(MxcCompositorContext* pContext);
void  mxcBindUpdateCompositor(const MxcCompositorCreateInfo* pInfo, MxcCompositor* pComp);
