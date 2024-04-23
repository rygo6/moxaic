#include "globals.h"
#include "renderer.h"

#include <vulkan/vk_enum_string_helper.h>

static struct {
  VkmTransform      cameraTransform;
  VkmGlobalSetState globalSetState;

  VkCommandBuffer cmd;
  VkRenderPass    pass;

  int           framebufferIndex;
  VkFramebuffer framebuffers[VKM_SWAP_COUNT];

  VkPipelineLayout standardPipelineLayout;
  VkPipeline       standardPipeline;

  VkmGlobalSetState* pGlobalSetMapped;
  VkDescriptorSet    globalSet;
  VkDescriptorSet    checkerMaterialSet;
  VkDescriptorSet    sphereObjectSet;

  uint32_t sphereIndexCount;
  VkBuffer sphereIndexBuffer, sphereVertexBuffer;

  VkDevice device;

  VkImage frameBufferColorImages[VKM_SWAP_COUNT];

  VkmSwap swap;
  VkImage swapImages[VKM_SWAP_COUNT];

  VkmTimeline timeline;
  VkQueue     graphicsQueue;

} node;

void mxcTestNodeUpdate() {
  if (vkmProcessInput(&node.cameraTransform)) {
    vkmUpdateGlobalSetView(&node.cameraTransform, &node.globalSetState, node.pGlobalSetMapped);
  }

  vkmCmdResetBegin(node.cmd);
  vkmCmdBeginPass(node.cmd, node.pass, node.framebuffers[node.framebufferIndex]);

  vkCmdBindPipeline(node.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, node.standardPipeline);
  vkCmdBindDescriptorSets(node.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, node.standardPipelineLayout, VKM_PIPE_SET_STD_GLOBAL_INDEX, 1, &node.globalSet, 0, NULL);
  vkCmdBindDescriptorSets(node.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, node.standardPipelineLayout, VKM_PIPE_SET_STD_MATERIAL_INDEX, 1, &node.checkerMaterialSet, 0, NULL);
  vkCmdBindDescriptorSets(node.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, node.standardPipelineLayout, VKM_PIPE_SET_STD_OBJECT_INDEX, 1, &node.sphereObjectSet, 0, NULL);

  vkCmdBindVertexBuffers(node.cmd, 0, 1, (const VkBuffer[]){node.sphereVertexBuffer}, (const VkDeviceSize[]){0});
  vkCmdBindIndexBuffer(node.cmd, node.sphereIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
  vkCmdDrawIndexed(node.cmd, node.sphereIndexCount, 1, 0, 0, 0);

  vkCmdEndRenderPass(node.cmd);

  vkAcquireNextImageKHR(node.device, node.swap.chain, UINT64_MAX, node.swap.acquireSemaphore, VK_NULL_HANDLE, &node.swap.index);

  {  // Blit Framebuffer
    const VkImage               framebufferColorImage = node.frameBufferColorImages[node.framebufferIndex];
    const VkImage               swapImage = node.swapImages[node.swap.index];
    const VkImageMemoryBarrier2 blitBarrier[] = {
        VKM_IMAGE_BARRIER(VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VKM_TRANSFER_SRC_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, framebufferColorImage),
        VKM_IMAGE_BARRIER(VKM_IMAGE_BARRIER_PRESENT, VKM_TRANSFER_DST_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, swapImage),
    };
    vkmCommandPipelineBarrier(node.cmd, 2, blitBarrier);
    vkmBlit(node.cmd, node.frameBufferColorImages[node.framebufferIndex], node.swapImages[node.swap.index]);
    const VkImageMemoryBarrier2 presentBarrier[] = {
        VKM_IMAGE_BARRIER(VKM_TRANSFER_SRC_IMAGE_BARRIER, VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, framebufferColorImage),
        VKM_IMAGE_BARRIER(VKM_TRANSFER_DST_IMAGE_BARRIER, VKM_IMAGE_BARRIER_PRESENT, VK_IMAGE_ASPECT_COLOR_BIT, swapImage),
    };
    vkmCommandPipelineBarrier(node.cmd, 2, presentBarrier);
  }

  vkEndCommandBuffer(node.cmd);
  vkmSubmitPresentCommandBuffer(node.cmd, node.graphicsQueue, &node.swap, &node.timeline);
  vkmTimelineWait(node.device, &node.timeline);
}

static struct {
  VkmFramebuffer framebuffers[VKM_SWAP_COUNT];
  VkmTexture     checkerTexture;
  VkmMesh        sphereMesh;

  VkDeviceMemory globalSetMemory;
  VkBuffer       globalSetBuffer;

  VkmStandardObjectSetState* pSphereObjectSetMapped;
  VkDeviceMemory             sphereObjectSetMemory;
  VkBuffer                   sphereObjectSetBuffer;

  VkmTransform              sphereTransform;
  VkmStandardObjectSetState sphereObjectState;

  VkmStandardPipe standardPipe;

} nodeInit;

void mxcTestNodeInit(const VkmContext* pContext) {
  VkmCreateFramebuffers(pContext->renderPass, VKM_SWAP_COUNT, nodeInit.framebuffers);

  VkmCreateSphereMesh(0.5f, 32, 32, &nodeInit.sphereMesh);

  VkmCreateStandardPipeline(pContext->renderPass, &nodeInit.standardPipe);

  VkmAllocateDescriptorSet(pContext->descriptorPool, &nodeInit.standardPipe.globalSetLayout, &node.globalSet);
  VkmCreateAllocateBindMapBuffer(VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(VkmGlobalSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &nodeInit.globalSetMemory, &nodeInit.globalSetBuffer, (void**)&node.pGlobalSetMapped);

  VkmAllocateDescriptorSet(pContext->descriptorPool, &nodeInit.standardPipe.materialSetLayout, &node.checkerMaterialSet);
  VkmCreateTextureFromFile("textures/uvgrid.jpg", &nodeInit.checkerTexture);

  VkmAllocateDescriptorSet(pContext->descriptorPool, &nodeInit.standardPipe.objectSetLayout, &node.sphereObjectSet);
  VkmCreateAllocateBindMapBuffer(VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(VkmStandardObjectSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &nodeInit.sphereObjectSetMemory, &nodeInit.sphereObjectSetBuffer, (void**)&nodeInit.pSphereObjectSetMapped);

  const VkWriteDescriptorSet writeSets[] = {
      VKM_SET_WRITE_STD_GLOBAL_BUFFER(node.globalSet, nodeInit.globalSetBuffer),
      VKM_SET_WRITE_STD_MATERIAL_IMAGE(node.checkerMaterialSet, nodeInit.checkerTexture.imageView, pContext->linearSampler),
      VKM_SET_WRITE_STD_OBJECT_BUFFER(node.sphereObjectSet, nodeInit.sphereObjectSetBuffer),
  };
  vkUpdateDescriptorSets(pContext->device, COUNT(writeSets), writeSets, 0, NULL);
  vkmUpdateGlobalSet(&node.cameraTransform, &node.globalSetState, node.pGlobalSetMapped);
  vkmUpdateObjectSet(&nodeInit.sphereTransform, &nodeInit.sphereObjectState, nodeInit.pSphereObjectSetMapped);

  node.cmd = pContext->graphicsCommandBuffer;
  node.pass = pContext->renderPass;
  node.standardPipelineLayout = nodeInit.standardPipe.pipelineLayout;
  node.standardPipeline = nodeInit.standardPipe.pipeline;
  for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
    node.framebuffers[i] = nodeInit.framebuffers[i].framebuffer;
    node.frameBufferColorImages[i] = nodeInit.framebuffers[i].colorImage;
    node.swapImages[i] = pContext->swapImages[i];
  }
  node.sphereIndexCount = nodeInit.sphereMesh.indexCount;
  node.sphereIndexBuffer = nodeInit.sphereMesh.indexBuffer;
  node.sphereVertexBuffer = nodeInit.sphereMesh.vertexBuffer;
  node.device = pContext->device;
  node.swap = pContext->swap;
  node.timeline = pContext->timeline;
  node.graphicsQueue = pContext->graphicsQueue;

  vkmCmdResetBegin(node.cmd);
  VkImageMemoryBarrier2 swapBarrier[VKM_SWAP_COUNT];
  for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
    swapBarrier[i] = VKM_IMAGE_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMAGE_BARRIER_PRESENT, VK_IMAGE_ASPECT_COLOR_BIT, node.swapImages[i]);
  }
  vkmCommandPipelineBarrier(node.cmd, VKM_SWAP_COUNT, swapBarrier);
  vkEndCommandBuffer(node.cmd);
  const VkSubmitInfo submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &node.cmd,
  };
  VKM_REQUIRE(vkQueueSubmit(node.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
  VKM_REQUIRE(vkQueueWaitIdle(node.graphicsQueue));
}