#include "comp_node.h"

#include <vulkan/vk_enum_string_helper.h>

static void CreateQuadMesh(VkmMesh* pMesh) {
  const uint16_t  indices[] = {0, 1, 2, 1, 3, 2};
  const VkmVertex vertices[] = {
      {.position = {-1, -1, 0}, .uv = {0, 0}},
      {.position = {1, -1, 0}, .uv = {1, 0}},
      {.position = {-1, 1, 0}, .uv = {0, 1}},
      {.position = {1, 1, 0}, .uv = {1, 1}},
  };
  const VkmMeshCreateInfo info = {
      .indexCount = 6,
      .vertexCount = 4,
      .pIndices = indices,
      .pVertices = vertices,
  };
  VkmCreateMesh(&info, pMesh);
}

void mxcCreateBasicComp(const MxcBasicCompCreateInfo* pInfo, MxcBasicComp* pComp) {
  {  // Create
    vkmCreateStandardFramebuffers(context.standardRenderPass, VKM_SWAP_COUNT, VKM_LOCALITY_CONTEXT, pComp->framebuffers);

    vkmAllocateDescriptorSet(context.descriptorPool, &context.standardPipe.materialSetLayout, &pComp->checkerMaterialSet);
    VkmSetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)pComp->checkerMaterialSet, "CompCheckerMaterialSet");

    vkmAllocateDescriptorSet(context.descriptorPool, &context.standardPipe.objectSetLayout, &pComp->sphereObjectSet);
    VkmSetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)pComp->sphereObjectSet, "CompSphereObjectSet");
    vkmCreateAllocBindMapBuffer(VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(VkmStandardObjectSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VKM_LOCALITY_CONTEXT, &pComp->sphereObjectSetMemory, &pComp->sphereObjectSetBuffer, (void**)&pComp->pSphereObjectSetMapped);

    vkmUpdateDescriptorSet(context.device, &VKM_SET_WRITE_STD_OBJECT_BUFFER(pComp->sphereObjectSet, pComp->sphereObjectSetBuffer));

    pComp->sphereTransform = (VkmTransform){.position = {0, 0, 0}};
    vkmUpdateObjectSet(&pComp->sphereTransform, &pComp->sphereObjectState, pComp->pSphereObjectSetMapped);

    CreateQuadMesh(&pComp->sphereMesh);

    VkBool32 presentSupport = VK_FALSE;
    VKM_REQUIRE(vkGetPhysicalDeviceSurfaceSupportKHR(context.physicalDevice, context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index, pInfo->surface, &presentSupport));
    REQUIRE(presentSupport, "Queue can't present to surface!")
    VkmCreateSwap(pInfo->surface, &pComp->swap);

    vkmCreateTimeline(&pComp->timeline);

    const VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].pool,
        .commandBufferCount = 1,
    };
    VKM_REQUIRE(vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfo, &pComp->cmd));
    VkmSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)pComp->cmd, "CompCmd");
  }


  {  // Initial State
    vkmCmdResetBegin(pComp->cmd);
    VkImageMemoryBarrier2 swapBarrier[VKM_SWAP_COUNT];
    for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
      swapBarrier[i] = VKM_IMAGE_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMAGE_BARRIER_PRESENT, VK_IMAGE_ASPECT_COLOR_BIT, pComp->swap.images[i]);
    }
    vkmCommandPipelineImageBarriers(pComp->cmd, VKM_SWAP_COUNT, swapBarrier);
    vkEndCommandBuffer(pComp->cmd);
    const VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &pComp->cmd,
    };
    VKM_REQUIRE(vkQueueSubmit(context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].queue, 1, &submitInfo, VK_NULL_HANDLE));
    VKM_REQUIRE(vkQueueWaitIdle(context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].queue));
  }

  {  // Copy needed state
    pComp->device = context.device;
    pComp->standardRenderPass = context.standardRenderPass;
    pComp->standardPipelineLayout = context.standardPipe.pipelineLayout;
    pComp->standardPipeline = context.standardPipe.pipeline;
    pComp->globalSet = context.globalSet;
  }
}

void mxcRunCompNode(const MxcBasicComp* pNode) {

  struct {
    VkCommandBuffer  cmd;
    int              framebufferIndex;
    VkFramebuffer    framebuffers[VKM_SWAP_COUNT];
    VkImage          frameBufferColorImages[VKM_SWAP_COUNT];
    VkRenderPass     standardRenderPass;
    VkPipelineLayout standardPipelineLayout;
    VkPipeline       standardPipeline;
    VkDescriptorSet  globalSet;
    VkDescriptorSet  checkerMaterialSet;
    VkDescriptorSet  sphereObjectSet;
    uint32_t         quadIndexCount;
    VkBuffer         quadBuffer;
    VkDeviceSize     quadIndexOffset;
    VkDeviceSize     quadVertexOffset;
    VkDevice         device;
    VkmSwap          swap;
    VkmTimeline      compTimeline;
    uint64_t         compBaseCycleValue;
    VkQueue          graphicsQueue;
  } hot;

  {
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
    hot.quadIndexCount = pNode->sphereMesh.indexCount;
    hot.quadBuffer = pNode->sphereMesh.buffer;
    hot.quadIndexOffset = pNode->sphereMesh.indexOffset;
    hot.quadVertexOffset = pNode->sphereMesh.vertexOffset;
    hot.device = pNode->device;
    hot.swap = pNode->swap;
    hot.framebufferIndex = 0;
    hot.compTimeline.semaphore = pNode->timeline;
    hot.compTimeline.value = 0;
    hot.compBaseCycleValue = MXC_CYCLE_COUNT;
    hot.graphicsQueue = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].queue;
  }

  bool nodeAvailable = false;

  while (isRunning) {

    vkmUpdateWindowInput();

    if (vkmProcessInput(&context.globalCameraTransform)) {
      vkmUpdateGlobalSetView(&context.globalCameraTransform, &context.globalSetState, context.pGlobalSetMapped);
    }

    // signal odd for input ready
    hot.compTimeline.value = hot.compBaseCycleValue + MXC_CYCLE_INPUT;
    vkmTimelineSignal(context.device, &hot.compTimeline);

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

      vkCmdBindVertexBuffers(hot.cmd, 0, 1, (const VkBuffer[]){hot.quadBuffer}, (const VkDeviceSize[]){hot.quadVertexOffset});
      vkCmdBindIndexBuffer(hot.cmd, hot.quadBuffer, hot.quadIndexOffset, VK_INDEX_TYPE_UINT16);
      vkCmdDrawIndexed(hot.cmd, hot.quadIndexCount, 1, 0, 0, 0);
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
    hot.compTimeline.value = hot.compBaseCycleValue + MXC_CYCLE_RENDER;
    vkmSubmitPresentCommandBuffer(hot.cmd, hot.graphicsQueue, &hot.swap, &hot.compTimeline);
    vkmTimelineWait(hot.device, &hot.compTimeline);

    hot.framebufferIndex = !hot.framebufferIndex;
    hot.compBaseCycleValue += MXC_CYCLE_COUNT;
  }
}