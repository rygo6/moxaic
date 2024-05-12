#pragma once

#include "node.h"
#include "renderer.h"

typedef struct MxcTestNodeCreateInfo {
  //  VkmContext
  VkSurfaceKHR surface;
  VkmTransform transform;
  VkmNodeFramebuffer* pFramebuffers;
} MxcTestNodeCreateInfo;

typedef struct MxcTestNode {

  VkRenderPass     standardRenderPass;
  VkPipelineLayout standardPipelineLayout;
  VkPipeline       standardPipeline;

  VkDevice device;

  VkCommandBuffer cmd;

  VkDescriptorSet globalSet;
  VkDescriptorSet checkerMaterialSet;
  VkDescriptorSet sphereObjectSet;

  VkmTexture checkerTexture;

  VkmMesh      sphereMesh;
  VkmTransform sphereTransform;

  VkmStandardObjectSetState  sphereObjectState;
  VkmStandardObjectSetState* pSphereObjectSetMapped;
  VkDeviceMemory             sphereObjectSetMemory;
  VkBuffer                   sphereObjectSetBuffer;

  VkmFramebuffer framebuffers[VKM_SWAP_COUNT];

  VkmSwap swap;

  VkQueue graphicsQueue;

} MxcTestNode;

void mxcCreateTestNode(const MxcTestNodeCreateInfo* pCreateInfo, MxcTestNode* pTestNode);
void mxcRunTestNode(const MxcNodeContext* pNodeContext);
