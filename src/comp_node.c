#include "comp_node.h"
#include "mid_shape.h"
#include "mid_window.h"
#include "window.h"

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
        .range = sizeof(MxcNodeCompositorSetState),      \
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
              .pImmutableSamplers = &midVk.context.linearSampler,
          },
          [SET_BIND_COMP_NORMAL_INDEX] = {
              .binding = SET_BIND_COMP_NORMAL_INDEX,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = stageFlags,
              .pImmutableSamplers = &midVk.context.linearSampler,
          },
          [SET_BIND_COMP_GBUFFER_INDEX] = {
              .binding = SET_BIND_COMP_GBUFFER_INDEX,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = stageFlags,
              .pImmutableSamplers = &midVk.context.linearSampler,
          },
      },
  };
  MIDVK_REQUIRE(vkCreateDescriptorSetLayout(midVk.context.device, &nodeSetLayoutCreateInfo, MIDVK_ALLOC, pLayout));
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
          [PIPE_SET_COMP_GLOBAL_INDEX] = midVk.context.stdPipeLayout.globalSetLayout,
          [PIPE_SET_COMP_NODE_INDEX] = nodeSetLayout,
      },
  };
  MIDVK_REQUIRE(vkCreatePipelineLayout(midVk.context.device, &createInfo, MIDVK_ALLOC, pNodePipeLayout));
}

static const MidVkSrcDstImageBarrier* VKM_IMG_BARRIER_COMP_SHADER_READ = &(const MidVkSrcDstImageBarrier){
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
        vkmCreateBasicPipe("./shaders/basic_comp.vert.spv", "./shaders/basic_comp.frag.spv", midVk.context.renderPass, pNode->compNodePipeLayout, &pNode->compNodePipe);
        CreateQuadMesh(0.5f, &pNode->quadMesh);
        break;
      case MXC_COMP_MODE_TESS:
        vkmCreateTessPipe("./shaders/tess_comp.vert.spv", "./shaders/tess_comp.tesc.spv", "./shaders/tess_comp.tese.spv", "./shaders/tess_comp.frag.spv", midVk.context.renderPass, pNode->compNodePipeLayout, &pNode->compNodePipe);
        CreateQuadPatchMeshSharedMemory(&pNode->quadMesh);
        break;
      case MXC_COMP_MODE_TASK_MESH:
        vkmCreateTaskMeshPipe("./shaders/mesh_comp.task.spv", "./shaders/mesh_comp.mesh.spv", "./shaders/mesh_comp.frag.spv", midVk.context.renderPass, pNode->compNodePipeLayout, &pNode->compNodePipe);
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
    MIDVK_REQUIRE(vkCreateQueryPool(midVk.context.device, &queryPoolCreateInfo, MIDVK_ALLOC, &pNode->timeQueryPool));

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
    MIDVK_REQUIRE(vkCreateDescriptorPool(midVk.context.device, &descriptorPoolCreateInfo, MIDVK_ALLOC, &threadContext.descriptorPool));

    // global set
    vkmAllocateDescriptorSet(threadContext.descriptorPool, &midVk.context.stdPipeLayout.globalSetLayout, &pNode->globalSet.set);
    const VkmRequestAllocationInfo globalSetAllocRequest = {
        .memoryPropertyFlags = VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
        .size = sizeof(VkmGlobalSetState),
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    };
    vkmCreateBufferSharedMemory(&globalSetAllocRequest, &pNode->globalSet.buffer, &pNode->globalSet.sharedMemory);

    // node set
    vkmAllocateDescriptorSet(threadContext.descriptorPool, &pNode->compNodeSetLayout, &pNode->compNodeSet);
    vkmSetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)pNode->compNodeSet, "NodeSet");
    const VkmRequestAllocationInfo nodeSetAllocRequest = {
        .memoryPropertyFlags = VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT,
        .size = sizeof(MxcNodeCompositorSetState),
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    };
    vkmCreateBufferSharedMemory(&nodeSetAllocRequest, &pNode->compNodeSetBuffer, &pNode->compNodeSetSharedMemory);

    midvkCreateFramebufferTexture(MIDVK_SWAP_COUNT, MID_LOCALITY_CONTEXT, pNode->framebuffers);
    vkmCreateFramebuffer(midVk.context.renderPass, &pNode->framebuffer);
    vkmSetDebugName(VK_OBJECT_TYPE_FRAMEBUFFER, (uint64_t)pNode->framebuffer, "CompFramebuffer"); // should be moved into method? probably
  }

  {  // Copy needed state
    // todo get rid lets go to context directly
    pNode->compMode = pInfo->compMode;
    pNode->device = midVk.context.device;
    pNode->compRenderPass = midVk.context.renderPass;
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
  MIDVK_REQUIRE(vkBindBufferMemory(midVk.context.device, pNode->globalSet.buffer, deviceMemory[pNode->globalSet.sharedMemory.type], pNode->globalSet.sharedMemory.offset));
  vkUpdateDescriptorSets(midVk.context.device, 1, &VKM_SET_WRITE_STD_GLOBAL_BUFFER(pNode->globalSet.set, pNode->globalSet.buffer), 0, NULL);
  MIDVK_REQUIRE(vkBindBufferMemory(midVk.context.device, pNode->compNodeSetBuffer, deviceMemory[pNode->compNodeSetSharedMemory.type], pNode->compNodeSetSharedMemory.offset));
  vkUpdateDescriptorSets(midVk.context.device, 1, &SET_WRITE_COMP_BUFFER(pNode->compNodeSet, pNode->compNodeSetBuffer), 0, NULL);
}

