#include "test_node.h"
#include "mid_shape.h"
#include <assert.h>

enum SetBindNodeProcessIndices {
  SET_BIND_NODE_PROCESS_SRC_INDEX,
  SET_BIND_NODE_PROCESS_DST_INDEX,
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
enum PipeSetNodeProcessIndices {
  PIPE_SET_NODE_PROCESS_INDEX,
  PIPE_SET_NODE_PROCESS_COUNT,
};

void mxcTestNodeRun(const MxcNodeContext* pNodeContext, const MxcTestNode* pNode) {

  const MxcNodeType     nodeType = pNodeContext->nodeType;   // figure out how to get rid of this

  const NodeHandle      handle = 0; // need to actually set this handle? or should just pass pointer? pasisng in pointer might be better for switching between process/thread

  const VkCommandBuffer cmd = pNodeContext->cmd;
  const VkRenderPass    nodeRenderPass = pNode->nodeRenderPass;
  const VkFramebuffer   framebuffer = pNode->framebuffer;

  const VkmGlobalSetState* pGlobalSetMapped = pNode->globalSet.pMapped;
  const VkDescriptorSet    globalSet = pNode->globalSet.set;

  const VkDescriptorSet  checkerMaterialSet = pNode->checkerMaterialSet;
  const VkDescriptorSet  sphereObjectSet = pNode->sphereObjectSet;
  const VkPipelineLayout pipeLayout = pNode->pipeLayout;
  const VkPipeline       pipe = pNode->basicPipe;
  const VkPipelineLayout nodeProcessPipeLayout = pNode->nodeProcessPipeLayout;
  const VkPipeline       nodeProcessBlitMipAveragePipe = pNode->nodeProcessBlitMipAveragePipe;
  const VkPipeline       nodeProcessBlitDownPipe = pNode->nodeProcessBlitDownPipe;

  struct {
    VkImage     color;
    VkImage     normal;
    VkImage     gBuffer;
    VkImage     depth;
    VkImageView colorView;
    VkImageView normalView;
    VkImageView depthView;
    VkImageView gBufferView;
  } framebufferImages[MIDVK_SWAP_COUNT];
  for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
    framebufferImages[i].color = pNodeContext->framebufferTextures[i].color.image;
    framebufferImages[i].normal = pNodeContext->framebufferTextures[i].normal.image;
    framebufferImages[i].gBuffer = pNodeContext->framebufferTextures[i].gbuffer.image;
    framebufferImages[i].depth = pNodeContext->framebufferTextures[i].depth.image;
    framebufferImages[i].colorView = pNodeContext->framebufferTextures[i].color.view;
    framebufferImages[i].normalView = pNodeContext->framebufferTextures[i].normal.view;
    framebufferImages[i].depthView = pNodeContext->framebufferTextures[i].depth.view;
    framebufferImages[i].gBufferView = pNodeContext->framebufferTextures[i].gbuffer.view;
  }
  VkImageView gBufferMipViews[MIDVK_SWAP_COUNT][MXC_NODE_GBUFFER_LEVELS];
  memcpy(&gBufferMipViews, &pNode->gBufferMipViews, sizeof(gBufferMipViews));

  const uint32_t     sphereIndexCount = pNode->sphereMesh.indexCount;
  const VkBuffer     sphereBuffer = pNode->sphereMesh.buffer;
  const VkDeviceSize sphereIndexOffset = pNode->sphereMesh.indexOffset;
  const VkDeviceSize sphereVertexOffset = pNode->sphereMesh.vertexOffset;

  const VkDevice device = pNode->device;

  const VkSemaphore compTimeline = pNodeContext->compTimeline;
  const VkSemaphore nodeTimeline = pNodeContext->nodeTimeline;

  uint64_t nodeTimelineValue;
  uint64_t compBaseCycleValue ;
  MIDVK_REQUIRE(vkGetSemaphoreCounterValue(device, compTimeline, &nodeTimelineValue));
  REQUIRE(nodeTimelineValue != 0xffffffffffffffff, "NodeTimeline imported as max value!");
  compBaseCycleValue = nodeTimelineValue - (nodeTimelineValue % MXC_CYCLE_COUNT);

  assert(__atomic_always_lock_free(sizeof(nodesShared[handle].pendingTimelineSignal), &nodesShared[handle].pendingTimelineSignal));
  assert(__atomic_always_lock_free(sizeof(nodesShared[handle].currentTimelineSignal), &nodesShared[handle].currentTimelineSignal));

