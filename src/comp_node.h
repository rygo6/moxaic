#pragma once

#include "renderer.h"

typedef struct MxcCompNodeCreateInfo {
  VkSurfaceKHR surface;
} MxcCompNodeCreateInfo;


_Thread_local static struct {
//  VkmTransform      cameraTransform;
//  VkmGlobalSetState globalSetState;
//
//  VkCommandBuffer       cmd;
//
//  int           framebufferIndex;
//  VkFramebuffer framebuffers[VKM_SWAP_COUNT];
//
//  VkRenderPass     standardRenderPass;
//  VkPipelineLayout standardPipelineLayout;
//  VkPipeline       standardPipeline;
//
//  VkmGlobalSetState* pGlobalSetMapped;
//  VkDescriptorSet    globalSet;
//  VkDescriptorSet    checkerMaterialSet;
//  VkDescriptorSet    sphereObjectSet;
//
//  uint32_t sphereIndexCount;
//  VkBuffer sphereIndexBuffer, sphereVertexBuffer;
//
//  VkDevice device;
//
//  VkImage frameBufferColorImages[VKM_SWAP_COUNT];
//
//  VkmSwap swap;
//
//  VkmTimeline timeline;
//  VkQueue     graphicsQueue;

} local;

void mxcUpdateCompNode() {
}

_Thread_local static struct {
  VkmFramebuffer framebuffers[VKM_SWAP_COUNT];
  VkmTexture     checkerTexture;
  VkmMesh        sphereMesh;

  VkmStandardObjectSetState* pSphereObjectSetMapped;
  VkDeviceMemory             sphereObjectSetMemory;
  VkBuffer                   sphereObjectSetBuffer;

  VkmTransform              sphereTransform;
  VkmStandardObjectSetState sphereObjectState;

} nodeContext;

void mxcCreateCompNode(void* pArg) {

  vkmCreateStandardFramebuffers(context.standardRenderPass, VKM_SWAP_COUNT, VKM_LOCALITY_NODE_LOCAL, nodeContext.framebuffers);

}
