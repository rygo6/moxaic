#pragma once

#include "node.h"
#include "mid_vulkan.h"

typedef struct MxcTestNode {
  VkRenderPass     nodeRenderPass;
  VkPipelineLayout pipeLayout;
  VkPipeline       basicPipe;

  VkFramebuffer             framebuffer;
  VkImageView               gBufferMipViews[MIDVK_SWAP_COUNT][MXC_NODE_GBUFFER_LEVELS];

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
  MidPose      sphereTransform;

  VkmStdObjectSetState  sphereObjectState;
  VkmStdObjectSetState* pSphereObjectSetMapped;
  VkDeviceMemory        sphereObjectSetMemory;
  VkBuffer              sphereObjectSetBuffer;

} MxcTestNode;

void* mxcTestNodeThread(const MxcNodeContext* pNodeContext);
