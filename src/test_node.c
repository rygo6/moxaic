#include "renderer.h"
#include "window.h"
#include "globals.h"

#include <vulkan/vk_enum_string_helper.h>

#define CACHE_ALIGN __attribute((aligned(64)))

static struct {
  Transform      cameraTransform;
  GlobalSetState globalSetState;

  VkCommandBuffer cmd;

  int           framebufferIndex;
  VkFramebuffer framebuffers[VK_SWAP_COUNT];

  VkPipelineLayout standardPipelineLayout;
  VkPipeline       standardPipeline;

  GlobalSetState* pGlobalSetMapped;
  VkDescriptorSet globalSet;
  VkDescriptorSet checkerMaterialSet;
  VkDescriptorSet sphereObjectSet;

  uint32_t sphereIndexCount;
  VkBuffer sphereIndexBuffer, sphereVertexBuffer;

  VkDevice device;

  VkSwapchainKHR swapchain;
  VkImage        swapImages[VK_SWAP_COUNT];
  VkSemaphore    acquireSwapSemaphore;
  VkSemaphore    renderCompleteSwapSemaphore;

  VkImage frameBufferColorImages[VK_SWAP_COUNT];

  VkQueue         graphicsQueue;

  VkSemaphore timelineSemaphore;
  uint64_t    timelineWaitValue;

} CACHE_ALIGN node;

void TestNodeInit() {
//  CreateFramebuffers(VK_SWAP_COUNT, nodeInit.framebuffers);
//
//  CreateSphereMeshBuffers(0.5f, 32, 32, &nodeInit.sphereMesh);
//
//  AllocateDescriptorSet(&context.globalSetLayout, &node.globalSet);
//  CreateAllocateBindMapBuffer(MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(GlobalSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &nodeInit.globalSetMemory, &nodeInit.globalSetBuffer, (void**)&node.pGlobalSetMapped);
//
//  AllocateDescriptorSet(&context.materialSetLayout, &node.checkerMaterialSet);
//  CreateTextureFromFile("textures/uvgrid.jpg", &nodeInit.checkerTexture);
//
//  AllocateDescriptorSet(&context.objectSetLayout, &node.sphereObjectSet);
//  CreateAllocateBindMapBuffer(MEMORY_HOST_VISIBLE_COHERENT, sizeof(StandardObjectSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &nodeInit.sphereObjectSetMemory, &nodeInit.sphereObjectSetBuffer, (void**)&nodeInit.pSphereObjectSetMapped);
//
//  const VkWriteDescriptorSet writeSets[] = {
//      SET_WRITE_GLOBAL(node.globalSet, nodeInit.globalSetBuffer),
//      SET_WRITE_STANDARD_MATERIAL(node.checkerMaterialSet, nodeInit.checkerTexture.imageView),
//      SET_WRITE_STANDARD_OBJECT(node.sphereObjectSet, nodeInit.sphereObjectSetBuffer),
//  };
//  vkUpdateDescriptorSets(context.device, COUNT(writeSets), writeSets, 0, NULL);
//  UpdateGlobalSet(&node.cameraTransform, &node.globalSetState, node.pGlobalSetMapped);
//  UpdateObjectSet(&nodeInit.sphereTransform, &nodeInit.sphereObjectState, nodeInit.pSphereObjectSetMapped);
//
//  node.cmd = context.graphicsCommandBuffer;
//
//  node.standardPipelineLayout = context.standardPipelineLayout;
//  node.standardPipeline = context.standardPipeline;
//
//  for (int i = 0; i < VK_SWAP_COUNT; ++i) {
//    node.framebuffers[i] = nodeInit.framebuffers[i].framebuffer;
//    node.frameBufferColorImages[i] = nodeInit.framebuffers[i].colorImage;
//  }
//
//  node.sphereIndexCount = nodeInit.sphereMesh.indexCount;
//  node.sphereIndexBuffer = nodeInit.sphereMesh.indexBuffer;
//  node.sphereVertexBuffer = nodeInit.sphereMesh.vertexBuffer;
}

void TestNodeUpdate() {
//  mxUpdateWindowInput();
//
//  if (ProcessInput(&node.cameraTransform)) {
//    UpdateGlobalSetView(&node.cameraTransform, &node.globalSetState, node.pGlobalSetMapped);
//  }
//  ResetBeginCommandBuffer(node.cmd);
//  BeginRenderPass(node.framebuffers[node.framebufferIndex]);
//
//  vkCmdBindPipeline(node.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, node.standardPipeline);
//  vkCmdBindDescriptorSets(node.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, node.standardPipelineLayout, PIPE_SET_BINDING_STANDARD_GLOBAL, 1, &node.globalSet, 0, NULL);
//  vkCmdBindDescriptorSets(node.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, node.standardPipelineLayout, PIPE_SET_BINDING_STANDARD_MATERIAL, 1, &node.checkerMaterialSet, 0, NULL);
//  vkCmdBindDescriptorSets(node.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, node.standardPipelineLayout, PIPE_SET_BINDING_STANDARD_OBJECT, 1, &node.sphereObjectSet, 0, NULL);
//
//  vkCmdBindVertexBuffers(node.cmd, 0, 1, (VkBuffer[]){node.sphereVertexBuffer}, (VkDeviceSize[]){0});
//  vkCmdBindIndexBuffer(node.cmd, node.sphereIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
//  vkCmdDrawIndexed(node.cmd, node.sphereIndexCount, 1, 0, 0, 0);
//
//  vkCmdEndRenderPass(node.cmd);
//
//  uint32_t swapIndex;
//  vkAcquireNextImageKHR(node.device, node.swapchain, UINT64_MAX, node.acquireSwapSemaphore, VK_NULL_HANDLE, &swapIndex);
//
//  Blit(node.cmd, node.frameBufferColorImages[node.framebufferIndex], node.swapImages[swapIndex]);
//
//  vkEndCommandBuffer(node.cmd);
//
//  SubmitPresentCommandBuffer(node.cmd, swapIndex, node.graphicsQueue);
//
//  const VkSemaphoreWaitInfo semaphoreWaitInfo = {
//      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
//      .semaphoreCount = 1,
//      .pSemaphores = &node.timelineSemaphore,
//      .pValues = &node.timelineWaitValue,
//  };
//  VK_REQUIRE(vkWaitSemaphores(node.device, &semaphoreWaitInfo, UINT64_MAX));
}