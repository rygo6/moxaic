#include "globals.h"
#include "renderer.h"

#include <vulkan/vk_enum_string_helper.h>

static struct {
  VkmTransform      cameraTransform;
  VkmGlobalSetState globalSetState;

  VkCommandBuffer cmd;
  VkRenderPass    renderPass;

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

  VkmTimeline timeline;
  VkQueue     graphicsQueue;

} node;

void mxcTestNodeUpdate() {
  if (vkmProcessInput(&node.cameraTransform)) {
    vkmUpdateGlobalSetView(&node.cameraTransform, &node.globalSetState, node.pGlobalSetMapped);
  }

  vkAcquireNextImageKHR(node.device, node.swap.chain, UINT64_MAX, node.swap.acquireSemaphore, VK_NULL_HANDLE, &node.swap.swapIndex);

  vkmCmdResetBegin(node.cmd);
  vkmCmdBeginPass(node.cmd, node.renderPass, node.framebuffers[node.framebufferIndex]);

  vkCmdBindPipeline(node.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, node.standardPipeline);
  vkCmdBindDescriptorSets(node.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, node.standardPipelineLayout, VKM_PIPE_SET_STD_GLOBAL_INDEX, 1, &node.globalSet, 0, NULL);
  vkCmdBindDescriptorSets(node.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, node.standardPipelineLayout, VKM_PIPE_SET_STD_MATERIAL_INDEX, 1, &node.checkerMaterialSet, 0, NULL);
  vkCmdBindDescriptorSets(node.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, node.standardPipelineLayout, VKM_PIPE_SET_STD_OBJECT_INDEX, 1, &node.sphereObjectSet, 0, NULL);

  vkCmdBindVertexBuffers(node.cmd, 0, 1, (const VkBuffer[]){node.sphereVertexBuffer}, (const VkDeviceSize[]){0});
  vkCmdBindIndexBuffer(node.cmd, node.sphereIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
  vkCmdDrawIndexed(node.cmd, node.sphereIndexCount, 1, 0, 0, 0);

  vkCmdEndRenderPass(node.cmd);

  {  // Blit Framebuffer
    const VkImage               framebufferColorImage = node.frameBufferColorImages[node.framebufferIndex];
    const VkImage               swapImage = node.swap.images[node.swap.swapIndex];
    const VkImageMemoryBarrier2 blitBarrier[] = {
        VKM_IMAGE_BARRIER(VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VKM_TRANSFER_SRC_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, framebufferColorImage),
        VKM_IMAGE_BARRIER(VKM_IMAGE_BARRIER_PRESENT, VKM_TRANSFER_DST_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, swapImage),
    };
    vkmCommandPipelineImageBarrier(node.cmd, 2, blitBarrier);
    vkmBlit(node.cmd, node.frameBufferColorImages[node.framebufferIndex], node.swap.images[node.swap.swapIndex]);
    const VkImageMemoryBarrier2 presentBarrier[] = {
        VKM_IMAGE_BARRIER(VKM_TRANSFER_SRC_IMAGE_BARRIER, VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, framebufferColorImage),
        VKM_IMAGE_BARRIER(VKM_TRANSFER_DST_IMAGE_BARRIER, VKM_IMAGE_BARRIER_PRESENT, VK_IMAGE_ASPECT_COLOR_BIT, swapImage),
    };
    vkmCommandPipelineImageBarrier(node.cmd, 2, presentBarrier);
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

  VkSampler linearSampler;

} nodeContext;

void mxcCreateTestNodeContext() {

  { // Create
    VkmCreateStandardRenderPass(&node.renderPass);
    VkmCreateFramebuffers(node.renderPass, VKM_SWAP_COUNT, nodeContext.framebuffers);
    VkmCreateStandardPipeline(node.renderPass, &nodeContext.standardPipe);
    VkmContextCreateSampler(&VKM_SAMPLER_LINEAR_CLAMP_DESC, &nodeContext.linearSampler);

    VkmAllocateDescriptorSet(context.descriptorPool, &nodeContext.standardPipe.globalSetLayout, &node.globalSet);
    VkmCreateAllocateBindMapBuffer(VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(VkmGlobalSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &nodeContext.globalSetMemory, &nodeContext.globalSetBuffer, (void**)&node.pGlobalSetMapped);

    VkmAllocateDescriptorSet(context.descriptorPool, &nodeContext.standardPipe.materialSetLayout, &node.checkerMaterialSet);
    VkmCreateTextureFromFile(&context.graphicsCommand, "textures/uvgrid.jpg", &nodeContext.checkerTexture);

    VkmAllocateDescriptorSet(context.descriptorPool, &nodeContext.standardPipe.objectSetLayout, &node.sphereObjectSet);
    VkmCreateAllocateBindMapBuffer(VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(VkmStandardObjectSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &nodeContext.sphereObjectSetMemory, &nodeContext.sphereObjectSetBuffer, (void**)&nodeContext.pSphereObjectSetMapped);

    const VkWriteDescriptorSet writeSets[] = {
        VKM_SET_WRITE_STD_GLOBAL_BUFFER(node.globalSet, nodeContext.globalSetBuffer),
        VKM_SET_WRITE_STD_MATERIAL_IMAGE(node.checkerMaterialSet, nodeContext.checkerTexture.imageView, nodeContext.linearSampler),
        VKM_SET_WRITE_STD_OBJECT_BUFFER(node.sphereObjectSet, nodeContext.sphereObjectSetBuffer),
    };
    vkUpdateDescriptorSets(context.device, COUNT(writeSets), writeSets, 0, NULL);
    vkmUpdateGlobalSet(&node.cameraTransform, &node.globalSetState, node.pGlobalSetMapped);
    vkmUpdateObjectSet(&nodeContext.sphereTransform, &nodeContext.sphereObjectState, nodeContext.pSphereObjectSetMapped);

    VkmCreateSphereMesh(&context.graphicsCommand, 0.5f, 32, 32, &nodeContext.sphereMesh);
    VkmContextCreateSwap(&node.swap);
  }

  { // Copy to cache-friendly loop queue
    node.cmd = context.graphicsCommand.buffer;
    node.standardPipelineLayout = nodeContext.standardPipe.pipelineLayout;
    node.standardPipeline = nodeContext.standardPipe.pipeline;
    for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
      node.framebuffers[i] = nodeContext.framebuffers[i].framebuffer;
      node.frameBufferColorImages[i] = nodeContext.framebuffers[i].colorImage;
    }
    node.sphereIndexCount = nodeContext.sphereMesh.indexCount;
    node.sphereIndexBuffer = nodeContext.sphereMesh.indexBuffer;
    node.sphereVertexBuffer = nodeContext.sphereMesh.vertexBuffer;
    node.device = context.device;
    node.timeline = context.timeline;
    node.graphicsQueue = context.graphicsCommand.queue;
  }

  { // Initial State
    vkmCmdResetBegin(node.cmd);
    VkImageMemoryBarrier2 swapBarrier[VKM_SWAP_COUNT];
    for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
      swapBarrier[i] = VKM_IMAGE_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMAGE_BARRIER_PRESENT, VK_IMAGE_ASPECT_COLOR_BIT, node.swap.images[i]);
    }
    vkmCommandPipelineImageBarrier(node.cmd, VKM_SWAP_COUNT, swapBarrier);
    vkEndCommandBuffer(node.cmd);
    const VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &node.cmd,
    };
    VKM_REQUIRE(vkQueueSubmit(node.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
    VKM_REQUIRE(vkQueueWaitIdle(node.graphicsQueue));
  }
}