  MIDVK_DEVICE_FUNC(ResetCommandBuffer);
  MIDVK_DEVICE_FUNC(BeginCommandBuffer);
  MIDVK_DEVICE_FUNC(CmdSetViewport);
  MIDVK_DEVICE_FUNC(CmdBeginRenderPass);
  MIDVK_DEVICE_FUNC(CmdSetScissor);
  MIDVK_DEVICE_FUNC(CmdBindPipeline);
  MIDVK_DEVICE_FUNC(CmdDispatch);
  MIDVK_DEVICE_FUNC(CmdBindDescriptorSets);
  MIDVK_DEVICE_FUNC(CmdBindVertexBuffers);
  MIDVK_DEVICE_FUNC(CmdBindIndexBuffer);
  MIDVK_DEVICE_FUNC(CmdDrawIndexed);
  MIDVK_DEVICE_FUNC(CmdEndRenderPass);
  MIDVK_DEVICE_FUNC(EndCommandBuffer);
  MIDVK_DEVICE_FUNC(CmdPipelineBarrier2);
  MIDVK_DEVICE_FUNC(CmdPushDescriptorSetKHR);
  MIDVK_DEVICE_FUNC(CmdClearColorImage);

run_loop:

  // Wait for next render cycle
  vkmTimelineWait(device, compBaseCycleValue + MXC_CYCLE_RECORD_COMPOSITE, compTimeline);

  __atomic_thread_fence(__ATOMIC_ACQUIRE);
  switch (nodeType) {
    case MXC_NODE_TYPE_THREAD: break;
    case MXC_NODE_TYPE_INTERPROCESS:
      //should make pointer to memory directly
      memcpy(&nodesShared[handle], (void*)&pNodeContext->pExternalMemory->nodeShared, sizeof(MxcNodeShared));
      break;
    default: PANIC("nodeType not supported");
  }
  memcpy(pGlobalSetMapped, (void*)&nodesShared[handle].globalSetState, sizeof(VkmGlobalSetState));

