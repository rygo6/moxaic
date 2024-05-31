#include "test_node.h"
#include <assert.h>


enum SetBindNodeProcessIndices {
  SET_BIND_NODE_PROCESS_SRC_INDEX,
  SET_BIND_NODE_PROCESS_DST_INDEX,
  SET_BIND_NODE_PROCESS_COUNT
};
#define SET_WRITE_NODE_GBUFFER_SRC(dst_set, src_image_view)      \
  (VkWriteDescriptorSet) {                                       \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,             \
    .dstSet = dst_set,                                           \
    .dstBinding = SET_BIND_NODE_GBUFFER_SRC_INDEX,               \
    .descriptorCount = 1,                                        \
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
    .pImageInfo = &(const VkDescriptorImageInfo){                \
        .imageView = src_image_view,                             \
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,                  \
    },                                                           \
  }
#define SET_WRITE_NODE_GBUFFER_DST(dst_set, dst_image_view) \
  (VkWriteDescriptorSet) {                                  \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,        \
    .dstSet = dst_set,                                      \
    .dstBinding = SET_BIND_NODE_GBUFFER_DST_INDEX,          \
    .descriptorCount = 1,                                   \
    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,     \
    .pImageInfo = &(const VkDescriptorImageInfo){           \
        .imageView = dst_image_view,                        \
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,             \
    },                                                      \
  }
static void CreateNodeProcessSetLayout(VkDescriptorSetLayout* pLayout) {
  const VkDescriptorSetLayoutCreateInfo nodeSetLayoutCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
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
    CreateNodeProcessPipe("./shaders/node_process_blitdown.comp.spv", pTestNode->nodeProcessPipeLayout, &pTestNode->nodeProcessPipe);


    vkmCreateNodeFramebufferImport(context.stdRenderPass, VKM_LOCALITY_CONTEXT, pCreateInfo->pFramebuffers, pTestNode->framebuffers);

    vkmCreateGlobalSet(&pTestNode->globalSet);
    memcpy(pTestNode->globalSet.pMapped, &context.globalSetState, sizeof(VkmGlobalSetState));

    vkmAllocateDescriptorSet(context.descriptorPool, &context.stdPipe.materialSetLayout, &pTestNode->checkerMaterialSet);
    vkmCreateTextureFromFile("textures/uvgrid.jpg", &pTestNode->checkerTexture);

    vkmAllocateDescriptorSet(context.descriptorPool, &context.stdPipe.objectSetLayout, &pTestNode->sphereObjectSet);
    vkmCreateAllocBindMapBuffer(VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(VkmStdObjectSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VKM_LOCALITY_CONTEXT, &pTestNode->sphereObjectSetMemory, &pTestNode->sphereObjectSetBuffer, (void**)&pTestNode->pSphereObjectSetMapped);

    const VkWriteDescriptorSet writeSets[] = {
        VKM_SET_WRITE_STD_MATERIAL_IMAGE(pTestNode->checkerMaterialSet, pTestNode->checkerTexture.imageView),
        VKM_SET_WRITE_STD_OBJECT_BUFFER(pTestNode->sphereObjectSet, pTestNode->sphereObjectSetBuffer),
    };
    vkUpdateDescriptorSets(context.device, COUNT(writeSets), writeSets, 0, NULL);

    pTestNode->sphereTransform = pCreateInfo->transform;
    vkmUpdateObjectSet(&pTestNode->sphereTransform, &pTestNode->sphereObjectState, pTestNode->pSphereObjectSetMapped);

    CreateSphereMesh(0.5, 32, 32, &pTestNode->sphereMesh);
    //    CreateQuadMesh(0.5, &pTestNode->sphereMesh);

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
    pTestNode->standardRenderPass = context.stdRenderPass;
    pTestNode->standardPipelineLayout = context.stdPipe.pipeLayout;
    pTestNode->standardPipeline = context.stdPipe.pipe;
    pTestNode->queueIndex = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index;
  }
}

