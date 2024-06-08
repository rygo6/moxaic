#include "test_node.h"
#include "mid_shape.h"
#include <assert.h>

enum SetBindNodeProcessIndices {
  SET_BIND_NODE_PROCESS_SRC_INDEX,
  SET_BIND_NODE_PROCESS_DST_INDEX,
//  SET_BIND_NODE_PROCESS_UBO_INDEX,
  SET_BIND_NODE_PROCESS_COUNT
};
#define SET_WRITE_NODE_PROCESS_SRC(src_image_view)               \
  (VkWriteDescriptorSet) {                                       \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,             \
    .dstBinding = SET_BIND_NODE_PROCESS_SRC_INDEX,               \
    .descriptorCount = 1,                                        \
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
    .pImageInfo = &(const VkDescriptorImageInfo){                \
        .imageView = src_image_view,                             \
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,                  \
    },                                                           \
  }
#define SET_WRITE_NODE_PROCESS_DST(dst_image_view)      \
  (VkWriteDescriptorSet) {                              \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,    \
    .dstBinding = SET_BIND_NODE_PROCESS_DST_INDEX,      \
    .descriptorCount = 1,                               \
    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, \
    .pImageInfo = &(const VkDescriptorImageInfo){       \
        .imageView = dst_image_view,                    \
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,         \
    },                                                  \
  }
//#define SET_WRITE_NODE_PROCESS_BUFFER(ubo)               \
//  (VkWriteDescriptorSet) {                               \
//    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,     \
//    .dstBinding = SET_BIND_NODE_PROCESS_UBO_INDEX,       \
//    .descriptorCount = 1,                                \
//    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, \
//    .pBufferInfo = &(const VkDescriptorBufferInfo){      \
//        .buffer = ubo,                                   \
//        .range = sizeof(NodeProcessUBO)},                \
//  }
static void CreateNodeProcessSetLayout(VkDescriptorSetLayout* pLayout) {
  const VkDescriptorSetLayoutCreateInfo nodeSetLayoutCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
      .bindingCount = SET_BIND_NODE_PROCESS_COUNT,
      .pBindings = (const VkDescriptorSetLayoutBinding[]){
          [SET_BIND_NODE_PROCESS_SRC_INDEX] = {
              .binding = SET_BIND_NODE_PROCESS_SRC_INDEX,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
              .pImmutableSamplers = &context.linearSampler,
          },
          [SET_BIND_NODE_PROCESS_DST_INDEX] = {
              .binding = SET_BIND_NODE_PROCESS_DST_INDEX,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .descriptorCount = 1,
              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
//          [SET_BIND_NODE_PROCESS_UBO_INDEX] = {
//              .binding = SET_BIND_NODE_PROCESS_UBO_INDEX,
//              .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
//              .descriptorCount = 1,
//              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
//          },
      },
  };
  VKM_REQUIRE(vkCreateDescriptorSetLayout(context.device, &nodeSetLayoutCreateInfo, VKM_ALLOC, pLayout));
}

enum PipeSetNodeProcessIndices {
  PIPE_SET_NODE_PROCESS_INDEX,
  PIPE_SET_NODE_PROCESS_COUNT,
};
static void CreateNodeProcessPipeLayout(const VkDescriptorSetLayout nodeProcessSetLayout, VkPipelineLayout* pPipeLayout) {
  const VkPipelineLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = (const VkDescriptorSetLayout[]){
          [PIPE_SET_NODE_PROCESS_INDEX] = nodeProcessSetLayout,
      },
  };
  VKM_REQUIRE(vkCreatePipelineLayout(context.device, &createInfo, VKM_ALLOC, pPipeLayout));
}

void CreateNodeProcessPipe(const char* shaderPath, const VkPipelineLayout layout, VkPipeline* pPipe) {
  const VkShaderModule              shader = vkmCreateShaderModule(shaderPath);
  const VkComputePipelineCreateInfo pipelineInfo = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_COMPUTE_BIT,
          .module = shader,
          .pName = "main",
      },
      .layout = layout,
  };
  VKM_REQUIRE(vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, VKM_ALLOC, pPipe));
  vkDestroyShaderModule(context.device, shader, VKM_ALLOC);
}

