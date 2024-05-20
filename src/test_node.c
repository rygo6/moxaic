#include "test_node.h"
#include <stdatomic.h>

void CreateSphereMesh(const float radius, const int slicesCount, const int stackCount, VkmMesh* pMesh) {
  VkmMeshCreateInfo info = {
      .indexCount = slicesCount * stackCount * 2 * 3,
      .vertexCount = (slicesCount + 1) * (stackCount + 1),
  };
  uint16_t indices[info.indexCount];
  int      index = 0;
  for (int i = 0; i < stackCount; i++) {
    for (int j = 0; j < slicesCount; j++) {
      const uint16_t v1 = i * (slicesCount + 1) + j;
      const uint16_t v2 = i * (slicesCount + 1) + j + 1;
      const uint16_t v3 = (i + 1) * (slicesCount + 1) + j;
      const uint16_t v4 = (i + 1) * (slicesCount + 1) + j + 1;
      indices[index++] = v1;
      indices[index++] = v2;
      indices[index++] = v3;
      indices[index++] = v2;
      indices[index++] = v4;
      indices[index++] = v3;
    }
  }
  VkmVertex   vertices[info.vertexCount];
  const float slices = (float)slicesCount;
  const float stacks = (float)stackCount;
  const float dtheta = 2.0f * VKM_PI / slices;
  const float dphi = VKM_PI / stacks;
  int         vertex = 0;
  for (int i = 0; +i <= stackCount; i++) {
    const float fi = (float)i;
    const float phi = fi * dphi;
    for (int j = 0; j <= slicesCount; j++) {
      const float ji = (float)j;
      const float theta = ji * dtheta;
      const float x = radius * sinf(phi) * cosf(theta);
      const float y = radius * sinf(phi) * sinf(theta);
      const float z = radius * cosf(phi);
      vertices[vertex++] = (VkmVertex){
          .position = {x, y, z},
          .normal = {x, y, z},
          .uv = {ji / slices, fi / stacks},
      };
    }
    info.pIndices = indices;
    info.pVertices = vertices;
    VkmCreateMesh(&info, pMesh);
  }
}

void mxcCreateTestNode(const MxcTestNodeCreateInfo* pCreateInfo, MxcTestNode* pTestNode) {
  {  // Create
    vkmCreateNodeFramebufferImport(context.standardRenderPass, VKM_LOCALITY_CONTEXT, pCreateInfo->pFramebuffers, pTestNode->framebuffers);

    vkmAllocateDescriptorSet(context.descriptorPool, &context.standardPipe.materialSetLayout, &pTestNode->checkerMaterialSet);
    vkmCreateTextureFromFile("textures/uvgrid.jpg", &pTestNode->checkerTexture);

    vkmAllocateDescriptorSet(context.descriptorPool, &context.standardPipe.objectSetLayout, &pTestNode->sphereObjectSet);
    vkmCreateAllocBindMapBuffer(VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(VkmStandardObjectSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VKM_LOCALITY_CONTEXT, &pTestNode->sphereObjectSetMemory, &pTestNode->sphereObjectSetBuffer, (void**)&pTestNode->pSphereObjectSetMapped);

    const VkWriteDescriptorSet writeSets[] = {
        VKM_SET_WRITE_STD_MATERIAL_IMAGE(pTestNode->checkerMaterialSet, pTestNode->checkerTexture.imageView),
        VKM_SET_WRITE_STD_OBJECT_BUFFER(pTestNode->sphereObjectSet, pTestNode->sphereObjectSetBuffer),
    };
    vkUpdateDescriptorSets(context.device, COUNT(writeSets), writeSets, 0, NULL);

    pTestNode->sphereTransform = pCreateInfo->transform;
    vkmUpdateObjectSet(&pTestNode->sphereTransform, &pTestNode->sphereObjectState, pTestNode->pSphereObjectSetMapped);

    CreateSphereMesh(0.5f, 32, 32, &pTestNode->sphereMesh);

    const VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index,
    };
    VKM_REQUIRE(vkCreateCommandPool(context.device, &graphicsCommandPoolCreateInfo, VKM_ALLOC, &pTestNode->pool));
    const VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pTestNode->pool,
        .commandBufferCount = 1,
    };
    VKM_REQUIRE(vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfo, &pTestNode->cmd));
    VkmSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)pTestNode->cmd, "TestNode");
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
    pTestNode->queueIndex = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index;
  }

  //  {
  //    // should do this for all things repeatedly called...
  //    VKM_DEVICE_FUNC(vkCmdBindPipeline);
  //    pTestNode->vkCmdBindPipeline = vkCmdBindPipeline;
  //  }
}