void mxcRunTestNode(const MxcNodeContext* pNodeContext) {

  MxcTestNode* pNode = (MxcTestNode*)pNodeContext->pNode;

  MxcNodeType     nodeType = pNodeContext->nodeType;
  mxc_node_handle handle = 0;
  VkCommandBuffer cmd = pNode->cmd;

  VkmGlobalSetState* pGlobalSetMapped = pNode->globalSet.pMapped;
  VkDescriptorSet    globalSet = pNode->globalSet.set;
  VkDescriptorSet    checkerMaterialSet = pNode->checkerMaterialSet;
  VkDescriptorSet    sphereObjectSet = pNode->sphereObjectSet;
  VkRenderPass       standardRenderPass = pNode->standardRenderPass;
  VkPipelineLayout   standardPipelineLayout = pNode->standardPipelineLayout;
  VkPipeline         standardPipeline = pNode->standardPipeline;

  VkFramebuffer framebuffers[VKM_SWAP_COUNT];
  VkImage       frameBufferColorImages[VKM_SWAP_COUNT];
  VkImage       frameBufferNormalImages[VKM_SWAP_COUNT];
  VkImage       frameBufferDepthImages[VKM_SWAP_COUNT];
  VkImage       frameBufferGBufferImages[VKM_SWAP_COUNT];
  for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
    framebuffers[i] = pNode->framebuffers[i].framebuffer;
    frameBufferColorImages[i] = pNode->framebuffers[i].color.image;
    frameBufferNormalImages[i] = pNode->framebuffers[i].normal.image;
    frameBufferDepthImages[i] = pNode->framebuffers[i].depth.image;
    frameBufferGBufferImages[i] = pNode->framebuffers[i].gBuffer.image;
  }

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

  assert(__atomic_always_lock_free(sizeof(MXC_NODE_SHARED[handle].pendingTimelineSignal), &MXC_NODE_SHARED[handle].pendingTimelineSignal));
  assert(__atomic_always_lock_free(sizeof(MXC_NODE_SHARED[handle].currentTimelineSignal), &MXC_NODE_SHARED[handle].currentTimelineSignal));

  VKM_DEVICE_FUNC(ResetCommandBuffer);
  VKM_DEVICE_FUNC(BeginCommandBuffer);
  VKM_DEVICE_FUNC(CmdSetViewport);
  VKM_DEVICE_FUNC(CmdSetScissor);
  VKM_DEVICE_FUNC(CmdBindPipeline);
  VKM_DEVICE_FUNC(CmdBindDescriptorSets);
  VKM_DEVICE_FUNC(CmdBindVertexBuffers);
  VKM_DEVICE_FUNC(CmdBindIndexBuffer);
  VKM_DEVICE_FUNC(CmdDrawIndexed);
  VKM_DEVICE_FUNC(CmdEndRenderPass);
  VKM_DEVICE_FUNC(EndCommandBuffer);
  VKM_DEVICE_FUNC(CmdPipelineBarrier2);

