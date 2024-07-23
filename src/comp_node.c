#include "comp_node.h"
#include "mid_shape.h"
#include "mid_window.h"

#include <assert.h>
#include <vulkan/vk_enum_string_helper.h>

enum SetBindCompIndices {
  SET_BIND_COMP_BUFFER_INDEX,
  SET_BIND_COMP_COLOR_INDEX,
  SET_BIND_COMP_NORMAL_INDEX,
  SET_BIND_COMP_GBUFFER_INDEX,
  SET_BIND_COMP_COUNT
};
#define SET_WRITE_COMP_BUFFER(node_set, node_buffer)     \
  (VkWriteDescriptorSet) {                               \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,     \
    .dstSet = node_set,                                  \
    .dstBinding = SET_BIND_COMP_BUFFER_INDEX,            \
    .descriptorCount = 1,                                \
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, \
    .pBufferInfo = &(const VkDescriptorBufferInfo){      \
        .buffer = node_buffer,                           \
        .range = sizeof(MxcNodeSetState),                \
    },                                                   \
  }
#define SET_WRITE_COMP_COLOR(node_set, color_image_view)         \
  (VkWriteDescriptorSet) {                                       \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,             \
    .dstSet = node_set,                                          \
    .dstBinding = SET_BIND_COMP_COLOR_INDEX,                     \
    .descriptorCount = 1,                                        \
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
    .pImageInfo = &(const VkDescriptorImageInfo){                \
        .imageView = color_image_view,                           \
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, \
    },                                                           \
  }
#define SET_WRITE_COMP_NORMAL(node_set, normal_image_view)       \
  (VkWriteDescriptorSet) {                                       \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,             \
    .dstSet = node_set,                                          \
    .dstBinding = SET_BIND_COMP_NORMAL_INDEX,                    \
    .descriptorCount = 1,                                        \
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
    .pImageInfo = &(const VkDescriptorImageInfo){                \
        .imageView = normal_image_view,                          \
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, \
    },                                                           \
  }
#define SET_WRITE_COMP_GBUFFER(node_set, gbuffer_image_view)     \
  (VkWriteDescriptorSet) {                                       \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,             \
    .dstSet = node_set,                                          \
    .dstBinding = SET_BIND_COMP_GBUFFER_INDEX,                   \
    .descriptorCount = 1,                                        \
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
    .pImageInfo = &(const VkDescriptorImageInfo){                \
        .imageView = gbuffer_image_view,                         \
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, \
    },                                                           \
  }
#define SHADER_STAGE_VERT_TESC_TESE_FRAG VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
#define SHADER_STAGE_TASK_MESH_FRAG VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT
static void CreateCompSetLayout(const MxcCompMode compMode, VkDescriptorSetLayout* pLayout) {
  VkShaderStageFlags stageFlags;
  switch (compMode) {
    case MXC_COMP_MODE_BASIC: stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; break;
    case MXC_COMP_MODE_TESS: stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; break;
    case MXC_COMP_MODE_TASK_MESH: stageFlags = VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT; break;
    default: PANIC("CompMode not supported!");
  }
  const VkDescriptorSetLayoutCreateInfo nodeSetLayoutCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = SET_BIND_COMP_COUNT,
      .pBindings = (const VkDescriptorSetLayoutBinding[]){
          [SET_BIND_COMP_BUFFER_INDEX] = {
              .binding = SET_BIND_COMP_BUFFER_INDEX,
              .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .descriptorCount = 1,
              .stageFlags = stageFlags,
          },
          [SET_BIND_COMP_COLOR_INDEX] = {
              .binding = SET_BIND_COMP_COLOR_INDEX,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = stageFlags,
              .pImmutableSamplers = &context.linearSampler,
          },
          [SET_BIND_COMP_NORMAL_INDEX] = {
              .binding = SET_BIND_COMP_NORMAL_INDEX,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = stageFlags,
              .pImmutableSamplers = &context.linearSampler,
          },
          [SET_BIND_COMP_GBUFFER_INDEX] = {
              .binding = SET_BIND_COMP_GBUFFER_INDEX,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = stageFlags,
              .pImmutableSamplers = &context.linearSampler,
          },
      },
  };
  VKM_REQUIRE(vkCreateDescriptorSetLayout(context.device, &nodeSetLayoutCreateInfo, VKM_ALLOC, pLayout));
}

