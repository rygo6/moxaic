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
	MxcCompositorMode compMode;
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

  VkSharedBuffer  globalBuf;
  VkDescriptorSet globalSet;

  VkMesh quadMesh;
  VkSharedMesh quadPatchMesh;

  VkFramebuffer             gfxFrame;
  VkBasicFramebufferTexture gfxFrameTex;

  VkDedicatedTexture compFrameAtomicTex;
  VkDedicatedTexture compFrameColorTex;
  VkDescriptorSet    compOutSet;

  VkQueryPool timeQryPool;

  VkSharedLineBuffer lineBuf;

} MxcCompositor;

//INLINE void vkLineClear(VkSharedLineBuffer* pLine)
//{
//	pLine->count = 0;
//}

//INLINE void vkLineAdd(VkSharedLineBuffer* pLine, vec3 start, vec3 end)
//{
//	pLine->state[pLine->count++] = start;
//	pLine->state[pLine->count++] = end;
//}

void  mxcCreateCompositor(const MxcCompositorCreateInfo* pInfo, MxcCompositor* pCst);
void* mxcCompNodeThread(MxcCompositorContext* pContext);
void  mxcBindUpdateCompositor(const MxcCompositorCreateInfo* pInfo, MxcCompositor* pCst);