void mxcRunTestNode(const MxcNodeContext* pNodeContext) {

  struct {
    MxcNodeType nodeType;

    VkCommandBuffer cmd;

    //    uint8_t framebufferIndex;

    VkFramebuffer framebuffers[VKM_SWAP_COUNT];
    VkImage       frameBufferColorImages[VKM_SWAP_COUNT];

    VkRenderPass     standardRenderPass;
    VkPipelineLayout standardPipelineLayout;
    VkPipeline       standardPipeline;

    VkDescriptorSet globalSet;
    VkDescriptorSet checkerMaterialSet;
    VkDescriptorSet sphereObjectSet;

    uint32_t     sphereIndexCount;
    VkBuffer     sphereBuffer;
    VkDeviceSize sphereIndexOffset;
    VkDeviceSize sphereVertexOffset;

    VkDevice device;

#ifdef DEBUG_TEST_NODE_SWAP
    VkmSwap swap;
#endif

    VkmTimeline compTimeline;
    VkmTimeline nodeTimeline;

    uint32_t queueIndex;
    VkQueue  queue;

    uint64_t compBaseCycleValue;
    uint64_t compCyclesToSkip;
  } hot;

  {
    MxcTestNode* pNode = (MxcTestNode*)pNodeContext->pNode;
    hot.nodeType = pNodeContext->nodeType;
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
    //    hot.sphereIndexBuffer = pNode->sphereMesh.indexBuffer;
    //    hot.sphereVertexBuffer = pNode->sphereMesh.vertexBuffer;
    hot.sphereBuffer = pNode->sphereMesh.buffer;
    hot.sphereIndexOffset = pNode->sphereMesh.indexOffset;
    hot.sphereVertexOffset = pNode->sphereMesh.vertexOffset;
    hot.device = pNode->device;
    hot.device = pNode->device;
    hot.queueIndex = pNode->queueIndex;
    hot.queue = pNode->queue;
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

    //    hot.framebufferIndex = 0;

    MXC_NODE_CONTEXT_HOT[0].nodeSetState.model = pNode->sphereObjectState.model;

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

    vkResetCommandBuffer(hot.cmd, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    vkBeginCommandBuffer(hot.cmd, &(const VkCommandBufferBeginInfo){.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT});




    const VkViewport viewport = {
        .x = 512,
        .y = 512,
        .width = 512,
        .height = 512,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(hot.cmd, 0, 1, &viewport);
    const VkRect2D scissor = {
        //        .offset = {
        //            .x = 512,
        //            .y = 512,
        //        },
        .extent = {
            .width = DEFAULT_WIDTH,
            .height = DEFAULT_HEIGHT,
        },
    };
    vkCmdSetScissor(hot.cmd, 0, 1, &scissor);

    switch (hot.nodeType) {
      case MXC_NODE_TYPE_THREAD: break;
      case MXC_NODE_TYPE_INTERPROCESS:
        vkmCommandPipelineImageBarrier(hot.cmd, &VKM_IMAGE_BARRIER_QUEUE_TRANSFER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, framebufferColorImage, VK_QUEUE_FAMILY_EXTERNAL, hot.queueIndex));
        break;
    }



    { // this is really all that'd be user exposed....
      vkmCmdBeginPass(hot.cmd, hot.standardRenderPass, (VkClearColorValue){0, 0, 0, 0}, framebuffer);

      vkCmdBindPipeline(hot.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hot.standardPipeline);
      vkCmdBindDescriptorSets(hot.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hot.standardPipelineLayout, VKM_PIPE_SET_STD_GLOBAL_INDEX, 1, &hot.globalSet, 0, NULL);
      vkCmdBindDescriptorSets(hot.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hot.standardPipelineLayout, VKM_PIPE_SET_STD_MATERIAL_INDEX, 1, &hot.checkerMaterialSet, 0, NULL);
      vkCmdBindDescriptorSets(hot.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hot.standardPipelineLayout, VKM_PIPE_SET_STD_OBJECT_INDEX, 1, &hot.sphereObjectSet, 0, NULL);

      vkCmdBindVertexBuffers(hot.cmd, 0, 1, (const VkBuffer[]){hot.sphereBuffer}, (const VkDeviceSize[]){hot.sphereVertexOffset});
      vkCmdBindIndexBuffer(hot.cmd, hot.sphereBuffer, hot.sphereIndexOffset, VK_INDEX_TYPE_UINT16);
      vkCmdDrawIndexed(hot.cmd, hot.sphereIndexCount, 1, 0, 0, 0);

      vkCmdEndRenderPass(hot.cmd);
    }


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

    switch (hot.nodeType) {
      case MXC_NODE_TYPE_THREAD:
        vkmCommandPipelineImageBarrier(hot.cmd, &VKM_IMAGE_BARRIER(VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VKM_IMAGE_BARRIER_EXTERNAL_RELEASE_GRAPHICS_READ, VK_IMAGE_ASPECT_COLOR_BIT, framebufferColorImage));
        break;
      case MXC_NODE_TYPE_INTERPROCESS:
        vkmCommandPipelineImageBarrier(hot.cmd, &VKM_IMAGE_BARRIER_QUEUE_TRANSFER(VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VKM_IMAGE_BARRIER_EXTERNAL_RELEASE_GRAPHICS_READ, VK_IMAGE_ASPECT_COLOR_BIT, framebufferColorImage, hot.queueIndex, VK_QUEUE_FAMILY_EXTERNAL));
        break;
    }

    vkEndCommandBuffer(hot.cmd);

    hot.nodeTimeline.value++;
#ifdef DEBUG_TEST_NODE_SWAP
    vkmSubmitPresentCommandBuffer(local.cmd, local.graphicsQueue, &local.swap, &local.timeline);
    vkmTimelineWait(local.device, &local.timeline);
#else
    MXC_NODE_CONTEXT_HOT[0].pendingTimelineSignal = hot.nodeTimeline.value;
#endif

    vkmTimelineWait(hot.device, &hot.nodeTimeline);
    MXC_NODE_CONTEXT_HOT[0].currentTimelineSignal = hot.nodeTimeline.value;


    // increment past input cycle and wait on render next cycle
    hot.compBaseCycleValue += hot.compCyclesToSkip;
    //    hot.framebufferIndex = !hot.framebufferIndex;
  }
}