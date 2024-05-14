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
  } local;

  {
    MxcTestNode* pNode = (MxcTestNode*)pNodeContext->pNode;
    local.cmd = pNode->cmd;
    local.globalSet = pNode->globalSet;
    local.checkerMaterialSet = pNode->checkerMaterialSet;
    local.sphereObjectSet = pNode->sphereObjectSet;
    local.standardRenderPass = pNode->standardRenderPass;
    local.standardPipelineLayout = pNode->standardPipelineLayout;
    local.standardPipeline = pNode->standardPipeline;
    for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
      local.framebuffers[i] = pNode->framebuffers[i].framebuffer;
      local.frameBufferColorImages[i] = pNode->framebuffers[i].color.image;
    }
    local.sphereIndexCount = pNode->sphereMesh.indexCount;
    local.sphereIndexBuffer = pNode->sphereMesh.indexBuffer;
    local.sphereVertexBuffer = pNode->sphereMesh.vertexBuffer;
    local.device = pNode->device;
    local.graphicsQueue = pNode->graphicsQueue;
    local.compTimeline.semaphore = pNodeContext->compTimeline;
    local.compTimeline.value = 0;
    local.nodeTimeline.semaphore = pNodeContext->nodeTimeline;
    local.nodeTimeline.value = 0;

    // set timeline value to start of next cycle
    vkmTimelineSync(local.device, &local.compTimeline);
    local.compBaseCycleValue = local.compTimeline.value - (local.compTimeline.value % MXC_CYCLE_COUNT);

    // wait on next cycle
    local.compTimeline.value = local.compBaseCycleValue + MXC_CYCLE_COUNT;

    local.compCyclesToSkip = MXC_CYCLE_COUNT * pNodeContext->compCycleSkip;

#ifdef DEBUG_TEST_NODE_SWAP
    local.swap = pNode->swap;
#endif
  }

  while (isRunning) {

    // wait for input cycle to finish
    local.compTimeline.value = local.compBaseCycleValue + MXC_CYCLE_INPUT;
    vkmTimelineWait(local.device, &local.compTimeline);

    const int           framebufferIndex = local.nodeTimeline.value % VKM_SWAP_COUNT;
    const VkFramebuffer framebuffer = local.framebuffers[framebufferIndex];
    const VkImage       framebufferColorImage = local.frameBufferColorImages[framebufferIndex];

//    printf("Rendering into %d...", framebufferIndex);

    vkmCmdResetBegin(local.cmd);

    vkmCommandPipelineImageBarrier(local.cmd, &VKM_IMAGE_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, framebufferColorImage));

    vkmCmdBeginPass(local.cmd, local.standardRenderPass, framebuffer);

    vkCmdBindPipeline(local.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, local.standardPipeline);
    vkCmdBindDescriptorSets(local.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, local.standardPipelineLayout, VKM_PIPE_SET_STD_GLOBAL_INDEX, 1, &local.globalSet, 0, NULL);
    vkCmdBindDescriptorSets(local.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, local.standardPipelineLayout, VKM_PIPE_SET_STD_MATERIAL_INDEX, 1, &local.checkerMaterialSet, 0, NULL);
    vkCmdBindDescriptorSets(local.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, local.standardPipelineLayout, VKM_PIPE_SET_STD_OBJECT_INDEX, 1, &local.sphereObjectSet, 0, NULL);

    vkCmdBindVertexBuffers(local.cmd, 0, 1, (const VkBuffer[]){local.sphereVertexBuffer}, (const VkDeviceSize[]){0});
    vkCmdBindIndexBuffer(local.cmd, local.sphereIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(local.cmd, local.sphereIndexCount, 1, 0, 0, 0);

    vkCmdEndRenderPass(local.cmd);

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

    vkmCommandPipelineImageBarrier(local.cmd, &VKM_IMAGE_BARRIER(VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VKM_IMAGE_BARRIER_EXTERNAL_RELEASE_GRAPHICS_READ, VK_IMAGE_ASPECT_COLOR_BIT, framebufferColorImage));

    vkEndCommandBuffer(local.cmd);

    local.nodeTimeline.value++;
#ifdef DEBUG_TEST_NODE_SWAP
    vkmSubmitPresentCommandBuffer(local.cmd, local.graphicsQueue, &local.swap, &local.timeline);
    vkmTimelineWait(local.device, &local.timeline);
#else
    vkmSubmitCommandBuffer(local.cmd, local.graphicsQueue, &local.nodeTimeline);
    vkmTimelineWait(local.device, &local.nodeTimeline);
#endif

    // increment past input cycle and wait on render next cycle
    local.compBaseCycleValue += local.compCyclesToSkip;
  }
}