static INLINE float CalcViewport(const float ulUV, const float lrUV, const float framebufferSize, const float screenSize) {
  if (framebufferSize > screenSize) {
    if (ulUV < 0.0f && lrUV > 1.0f)
      return 0.0f;
    else if (lrUV < 1.0f)
      return (1.0f - lrUV) * screenSize;
    else
      return -ulUV * screenSize;
  } else {
    return -ulUV * screenSize;
  }
}

void mxcCreateTestNode(const MxcTestNodeCreateInfo* pCreateInfo, MxcTestNode* pTestNode) {
  {  // Create
    CreateNodeProcessSetLayout(&pTestNode->nodeProcessSetLayout);
    CreateNodeProcessPipeLayout(pTestNode->nodeProcessSetLayout, &pTestNode->nodeProcessPipeLayout);
//    CreateNodeProcessPipe("./shaders/node_process_blit.comp.spv", pTestNode->nodeProcessPipeLayout, &pTestNode->nodeProcessBlit);
    CreateNodeProcessPipe("./shaders/node_process_blitup.comp.spv", pTestNode->nodeProcessPipeLayout, &pTestNode->nodeProcessBlitMipAveragePipe);
    CreateNodeProcessPipe("./shaders/node_process_blitdown.comp.spv", pTestNode->nodeProcessPipeLayout, &pTestNode->nodeProcessBlitDownPipe);
//    vkmCreateAllocBindMapBuffer(VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(VkmGlobalSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VKM_LOCALITY_CONTEXT, &pTestNode->nodeProcessUBOMemory, &pTestNode->nodeProcessUBOBuffer, (void**)&pTestNode->pNodeProcessUBOMapped);

    vkmCreateNodeFramebufferImport(context.stdRenderPass, VKM_LOCALITY_CONTEXT, pCreateInfo->pFramebuffers, pTestNode->framebuffers);

    for (uint32_t bufferIndex = 0; bufferIndex < VKM_SWAP_COUNT; ++bufferIndex) {
      for (uint32_t mipIndex = 0; mipIndex < VKM_G_BUFFER_LEVELS; ++mipIndex) {
        const VkImageViewCreateInfo imageViewCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = pTestNode->framebuffers[bufferIndex].gBuffer.img,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VKM_G_BUFFER_FORMAT,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = mipIndex,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        VKM_REQUIRE(vkCreateImageView(context.device, &imageViewCreateInfo, VKM_ALLOC, &pTestNode->gBufferMipViews[bufferIndex][mipIndex]));
      }
    }

    vkmCreateGlobalSet(&pTestNode->globalSet);
    memcpy(pTestNode->globalSet.pMapped, &context.globalSetState, sizeof(VkmGlobalSetState));

    vkmAllocateDescriptorSet(context.descriptorPool, &context.stdPipe.materialSetLayout, &pTestNode->checkerMaterialSet);
    vkmCreateTextureFromFile("textures/uvgrid.jpg", &pTestNode->checkerTexture);

    vkmAllocateDescriptorSet(context.descriptorPool, &context.stdPipe.objectSetLayout, &pTestNode->sphereObjectSet);
    vkmCreateAllocBindMapBuffer(VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(VkmStdObjectSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VKM_LOCALITY_CONTEXT, &pTestNode->sphereObjectSetMemory, &pTestNode->sphereObjectSetBuffer, (void**)&pTestNode->pSphereObjectSetMapped);

    const VkWriteDescriptorSet writeSets[] = {
        VKM_SET_WRITE_STD_MATERIAL_IMAGE(pTestNode->checkerMaterialSet, pTestNode->checkerTexture.view),
        VKM_SET_WRITE_STD_OBJECT_BUFFER(pTestNode->sphereObjectSet, pTestNode->sphereObjectSetBuffer),
    };
    vkUpdateDescriptorSets(context.device, COUNT(writeSets), writeSets, 0, NULL);

    pTestNode->sphereTransform = pCreateInfo->transform;  // pcreateinfo->transform should go to some kind of node transform
    vkmUpdateObjectSet(&pTestNode->sphereTransform, &pTestNode->sphereObjectState, pTestNode->pSphereObjectSetMapped);

    CreateSphereMesh(0.5, 32, 32, &pTestNode->sphereMesh);

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

  {  // Copy needed state
    pTestNode->device = context.device;
    pTestNode->stdRenderPass = context.stdRenderPass;
    pTestNode->stdPipelineLayout = context.stdPipe.pipeLayout;
    pTestNode->stdPipeline = context.stdPipe.pipe;
    pTestNode->queueIndex = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index;
  }
}

void mxcRunTestNode(const MxcNodeContext* pNodeContext) {

  MxcTestNode* pNode = (MxcTestNode*)pNodeContext->pNode;

  MxcNodeType     nodeType = pNodeContext->nodeType;
  NodeHandle      handle = 0;
  VkCommandBuffer cmd = pNode->cmd;

  VkmGlobalSetState* pGlobalSetMapped = pNode->globalSet.pMapped;
  VkDescriptorSet    globalSet = pNode->globalSet.set;
  VkDescriptorSet    checkerMaterialSet = pNode->checkerMaterialSet;
  VkDescriptorSet    sphereObjectSet = pNode->sphereObjectSet;
  VkRenderPass       stdRenderPass = pNode->stdRenderPass;
  VkPipelineLayout   stdPipeLayout = pNode->stdPipelineLayout;
  VkPipeline         stdPipeline = pNode->stdPipeline;
  VkPipelineLayout   nodeProcessPipeLayout = pNode->nodeProcessPipeLayout;
//  VkPipeline         nodeProcessBlit = pNode->nodeProcessBlit;
  VkPipeline         nodeProcessBlitMipAveragePipe = pNode->nodeProcessBlitMipAveragePipe;
  VkPipeline         nodeProcessBlitDownPipe = pNode->nodeProcessBlitDownPipe;
//  NodeProcessUBO*    pNodeProcessUBOMapped = pNode->pNodeProcessUBOMapped;
//  VkBuffer           nodeProcessUBOBuffer = pNode->nodeProcessUBOBuffer;

  // these shoudl go into a struct so all the images from one frame are side by side
  VkFramebuffer framebuffers[VKM_SWAP_COUNT];
  VkImage       framebufferColorImgs[VKM_SWAP_COUNT];
  VkImage       framebufferNormalImgs[VKM_SWAP_COUNT];
  VkImage       framebufferDepthImgs[VKM_SWAP_COUNT];
  VkImage       framebufferGBufferImgs[VKM_SWAP_COUNT];
  VkImageView   framebufferDepthImgViews[VKM_SWAP_COUNT];
  VkImageView   framebufferGBufferImgViews[VKM_SWAP_COUNT];
  for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
    framebuffers[i] = pNode->framebuffers[i].framebuffer;
    framebufferColorImgs[i] = pNode->framebuffers[i].color.img;
    framebufferNormalImgs[i] = pNode->framebuffers[i].normal.img;
    framebufferDepthImgs[i] = pNode->framebuffers[i].depth.img;
    framebufferGBufferImgs[i] = pNode->framebuffers[i].gBuffer.img;

    framebufferDepthImgViews[i] = pNode->framebuffers[i].depth.view;
    framebufferGBufferImgViews[i] = pNode->framebuffers[i].gBuffer.view;
  }
  VkImageView gBufferMipViews[VKM_SWAP_COUNT][VKM_G_BUFFER_LEVELS];
  memcpy(&gBufferMipViews, &pNode->gBufferMipViews, sizeof(gBufferMipViews));

  uint32_t     sphereIndexCount = pNode->sphereMesh.indexCount;
  VkBuffer     sphereBuffer = pNode->sphereMesh.buffer;
  VkDeviceSize sphereIndexOffset = pNode->sphereMesh.indexOffset;
  VkDeviceSize sphereVertexOffset = pNode->sphereMesh.vertexOffset;

  VkDevice device = pNode->device;
  uint32_t queueIndex = pNode->queueIndex;

  VkmTimeline compTimeline = {.semaphore = pNodeContext->compTimeline};
  VkmTimeline nodeTimeline = {.semaphore = pNodeContext->nodeTimeline};
  uint64_t    compBaseCycleValue;
  uint64_t    compCyclesToSkip;

  // set timeline value to start of next cycle
  vkmTimelineSync(device, &compTimeline);
  compBaseCycleValue = compTimeline.value - (compTimeline.value % MXC_CYCLE_COUNT);

  // wait on next cycle
  compCyclesToSkip = MXC_CYCLE_COUNT * pNodeContext->compCycleSkip;
  compTimeline.value = compCyclesToSkip;

  assert(__atomic_always_lock_free(sizeof(nodesShared[handle].pendingTimelineSignal), &nodesShared[handle].pendingTimelineSignal));
  assert(__atomic_always_lock_free(sizeof(nodesShared[handle].currentTimelineSignal), &nodesShared[handle].currentTimelineSignal));

  VKM_DEVICE_FUNC(ResetCommandBuffer);
  VKM_DEVICE_FUNC(BeginCommandBuffer);
  VKM_DEVICE_FUNC(CmdSetViewport);
  VKM_DEVICE_FUNC(CmdSetScissor);
  VKM_DEVICE_FUNC(CmdBindPipeline);
  VKM_DEVICE_FUNC(CmdDispatch);
  VKM_DEVICE_FUNC(CmdBindDescriptorSets);
  VKM_DEVICE_FUNC(CmdBindVertexBuffers);
  VKM_DEVICE_FUNC(CmdBindIndexBuffer);
  VKM_DEVICE_FUNC(CmdDrawIndexed);
  VKM_DEVICE_FUNC(CmdEndRenderPass);
  VKM_DEVICE_FUNC(EndCommandBuffer);
  VKM_DEVICE_FUNC(CmdPipelineBarrier2);
  VKM_DEVICE_FUNC(CmdPushDescriptorSetKHR);
  VKM_DEVICE_FUNC(CmdClearColorImage);

run_loop:

  // Wait for next render cycle
  compTimeline.value = compBaseCycleValue + MXC_CYCLE_RENDER;
  vkmTimelineWait(device, &compTimeline);

  memcpy(pGlobalSetMapped, (void*)&nodesShared[handle].globalSetState, sizeof(VkmGlobalSetState));
  __atomic_thread_fence(__ATOMIC_ACQUIRE);

  ResetCommandBuffer(cmd, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
  BeginCommandBuffer(cmd, &(const VkCommandBufferBeginInfo){.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT});

  const VkViewport viewport = {
      .x = CalcViewport(nodesShared[handle].ulUV.x, nodesShared[handle].lrUV.x, nodesShared[handle].globalSetState.framebufferSize.x, DEFAULT_WIDTH),
      .y = CalcViewport(nodesShared[handle].ulUV.y, nodesShared[handle].lrUV.y, nodesShared[handle].globalSetState.framebufferSize.y, DEFAULT_HEIGHT),
      .width = DEFAULT_WIDTH,
      .height = DEFAULT_HEIGHT,
      .maxDepth = 1.0f,
  };
  const VkRect2D scissor = {
      .extent = {
          .width = nodesShared[handle].globalSetState.framebufferSize.x > DEFAULT_WIDTH ? DEFAULT_WIDTH : nodesShared[handle].globalSetState.framebufferSize.x,
          .height = nodesShared[handle].globalSetState.framebufferSize.y > DEFAULT_HEIGHT ? DEFAULT_HEIGHT : nodesShared[handle].globalSetState.framebufferSize.y,
      },
  };
  CmdSetViewport(cmd, 0, 1, &viewport);
  CmdSetScissor(cmd, 0, 1, &scissor);

  const int framebufferIndex = nodeTimeline.value % VKM_SWAP_COUNT;

  switch (nodeType) {
    case MXC_NODE_TYPE_THREAD:
      //      CmdPipelineImageBarrier(cmd, &VKM_COLOR_IMAGE_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, frameBufferColorImages[framebufferIndex]));
      break;
    case MXC_NODE_TYPE_INTERPROCESS:
      CmdPipelineImageBarrier2(cmd, &VKM_IMAGE_BARRIER_TRANSFER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, framebufferColorImgs[framebufferIndex], VK_QUEUE_FAMILY_EXTERNAL, queueIndex));
      break;
    default: PANIC("nodeType not supported");
  }

  {  // this is really all that'd be user exposed....
    vkmCmdBeginPass(cmd, stdRenderPass, (VkClearColorValue){0, 0, 0.1, 0}, framebuffers[framebufferIndex]);

    CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, stdPipeline);
    CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, stdPipeLayout, VKM_PIPE_SET_STD_GLOBAL_INDEX, 1, &globalSet, 0, NULL);
    CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, stdPipeLayout, VKM_PIPE_SET_STD_MATERIAL_INDEX, 1, &checkerMaterialSet, 0, NULL);
    CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, stdPipeLayout, VKM_PIPE_SET_STD_OBJECT_INDEX, 1, &sphereObjectSet, 0, NULL);

    CmdBindVertexBuffers(cmd, 0, 1, (const VkBuffer[]){sphereBuffer}, (const VkDeviceSize[]){sphereVertexOffset});
    CmdBindIndexBuffer(cmd, sphereBuffer, sphereIndexOffset, VK_INDEX_TYPE_UINT16);
    CmdDrawIndexed(cmd, sphereIndexCount, 1, 0, 0, 0);

    CmdEndRenderPass(cmd);
  }

  {  // Blit Framebuffer
    const VkImageMemoryBarrier2 blitBarrier[] = {
        VKM_IMAGE_BARRIER(VKM_DEPTH_ATTACHMENT_IMAGE_BARRIER, VKM_IMAGE_BARRIER_COMPUTE_READ, VK_IMAGE_ASPECT_DEPTH_BIT, framebufferDepthImgs[framebufferIndex]),
        VKM_COLOR_IMAGE_BARRIER_MIP(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMAGE_BARRIER_TRANSFER_DST_GENERAL, framebufferGBufferImgs[framebufferIndex], 0, VKM_G_BUFFER_LEVELS),
    };
    CmdPipelineImageBarriers2(cmd, COUNT(blitBarrier), blitBarrier);
    CmdClearColorImage(cmd, framebufferGBufferImgs[framebufferIndex], VK_IMAGE_LAYOUT_GENERAL, &MXC_NODE_CLEAR_COLOR, 1, &(const VkImageSubresourceRange){.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = VKM_G_BUFFER_LEVELS, .layerCount = 1});
    CmdPipelineImageBarrier2(cmd, &VKM_COLOR_IMAGE_BARRIER_MIP(VKM_IMAGE_BARRIER_TRANSFER_DST_GENERAL, VKM_IMAGE_BARRIER_COMPUTE_WRITE, framebufferGBufferImgs[framebufferIndex], 0, VKM_G_BUFFER_LEVELS));
    const VkWriteDescriptorSet initialPushSet[] = {
        SET_WRITE_NODE_PROCESS_SRC(framebufferDepthImgViews[framebufferIndex]),
        SET_WRITE_NODE_PROCESS_DST(framebufferGBufferImgViews[framebufferIndex]),
    };
    CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeProcessBlitMipAveragePipe);
    CmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeProcessPipeLayout, PIPE_SET_NODE_PROCESS_INDEX, COUNT(initialPushSet), initialPushSet);
    const ivec2 extent = {scissor.extent.width, scissor.extent.height};
    const ivec2 groupCount = iVec2Min(iVec2CeiDivide(extent, 32), 1);
    CmdDispatch(cmd, groupCount.x, groupCount.y, 1);

    for (uint32_t i = 1; i < VKM_G_BUFFER_LEVELS; ++i) {
      // contain these barriers to the mip level
      CmdPipelineImageBarrier2(cmd, &VKM_COLOR_IMAGE_BARRIER_MIP(VKM_IMAGE_BARRIER_COMPUTE_WRITE, VKM_IMAGE_BARRIER_COMPUTE_READ, framebufferGBufferImgs[framebufferIndex], i - 1, 1));
      CmdPipelineImageBarrier2(cmd, &VKM_COLOR_IMAGE_BARRIER_MIP(VKM_IMAGE_BARRIER_COMPUTE_READ, VKM_IMAGE_BARRIER_COMPUTE_WRITE, framebufferGBufferImgs[framebufferIndex], i, 1));
      const VkWriteDescriptorSet pushSet[] = {
          SET_WRITE_NODE_PROCESS_SRC(gBufferMipViews[framebufferIndex][i - 1]),
          SET_WRITE_NODE_PROCESS_DST(gBufferMipViews[framebufferIndex][i]),
      };
      CmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeProcessPipeLayout, PIPE_SET_NODE_PROCESS_INDEX, COUNT(pushSet), pushSet);
      const ivec2 mipGroupCount = iVec2Min(iVec2CeiDivide((ivec2){extent.vec >> 1}, 32), 1);
      CmdDispatch(cmd, mipGroupCount.x, mipGroupCount.y, 1);
    }

    CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeProcessBlitDownPipe);
    for (uint32_t i = VKM_G_BUFFER_LEVELS - 1; i > 0; --i) {
      CmdPipelineImageBarrier2(cmd, &VKM_COLOR_IMAGE_BARRIER_MIP(VKM_IMAGE_BARRIER_COMPUTE_WRITE, VKM_IMAGE_BARRIER_COMPUTE_READ, framebufferGBufferImgs[framebufferIndex], i, 1));
      CmdPipelineImageBarrier2(cmd, &VKM_COLOR_IMAGE_BARRIER_MIP(VKM_IMAGE_BARRIER_COMPUTE_READ, VKM_IMAGE_BARRIER_COMPUTE_WRITE, framebufferGBufferImgs[framebufferIndex], i - 1, 1));
      const VkWriteDescriptorSet pushSet[] = {
          SET_WRITE_NODE_PROCESS_SRC(gBufferMipViews[framebufferIndex][i]),
          SET_WRITE_NODE_PROCESS_DST(gBufferMipViews[framebufferIndex][i - 1]),
      };
      CmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeProcessPipeLayout, PIPE_SET_NODE_PROCESS_INDEX, COUNT(pushSet), pushSet);
      const ivec2 mipGroupCount = iVec2Min(iVec2CeiDivide((ivec2){extent.vec >> (1 - 1)}, 32), 1);
      CmdDispatch(cmd, mipGroupCount.x, mipGroupCount.y, 1);
    }
  }

  switch (nodeType) {
    case MXC_NODE_TYPE_THREAD:
      const VkImageMemoryBarrier2 barriers[] = {
          VKM_COLOR_IMAGE_BARRIER(VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VKM_IMAGE_BARRIER_EXTERNAL_RELEASE_GRAPHICS_READ, framebufferColorImgs[framebufferIndex]),
          VKM_COLOR_IMAGE_BARRIER(VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VKM_IMAGE_BARRIER_EXTERNAL_RELEASE_GRAPHICS_READ, framebufferNormalImgs[framebufferIndex]),
          VKM_COLOR_IMAGE_BARRIER_MIP(VKM_IMAGE_BARRIER_COMPUTE_WRITE, VKM_IMAGE_BARRIER_EXTERNAL_RELEASE_GRAPHICS_READ, framebufferGBufferImgs[framebufferIndex], 0, VKM_G_BUFFER_LEVELS),
      };
      CmdPipelineImageBarriers2(cmd, COUNT(barriers), barriers);
      break;
    case MXC_NODE_TYPE_INTERPROCESS:
      //      CmdPipelineImageBarrier(cmd, &VKM_IMAGE_BARRIER_TRANSFER(VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VKM_IMAGE_BARRIER_EXTERNAL_RELEASE_GRAPHICS_READ, VK_IMAGE_ASPECT_COLOR_BIT, frameBufferColorImages[framebufferIndex], queueIndex, VK_QUEUE_FAMILY_EXTERNAL));
      break;
    default: PANIC("nodeType not supported");
  }

  EndCommandBuffer(cmd);

  nodeTimeline.value++;
  __atomic_thread_fence(__ATOMIC_RELEASE);
  nodesShared[handle].pendingTimelineSignal = nodeTimeline.value;

  vkmTimelineWait(device, &nodeTimeline);
  __atomic_thread_fence(__ATOMIC_RELEASE);
  nodesShared[handle].currentTimelineSignal = nodeTimeline.value;

  compBaseCycleValue += compCyclesToSkip;

//  _Thread_local static int count;
//  if (count++ > 10)
//    return;

  CHECK_RUNNING
  goto run_loop;
}