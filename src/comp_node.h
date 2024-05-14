#pragma once

#include "node.h"
#include "renderer.h"

typedef struct MxcCompNodeCreateInfo {
  VkSurfaceKHR    surface;
} MxcCompNodeCreateInfo;

typedef struct MxcCompNode {
  VkCommandPool pool;

  VkRenderPass     standardRenderPass;
  VkPipelineLayout standardPipelineLayout;
  VkPipeline       standardPipeline;

  VkSampler       sampler;

  VkDevice device;

  VkCommandBuffer cmd;

  VkDescriptorSet globalSet;
  VkDescriptorSet checkerMaterialSet;
  VkDescriptorSet sphereObjectSet;

  VkmMesh      sphereMesh;
  VkmTransform sphereTransform;

  VkmStandardObjectSetState  sphereObjectState;
  VkmStandardObjectSetState* pSphereObjectSetMapped;
  VkDeviceMemory             sphereObjectSetMemory;
  VkBuffer                   sphereObjectSetBuffer;

  VkmFramebuffer framebuffers[VKM_SWAP_COUNT];

  VkmSwap swap;

  VkQueue graphicsQueue;

} MxcCompNode;


void mxcCreateCompNode(const MxcCompNodeCreateInfo* pCreateInfo, MxcCompNode* pTestNode);
void mxcRunCompNode(const MxcNodeContext* pNodeContext);
