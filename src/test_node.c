#include "test_node.h"

void mxcCreateTestNode(const MxcTestNodeCreateInfo* pCreateInfo, MxcTestNode* pTestNode) {
  {  // Create
    const VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
    };
    VKM_REQUIRE(vkCreateCommandPool(context.device, &graphicsCommandPoolCreateInfo, VKM_ALLOC, &pTestNode->pool));

    vkGetDeviceQueue(context.device, context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index, 0, &pTestNode->graphicsQueue);
    const VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pTestNode->pool,
        .commandBufferCount = 1,
    };
    VKM_REQUIRE(vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfo, &pTestNode->cmd));
    VkmSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)pTestNode->cmd, "TestNode");

    vkmCreateNodeFramebufferImport(context.standardRenderPass, VKM_LOCALITY_CONTEXT, pCreateInfo->pFramebuffers, pTestNode->framebuffers);
    //    vkmCreateStandardFramebuffers(context.standardRenderPass, VKM_SWAP_COUNT, VKM_LOCALITY_PROCESS_EXPORTED, pTestNode->framebuffers);

    vkmAllocateDescriptorSet(context.descriptorPool, &context.standardPipe.materialSetLayout, &pTestNode->checkerMaterialSet);
    vkmCreateTextureFromFile(pTestNode->pool, pTestNode->graphicsQueue, "textures/uvgrid.jpg", &pTestNode->checkerTexture);

    vkmAllocateDescriptorSet(context.descriptorPool, &context.standardPipe.objectSetLayout, &pTestNode->sphereObjectSet);
    vkmCreateAllocBindMapBuffer(VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(VkmStandardObjectSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VKM_LOCALITY_CONTEXT, &pTestNode->sphereObjectSetMemory, &pTestNode->sphereObjectSetBuffer, (void**)&pTestNode->pSphereObjectSetMapped);

    const VkWriteDescriptorSet writeSets[] = {
        VKM_SET_WRITE_STD_MATERIAL_IMAGE(pTestNode->checkerMaterialSet, pTestNode->checkerTexture.imageView),
        VKM_SET_WRITE_STD_OBJECT_BUFFER(pTestNode->sphereObjectSet, pTestNode->sphereObjectSetBuffer),
    };
    vkUpdateDescriptorSets(context.device, COUNT(writeSets), writeSets, 0, NULL);

    pTestNode->sphereTransform = pCreateInfo->transform;
    vkmUpdateObjectSet(&pTestNode->sphereTransform, &pTestNode->sphereObjectState, pTestNode->pSphereObjectSetMapped);

    vkmCreateSphereMesh(pTestNode->pool, pTestNode->graphicsQueue, 0.5f, 32, 32, &pTestNode->sphereMesh);
  }

#ifdef DEBUG_TEST_NODE_SWAP
  {  // Swap
    VkBool32 presentSupport = VK_FALSE;
    VKM_REQUIRE(vkGetPhysicalDeviceSurfaceSupportKHR(context.physicalDevice, context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index, pCreateInfo->surface, &presentSupport));
    REQUIRE(presentSupport, "Queue can't present to surface!")

    VkmCreateSwap(pCreateInfo->surface, &pTestNode->swap);
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
#endif

  {  // Copy needed state
    pTestNode->device = context.device;
    pTestNode->standardRenderPass = context.standardRenderPass;
    pTestNode->standardPipelineLayout = context.standardPipe.pipelineLayout;
    pTestNode->standardPipeline = context.standardPipe.pipeline;
    pTestNode->globalSet = context.globalSet;
  }

  //  {
  //    // should do this for all things repeatedly called...
  //    VKM_DEVICE_FUNC(vkCmdBindPipeline);
  //    pTestNode->vkCmdBindPipeline = vkCmdBindPipeline;
  //  }
}