  ResetCommandBuffer(cmd, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
  BeginCommandBuffer(cmd, &(const VkCommandBufferBeginInfo){.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT});

  const VkViewport viewport = {
      .x = -nodesShared[handle].ulUV.x * DEFAULT_WIDTH,
      .y = -nodesShared[handle].ulUV.y * DEFAULT_HEIGHT,
      .width = DEFAULT_WIDTH,
      .height = DEFAULT_HEIGHT,
      .maxDepth = 1.0f,
  };
  const VkRect2D scissor = {
      .offset = {
          .x = 0,
          .y = 0,
      },
      .extent = {
          .width = nodesShared[handle].globalSetState.framebufferSize.x,
          .height = nodesShared[handle].globalSetState.framebufferSize.y,
      },
  };
  CmdSetViewport(cmd, 0, 1, &viewport);
  CmdSetScissor(cmd, 0, 1, &scissor);

  const int framebufferIndex = nodeTimelineValue % MIDVK_SWAP_COUNT;

  switch (nodeType) {
    case MXC_NODE_TYPE_THREAD:
      break;
    case MXC_NODE_TYPE_INTERPROCESS:
      // will need to some kind of interprocess acquire
      //      const VkImageMemoryBarrier2 barrier[] = {
      //          VKM_COLOR_IMG_BARRIER(VKM_IMG_BARRIER_UNDEFINED, VKM_IMG_BARRIER_COLOR_ATTACHMENT_WRITE, framebufferColorImgs[framebufferIndex]),
      //          VKM_COLOR_IMG_BARRIER(VKM_IMG_BARRIER_UNDEFINED, VKM_IMG_BARRIER_COLOR_ATTACHMENT_WRITE, framebufferNormalImgs[framebufferIndex]),
      //          VKM_IMG_BARRIER(VKM_IMG_BARRIER_UNDEFINED, VKM_IMG_BARRIER_DEPTH_ATTACHMENT, VK_IMAGE_ASPECT_DEPTH_BIT, framebufferDepthImgs[framebufferIndex]),
      //      };
      //      CmdPipelineImageBarriers2(cmd, COUNT(barrier), barrier);
      break;
    default: PANIC("nodeType not supported");
  }

  {  // this is really all that'd be user exposed....
    const VkClearColorValue clearColor = (VkClearColorValue){0, 0, 0.1, 0};
    CmdBeginRenderPass(cmd, nodeRenderPass, framebuffer, clearColor, framebufferImages[framebufferIndex].colorView, framebufferImages[framebufferIndex].normalView, framebufferImages[framebufferIndex].depthView);

    CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
    CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout, MIDVK_PIPE_SET_STD_GLOBAL_INDEX, 1, &globalSet, 0, NULL);
    CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout, MIDVK_PIPE_SET_STD_MATERIAL_INDEX, 1, &checkerMaterialSet, 0, NULL);
    CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeLayout, MIDVK_PIPE_SET_STD_OBJECT_INDEX, 1, &sphereObjectSet, 0, NULL);

    CmdBindVertexBuffers(cmd, 0, 1, (const VkBuffer[]){sphereBuffer}, (const VkDeviceSize[]){sphereVertexOffset});
    CmdBindIndexBuffer(cmd, sphereBuffer, sphereIndexOffset, VK_INDEX_TYPE_UINT16);
    CmdDrawIndexed(cmd, sphereIndexCount, 1, 0, 0, 0);

    CmdEndRenderPass(cmd);
  }

  {  // Blit GBuffer
    const ivec2 extent = {scissor.extent.width, scissor.extent.height};
    const ivec2 groupCount = iVec2Min(iVec2CeiDivide(extent, 32), 1);
    {
      CmdPipelineImageBarrier2(cmd, &VKM_COLOR_IMAGE_BARRIER_MIPS(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMAGE_BARRIER_TRANSFER_DST_GENERAL, framebufferImages[framebufferIndex].gBuffer, 0, MXC_NODE_GBUFFER_LEVELS));
      CmdClearColorImage(cmd, framebufferImages[framebufferIndex].gBuffer, VK_IMAGE_LAYOUT_GENERAL, &MXC_NODE_CLEAR_COLOR, 1, &(const VkImageSubresourceRange){.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = MXC_NODE_GBUFFER_LEVELS, .layerCount = 1});
      const VkImageMemoryBarrier2 barriers[] = {
          // could technically alter shader to take VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL and not need this, can different mips be in different layouts?
          VKM_IMG_BARRIER(VKM_IMG_BARRIER_COMPUTE_SHADER_READ_ONLY, VKM_IMAGE_BARRIER_COMPUTE_READ, VK_IMAGE_ASPECT_DEPTH_BIT, framebufferImages[framebufferIndex].depth),
          VKM_COLOR_IMAGE_BARRIER_MIPS(VKM_IMAGE_BARRIER_TRANSFER_DST_GENERAL, VKM_IMAGE_BARRIER_COMPUTE_WRITE, framebufferImages[framebufferIndex].gBuffer, 0, MXC_NODE_GBUFFER_LEVELS),
      };
      CmdPipelineImageBarriers2(cmd, COUNT(barriers), barriers);
      const VkWriteDescriptorSet initialPushSet[] = {
          SET_WRITE_NODE_PROCESS_SRC(framebufferImages[framebufferIndex].depthView),
          SET_WRITE_NODE_PROCESS_DST(framebufferImages[framebufferIndex].gBufferView),
      };
      CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeProcessBlitMipAveragePipe);
      CmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeProcessPipeLayout, PIPE_SET_NODE_PROCESS_INDEX, COUNT(initialPushSet), initialPushSet);
      CmdDispatch(cmd, groupCount.x, groupCount.y, 1);
    }

    for (uint32_t i = 1; i < MXC_NODE_GBUFFER_LEVELS; ++i) {
      const VkImageMemoryBarrier2 barriers[] = {
          VKM_COLOR_IMAGE_BARRIER_MIPS(VKM_IMAGE_BARRIER_COMPUTE_WRITE, VKM_IMAGE_BARRIER_COMPUTE_READ, framebufferImages[framebufferIndex].gBuffer, i - 1, 1),
          VKM_COLOR_IMAGE_BARRIER_MIPS(VKM_IMAGE_BARRIER_COMPUTE_READ, VKM_IMAGE_BARRIER_COMPUTE_WRITE, framebufferImages[framebufferIndex].gBuffer, i, 1),
      };
      CmdPipelineImageBarriers2(cmd, COUNT(barriers), barriers);
      const VkWriteDescriptorSet pushSet[] = {
          SET_WRITE_NODE_PROCESS_SRC(gBufferMipViews[framebufferIndex][i - 1]),
          SET_WRITE_NODE_PROCESS_DST(gBufferMipViews[framebufferIndex][i]),
      };
      CmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeProcessPipeLayout, PIPE_SET_NODE_PROCESS_INDEX, COUNT(pushSet), pushSet);
      const ivec2 mipGroupCount = iVec2Min(iVec2CeiDivide((ivec2){extent.vec >> 1}, 32), 1);
      CmdDispatch(cmd, mipGroupCount.x, mipGroupCount.y, 1);
    }

    CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nodeProcessBlitDownPipe);
    for (uint32_t i = MXC_NODE_GBUFFER_LEVELS - 1; i > 0; --i) {
      const VkImageMemoryBarrier2 barriers[] = {
          VKM_COLOR_IMAGE_BARRIER_MIPS(VKM_IMAGE_BARRIER_COMPUTE_WRITE, VKM_IMAGE_BARRIER_COMPUTE_READ, framebufferImages[framebufferIndex].gBuffer, i, 1),
          VKM_COLOR_IMAGE_BARRIER_MIPS(VKM_IMAGE_BARRIER_COMPUTE_READ, VKM_IMAGE_BARRIER_COMPUTE_WRITE, framebufferImages[framebufferIndex].gBuffer, i - 1, 1),
      };
      CmdPipelineImageBarriers2(cmd, COUNT(barriers), barriers);
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
      CmdPipelineImageBarrier2(cmd, &VKM_COLOR_IMAGE_BARRIER_MIPS(VKM_IMAGE_BARRIER_COMPUTE_WRITE, VKM_IMG_BARRIER_EXTERNAL_RELEASE_SHADER_READ, framebufferImages[framebufferIndex].gBuffer, 0, MXC_NODE_GBUFFER_LEVELS));
      break;
    case MXC_NODE_TYPE_INTERPROCESS:
      const VkImageMemoryBarrier2 interProcessBarriers[] = {
          VKM_COLOR_IMAGE_BARRIER(VKM_IMG_BARRIER_SHADER_READ, VKM_IMG_BARRIER_EXTERNAL_RELEASE_SHADER_READ, framebufferImages[framebufferIndex].color),
          VKM_COLOR_IMAGE_BARRIER(VKM_IMG_BARRIER_SHADER_READ, VKM_IMG_BARRIER_EXTERNAL_RELEASE_SHADER_READ, framebufferImages[framebufferIndex].normal),
          VKM_COLOR_IMAGE_BARRIER_MIPS(VKM_IMAGE_BARRIER_COMPUTE_WRITE, VKM_IMG_BARRIER_EXTERNAL_RELEASE_SHADER_READ, framebufferImages[framebufferIndex].gBuffer, 0, MXC_NODE_GBUFFER_LEVELS),
      };
      CmdPipelineImageBarriers2(cmd, COUNT(interProcessBarriers), interProcessBarriers);
      break;
    default: PANIC("nodeType not supported");
  }

  EndCommandBuffer(cmd);

  nodeTimelineValue++;

  nodesShared[handle].pendingTimelineSignal = nodeTimelineValue;
  __atomic_thread_fence(__ATOMIC_RELEASE);
  vkmTimelineWait(device, nodeTimelineValue, nodeTimeline);
  nodesShared[handle].currentTimelineSignal = nodeTimelineValue;
  switch (nodeType) {
    case MXC_NODE_TYPE_THREAD: break;
    case MXC_NODE_TYPE_INTERPROCESS:
      //should make pointer to memory directly
      pNodeContext->pExternalMemory->nodeShared.currentTimelineSignal = nodeTimelineValue;
      break;
    default: PANIC("nodeType not supported");
  }
  __atomic_thread_fence(__ATOMIC_RELEASE);

  compBaseCycleValue += MXC_CYCLE_COUNT * nodesShared[handle].compCycleSkip;

  //  _Thread_local static int count;
  //  if (count++ > 10)
  //    return;

  CHECK_RUNNING
  goto run_loop;
}

