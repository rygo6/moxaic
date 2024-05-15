#include "comp_node.h"

#include <vulkan/vk_enum_string_helper.h>

static void CreateQuadMesh(const VkCommandPool pool, const VkQueue queue, VkmMesh* pMesh) {
  {
    pMesh->indexCount = 6;
    const uint16_t pIndices[] = {0, 1, 2, 1, 3, 2};
    const uint32_t indexBufferSize = sizeof(uint16_t) * pMesh->indexCount;
    VkmCreateAllocBindBuffer(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VKM_LOCALITY_CONTEXT, &pMesh->indexMemory, &pMesh->indexBuffer);
    VkmPopulateBufferViaStaging(pool, queue, pIndices, indexBufferSize, pMesh->indexBuffer);
  }
  {
    pMesh->vertexCount = 4;
    const VkmVertex pVertices[] = {
        {.position = {-1, -1, 0}, .uv = {0, 0}},
        {.position = {1, -1, 0}, .uv = {1, 0}},
        {.position = {-1, 1, 0}, .uv = {0, 1}},
        {.position = {1, 1, 0}, .uv = {1, 1}},
    };
    const uint32_t vertexBufferSize = sizeof(VkmVertex) * pMesh->vertexCount;
    VkmCreateAllocBindBuffer(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VKM_LOCALITY_CONTEXT, &pMesh->vertexMemory, &pMesh->vertexBuffer);
    VkmPopulateBufferViaStaging(pool, queue, pVertices, vertexBufferSize, pMesh->vertexBuffer);
  }
}


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

    pTestNode->sphereTransform = (VkmTransform){.position = {0, 0, 0}};
    vkmUpdateObjectSet(&pTestNode->sphereTransform, &pTestNode->sphereObjectState, pTestNode->pSphereObjectSetMapped);

    CreateQuadMesh(pTestNode->pool, pTestNode->graphicsQueue, &pTestNode->sphereMesh);

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

    VkRenderPass     standardRenderPass;
    VkPipelineLayout standardPipelineLayout;
    VkPipeline       standardPipeline;

    VkDescriptorSet globalSet;
    VkDescriptorSet checkerMaterialSet;
    VkDescriptorSet sphereObjectSet;

    uint32_t sphereIndexCount;
    VkBuffer sphereIndexBuffer, sphereVertexBuffer;

    VkDevice device;

    VkmSwap swap;

    VkmTimeline compTimeline;

    VkQueue graphicsQueue;
  } hot;

  {
    MxcCompNode* pNode = (MxcCompNode*)pNodeContext->pNode;
    hot.cmd = pNode->cmd;
    hot.globalSet = pNode->globalSet;
    hot.checkerMaterialSet = pNode->checkerMaterialSet;
    hot.sphereObjectSet = pNode->sphereObjectSet;
    hot.standardRenderPass = pNode->standardRenderPass;
    hot.standardPipelineLayout = pNode->standardPipelineLayout;
    hot.standardPipeline = pNode->standardPipeline;
    hot.standardPipeline = pNode->standardPipeline;
    for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
      hot.framebuffers[i] = pNode->framebuffers[i].framebuffer;
      hot.frameBufferColorImages[i] = pNode->framebuffers[i].color.image;
    }
    hot.sphereIndexCount = pNode->sphereMesh.indexCount;
    hot.sphereIndexBuffer = pNode->sphereMesh.indexBuffer;
    hot.sphereVertexBuffer = pNode->sphereMesh.vertexBuffer;
    hot.device = pNode->device;
    hot.swap = pNode->swap;
    hot.graphicsQueue = pNode->graphicsQueue;

    hot.framebufferIndex = 0;

    hot.compTimeline.semaphore = pNodeContext->compTimeline;
    hot.compTimeline.value = 0;
  }

  bool nodeAvailable = false;

  while (isRunning) {

    // wait on odd value for input update complete
    hot.compTimeline.value++;
    vkmTimelineWait(hot.device, &hot.compTimeline);

    vkmCmdResetBegin(hot.cmd);

    const VkFramebuffer framebuffer = hot.framebuffers[hot.framebufferIndex];
    const VkImage       framebufferColorImage = hot.frameBufferColorImages[hot.framebufferIndex];

    vkmCmdBeginPass(hot.cmd, hot.standardRenderPass, framebuffer);

    const mxc_node_handle tempHandle = 0;
    if (vkmTimelineSyncCheck(hot.device, &MXC_HOT_NODE_CONTEXTS[tempHandle].nodeTimeline) && MXC_HOT_NODE_CONTEXTS[tempHandle].nodeTimeline.value > 1) {
      const int         nodeFramebufferIndex = !(MXC_HOT_NODE_CONTEXTS[tempHandle].nodeTimeline.value % VKM_SWAP_COUNT);
      const VkImageView nodeFramebufferColorImageView = MXC_HOT_NODE_CONTEXTS[tempHandle].framebufferColorImageViews[nodeFramebufferIndex];
      const VkImage     nodeFramebufferColorImage = MXC_HOT_NODE_CONTEXTS[tempHandle].framebufferColorImages[nodeFramebufferIndex];
      //      vkmCommandPipelineImageBarrier(local.cmd, &VKM_IMAGE_BARRIER(VKM_IMAGE_BARRIER_EXTERNAL_ACQUIRE_GRAPHICS_ATTACH, VKM_IMAGE_BARRIER_SHADER_READ, VK_IMAGE_ASPECT_COLOR_BIT, nodeFramebufferColorImage));
      vkmUpdateDescriptorSet(hot.device, &VKM_SET_WRITE_STD_MATERIAL_IMAGE(hot.checkerMaterialSet, nodeFramebufferColorImageView));
      nodeAvailable = true;
    }

    if (nodeAvailable) {
      vkCmdBindPipeline(hot.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hot.standardPipeline);
      // move to array
      vkCmdBindDescriptorSets(hot.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hot.standardPipelineLayout, VKM_PIPE_SET_STD_GLOBAL_INDEX, 1, &hot.globalSet, 0, NULL);
      vkCmdBindDescriptorSets(hot.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hot.standardPipelineLayout, VKM_PIPE_SET_STD_MATERIAL_INDEX, 1, &hot.checkerMaterialSet, 0, NULL);
      vkCmdBindDescriptorSets(hot.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, hot.standardPipelineLayout, VKM_PIPE_SET_STD_OBJECT_INDEX, 1, &hot.sphereObjectSet, 0, NULL);

      vkCmdBindVertexBuffers(hot.cmd, 0, 1, (const VkBuffer[]){hot.sphereVertexBuffer}, (const VkDeviceSize[]){0});
      vkCmdBindIndexBuffer(hot.cmd, hot.sphereIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
      vkCmdDrawIndexed(hot.cmd, hot.sphereIndexCount, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(hot.cmd);

    {  // Blit Framebuffer
      vkAcquireNextImageKHR(hot.device, hot.swap.chain, UINT64_MAX, hot.swap.acquireSemaphore, VK_NULL_HANDLE, &hot.swap.swapIndex);
      const VkImage               swapImage = hot.swap.images[hot.swap.swapIndex];
      const VkImageMemoryBarrier2 blitBarrier[] = {
          VKM_IMAGE_BARRIER(VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VKM_TRANSFER_SRC_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, framebufferColorImage),
          VKM_IMAGE_BARRIER(VKM_IMAGE_BARRIER_PRESENT, VKM_TRANSFER_DST_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, swapImage),
      };
      vkmCommandPipelineImageBarriers(hot.cmd, 2, blitBarrier);
      vkmBlit(hot.cmd, framebufferColorImage, swapImage);
      const VkImageMemoryBarrier2 presentBarrier[] = {
          //        VKM_IMAGE_BARRIER(VKM_TRANSFER_SRC_IMAGE_BARRIER, VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, framebufferColorImage),
          VKM_IMAGE_BARRIER(VKM_TRANSFER_DST_IMAGE_BARRIER, VKM_IMAGE_BARRIER_PRESENT, VK_IMAGE_ASPECT_COLOR_BIT, swapImage),
      };
      vkmCommandPipelineImageBarriers(hot.cmd, 1, presentBarrier);
    }

    vkEndCommandBuffer(hot.cmd);

    // signal even value for render complete
    hot.compTimeline.value++;
    vkmSubmitPresentCommandBuffer(hot.cmd, hot.graphicsQueue, &hot.swap, &hot.compTimeline);

    vkmTimelineWait(hot.device, &hot.compTimeline);

    hot.framebufferIndex = !hot.framebufferIndex;
  }
}