void mxcRunTestNode(const MxcNodeContext* pNodeContext) {

  struct {
    VkCommandBuffer cmd;

    VkFramebuffer framebuffers[VKM_SWAP_COUNT];
    VkImage       frameBufferColorImages[VKM_SWAP_COUNT];

    VkRenderPass     standardRenderPass;
    VkPipelineLayout standardPipelineLayout;
    VkPipeline       standardPipeline;

    VkDescriptorSet globalSet;
    VkDescriptorSet checkerMaterialSet;
    VkDescriptorSet sphereObjectSet;

    uint32_t sphereIndexCount;
    VkBuffer sphereIndexBuffer, sphereVertexBuffer;

    VkDevice device;

#ifdef DEBUG_TEST_NODE_SWAP
    VkmSwap swap;
#endif

    VkmTimeline compTimeline;
    VkmTimeline nodeTimeline;
    VkQueue     graphicsQueue;

    uint64_t compBaseCycleValue;
    uint64_t compCyclesToSkip;
  } hot;

  {
    MxcTestNode* pNode = (MxcTestNode*)pNodeContext->pNode;
    hot.cmd = pNode->cmd;
    hot.globalSet = pNode->globalSet;
    hot.checkerMaterialSet = pNode->checkerMaterialSet;
    hot.sphereObjectSet = pNode->sphereObjectSet;
    hot.standardRenderPass = pNode->standardRenderPass;
    hot.standardPipelineLayout = pNode->standardPipelineLayout;
    hot.standardPipeline = pNode->standardPipeline;
    for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
      hot.framebuffers[i] = pNode->framebuffers[i].framebuffer;
      hot.frameBufferColorImages[i] = pNode->framebuffers[i].color.image;
    }
    hot.sphereIndexCount = pNode->sphereMesh.indexCount;
    hot.sphereIndexBuffer = pNode->sphereMesh.indexBuffer;
    hot.sphereVertexBuffer = pNode->sphereMesh.vertexBuffer;
    hot.device = pNode->device;
    hot.graphicsQueue = pNode->graphicsQueue;
    hot.compTimeline.semaphore = pNodeContext->compTimeline;
    hot.compTimeline.value = 0;
    hot.nodeTimeline.semaphore = pNodeContext->nodeTimeline;
    hot.nodeTimeline.value = 0;

    // set timeline value to start of next cycle
    vkmTimelineSync(hot.device, &hot.compTimeline);
    hot.compBaseCycleValue = hot.compTimeline.value - (hot.compTimeline.value % MXC_CYCLE_COUNT);

    // wait on next cycle
    hot.compTimeline.value = hot.compBaseCycleValue + MXC_CYCLE_COUNT;

    hot.compCyclesToSkip = MXC_CYCLE_COUNT * pNodeContext->compCycleSkip;

#ifdef DEBUG_TEST_NODE_SWAP
    local.swap = pNode->swap;
#endif
  }

  while (isRunning) {

    // wait for input cycle to finish
    hot.compTimeline.value = hot.compBaseCycleValue + MXC_CYCLE_INPUT;
    vkmTimelineWait(hot.device, &hot.compTimeline);

    const int           framebufferIndex = hot.nodeTimeline.value % VKM_SWAP_COUNT;
    const VkFramebuffer framebuffer = hot.framebuffers[framebufferIndex];
    const VkImage       framebufferColorImage = hot.frameBufferColorImages[framebufferIndex];

//    printf("Rendering into %d...", framebufferIndex);

    vkmCmdResetBegin(hot.cmd);

    vkmCommandPipelineImageBarrier(hot.cmd, &VKM_IMAGE_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, framebufferColorImage));

    vkmCmdBeginPass(hot.cmd, hot.standardRenderPass, framebuffer);

    vkCmdBindPipeline(hot.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hot.standardPipeline);
    vkCmdBindDescriptorSets(hot.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hot.standardPipelineLayout, VKM_PIPE_SET_STD_GLOBAL_INDEX, 1, &hot.globalSet, 0, NULL);
    vkCmdBindDescriptorSets(hot.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hot.standardPipelineLayout, VKM_PIPE_SET_STD_MATERIAL_INDEX, 1, &hot.checkerMaterialSet, 0, NULL);
    vkCmdBindDescriptorSets(hot.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hot.standardPipelineLayout, VKM_PIPE_SET_STD_OBJECT_INDEX, 1, &hot.sphereObjectSet, 0, NULL);

    vkCmdBindVertexBuffers(hot.cmd, 0, 1, (const VkBuffer[]){hot.sphereVertexBuffer}, (const VkDeviceSize[]){0});
    vkCmdBindIndexBuffer(hot.cmd, hot.sphereIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(hot.cmd, hot.sphereIndexCount, 1, 0, 0, 0);

    vkCmdEndRenderPass(hot.cmd);

#ifdef DEBUG_TEST_NODE_SWAP
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
#endif

    vkmCommandPipelineImageBarrier(hot.cmd, &VKM_IMAGE_BARRIER(VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VKM_IMAGE_BARRIER_EXTERNAL_RELEASE_GRAPHICS_READ, VK_IMAGE_ASPECT_COLOR_BIT, framebufferColorImage));

    vkEndCommandBuffer(hot.cmd);

    hot.nodeTimeline.value++;
#ifdef DEBUG_TEST_NODE_SWAP
    vkmSubmitPresentCommandBuffer(local.cmd, local.graphicsQueue, &local.swap, &local.timeline);
    vkmTimelineWait(local.device, &local.timeline);
#else
    vkmSubmitCommandBuffer(hot.cmd, hot.graphicsQueue, &hot.nodeTimeline);
    vkmTimelineWait(hot.device, &hot.nodeTimeline);
#endif

    // increment past input cycle and wait on render next cycle
    hot.compBaseCycleValue += hot.compCyclesToSkip;
  }
}