enum PipeSetCompIndices {
  PIPE_SET_COMP_GLOBAL_INDEX,
  PIPE_SET_COMP_NODE_INDEX,
  PIPE_SET_COMP_COUNT,
};
static void CreateCompPipeLayout(const VkDescriptorSetLayout nodeSetLayout, VkPipelineLayout* pNodePipeLayout) {
  const VkPipelineLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = PIPE_SET_COMP_COUNT,
      .pSetLayouts = (const VkDescriptorSetLayout[]){
          [PIPE_SET_COMP_GLOBAL_INDEX] = context.stdPipeLayout.globalSetLayout,
          [PIPE_SET_COMP_NODE_INDEX] = nodeSetLayout,
      },
  };
  VKM_REQUIRE(vkCreatePipelineLayout(context.device, &createInfo, VKM_ALLOC, pNodePipeLayout));
}

static const VkmImageBarrier* VKM_IMG_BARRIER_COMP_SHADER_READ = &(const VkmImageBarrier){
    .stageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT | VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
    .accessMask = VK_ACCESS_2_SHADER_READ_BIT,
    .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
};

void mxcCreateCompNode(const MxcCompNodeCreateInfo* pInfo, MxcCompNode* pNode) {
  {  // Create

     // layouts
    CreateCompSetLayout(pInfo->compMode, &pNode->compNodeSetLayout);
    CreateCompPipeLayout(pNode->compNodeSetLayout, &pNode->compNodePipeLayout);

    // meshes
    switch (pInfo->compMode) {
      case MXC_COMP_MODE_BASIC:
        vkmCreateBasicPipe("./shaders/basic_comp.vert.spv", "./shaders/basic_comp.frag.spv", context.renderPass, pNode->compNodePipeLayout, &pNode->compNodePipe);
        CreateQuadMesh(0.5f, &pNode->quadMesh);
        break;
      case MXC_COMP_MODE_TESS:
        vkmCreateTessPipe("./shaders/tess_comp.vert.spv", "./shaders/tess_comp.tesc.spv", "./shaders/tess_comp.tese.spv", "./shaders/tess_comp.frag.spv", context.renderPass, pNode->compNodePipeLayout, &pNode->compNodePipe);
        CreateQuadPatchMeshSharedMemory(&pNode->quadMesh);
        break;
      case MXC_COMP_MODE_TASK_MESH:
        vkmCreateTaskMeshPipe("./shaders/mesh_comp.task.spv", "./shaders/mesh_comp.mesh.spv", "./shaders/mesh_comp.frag.spv", context.renderPass, pNode->compNodePipeLayout, &pNode->compNodePipe);
        CreateQuadMesh(0.5f, &pNode->quadMesh);
        break;
      default: PANIC("CompMode not supported!");
    }

    // sets
    const VkQueryPoolCreateInfo queryPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = 2,
    };
    VKM_REQUIRE(vkCreateQueryPool(context.device, &queryPoolCreateInfo, VKM_ALLOC, &pNode->timeQueryPool));

    // global set
    vkmAllocateDescriptorSet(context.descriptorPool, &context.stdPipeLayout.globalSetLayout, &pNode->globalSet.set);
    const VkmRequestAllocationInfo globalSetAllocRequest = {
        .memoryPropertyFlags = VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
        .size = sizeof(VkmGlobalSetState),
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    };
    vkmCreateBufferSharedMemory(&globalSetAllocRequest, &pNode->globalSet.buffer, &pNode->globalSet.sharedMemory);

    // node set
    vkmAllocateDescriptorSet(context.descriptorPool, &pNode->compNodeSetLayout, &pNode->compNodeSet);
    vkmSetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)pNode->compNodeSet, "NodeSet");
    const VkmRequestAllocationInfo nodeSetAllocRequest = {
        .memoryPropertyFlags = VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
        .size = sizeof(MxcNodeSetState),
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    };
    vkmCreateBufferSharedMemory(&nodeSetAllocRequest, &pNode->compNodeSetBuffer, &pNode->compNodeSetMemory);

    // cmd
    const VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].pool,  // so we may want to get rid of global pools
        .commandBufferCount = 1,
    };
    VKM_REQUIRE(vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfo, &pNode->cmd));
    vkmSetDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)pNode->cmd, "CompCmd");

    vkmCreateSwap(pInfo->surface, VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS, &pNode->swap);
    vkmCreateStdFramebuffers(VKM_SWAP_COUNT, VKM_LOCALITY_CONTEXT, pNode->framebuffers);
    vkmCreateTimeline(&pNode->timeline);
  }

  {  // Copy needed state
    pNode->compMode = pInfo->compMode;
    pNode->device = context.device;
    pNode->compRenderPass = context.renderPass;
  }
}

