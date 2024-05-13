#include "comp_node.h"

#include <vulkan/vk_enum_string_helper.h>

void mxcCreateCompNode(const MxcCompNodeCreateInfo* pCreateInfo, MxcCompNode* pTestNode) {
  {  // Create
    const VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
    };
    VKM_REQUIRE(vkCreateCommandPool(context.device, &graphicsCommandPoolCreateInfo, VKM_ALLOC, &pTestNode->pool));

    vkGetDeviceQueue(context.device, context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index, 1, &pTestNode->graphicsQueue);
    const VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pTestNode->pool,
        .commandBufferCount = 1,
    };
    VKM_REQUIRE(vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfo, &pTestNode->cmd));
    VkmSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)pTestNode->cmd, "CompNode");

    vkmCreateStandardFramebuffers(context.standardRenderPass, VKM_SWAP_COUNT, VKM_LOCALITY_PROCESS_EXPORTED, pTestNode->framebuffers);

    vkmAllocateDescriptorSet(context.descriptorPool, &context.standardPipe.materialSetLayout, &pTestNode->checkerMaterialSet);
    VkmSetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)pTestNode->checkerMaterialSet, "CompCheckerMaterialSet");

    vkmAllocateDescriptorSet(context.descriptorPool, &context.standardPipe.objectSetLayout, &pTestNode->sphereObjectSet);
    VkmSetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)pTestNode->sphereObjectSet, "CompSphereObjectSet");
    vkmCreateAllocBindMapBuffer(VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(VkmStandardObjectSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VKM_LOCALITY_CONTEXT, &pTestNode->sphereObjectSetMemory, &pTestNode->sphereObjectSetBuffer, (void**)&pTestNode->pSphereObjectSetMapped);

    vkmUpdateDescriptorSet(context.device, &VKM_SET_WRITE_STD_OBJECT_BUFFER(pTestNode->sphereObjectSet, pTestNode->sphereObjectSetBuffer));

    pTestNode->sphereTransform = (VkmTransform){.position = {1, 0, 0}};
    vkmUpdateObjectSet(&pTestNode->sphereTransform, &pTestNode->sphereObjectState, pTestNode->pSphereObjectSetMapped);

    vkmCreateSphereMesh(pTestNode->pool, pTestNode->graphicsQueue, 0.5f, 32, 32, &pTestNode->sphereMesh);

    VkBool32 presentSupport = VK_FALSE;
    VKM_REQUIRE(vkGetPhysicalDeviceSurfaceSupportKHR(context.physicalDevice, context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index, pCreateInfo->surface, &presentSupport));
    REQUIRE(presentSupport, "Queue can't present to surface!")
    VkmCreateSwap(pCreateInfo->surface, &pTestNode->swap);
  }

  {  // Initial State
    vkmCmdResetBegin(pTestNode->cmd);
    VkImageMemoryBarrier2 swapBarrier[VKM_SWAP_COUNT];
    for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
      swapBarrier[i] = VKM_IMAGE_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMAGE_BARRIER_PRESENT, VK_IMAGE_ASPECT_COLOR_BIT, pTestNode->swap.images[i]);
    }
    vkmCommandPipelineImageBarriers(pTestNode->cmd, VKM_SWAP_COUNT, swapBarrier);
    vkEndCommandBuffer(pTestNode->cmd);
    const VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &pTestNode->cmd,
    };
    VKM_REQUIRE(vkQueueSubmit(pTestNode->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
    VKM_REQUIRE(vkQueueWaitIdle(pTestNode->graphicsQueue));
  }

  {  // Copy needed state
    pTestNode->device = context.device;
    pTestNode->standardRenderPass = context.standardRenderPass;
    pTestNode->standardPipelineLayout = context.standardPipe.pipelineLayout;
    pTestNode->standardPipeline = context.standardPipe.pipeline;
    pTestNode->sampler = context.linearSampler;
    pTestNode->globalSet = context.globalSet;
  }
}