void mxcCompNodeRun(const MxcCompositorNodeContext* pNodeContext, const MxcCompNode* pNode) {

  const MxcCompMode compMode = pNode->compMode;

  const VkDevice        device = pNode->device;
  const VkCommandBuffer cmd = pNodeContext->cmd;
  const VkRenderPass    renderPass = pNode->compRenderPass;
  const VkFramebuffer   framebuffer = pNode->framebuffer;
  const VkDescriptorSet globalSet = pNode->globalSet.set;

  MidPose                    globalCameraTransform = {};
  VkmGlobalSetState          globalSetState = {};
  VkmGlobalSetState*         pGlobalSetMapped = pMappedMemory[pNode->globalSet.sharedMemory.type] + pNode->globalSet.sharedMemory.offset;
  MxcNodeCompositorSetState* pCompNodeSetMapped = pMappedMemory[pNode->compNodeSetSharedMemory.type] + pNode->compNodeSetSharedMemory.offset;
  vkmUpdateGlobalSetViewProj(&globalCameraTransform, &globalSetState, pGlobalSetMapped);

  const VkPipelineLayout compNodePipeLayout = pNode->compNodePipeLayout;
  const VkPipeline       compNodePipe = pNode->compNodePipe;
  const VkDescriptorSet  compNodeSet = pNode->compNodeSet;
  
  const uint32_t     quadIndexCount = pNode->quadMesh.indexCount;
  const VkBuffer     quadBuffer = pNode->quadMesh.buffer;
  const VkDeviceSize quadIndexOffset = pNode->quadMesh.indexOffset;
  const VkDeviceSize quadVertexOffset = pNode->quadMesh.vertexOffset;

  uint64_t    compBaseCycleValue = 0;
  const VkSemaphore compTimeline = pNodeContext->compTimeline;
  
  const MidVkSwap swap = pNodeContext->swap;
  
  const VkQueryPool timeQueryPool = pNode->timeQueryPool;

  struct {
    VkImageView color;
    VkImageView normal;
    VkImageView depth;
  } compositorFramebufferViews[MIDVK_SWAP_COUNT];
  VkImage compositorFramebufferColorImages[MIDVK_SWAP_COUNT];
  for (int i = 0; i < MIDVK_SWAP_COUNT; ++i) {
    compositorFramebufferViews[i].color = pNode->framebuffers[i].color.view;
    compositorFramebufferViews[i].normal = pNode->framebuffers[i].normal.view;
    compositorFramebufferViews[i].depth = pNode->framebuffers[i].depth.view;
    compositorFramebufferColorImages[i] = pNode->framebuffers[i].color.image;
  }
  int compositorFramebufferIndex = 0;

  // just making sure atomics are only using barriers, not locks
  for (int i = 0; i < nodeCount; ++i) {
    assert(__atomic_always_lock_free(sizeof(nodesShared[i].nodeCurrentTimelineSignal), &nodesShared[i].nodeCurrentTimelineSignal));
  }

  // very common ones should be global to potentially share higher level cache
  // but maybe do it anyways because it'd just be better? each func pointer is 8 bytes. 8 can fit on a cache line
  MIDVK_DEVICE_FUNC(CmdPipelineBarrier2);
  MIDVK_DEVICE_FUNC(ResetQueryPool);
  MIDVK_DEVICE_FUNC(GetQueryPoolResults);
  MIDVK_DEVICE_FUNC(CmdWriteTimestamp2);
  MIDVK_DEVICE_FUNC(CmdBeginRenderPass);
  MIDVK_DEVICE_FUNC(CmdBindPipeline);
  MIDVK_DEVICE_FUNC(CmdBlitImage);
  MIDVK_DEVICE_FUNC(CmdBindDescriptorSets);
  MIDVK_DEVICE_FUNC(CmdBindVertexBuffers);
  MIDVK_DEVICE_FUNC(CmdBindIndexBuffer);
  MIDVK_DEVICE_FUNC(CmdDrawIndexed);
  MIDVK_DEVICE_FUNC(CmdEndRenderPass);
  MIDVK_DEVICE_FUNC(EndCommandBuffer);
  MIDVK_DEVICE_FUNC(AcquireNextImageKHR);
  MIDVK_DEVICE_FUNC(CmdDrawMeshTasksEXT);

run_loop:

  vkmTimelineWait(device, compBaseCycleValue + MXC_CYCLE_PROCESS_INPUT, compTimeline);
  vkmProcessCameraMouseInput(midWindowInput.deltaTime, mxcInput.mouseDelta, &globalCameraTransform);
  vkmProcessCameraKeyInput(midWindowInput.deltaTime, mxcInput.move, &globalCameraTransform);
  vkmUpdateGlobalSetView(&globalCameraTransform, &globalSetState, pGlobalSetMapped);

  // Update and Recording must be separate cycles because it's not ideal to acquire and transition images after being a renderpess
  {  // Update Nodes
    vkmTimelineSignal(device, compBaseCycleValue + MXC_CYCLE_UPDATE_NODE_STATES, compTimeline);
    vkmCmdResetBegin(cmd);

    for (int i = 0; i < nodeCount; ++i) {
      MxcNodeShared* pNodeShared = activeNodesShared[i];

      // tests show reading from shared memory is 500~ x faster than vkGetSemaphoreCounterValue
      // shared: 569 - semaphore: 315416 ratio: 554.333919
      __atomic_thread_fence(__ATOMIC_ACQUIRE);
      const uint64_t nodeCurrentTimelineSignal = pNodeShared->nodeCurrentTimelineSignal;

      // I don't like this but it waits until the node renders something. Prediction should be okay here.
      if (nodeCurrentTimelineSignal < 1)
        continue;
      
      // We need logic here. Some node you'd want to allow to move themselves. Other locked in specific place. Other move their offset.
      memcpy(&nodeCompositorData[i].rootPose, &pNodeShared->rootPose, sizeof(MidPose));
      //nodeCompositorData[i].rootPose.rotation = QuatFromEuler(nodeCompositorData[i].rootPose.euler);

      // update node model mat... this should happen every frame so user can move it in comp
      nodeCompositorData[i].nodeSetState.model = Mat4FromPosRot(nodeCompositorData[i].rootPose.position, nodeCompositorData[i].rootPose.rotation);

      if (nodeCurrentTimelineSignal <= nodeCompositorData[i].lastTimelineSwap)
        continue;

      nodeCompositorData[i].lastTimelineSwap = nodeCurrentTimelineSignal;

      {  // Acquire new framebuffers from node
        const int nodeFramebufferIndex = !(nodeCurrentTimelineSignal % MIDVK_SWAP_COUNT);
        CmdPipelineImageBarriers2(cmd, 3, nodeCompositorData[i].framebuffers[nodeFramebufferIndex].acquireBarriers);
        // these could be pre-recorded too
        const VkWriteDescriptorSet writeSets[] = {
            SET_WRITE_COMP_COLOR(compNodeSet, nodeCompositorData[i].framebuffers[nodeFramebufferIndex].colorView),
            SET_WRITE_COMP_NORMAL(compNodeSet, nodeCompositorData[i].framebuffers[nodeFramebufferIndex].normalView),
            SET_WRITE_COMP_GBUFFER(compNodeSet, nodeCompositorData[i].framebuffers[nodeFramebufferIndex].gBufferView),
        };
        vkUpdateDescriptorSets(device, COUNT(writeSets), writeSets, 0, NULL);
      }

      { // Calc new node uniform and shared data

        // move the global set state that was previously used to render into the node set state to use in comp
        memcpy(&nodeCompositorData[i].nodeSetState.view, (void*)&pNodeShared->nodeGlobalSetState, sizeof(VkmGlobalSetState));
        nodeCompositorData[i].nodeSetState.ulUV = pNodeShared->compositorULScreenUV;
        nodeCompositorData[i].nodeSetState.lrUV = pNodeShared->compositorLRScreenUV;

        memcpy(pCompNodeSetMapped, &nodeCompositorData[i].nodeSetState, sizeof(MxcNodeCompositorSetState));

        const float radius = pNodeShared->compositorRadius;

        const vec4 ulbModel = Vec4Rot(globalCameraTransform.rotation, (vec4){.x = -radius, .y = -radius, .z = -radius, .w = 1});
        const vec4 ulbWorld = Vec4MulMat4(nodeCompositorData[i].nodeSetState.model, ulbModel);
        const vec4 ulbClip = Vec4MulMat4(globalSetState.view, ulbWorld);
        const vec3 ulbNDC = Vec4WDivide(Vec4MulMat4(globalSetState.proj, ulbClip));
        const vec2 ulbUV = Vec2Clamp(UVFromNDC(ulbNDC), 0.0f, 1.0f);

        const vec4 ulfModel = Vec4Rot(globalCameraTransform.rotation, (vec4){.x = -radius, .y = -radius, .z = radius, .w = 1});
        const vec4 ulfWorld = Vec4MulMat4(nodeCompositorData[i].nodeSetState.model, ulfModel);
        const vec4 ulfClip = Vec4MulMat4(globalSetState.view, ulfWorld);
        const vec3 ulfNDC = Vec4WDivide(Vec4MulMat4(globalSetState.proj, ulfClip));
        const vec2 ulfUV = Vec2Clamp(UVFromNDC(ulfNDC), 0.0f, 1.0f);

        const vec2 ulUV = Vec2Min(ulfUV, ulbUV);

        const vec4 lrbModel = Vec4Rot(globalCameraTransform.rotation, (vec4){.x = radius, .y = radius, .z = -radius, .w = 1});
        const vec4 lrbWorld = Vec4MulMat4(nodeCompositorData[i].nodeSetState.model, lrbModel);
        const vec4 lrbClip = Vec4MulMat4(globalSetState.view, lrbWorld);
        const vec3 lrbNDC = Vec4WDivide(Vec4MulMat4(globalSetState.proj, lrbClip));
        const vec2 lrbUV = Vec2Clamp(UVFromNDC(lrbNDC), 0.0f, 1.0f);

        const vec4 lrfModel = Vec4Rot(globalCameraTransform.rotation, (vec4){.x = radius, .y = radius, .z = radius, .w = 1});
        const vec4 lrfWorld = Vec4MulMat4(nodeCompositorData[i].nodeSetState.model, lrfModel);
        const vec4 lrfClip = Vec4MulMat4(globalSetState.view, lrfWorld);
        const vec3 lrfNDC = Vec4WDivide(Vec4MulMat4(globalSetState.proj, lrfClip));
        const vec2 lrfUV = Vec2Clamp(UVFromNDC(lrfNDC), 0.0f, 1.0f);

        const vec2 lrUV = Vec2Max(lrbUV, lrfUV);

        const vec2 diff = {.vec = lrUV.vec - ulUV.vec};

        // write current global set state to node's global set state to use for next node render with new the framebuffer size
        memcpy(&pNodeShared->nodeGlobalSetState, &globalSetState, sizeof(VkmGlobalSetState) - sizeof(ivec2));
        pNodeShared->nodeGlobalSetState.framebufferSize = (ivec2){diff.x * DEFAULT_WIDTH, diff.y * DEFAULT_HEIGHT};
        pNodeShared->compositorULScreenUV = ulUV;
        pNodeShared->compositorLRScreenUV = lrUV;
        __atomic_thread_fence(__ATOMIC_RELEASE);
      }
    }

    {  // Recording Cycle
      vkmTimelineSignal(device, compBaseCycleValue + MXC_CYCLE_COMPOSITOR_RECORD, compTimeline);

      compositorFramebufferIndex = !compositorFramebufferIndex;

      ResetQueryPool(device, timeQueryPool, 0, 2);
      CmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_NONE, timeQueryPool, 0);

      CmdBeginRenderPass(cmd, renderPass, framebuffer, MIDVK_PASS_CLEAR_COLOR,
                         compositorFramebufferViews[compositorFramebufferIndex].color,
                         compositorFramebufferViews[compositorFramebufferIndex].normal,
                         compositorFramebufferViews[compositorFramebufferIndex].depth);

      CmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, compNodePipe);
      CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, compNodePipeLayout, PIPE_SET_COMP_GLOBAL_INDEX, 1, &globalSet, 0, NULL);

      for (int i = 0; i < nodeCount; ++i) {
        MxcNodeShared* pNodeShared = activeNodesShared[i];

        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        const uint64_t nodeCurrentTimelineSignal = pNodeShared->nodeCurrentTimelineSignal;

        // I don't like this but it waits until the node renders something. Prediction should be okay here.
        if (nodeCurrentTimelineSignal < 1)
          continue;

        const int nodeFramebufferIndex = !(nodeCurrentTimelineSignal % MIDVK_SWAP_COUNT);

        // this needs to be a push set
        CmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, compNodePipeLayout, PIPE_SET_COMP_NODE_INDEX, 1, &compNodeSet, 0, NULL);

        // move this into method delegate in node compositor data
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
        AcquireNextImageKHR(device, swap.chain, UINT64_MAX, swap.acquireSemaphore, VK_NULL_HANDLE, &compositorNodeContext.swapIndex);
        __atomic_thread_fence(__ATOMIC_RELEASE);
        const VkImage swapImage = swap.images[compositorNodeContext.swapIndex];
        CmdPipelineImageBarrier2(cmd, &VKM_COLOR_IMAGE_BARRIER(VKM_IMAGE_BARRIER_COLOR_ATTACHMENT_UNDEFINED, VKM_IMG_BARRIER_BLIT_DST, swapImage));
        CmdBlitImageFullScreen(cmd, compositorFramebufferColorImages[compositorFramebufferIndex], swapImage);
        CmdPipelineImageBarrier2(cmd, &VKM_COLOR_IMAGE_BARRIER(VKM_IMG_BARRIER_BLIT_DST, VKM_IMG_BARRIER_PRESENT_BLIT_RELEASE, swapImage));
      }

      EndCommandBuffer(cmd);

      vkmTimelineSignal(device, compBaseCycleValue + MXC_CYCLE_RENDER_COMPOSITE, compTimeline);
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
}

void* mxcCompNodeThread(const MxcCompositorNodeContext* pNodeContext) {
  MxcCompNode compNode;

  vkmBeginAllocationRequests();
  const MxcCompNodeCreateInfo compNodeInfo = {
      .compMode = MXC_COMP_MODE_TESS,
  };
  mxcCreateCompNode(&compNodeInfo, &compNode);
  vkmEndAllocationRequests();

  mxcBindUpdateCompNode(&compNodeInfo, &compNode);
  mxcCompNodeRun(pNodeContext, &compNode);
  return NULL;
}