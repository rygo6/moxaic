#pragma once

#include "node.h"
#include "renderer.h"

// we probably want to keep the comp node on its own thread so the main
// thread/context can be used for loading in content
// but the comp node can't make new things from context, main thread has to do that
// but it seems so much simpler if main thread is comp thread, or node process thread
// truth is context loading is side effort... it should be occasional thread

typedef struct MxcCompNodeCreateInfo {
  VkSurfaceKHR surface;
} MxcBasicCompCreateInfo;

typedef struct MxcCompNode {
//  VkCommandPool   pool;
  VkCommandBuffer cmd;

  VkRenderPass     standardRenderPass;
  VkPipelineLayout standardPipelineLayout;
  VkPipeline       standardPipeline;

  VkDevice device;

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

  VkSemaphore timeline;

} MxcBasicComp;


void mxcCreateBasicComp(const MxcBasicCompCreateInfo* pInfo, MxcBasicComp* pComp);
void mxcRunCompNode(const MxcBasicComp* pNode);