void mxcRunCompNode(const MxcNodeContext* pNodeContext) {

  struct {
    VkCommandBuffer cmd;

    int           framebufferIndex;
    VkFramebuffer framebuffers[VKM_SWAP_COUNT];
    VkImage       frameBufferColorImages[VKM_SWAP_COUNT];

    VkSampler sampler; // move to immutable sampler

    VkRenderPass     standardRenderPass;
    VkPipelineLayout standardPipelineLayout;
    VkPipeline       standardPipeline;

    VkDescriptorSet globalSet;
    VkDescriptorSet checkerMaterialSet;
    VkDescriptorSet sphereObjectSet;

    uint32_t sphereIndexCount;
    VkBuffer sphereIndexBuffer, sphereVertexBuffer;

    VkDevice device;

    VkImageView externalFramebufferColorImageViews[VKM_SWAP_COUNT];
    VkImage     externalFramebufferColorImages[VKM_SWAP_COUNT];

    VkmSwap swap;

    VkmTimeline compTimeline;
    VkmTimeline nodeTimeline;

    VkQueue graphicsQueue;
  } local;

  {
    MxcCompNode* pNode = (MxcCompNode*)pNodeContext->pNode;
    local.cmd = pNode->cmd;
    local.globalSet = pNode->globalSet;
    local.checkerMaterialSet = pNode->checkerMaterialSet;
    local.sphereObjectSet = pNode->sphereObjectSet;
    local.standardRenderPass = pNode->standardRenderPass;
    local.standardPipelineLayout = pNode->standardPipelineLayout;
    local.standardPipeline = pNode->standardPipeline;
    local.standardPipeline = pNode->standardPipeline;
    local.sampler = pNode->sampler;
    for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
      local.framebuffers[i] = pNode->framebuffers[i].framebuffer;
      local.frameBufferColorImages[i] = pNode->framebuffers[i].color.image;
      local.externalFramebufferColorImageViews[i] = pNode->pExternalsContexts->framebuffers[i].color.imageView;
      local.externalFramebufferColorImages[i] = pNode->pExternalsContexts->framebuffers[i].color.image;
    }
    local.sphereIndexCount = pNode->sphereMesh.indexCount;
    local.sphereIndexBuffer = pNode->sphereMesh.indexBuffer;
    local.sphereVertexBuffer = pNode->sphereMesh.vertexBuffer;
    local.device = pNode->device;
    local.swap = pNode->swap;
    local.graphicsQueue = pNode->graphicsQueue;

    local.framebufferIndex = 0;

    local.compTimeline.semaphore = pNodeContext->compTimeline;
    local.compTimeline.value = 0;

    local.nodeTimeline.semaphore = pNode->pExternalsContexts->nodeTimeline;
    local.nodeTimeline.value = 0;
  }

  bool nodeAvailable = false;

  while (isRunning) {

    // wait on odd value for input update complete
    local.compTimeline.value++;
    vkmTimelineWait(local.device, &local.compTimeline);

    vkmCmdResetBegin(local.cmd);

    if (vkmTimelineSyncCheck(local.device, &local.nodeTimeline) && local.nodeTimeline.value > 1) {
      const int     nodeFramebufferIndex = (local.nodeTimeline.value % VKM_SWAP_COUNT);
      const VkImage nodeFramebufferColorImage = local.externalFramebufferColorImages[nodeFramebufferIndex];
//      printf("Compositing %d...", nodeFramebufferIndex);
      const VkImageView nodeFramebufferColorImageView = local.externalFramebufferColorImageViews[nodeFramebufferIndex];
//      vkmCommandPipelineImageBarrier(local.cmd, &VKM_IMAGE_BARRIER(VKM_IMAGE_BARRIER_EXTERNAL_ACQUIRE_GRAPHICS_ATTACH, VKM_IMAGE_BARRIER_SHADER_READ, VK_IMAGE_ASPECT_COLOR_BIT, nodeFramebufferColorImage));
      vkmUpdateDescriptorSet(local.device, &VKM_SET_WRITE_STD_MATERIAL_IMAGE(local.checkerMaterialSet, nodeFramebufferColorImageView, local.sampler));
      nodeAvailable = true;
    }

    const VkFramebuffer framebuffer = local.framebuffers[local.framebufferIndex];
    const VkImage framebufferColorImage = local.frameBufferColorImages[local.framebufferIndex];

    vkmCmdBeginPass(local.cmd, local.standardRenderPass, framebuffer);

    if (nodeAvailable) {
      vkCmdBindPipeline(local.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, local.standardPipeline);
      // move to array
      vkCmdBindDescriptorSets(local.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, local.standardPipelineLayout, VKM_PIPE_SET_STD_GLOBAL_INDEX, 1, &local.globalSet, 0, NULL);
      vkCmdBindDescriptorSets(local.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, local.standardPipelineLayout, VKM_PIPE_SET_STD_MATERIAL_INDEX, 1, &local.checkerMaterialSet, 0, NULL);
      vkCmdBindDescriptorSets(local.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, local.standardPipelineLayout, VKM_PIPE_SET_STD_OBJECT_INDEX, 1, &local.sphereObjectSet, 0, NULL);

      vkCmdBindVertexBuffers(local.cmd, 0, 1, (const VkBuffer[]){local.sphereVertexBuffer}, (const VkDeviceSize[]){0});
      vkCmdBindIndexBuffer(local.cmd, local.sphereIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
      vkCmdDrawIndexed(local.cmd, local.sphereIndexCount, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(local.cmd);

    {  // Blit Framebuffer
      vkAcquireNextImageKHR(local.device, local.swap.chain, UINT64_MAX, local.swap.acquireSemaphore, VK_NULL_HANDLE, &local.swap.swapIndex);
      const VkImage               swapImage = local.swap.images[local.swap.swapIndex];
      const VkImageMemoryBarrier2 blitBarrier[] = {
          VKM_IMAGE_BARRIER(VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VKM_TRANSFER_SRC_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, framebufferColorImage),
          VKM_IMAGE_BARRIER(VKM_IMAGE_BARRIER_PRESENT, VKM_TRANSFER_DST_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, swapImage),
      };
      vkmCommandPipelineImageBarriers(local.cmd, 2, blitBarrier);
      vkmBlit(local.cmd, framebufferColorImage, swapImage);
      const VkImageMemoryBarrier2 presentBarrier[] = {
          //        VKM_IMAGE_BARRIER(VKM_TRANSFER_SRC_IMAGE_BARRIER, VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, framebufferColorImage),
          VKM_IMAGE_BARRIER(VKM_TRANSFER_DST_IMAGE_BARRIER, VKM_IMAGE_BARRIER_PRESENT, VK_IMAGE_ASPECT_COLOR_BIT, swapImage),
      };
      vkmCommandPipelineImageBarriers(local.cmd, 1, presentBarrier);
    }

    vkEndCommandBuffer(local.cmd);

    // signal even value for render complete
    local.compTimeline.value++;
    vkmSubmitPresentCommandBuffer(local.cmd, local.graphicsQueue, &local.swap, &local.compTimeline);

    vkmTimelineWait(local.device, &local.compTimeline);

    local.framebufferIndex = !local.framebufferIndex;
  }
}