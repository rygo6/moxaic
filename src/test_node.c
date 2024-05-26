#include "test_node.h"
#include <assert.h>
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

    vkmCreateGlobalSet(&pTestNode->globalSet);
    memcpy(pTestNode->globalSet.pMapped, &context.globalSetState, sizeof(VkmGlobalSetState));

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

  {  // Copy needed state
    pTestNode->device = context.device;
    pTestNode->standardRenderPass = context.standardRenderPass;
    pTestNode->standardPipelineLayout = context.standardPipe.pipelineLayout;
    pTestNode->standardPipeline = context.standardPipe.pipeline;
    pTestNode->queueIndex = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].index;
  }
}

_Noreturn void mxcRunTestNode(const MxcNodeContext* pNodeContext) {

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
  for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
    framebuffers[i] = pNode->framebuffers[i].framebuffer;
    frameBufferColorImages[i] = pNode->framebuffers[i].color.image;
  }

  uint32_t sphereIndexCount = pNode->sphereMesh.indexCount;
  VkBuffer sphereBuffer = pNode->sphereMesh.buffer;
  VkDeviceSize sphereIndexOffset = pNode->sphereMesh.indexOffset;
  VkDeviceSize sphereVertexOffset = pNode->sphereMesh.vertexOffset;

  VkDevice device = pNode->device;
  uint32_t queueIndex = pNode->queueIndex;

  VkmTimeline compTimeline = {.semaphore = pNodeContext->compTimeline};
  VkmTimeline nodeTimeline = {.semaphore = pNodeContext->nodeTimeline};
  uint64_t compBaseCycleValue;
  uint64_t compCyclesToSkip;

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
      .x = 0,
      .y = 0,
      .width = MXC_NODE_SHARED[handle].globalSetState.framebufferSize.x,
      .height = MXC_NODE_SHARED[handle].globalSetState.framebufferSize.y,
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  CmdSetViewport(cmd, 0, 1, &viewport);
  const VkRect2D scissor = {
      .offset = {
          .x = 0,
          .y = 0,
      },
      .extent = {
          .width = viewport.width,
          .height = viewport.height,
      },
  };
  CmdSetScissor(cmd, 0, 1, &scissor);

  const int framebufferIndex = nodeTimeline.value % VKM_SWAP_COUNT;

  switch (nodeType) {
    case MXC_NODE_TYPE_THREAD: break;
    case MXC_NODE_TYPE_INTERPROCESS:
      CmdPipelineImageBarrier(cmd, &VKM_IMAGE_BARRIER_TRANSFER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, frameBufferColorImages[framebufferIndex], VK_QUEUE_FAMILY_EXTERNAL, queueIndex));
      break;
  }

  {  // this is really all that'd be user exposed....
    vkmCmdBeginPass(cmd, standardRenderPass, (VkClearColorValue){0, 0, 0, 0}, framebuffers[framebufferIndex]);

    CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, standardPipeline);
    CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, standardPipelineLayout, VKM_PIPE_SET_STD_GLOBAL_INDEX, 1, &globalSet, 0, NULL);
    CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, standardPipelineLayout, VKM_PIPE_SET_STD_MATERIAL_INDEX, 1, &checkerMaterialSet, 0, NULL);
    CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, standardPipelineLayout, VKM_PIPE_SET_STD_OBJECT_INDEX, 1, &sphereObjectSet, 0, NULL);

    CmdBindVertexBuffers(cmd, 0, 1, (const VkBuffer[]){sphereBuffer}, (const VkDeviceSize[]){sphereVertexOffset});
    CmdBindIndexBuffer(cmd, sphereBuffer, sphereIndexOffset, VK_INDEX_TYPE_UINT16);
    CmdDrawIndexed(cmd, sphereIndexCount, 1, 0, 0, 0);

    CmdEndRenderPass(cmd);
  }

  switch (nodeType) {
    case MXC_NODE_TYPE_THREAD:
      CmdPipelineImageBarrier(cmd, &VKM_IMAGE_BARRIER(VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VKM_IMAGE_BARRIER_EXTERNAL_RELEASE_GRAPHICS_READ, VK_IMAGE_ASPECT_COLOR_BIT, frameBufferColorImages[framebufferIndex]));
      break;
    case MXC_NODE_TYPE_INTERPROCESS:
      CmdPipelineImageBarrier(cmd, &VKM_IMAGE_BARRIER_TRANSFER(VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VKM_IMAGE_BARRIER_EXTERNAL_RELEASE_GRAPHICS_READ, VK_IMAGE_ASPECT_COLOR_BIT, frameBufferColorImages[framebufferIndex], queueIndex, VK_QUEUE_FAMILY_EXTERNAL));
      break;
  }

  EndCommandBuffer(cmd);

  nodeTimeline.value++;
  __atomic_thread_fence(__ATOMIC_RELEASE);
  MXC_NODE_SHARED[handle].pendingTimelineSignal = nodeTimeline.value;

  vkmTimelineWait(device, &nodeTimeline);
  __atomic_thread_fence(__ATOMIC_RELEASE);
  MXC_NODE_SHARED[handle].currentTimelineSignal = nodeTimeline.value;

  compBaseCycleValue += compCyclesToSkip;

  goto run_loop;
}