//
///Create
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
      },
  };
  MIDVK_REQUIRE(vkCreateDescriptorSetLayout(context.device, &nodeSetLayoutCreateInfo, MIDVK_ALLOC, pLayout));
}

static void CreateNodeProcessPipeLayout(const VkDescriptorSetLayout nodeProcessSetLayout, VkPipelineLayout* pPipeLayout) {
  const VkPipelineLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = (const VkDescriptorSetLayout[]){
          [PIPE_SET_NODE_PROCESS_INDEX] = nodeProcessSetLayout,
      },
  };
  MIDVK_REQUIRE(vkCreatePipelineLayout(context.device, &createInfo, MIDVK_ALLOC, pPipeLayout));
}
static void CreateNodeProcessPipe(const char* shaderPath, const VkPipelineLayout layout, VkPipeline* pPipe) {
  VkShaderModule shader;
  vkmCreateShaderModule(shaderPath, &shader);
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
  MIDVK_REQUIRE(vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, MIDVK_ALLOC, pPipe));
  vkDestroyShaderModule(context.device, shader, MIDVK_ALLOC);
}
static void mxcCreateTestNode(const MxcNodeContext* pTestNodeContext, MxcTestNode* pTestNode) {
  {  // Create
    CreateNodeProcessSetLayout(&pTestNode->nodeProcessSetLayout);
    CreateNodeProcessPipeLayout(pTestNode->nodeProcessSetLayout, &pTestNode->nodeProcessPipeLayout);
    CreateNodeProcessPipe("./shaders/node_process_blit_slope_average_up.comp.spv", pTestNode->nodeProcessPipeLayout, &pTestNode->nodeProcessBlitMipAveragePipe);
    CreateNodeProcessPipe("./shaders/node_process_blit_down_alpha_omit.comp.spv", pTestNode->nodeProcessPipeLayout, &pTestNode->nodeProcessBlitDownPipe);

    vkmCreateFramebuffer(context.nodeRenderPass, &pTestNode->framebuffer);
    vkmSetDebugName(VK_OBJECT_TYPE_FRAMEBUFFER, (uint64_t)pTestNode->framebuffer, "TestNodeFramebuffer");

    for (uint32_t bufferIndex = 0; bufferIndex < MIDVK_SWAP_COUNT; ++bufferIndex) {
      for (uint32_t mipIndex = 0; mipIndex < MXC_NODE_GBUFFER_LEVELS; ++mipIndex) {
        const VkImageViewCreateInfo imageViewCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = pTestNodeContext->framebufferTextures[bufferIndex].gbuffer.image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = MXC_NODE_GBUFFER_FORMAT,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = mipIndex,
                .levelCount = 1,
                .layerCount = 1,
            },
        };
        MIDVK_REQUIRE(vkCreateImageView(context.device, &imageViewCreateInfo, MIDVK_ALLOC, &pTestNode->gBufferMipViews[bufferIndex][mipIndex]));
      }
    }

    // This is way too many descriptors... optimize this
    const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 30,
        .poolSizeCount = 3,
        .pPoolSizes = (const VkDescriptorPoolSize[]){
            {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 10},
            {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 10},
            {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 10},
        },
    };
    MIDVK_REQUIRE(vkCreateDescriptorPool(context.device, &descriptorPoolCreateInfo, MIDVK_ALLOC, &threadContext.descriptorPool));

    vkmCreateGlobalSet(&pTestNode->globalSet);

    vkmAllocateDescriptorSet(threadContext.descriptorPool, &context.stdPipeLayout.materialSetLayout, &pTestNode->checkerMaterialSet);
    vkmCreateTextureFromFile("textures/uvgrid.jpg", &pTestNode->checkerTexture);

    vkmAllocateDescriptorSet(threadContext.descriptorPool, &context.stdPipeLayout.objectSetLayout, &pTestNode->sphereObjectSet);
    vkmCreateAllocBindMapBuffer(VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(VkmStdObjectSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, MID_LOCALITY_CONTEXT, &pTestNode->sphereObjectSetMemory, &pTestNode->sphereObjectSetBuffer, (void**)&pTestNode->pSphereObjectSetMapped);

    const VkWriteDescriptorSet writeSets[] = {
        VKM_SET_WRITE_STD_MATERIAL_IMAGE(pTestNode->checkerMaterialSet, pTestNode->checkerTexture.view),
        VKM_SET_WRITE_STD_OBJECT_BUFFER(pTestNode->sphereObjectSet, pTestNode->sphereObjectSetBuffer),
    };
    vkUpdateDescriptorSets(context.device, _countof(writeSets), writeSets, 0, NULL);

    pTestNode->sphereTransform = (MidTransform){.position = {0,0,0}};
    vkmUpdateObjectSet(&pTestNode->sphereTransform, &pTestNode->sphereObjectState, pTestNode->pSphereObjectSetMapped);

    CreateSphereMesh(0.5, 32, 32, &pTestNode->sphereMesh);
  }

  {  // Copy needed state
    pTestNode->device = context.device;
    pTestNode->nodeRenderPass = context.nodeRenderPass;
    pTestNode->pipeLayout = context.stdPipeLayout.pipeLayout;
    pTestNode->basicPipe = context.basicPipe;
    pTestNode->queueIndex = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index;
  }
}

void* mxcTestNodeThread(const MxcNodeContext* pNodeContext) {
  MxcTestNode testNode;
  mxcCreateTestNode(pNodeContext, &testNode);
  mxcTestNodeRun(pNodeContext, &testNode);
  return NULL;
}