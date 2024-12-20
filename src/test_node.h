#pragma once

#include "mid_vulkan.h"

#include "node.h"

typedef struct MxcTestNode {
  VkRenderPass     nodeRenderPass;
  VkPipelineLayout pipeLayout;
  VkPipeline       basicPipe;

  VkFramebuffer             framebuffer;
  VkImageView               gBufferMipViews[VK_SWAP_COUNT][MXC_NODE_GBUFFER_LEVELS];

  VkDescriptorSetLayout nodeProcessSetLayout;
  VkPipelineLayout      nodeProcessPipeLayout;
  VkPipeline            nodeProcessBlitMipAveragePipe;
  VkPipeline            nodeProcessBlitDownPipe;

  VkDevice device;

  VkmGlobalSet globalSet;

  VkDescriptorSet checkerMaterialSet;
  VkDescriptorSet sphereObjectSet;

  VkDedicatedTexture checkerTexture;

  VkMesh       sphereMesh;
  MidPose      sphereTransform;

  VkmStdObjectSetState  sphereObjectState;
  VkmStdObjectSetState* pSphereObjectSetMapped;
  VkDeviceMemory        sphereObjectSetMemory;
  VkBuffer              sphereObjectSetBuffer;

} MxcTestNode;

void* mxcTestNodeThread(const MxcNodeContext* pNodeContext);
