#pragma once

#include "renderer.h"

typedef struct MxcCompNodeCreateInfo {
  VkSurfaceKHR surface;
} MxcCompNodeCreateInfo;


typedef struct MxcCompNode {
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

} MxcCompNode;


void mxcCreateCompNode(const MxcCompNodeCreateInfo* pCreateInfo, MxcCompNode* pTestNode);
void mxcRunTestNode(const MxcNodeContext* pNodeContext);
