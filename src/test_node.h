#pragma once

#include "node.h"
#include "renderer.h"

//#define DEBUG_TEST_NODE_SWAP

typedef struct MxcTestNodeCreateInfo {
  //  VkmContext
  VkSurfaceKHR        surface;
  VkmTransform        transform;
  VkmNodeFramebuffer* pFramebuffers;
} MxcTestNodeCreateInfo;

typedef struct MxcTestNode {
  VkCommandPool pool;

  VkRenderPass     stdRenderPass;
  VkPipelineLayout stdPipelineLayout;
  VkPipeline       stdPipeline;

  VkmFramebuffer framebuffers[VKM_SWAP_COUNT];
  VkImageView gBufferMipViews[VKM_SWAP_COUNT][VKM_G_BUFFER_LEVELS];

  VkDescriptorSetLayout nodeProcessSetLayout;
  VkPipelineLayout      nodeProcessPipeLayout;
  VkPipeline            nodeProcessPipe;

  VkDevice device;

  VkCommandBuffer cmd;

  VkmGlobalSet globalSet;

  VkDescriptorSet checkerMaterialSet;
  VkDescriptorSet sphereObjectSet;

  VkmTexture checkerTexture;

  VkmMesh      sphereMesh;
  VkmTransform sphereTransform;

  VkmStdObjectSetState  sphereObjectState;
  VkmStdObjectSetState* pSphereObjectSetMapped;
  VkDeviceMemory        sphereObjectSetMemory;
  VkBuffer              sphereObjectSetBuffer;

  uint32_t queueIndex;

} MxcTestNode;

void mxcCreateTestNode(const MxcTestNodeCreateInfo* pCreateInfo, MxcTestNode* pTestNode);
void mxcRunTestNode(const MxcNodeContext* pNodeContext);
