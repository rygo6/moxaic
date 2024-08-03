#pragma once

#include "mid_vulkan.h"
#include "node.h"

typedef struct MxcTestNodeCreateInfo {
  VkmTransform        transform;
  MxcNodeFramebufferTexture* pFramebuffers;
} MxcTestNodeCreateInfo;

typedef struct MxcTestNode {
  VkRenderPass     nodeRenderPass;
  VkPipelineLayout stdPipeLayout;
  VkPipeline       basicPipe;

  VkFramebuffer             framebuffer;
  MxcNodeFramebufferTexture framebufferTextures[MIDVK_SWAP_COUNT];
  VkImageView               gBufferMipViews[MIDVK_SWAP_COUNT][VKM_G_BUFFER_LEVELS];

  VkDescriptorSetLayout nodeProcessSetLayout;
  VkPipelineLayout      nodeProcessPipeLayout;
  VkPipeline            nodeProcessBlitMipAveragePipe;
  VkPipeline            nodeProcessBlitDownPipe;

  VkDevice device;

  VkmGlobalSet globalSet;

  VkDescriptorSet checkerMaterialSet;
  VkDescriptorSet sphereObjectSet;

  MidVkTexture checkerTexture;

  VkmMesh      sphereMesh;
  VkmTransform sphereTransform;

  VkmStdObjectSetState  sphereObjectState;
  VkmStdObjectSetState* pSphereObjectSetMapped;
  VkDeviceMemory        sphereObjectSetMemory;
  VkBuffer              sphereObjectSetBuffer;

  uint32_t queueIndex;

} MxcTestNode;

void  mxcCreateTestNode(const MxcTestNodeCreateInfo* pCreateInfo, MxcTestNode* pTestNode);
void* mxcTestNodeThread(const MxcNode* pNodeContext);
