#include "test_node.h"
#include "globals.h"
#include "renderer.h"

#include <vulkan/vk_enum_string_helper.h>

_Thread_local static struct {
  VkmTransform      cameraTransform;
  VkmGlobalSetState globalSetState;

  VkCommandBuffer       cmd;
  PFN_vkCmdBindPipeline vkCmdBindPipeline;

  int           framebufferIndex;
  VkFramebuffer framebuffers[VKM_SWAP_COUNT];

  VkRenderPass     standardRenderPass;
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

void mxcUpdateTestNode() {

  node.timeline.value++;
  vkmTimelineWait(node.device, &node.timeline);

  if (vkmProcessInput(&node.cameraTransform)) {
    vkmUpdateGlobalSetView(&node.cameraTransform, &node.globalSetState, node.pGlobalSetMapped);
  }

  vkmCmdResetBegin(node.cmd);

  vkmCmdBeginPass(node.cmd, node.standardRenderPass, node.framebuffers[node.framebufferIndex]);

  node.vkCmdBindPipeline(node.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, node.standardPipeline);
  vkCmdBindDescriptorSets(node.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, node.standardPipelineLayout, VKM_PIPE_SET_STD_GLOBAL_INDEX, 1, &node.globalSet, 0, NULL);
  vkCmdBindDescriptorSets(node.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, node.standardPipelineLayout, VKM_PIPE_SET_STD_MATERIAL_INDEX, 1, &node.checkerMaterialSet, 0, NULL);
  vkCmdBindDescriptorSets(node.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, node.standardPipelineLayout, VKM_PIPE_SET_STD_OBJECT_INDEX, 1, &node.sphereObjectSet, 0, NULL);

  vkCmdBindVertexBuffers(node.cmd, 0, 1, (const VkBuffer[]){node.sphereVertexBuffer}, (const VkDeviceSize[]){0});
  vkCmdBindIndexBuffer(node.cmd, node.sphereIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
  vkCmdDrawIndexed(node.cmd, node.sphereIndexCount, 1, 0, 0, 0);

  vkCmdEndRenderPass(node.cmd);

  {  // Blit Framebuffer
    vkAcquireNextImageKHR(node.device, node.swap.chain, UINT64_MAX, node.swap.acquireSemaphore, VK_NULL_HANDLE, &node.swap.swapIndex);
    const VkImage               framebufferColorImage = node.frameBufferColorImages[node.framebufferIndex];
    const VkImage               swapImage = node.swap.images[node.swap.swapIndex];
    const VkImageMemoryBarrier2 blitBarrier[] = {
        VKM_IMAGE_BARRIER(VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VKM_TRANSFER_SRC_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, framebufferColorImage),
        VKM_IMAGE_BARRIER(VKM_IMAGE_BARRIER_PRESENT, VKM_TRANSFER_DST_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, swapImage),
    };
    vkmCommandPipelineImageBarriers(node.cmd, 2, blitBarrier);
    vkmBlit(node.cmd, framebufferColorImage, swapImage);
    const VkImageMemoryBarrier2 presentBarrier[] = {
//        VKM_IMAGE_BARRIER(VKM_TRANSFER_SRC_IMAGE_BARRIER, VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, framebufferColorImage),
        VKM_IMAGE_BARRIER(VKM_TRANSFER_DST_IMAGE_BARRIER, VKM_IMAGE_BARRIER_PRESENT, VK_IMAGE_ASPECT_COLOR_BIT, swapImage),
    };
    vkmCommandPipelineImageBarriers(node.cmd, 1, presentBarrier);
  }

  vkEndCommandBuffer(node.cmd);
  vkmSubmitPresentCommandBuffer(node.cmd, node.graphicsQueue, &node.swap, &node.timeline);
  vkmTimelineWait(node.device, &node.timeline);

  node.framebufferIndex = !node.framebufferIndex;
}

_Thread_local static struct {
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

void mxcCreateTestNode(void* pArg) {
  const MxcTestNodeCreateInfo* pCreateInfo = (MxcTestNodeCreateInfo*)pArg;

  {  // Create
    VkBool32 presentSupport = VK_FALSE;
    VKM_REQUIRE(vkGetPhysicalDeviceSurfaceSupportKHR(context.physicalDevice, context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index, pCreateInfo->surface, &presentSupport));
    REQUIRE(presentSupport, "Queue can't present to surface!")

    vkGetDeviceQueue(context.device, context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index, 0, &node.graphicsQueue);
    const VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].pool,
        .commandBufferCount = 1,
    };
    VKM_REQUIRE(vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfo, &node.cmd));
    VkmSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)node.cmd, "TestNode");

    VkmCreateStandardRenderPass(&node.standardRenderPass);
    vkmCreateStandardFramebuffers(node.standardRenderPass, VKM_SWAP_COUNT, VKM_LOCALITY_EXTERNAL_PROCESS_SHARED, nodeContext.framebuffers);
    vkmCreateStandardPipeline(node.standardRenderPass, &nodeContext.standardPipe);
    VkmCreateSampler(&VKM_SAMPLER_LINEAR_CLAMP_DESC, &nodeContext.linearSampler);

    vkmAllocateDescriptorSet(context.descriptorPool, &nodeContext.standardPipe.globalSetLayout, &node.globalSet);
    vkmCreateAllocBindMapBuffer(VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(VkmGlobalSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VKM_LOCALITY_NODE_LOCAL, &nodeContext.globalSetMemory, &nodeContext.globalSetBuffer, (void**)&node.pGlobalSetMapped);

    vkmAllocateDescriptorSet(context.descriptorPool, &nodeContext.standardPipe.materialSetLayout, &node.checkerMaterialSet);
    vkmCreateTextureFromFile(context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].pool, node.graphicsQueue, "textures/uvgrid.jpg", &nodeContext.checkerTexture);

    vkmAllocateDescriptorSet(context.descriptorPool, &nodeContext.standardPipe.objectSetLayout, &node.sphereObjectSet);
    vkmCreateAllocBindMapBuffer(VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(VkmStandardObjectSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VKM_LOCALITY_NODE_LOCAL, &nodeContext.sphereObjectSetMemory, &nodeContext.sphereObjectSetBuffer, (void**)&nodeContext.pSphereObjectSetMapped);

    const VkWriteDescriptorSet writeSets[] = {
        VKM_SET_WRITE_STD_GLOBAL_BUFFER(node.globalSet, nodeContext.globalSetBuffer),
        VKM_SET_WRITE_STD_MATERIAL_IMAGE(node.checkerMaterialSet, nodeContext.checkerTexture.imageView, nodeContext.linearSampler),
        VKM_SET_WRITE_STD_OBJECT_BUFFER(node.sphereObjectSet, nodeContext.sphereObjectSetBuffer),
    };
    vkUpdateDescriptorSets(context.device, COUNT(writeSets), writeSets, 0, NULL);
    vkmUpdateGlobalSet(&node.cameraTransform, &node.globalSetState, node.pGlobalSetMapped);
    nodeContext.sphereTransform = pCreateInfo->transform;
    vkmUpdateObjectSet(&nodeContext.sphereTransform, &nodeContext.sphereObjectState, nodeContext.pSphereObjectSetMapped);

    vkmCreateSphereMesh(context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].pool, node.graphicsQueue, 0.5f, 32, 32, &nodeContext.sphereMesh);
    VkmCreateSwap(pCreateInfo->surface, &node.swap);
  }

  {  // Copy to cache-friendly loop struct
    node.standardPipelineLayout = nodeContext.standardPipe.pipelineLayout;
    node.standardPipeline = nodeContext.standardPipe.pipeline;
    for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
      node.framebuffers[i] = nodeContext.framebuffers[i].framebuffer;
      node.frameBufferColorImages[i] = nodeContext.framebuffers[i].color.image;
    }
    node.sphereIndexCount = nodeContext.sphereMesh.indexCount;
    node.sphereIndexBuffer = nodeContext.sphereMesh.indexBuffer;
    node.sphereVertexBuffer = nodeContext.sphereMesh.vertexBuffer;
    node.device = context.device;
    node.timeline.semaphore = context.timeline.semaphore;  // do not copy value as it may be above 0
  }

  {  // Initial State
    vkmCmdResetBegin(node.cmd);
    VkImageMemoryBarrier2 swapBarrier[VKM_SWAP_COUNT];
    for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
      swapBarrier[i] = VKM_IMAGE_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMAGE_BARRIER_PRESENT, VK_IMAGE_ASPECT_COLOR_BIT, node.swap.images[i]);
    }
    vkmCommandPipelineImageBarriers(node.cmd, VKM_SWAP_COUNT, swapBarrier);
    vkEndCommandBuffer(node.cmd);
    const VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &node.cmd,
    };
    VKM_REQUIRE(vkQueueSubmit(node.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
    VKM_REQUIRE(vkQueueWaitIdle(node.graphicsQueue));
  }

  {
    // should do this for all things repeatedly called...
    VKM_DEVICE_FUNC(vkCmdBindPipeline);
    node.vkCmdBindPipeline = vkCmdBindPipeline;
  }
}