void mxcBindUpdateCompNode(const MxcCompNodeCreateInfo* pInfo, MxcCompNode* pNode) {
  switch (pInfo->compMode) {
    case MXC_COMP_MODE_BASIC:
      break;
    case MXC_COMP_MODE_TESS:
      BindUpdateQuadPatchMesh(0.5f, &pNode->quadMesh);
      break;
    case MXC_COMP_MODE_TASK_MESH:
      break;
    default: PANIC("CompMode not supported!");
  }
  VKM_REQUIRE(vkBindBufferMemory(context.device, pNode->globalSet.buffer, deviceMemory[pNode->globalSet.sharedMemory.type], pNode->globalSet.sharedMemory.offset));
  vkUpdateDescriptorSets(context.device, 1, &VKM_SET_WRITE_STD_GLOBAL_BUFFER(pNode->globalSet.set, pNode->globalSet.buffer), 0, NULL);
  VKM_REQUIRE(vkBindBufferMemory(context.device, pNode->compNodeSetBuffer, deviceMemory[pNode->compNodeSetMemory.type], pNode->compNodeSetMemory.offset));
  vkUpdateDescriptorSets(context.device, 1, &SET_WRITE_COMP_BUFFER(pNode->compNodeSet, pNode->compNodeSetBuffer), 0, NULL);
}

void* mxcCompNodeThread(const MxcNodeContext* pNodeContext) {

  MxcCompNode* pNode = (MxcCompNode*)pNodeContext->pNode;

  MxcCompMode  compMode = pNode->compMode;
  VkCommandBuffer cmd = pNode->cmd;
  VkRenderPass    stdRenderPass = pNode->compRenderPass;

  VkmTransform       globalCameraTransform = {};
  VkmGlobalSetState  globalSetState = {};
  VkDescriptorSet    globalSet = pNode->globalSet.set;
  VkmGlobalSetState* pGlobalSetMapped = pMappedMemory[pNode->globalSet.sharedMemory.type] + pNode->globalSet.sharedMemory.offset;
  vkmUpdateGlobalSetViewProj(&globalCameraTransform, &globalSetState, pGlobalSetMapped);

  MxcNodeSetState* pCompNodeSetMapped = pMappedMemory[pNode->compNodeSetMemory.type] + pNode->compNodeSetMemory.offset;
  VkPipelineLayout compNodePipeLayout = pNode->compNodePipeLayout;
  VkPipeline       compNodePipe = pNode->compNodePipe;
  VkDescriptorSet  compNodeSet = pNode->compNodeSet;

  VkDevice device = pNode->device;
  VkmSwap  swap = pNode->swap;

  uint32_t     quadIndexCount = pNode->quadMesh.indexCount;
  VkBuffer     quadBuffer = pNode->quadMesh.buffer;
  VkDeviceSize quadIndexOffset = pNode->quadMesh.indexOffset;
  VkDeviceSize quadVertexOffset = pNode->quadMesh.vertexOffset;

  VkSemaphore timeline = pNode->timeline;
  uint64_t    compBaseCycleValue = 0;

  VkQueryPool timeQueryPool = pNode->timeQueryPool;

  int           framebufferIndex = 0;
  VkFramebuffer framebuffers[VKM_SWAP_COUNT];
  VkImage       frameBufferColorImages[VKM_SWAP_COUNT];
  for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
    framebuffers[i] = pNode->framebuffers[i].framebuffer;
    frameBufferColorImages[i] = pNode->framebuffers[i].color.img;
  }

  // just making sure atomics are only using barriers, not locks
  for (int i = 0; i < nodeCount; ++i) {
    assert(__atomic_always_lock_free(sizeof(nodesShared[i].pendingTimelineSignal), &nodesShared[i].pendingTimelineSignal));
    assert(__atomic_always_lock_free(sizeof(nodesShared[i].currentTimelineSignal), &nodesShared[i].currentTimelineSignal));
  }

  // very common ones should be global to potentially share higher level cache
  VKM_DEVICE_FUNC(CmdPipelineBarrier2);
  VKM_DEVICE_FUNC(ResetQueryPool);
  VKM_DEVICE_FUNC(GetQueryPoolResults);
  VKM_DEVICE_FUNC(CmdWriteTimestamp2);
  VKM_DEVICE_FUNC(CmdBindPipeline);
  VKM_DEVICE_FUNC(CmdBlitImage);
  VKM_DEVICE_FUNC(CmdBindDescriptorSets);
  VKM_DEVICE_FUNC(CmdBindVertexBuffers);
  VKM_DEVICE_FUNC(CmdBindIndexBuffer);
  VKM_DEVICE_FUNC(CmdDrawIndexed);
  VKM_DEVICE_FUNC(CmdEndRenderPass);
  VKM_DEVICE_FUNC(EndCommandBuffer);
  VKM_DEVICE_FUNC(AcquireNextImageKHR);
  VKM_DEVICE_FUNC(CmdDrawMeshTasksEXT);