run_loop:

  // Wait for next render cycle
  compTimeline.value = compBaseCycleValue + MXC_CYCLE_RENDER;
  vkmTimelineWait(device, &compTimeline);

  memcpy(pGlobalSetMapped, (void*)&MXC_NODE_SHARED[handle].globalSetState, sizeof(VkmGlobalSetState));
  __atomic_thread_fence(__ATOMIC_ACQUIRE);

  ResetCommandBuffer(cmd, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
  BeginCommandBuffer(cmd, &(const VkCommandBufferBeginInfo){.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT});

  const VkViewport viewport = {
      .x = CalcViewport(MXC_NODE_SHARED[handle].ulUV.x, MXC_NODE_SHARED[handle].lrUV.x, MXC_NODE_SHARED[handle].globalSetState.framebufferSize.x, DEFAULT_WIDTH),
      .y = CalcViewport(MXC_NODE_SHARED[handle].ulUV.y, MXC_NODE_SHARED[handle].lrUV.y, MXC_NODE_SHARED[handle].globalSetState.framebufferSize.y, DEFAULT_HEIGHT),
      .width = DEFAULT_WIDTH,
      .height = DEFAULT_HEIGHT,
      .maxDepth = 1.0f,
  };
  CmdSetViewport(cmd, 0, 1, &viewport);
  const VkRect2D scissor = {
      .extent = {
          .width = MXC_NODE_SHARED[handle].globalSetState.framebufferSize.x > DEFAULT_WIDTH ? DEFAULT_WIDTH : MXC_NODE_SHARED[handle].globalSetState.framebufferSize.x,
          .height = MXC_NODE_SHARED[handle].globalSetState.framebufferSize.y > DEFAULT_HEIGHT ? DEFAULT_HEIGHT : MXC_NODE_SHARED[handle].globalSetState.framebufferSize.y,
      },
  };
  CmdSetScissor(cmd, 0, 1, &scissor);

  const int framebufferIndex = nodeTimeline.value % VKM_SWAP_COUNT;

  switch (nodeType) {
    case MXC_NODE_TYPE_THREAD:
      CmdPipelineImageBarrier(cmd, &VKM_COLOR_IMAGE_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, frameBufferGBufferImages[framebufferIndex]));
      break;
    case MXC_NODE_TYPE_INTERPROCESS:
      CmdPipelineImageBarrier(cmd, &VKM_IMAGE_BARRIER_TRANSFER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, frameBufferColorImages[framebufferIndex], VK_QUEUE_FAMILY_EXTERNAL, queueIndex));
      break;
    default:
      PANIC("nodeType not supported");
  }

  {  // this is really all that'd be user exposed....
    vkmCmdBeginPass(cmd, standardRenderPass, (VkClearColorValue){0, 0, 0.1, 0}, framebuffers[framebufferIndex]);

    CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, standardPipeline);
    CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, standardPipelineLayout, VKM_PIPE_SET_STD_GLOBAL_INDEX, 1, &globalSet, 0, NULL);
    CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, standardPipelineLayout, VKM_PIPE_SET_STD_MATERIAL_INDEX, 1, &checkerMaterialSet, 0, NULL);
    CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, standardPipelineLayout, VKM_PIPE_SET_STD_OBJECT_INDEX, 1, &sphereObjectSet, 0, NULL);

    CmdBindVertexBuffers(cmd, 0, 1, (const VkBuffer[]){sphereBuffer}, (const VkDeviceSize[]){sphereVertexOffset});
    CmdBindIndexBuffer(cmd, sphereBuffer, sphereIndexOffset, VK_INDEX_TYPE_UINT16);
    CmdDrawIndexed(cmd, sphereIndexCount, 1, 0, 0, 0);

    CmdEndRenderPass(cmd);
  }

  {  // Blit Framebuffer
    const VkImageMemoryBarrier2 blitBarrier[] = {
        VKM_IMAGE_BARRIER(VKM_DEPTH_ATTACHMENT_IMAGE_BARRIER, VKM_TRANSFER_SRC_IMAGE_BARRIER, VK_IMAGE_ASPECT_DEPTH_BIT, frameBufferDepthImages[framebufferIndex]),
        VKM_COLOR_IMAGE_BARRIER(VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VKM_TRANSFER_DST_IMAGE_BARRIER, frameBufferGBufferImages[framebufferIndex]),
    };
    CmdPipelineImageBarriers(cmd, 2, blitBarrier);
    vkmBlit(cmd, frameBufferDepthImages[framebufferIndex], frameBufferGBufferImages[framebufferIndex]);
    //    const VkImageMemoryBarrier2 presentBarrier[] = {
    //        //        VKM_IMAGE_BARRIER(VKM_TRANSFER_SRC_IMAGE_BARRIER, VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, framebufferColorImage),
    //        VKM_COLOR_IMAGE_BARRIER(VKM_TRANSFER_DST_IMAGE_BARRIER, VKM_IMAGE_BARRIER_PRESENT, swapImage),
    //    };
    //    CmdPipelineImageBarriers(cmd, 1, presentBarrier);
  }

  switch (nodeType) {
    case MXC_NODE_TYPE_THREAD:
      const VkImageMemoryBarrier2 barriers[] = {
          VKM_COLOR_IMAGE_BARRIER(VKM_TRANSFER_DST_IMAGE_BARRIER, VKM_IMAGE_BARRIER_EXTERNAL_RELEASE_GRAPHICS_READ, frameBufferColorImages[framebufferIndex]),
          VKM_COLOR_IMAGE_BARRIER(VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VKM_IMAGE_BARRIER_EXTERNAL_RELEASE_GRAPHICS_READ, frameBufferNormalImages[framebufferIndex]),
          VKM_COLOR_IMAGE_BARRIER(VKM_TRANSFER_SRC_IMAGE_BARRIER, VKM_IMAGE_BARRIER_EXTERNAL_RELEASE_GRAPHICS_READ, frameBufferGBufferImages[framebufferIndex]),
      };
      CmdPipelineImageBarriers(cmd, COUNT(barriers), barriers);
      break;
    case MXC_NODE_TYPE_INTERPROCESS:
      CmdPipelineImageBarrier(cmd, &VKM_IMAGE_BARRIER_TRANSFER(VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VKM_IMAGE_BARRIER_EXTERNAL_RELEASE_GRAPHICS_READ, VK_IMAGE_ASPECT_COLOR_BIT, frameBufferColorImages[framebufferIndex], queueIndex, VK_QUEUE_FAMILY_EXTERNAL));
      break;
    default:
      PANIC("nodeType not supported");
  }

  EndCommandBuffer(cmd);

  nodeTimeline.value++;
  __atomic_thread_fence(__ATOMIC_RELEASE);
  MXC_NODE_SHARED[handle].pendingTimelineSignal = nodeTimeline.value;

  vkmTimelineWait(device, &nodeTimeline);
  __atomic_thread_fence(__ATOMIC_RELEASE);
  MXC_NODE_SHARED[handle].currentTimelineSignal = nodeTimeline.value;

  compBaseCycleValue += compCyclesToSkip;

  CHECK_RUNNING
  goto run_loop;
}