run_loop:

  vkmTimelineWait(device, compBaseCycleValue + MXC_CYCLE_PROCESS_INPUT, timeline);
  vkmProcessCameraMouseInput(midWindowInput.deltaTime, mxcInput.mouseDelta, &globalCameraTransform);
  vkmProcessCameraKeyInput(midWindowInput.deltaTime, mxcInput.move, &globalCameraTransform);
  vkmUpdateGlobalSetView(&globalCameraTransform, &globalSetState, pGlobalSetMapped);

  {  // Node cycle
    vkmTimelineSignal(device, compBaseCycleValue + MXC_CYCLE_UPDATE_NODE_STATES, timeline);

    vkmCmdResetBegin(cmd);

    for (int i = 0; i < nodeCount; ++i) {
      if (!nodesShared[i].active || nodesShared[i].currentTimelineSignal < 1)
        continue;

      // update node model mat... this should happen every frame so user can move it in comp
      nodesShared[i].transform.rotation = QuatFromEuler(nodesShared[i].transform.euler);
      nodesShared[i].nodeSetState.model = Mat4FromPosRot(nodesShared[i].transform.position, nodesShared[i].transform.rotation);

      {  // check frame available
        uint64_t value = nodesShared[i].currentTimelineSignal;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (value > nodesShared[i].lastTimelineSwap) {
          {  // update framebuffer for comp
            nodesShared[i].lastTimelineSwap = value;
            const int                  nodeFramebufferIndex = !(value % VKM_SWAP_COUNT);
            const VkWriteDescriptorSet writeSets[] = {
                SET_WRITE_COMP_COLOR(compNodeSet, nodesShared[i].framebufferColorImageViews[nodeFramebufferIndex]),
                SET_WRITE_COMP_NORMAL(compNodeSet, nodesShared[i].framebufferNormalImageViews[nodeFramebufferIndex]),
                SET_WRITE_COMP_GBUFFER(compNodeSet, nodesShared[i].framebufferGBufferImageViews[nodeFramebufferIndex]),
            };
            vkUpdateDescriptorSets(device, _countof(writeSets), writeSets, 0, NULL);
            switch (nodesShared[i].type) {
              case MXC_NODE_TYPE_THREAD:
                const VkImageMemoryBarrier2 barriers[] = {
                    VKM_COLOR_IMG_BARRIER(VKM_IMG_BARRIER_EXTERNAL_ACQUIRE_SHADER_READ, VKM_IMG_BARRIER_COMP_SHADER_READ, nodesShared[i].framebufferColorImages[nodeFramebufferIndex]),
                    VKM_COLOR_IMG_BARRIER(VKM_IMG_BARRIER_EXTERNAL_ACQUIRE_SHADER_READ, VKM_IMG_BARRIER_COMP_SHADER_READ, nodesShared[i].framebufferNormalImages[nodeFramebufferIndex]),
                    VKM_COLOR_IMAGE_BARRIER_MIPS(VKM_IMG_BARRIER_EXTERNAL_ACQUIRE_SHADER_READ, VKM_IMG_BARRIER_COMP_SHADER_READ, nodesShared[i].framebufferGBufferImages[nodeFramebufferIndex], 0, VKM_G_BUFFER_LEVELS),
                };
                CmdPipelineImageBarriers2(cmd, _countof(barriers), barriers);
                break;
              case MXC_NODE_TYPE_INTERPROCESS:
                //              CmdPipelineImageBarrier(cmd, &VKM_COLOR_IMAGE_BARRIER(VKM_IMAGE_BARRIER_EXTERNAL_ACQUIRE_GRAPHICS_ATTACH, VKM_IMAGE_BARRIER_SHADER_READ, MXC_NODE_SHARED[i].framebufferColorImages[nodeFramebufferIndex]));
                //              CmdPipelineImageBarrier(cmd, &VKM_COLOR_IMAGE_BARRIER(VKM_IMAGE_BARRIER_EXTERNAL_ACQUIRE_GRAPHICS_ATTACH, VKM_IMAGE_BARRIER_SHADER_READ, MXC_NODE_SHARED[i].framebufferNormalImages[nodeFramebufferIndex]));
                //              CmdPipelineImageBarrier(cmd, &VKM_COLOR_IMAGE_BARRIER(VKM_IMAGE_BARRIER_EXTERNAL_ACQUIRE_GRAPHICS_ATTACH, VKM_IMAGE_BARRIER_SHADER_READ, MXC_NODE_SHARED[i].framebufferGBufferImages[nodeFramebufferIndex]));
                break;
              default: PANIC("nodeType not supported");
            }
          }
          {
            // move the global set state that was previously used to render into the node set state to use in comp
            memcpy(&nodesShared[i].nodeSetState.view, (void*)&nodesShared[i].globalSetState, sizeof(VkmGlobalSetState));
            nodesShared[i].nodeSetState.ulUV = nodesShared[i].lrUV;
            nodesShared[i].nodeSetState.lrUV = nodesShared[i].ulUV;

            memcpy(pCompNodeSetMapped, &nodesShared[i].nodeSetState, sizeof(MxcNodeSetState));

            // calc framebuffersize
            const float radius = nodesShared[i].radius;

            const vec4 ulbModel = Vec4Rot(globalCameraTransform.rotation, (vec4){.x = -radius, .y = -radius, .z = -radius, .w = 1});
            const vec4 ulbWorld = Vec4MulMat4(nodesShared[i].nodeSetState.model, ulbModel);
            const vec4 ulbClip = Vec4MulMat4(globalSetState.view, ulbWorld);
            const vec3 ulbNDC = Vec4WDivide(Vec4MulMat4(globalSetState.proj, ulbClip));
            const vec2 ulbUV = Vec2Clamp(UVFromNDC(ulbNDC), 0.0f, 1.0f);

            const vec4 ulbfModel = Vec4Rot(globalCameraTransform.rotation, (vec4){.x = -radius, .y = -radius, .z = radius, .w = 1});
            const vec4 ulbfWorld = Vec4MulMat4(nodesShared[i].nodeSetState.model, ulbfModel);
            const vec4 ulbfClip = Vec4MulMat4(globalSetState.view, ulbfWorld);
            const vec3 ulbfNDC = Vec4WDivide(Vec4MulMat4(globalSetState.proj, ulbfClip));
            const vec2 ulfUV = Vec2Clamp(UVFromNDC(ulbfNDC), 0.0f, 1.0f);

            const vec2 ulUV = Vec2Min(ulfUV, ulbUV);

            const vec4 lrbModel = Vec4Rot(globalCameraTransform.rotation, (vec4){.x = radius, .y = radius, .z = -radius, .w = 1});
            const vec4 lrbWorld = Vec4MulMat4(nodesShared[i].nodeSetState.model, lrbModel);
            const vec4 lrbClip = Vec4MulMat4(globalSetState.view, lrbWorld);
            const vec3 lrbNDC = Vec4WDivide(Vec4MulMat4(globalSetState.proj, lrbClip));
            const vec2 lrbUV = Vec2Clamp(UVFromNDC(lrbNDC), 0.0f, 1.0f);

            const vec4 lrfModel = Vec4Rot(globalCameraTransform.rotation, (vec4){.x = radius, .y = radius, .z = radius, .w = 1});
            const vec4 lrfWorld = Vec4MulMat4(nodesShared[i].nodeSetState.model, lrfModel);
            const vec4 lrfClip = Vec4MulMat4(globalSetState.view, lrfWorld);
            const vec3 lrfNDC = Vec4WDivide(Vec4MulMat4(globalSetState.proj, lrfClip));
            const vec2 lrfUV = Vec2Clamp(UVFromNDC(lrfNDC), 0.0f, 1.0f);

            const vec2 lrUV = Vec2Max(lrbUV, lrfUV);

            const vec2 diff = {.vec = lrUV.vec - ulUV.vec};

            __atomic_thread_fence(__ATOMIC_RELEASE);
            // write current global set state to node's global set state to use for next node render with new the framebuffer size
            memcpy((void*)&nodesShared[i].globalSetState, &globalSetState, sizeof(VkmGlobalSetState) - sizeof(ivec2));
            nodesShared[i].globalSetState.framebufferSize = (ivec2){diff.x * DEFAULT_WIDTH, diff.y * DEFAULT_HEIGHT};
            nodesShared[i].ulUV = ulUV;
            nodesShared[i].lrUV = lrUV;
          }
        }
      }
    }

    {  // Recording Cycle
      vkmTimelineSignal(device, compBaseCycleValue + MXC_CYCLE_RECORD_COMPOSITE, timeline);

      framebufferIndex = !framebufferIndex;

      ResetQueryPool(device, timeQueryPool, 0, 2);
      CmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_NONE, timeQueryPool, 0);

      vkmCmdBeginPass(cmd, stdRenderPass, VKM_PASS_CLEAR_COLOR, framebuffers[framebufferIndex]);

      CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, compNodePipe);
      CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, compNodePipeLayout, PIPE_SET_COMP_GLOBAL_INDEX, 1, &globalSet, 0, NULL);

      for (int i = 0; i < nodeCount; ++i) {
        if (!nodesShared[i].active || nodesShared[i].currentTimelineSignal < 1)
          continue;

        CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, compNodePipeLayout, PIPE_SET_COMP_NODE_INDEX, 1, &compNodeSet, 0, NULL);

        switch (compMode) {
          case MXC_COMP_MODE_BASIC:
          case MXC_COMP_MODE_TESS:
            CmdBindVertexBuffers(cmd, 0, 1, (const VkBuffer[]){quadBuffer}, (const VkDeviceSize[]){quadVertexOffset});
            CmdBindIndexBuffer(cmd, quadBuffer, quadIndexOffset, VK_INDEX_TYPE_UINT16);
            CmdDrawIndexed(cmd, quadIndexCount, 1, 0, 0, 0);
            break;
          case MXC_COMP_MODE_TASK_MESH:
            CmdDrawMeshTasksEXT(cmd, 1, 1, 1);
            break;
          default: PANIC("CompMode not supported!");
        }
      }

      CmdEndRenderPass(cmd);

      CmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, timeQueryPool, 1);

      {  // Blit Framebuffer
        AcquireNextImageKHR(device, swap.chain, UINT64_MAX, swap.acquireSemaphore, VK_NULL_HANDLE, &compNodeShared.swapIndex);
        const VkImage swapImage = swap.images[compNodeShared.swapIndex];
        CmdPipelineImageBarrier2(cmd, &VKM_COLOR_IMG_BARRIER(VKM_IMAGE_BARRIER_COLOR_ATTACHMENT_UNDEFINED, VKM_IMG_BARRIER_BLIT_DST, swapImage));
        CmdBlitImageFullScreen(cmd, frameBufferColorImages[framebufferIndex], swapImage);
        CmdPipelineImageBarrier2(cmd, &VKM_COLOR_IMG_BARRIER(VKM_IMG_BARRIER_BLIT_DST, VKM_IMG_BARRIER_PRESENT_BLIT_RELEASE, swapImage));
      }

      EndCommandBuffer(cmd);

      __atomic_thread_fence(__ATOMIC_RELEASE);
      vkmTimelineSignal(device, compBaseCycleValue + MXC_CYCLE_RENDER_COMPOSITE, timeline);
    }

    {  // update timequery
      uint64_t timestampsNS[2];
      GetQueryPoolResults(device, timeQueryPool, 0, 2, sizeof(uint64_t) * 2, timestampsNS, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
      double timestampsMS[2];
      for (uint32_t i = 0; i < 2; ++i) {
        timestampsMS[i] = (double)timestampsNS[i] / (double)1000000;  // ns to ms
      }
      timeQueryMs = timestampsMS[1] - timestampsMS[0];
    }

    compBaseCycleValue += MXC_CYCLE_COUNT;
  }

  CHECK_RUNNING;

  goto run_loop;

  return NULL;
}