#include "comp_node.h"

#include <assert.h>
#include <vulkan/vk_enum_string_helper.h>


enum SetBindBasicCompIndices {
  SET_BIND_BASIC_COMP_BUFFER_INDEX,
  SET_BIND_BASIC_COMP_COLOR_INDEX,
  SET_BIND_BASIC_COMP_NORMAL_INDEX,
  SET_BIND_BASIC_COMP_GBUFFER_INDEX,
  SET_BIND_BASIC_COMP_COUNT
};
#define SET_WRITE_COMP_NODE_BUFFER(node_set, node_buffer) \
  (VkWriteDescriptorSet) {                                \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,      \
    .dstSet = node_set,                                   \
    .dstBinding = SET_BIND_BASIC_COMP_BUFFER_INDEX,       \
    .descriptorCount = 1,                                 \
    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  \
    .pBufferInfo = &(const VkDescriptorBufferInfo){       \
        .buffer = node_buffer,                            \
        .range = sizeof(MxcNodeSetState),                 \
    },                                                    \
  }
#define SET_WRITE_COMP_NODE_COLOR(node_set, color_image_view)    \
  (VkWriteDescriptorSet) {                                       \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,             \
    .dstSet = node_set,                                          \
    .dstBinding = SET_BIND_BASIC_COMP_COLOR_INDEX,               \
    .descriptorCount = 1,                                        \
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
    .pImageInfo = &(const VkDescriptorImageInfo){                \
        .imageView = color_image_view,                           \
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, \
    },                                                           \
  }
#define SET_WRITE_COMP_NODE_NORMAL(node_set, normal_image_view)  \
  (VkWriteDescriptorSet) {                                       \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,             \
    .dstSet = node_set,                                          \
    .dstBinding = SET_BIND_BASIC_COMP_NORMAL_INDEX,              \
    .descriptorCount = 1,                                        \
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, \
    .pImageInfo = &(const VkDescriptorImageInfo){                \
        .imageView = normal_image_view,                          \
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, \
    },                                                           \
  }
#define SET_WRITE_COMP_NODE_GBUFFER(node_set, gbuffer_image_view) \
  (VkWriteDescriptorSet) {                                        \
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,              \
    .dstSet = node_set,                                           \
    .dstBinding = SET_BIND_BASIC_COMP_GBUFFER_INDEX,              \
    .descriptorCount = 1,                                         \
    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  \
    .pImageInfo = &(const VkDescriptorImageInfo){                 \
        .imageView = gbuffer_image_view,                          \
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,  \
    },                                                            \
  }
#define SHADER_STAGE_VERT_TESC_TESE_FRAG VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
static void CreateNodeSetLayout(VkDescriptorSetLayout* pNodeLayout) {
  const VkDescriptorSetLayoutCreateInfo nodeSetLayoutCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = SET_BIND_BASIC_COMP_COUNT,
      .pBindings = (const VkDescriptorSetLayoutBinding[]){
          [SET_BIND_BASIC_COMP_BUFFER_INDEX] = {
              .binding = SET_BIND_BASIC_COMP_BUFFER_INDEX,
              .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .descriptorCount = 1,
              .stageFlags = SHADER_STAGE_VERT_TESC_TESE_FRAG},
          [SET_BIND_BASIC_COMP_COLOR_INDEX] = {
              .binding = SET_BIND_BASIC_COMP_COLOR_INDEX,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = SHADER_STAGE_VERT_TESC_TESE_FRAG,
              .pImmutableSamplers = &context.linearSampler,
          },
          [SET_BIND_BASIC_COMP_NORMAL_INDEX] = {
              .binding = SET_BIND_BASIC_COMP_NORMAL_INDEX,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = SHADER_STAGE_VERT_TESC_TESE_FRAG,
              .pImmutableSamplers = &context.linearSampler,
          },
          [SET_BIND_BASIC_COMP_GBUFFER_INDEX] = {
              .binding = SET_BIND_BASIC_COMP_GBUFFER_INDEX,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .stageFlags = SHADER_STAGE_VERT_TESC_TESE_FRAG,
              .pImmutableSamplers = &context.linearSampler,
          },
      },
  };
  VKM_REQUIRE(vkCreateDescriptorSetLayout(context.device, &nodeSetLayoutCreateInfo, VKM_ALLOC, pNodeLayout));
}

enum PipeSetBasicCompIndices {
  PIPE_SET_BASIC_COMP_GLOBAL_INDEX,
  PIPE_SET_BASIC_COMP_NODE_INDEX,
  PIPE_SET_BASIC_COMP_COUNT,
};
static void CreateNodePipeLayout(const VkDescriptorSetLayout nodeSetLayout, VkPipelineLayout* pNodePipeLayout) {
  const VkPipelineLayoutCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = PIPE_SET_BASIC_COMP_COUNT,
      .pSetLayouts = (const VkDescriptorSetLayout[]){
          [PIPE_SET_BASIC_COMP_GLOBAL_INDEX] = context.stdPipe.globalSetLayout,
          [PIPE_SET_BASIC_COMP_NODE_INDEX] = nodeSetLayout,
      },
  };
  VKM_REQUIRE(vkCreatePipelineLayout(context.device, &createInfo, VKM_ALLOC, pNodePipeLayout));
}

static void CreateQuadMesh(const float size, VkmMesh* pMesh) {
  const uint16_t  indices[] = {0, 1, 2, 1, 3, 2};
  const VkmVertex vertices[] = {
      {.position = {-size, -size, 0}, .uv = {0, 0}},
      {.position = {size, -size, 0}, .uv = {1, 0}},
      {.position = {-size, size, 0}, .uv = {0, 1}},
      {.position = {size, size, 0}, .uv = {1, 1}},
  };
  const VkmMeshCreateInfo info = {
      .indexCount = 6,
      .vertexCount = 4,
      .pIndices = indices,
      .pVertices = vertices,
  };
  VkmCreateMesh(&info, pMesh);
}
static void CreateQuadPatchMesh(const float size, VkmMesh* pMesh) {
  const uint16_t  indices[] = {0, 1, 3, 2};
  const VkmVertex vertices[] = {
      {.position = {-size, -size, 0}, .uv = {0, 0}},
      {.position = {size, -size, 0}, .uv = {1, 0}},
      {.position = {-size, size, 0}, .uv = {0, 1}},
      {.position = {size, size, 0}, .uv = {1, 1}},
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

    vkmCreateStdFramebuffers(context.stdRenderPass, VKM_SWAP_COUNT, VKM_LOCALITY_CONTEXT, pComp->framebuffers);

    // node set
    CreateNodeSetLayout(&pComp->nodeSetLayout);
    CreateNodePipeLayout(pComp->nodeSetLayout, &pComp->nodePipeLayout);
    //        vkmCreateBasicPipe("./shaders/basic_comp.vert.spv", "./shaders/basic_comp.frag.spv", pComp->nodePipeLayout, &pComp->nodePipe);
    vkmCreateTessPipe("./shaders/tess_comp.vert.spv", "./shaders/tess_comp.tesc.spv", "./shaders/tess_comp.tese.spv", "./shaders/tess_comp.frag.spv", pComp->nodePipeLayout, &pComp->nodePipe);

    vkmAllocateDescriptorSet(context.descriptorPool, &pComp->nodeSetLayout, &pComp->nodeSet);
    VkmSetDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)pComp->nodeSet, "NodeSet");
    vkmCreateAllocBindMapBuffer(VKM_MEMORY_LOCAL_HOST_VISIBLE_COHERENT, sizeof(MxcNodeSetState), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VKM_LOCALITY_CONTEXT, &pComp->nodeSetMemory, &pComp->nodeSetBuffer, (void**)&pComp->pNodeSetMapped);
    vkmUpdateDescriptorSet(context.device, &SET_WRITE_COMP_NODE_BUFFER(pComp->nodeSet, pComp->nodeSetBuffer));
    memcpy(&pComp->pNodeSetMapped->model, &MAT4_IDENT, sizeof(mat4));

    //    CreateQuadMesh(0.5f, &pComp->quadMesh);
    CreateQuadPatchMesh(0.5f, &pComp->quadMesh);

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
    vkResetCommandBuffer(pComp->cmd, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    vkBeginCommandBuffer(pComp->cmd, &(const VkCommandBufferBeginInfo){.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT});
    VkImageMemoryBarrier2 swapBarrier[VKM_SWAP_COUNT];
    for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
      swapBarrier[i] = VKM_COLOR_IMAGE_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMAGE_BARRIER_PRESENT, pComp->swap.images[i]);
    }
    vkCmdPipelineBarrier2(pComp->cmd, &(const VkDependencyInfo){.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = VKM_SWAP_COUNT, .pImageMemoryBarriers = swapBarrier});
    vkEndCommandBuffer(pComp->cmd);
    const VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &pComp->cmd,
    };
    VKM_REQUIRE(vkQueueSubmit(context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].queue, 1, &submitInfo, VK_NULL_HANDLE));
    // WE PROBABLY DONT WANT TO DO THIS HERE, GET IT IN THE COMP THREAD
    VKM_REQUIRE(vkQueueWaitIdle(context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].queue));
  }

  {  // Copy needed state
    pComp->device = context.device;
    pComp->stdRenderPass = context.stdRenderPass;
    pComp->globalSet = context.globalSet.set;
  }
}

// this should run on a different thread...
void mxcRunCompNode(const MxcBasicComp* pNode) {

  VkCommandBuffer cmd = pNode->cmd;
  VkRenderPass    stdRenderPass = pNode->stdRenderPass;
  VkDescriptorSet globalSet = pNode->globalSet;

  MxcNodeSetState* pNodeSetMapped = pNode->pNodeSetMapped;
  VkPipelineLayout nodePipeLayout = pNode->nodePipeLayout;
  VkPipeline       nodePipe = pNode->nodePipe;
  VkDescriptorSet  nodeSet = pNode->nodeSet;

  VkDevice device = pNode->device;
  VkmSwap  swap = pNode->swap;

  uint32_t     quadIndexCount = pNode->quadMesh.indexCount;
  VkBuffer     quadBuffer = pNode->quadMesh.buffer;
  VkDeviceSize quadIndexOffset = pNode->quadMesh.indexOffset;
  VkDeviceSize quadVertexOffset = pNode->quadMesh.vertexOffset;

  VkmTimeline compTimeline = {.semaphore = pNode->timeline};
  uint64_t    compBaseCycleValue = 0;

  VkQueue graphicsQueue = context.queueFamilies[VKM_QUEUE_FAMILY_TYPE_MAIN_GRAPHICS].queue;

  int           framebufferIndex = 0;
  VkFramebuffer framebuffers[VKM_SWAP_COUNT];
  VkImage       frameBufferColorImages[VKM_SWAP_COUNT];
  for (int i = 0; i < VKM_SWAP_COUNT; ++i) {
    framebuffers[i] = pNode->framebuffers[i].framebuffer;
    frameBufferColorImages[i] = pNode->framebuffers[i].color.img;
  }

  // just making sure atomics are only using barriers, not locks
  for (int i = 0; i < MXC_NODE_COUNT; ++i) {
    assert(__atomic_always_lock_free(sizeof(MXC_NODE_SHARED[i].pendingTimelineSignal), &MXC_NODE_SHARED[i].pendingTimelineSignal));
    assert(__atomic_always_lock_free(sizeof(MXC_NODE_SHARED[i].currentTimelineSignal), &MXC_NODE_SHARED[i].currentTimelineSignal));
  }

  VKM_DEVICE_FUNC(CmdPipelineBarrier2);

  bool debugSwap;

run_loop:

{  // Input Cycle
  compTimeline.value = compBaseCycleValue + MXC_CYCLE_INPUT;
  vkmTimelineWait(device, &compTimeline);

  vkmUpdateWindowInput();

  if (vkmProcessInput(&context.globalCameraTransform)) {
    vkmUpdateGlobalSetView(&context.globalCameraTransform, &context.globalSetState, context.globalSet.pMapped);  // move global to hot?
  }
}

  {  // Node cycle
    compTimeline.value = compBaseCycleValue + MXC_CYCLE_NODE;
    vkmTimelineSignal(context.device, &compTimeline);

    for (int i = 0; i < MXC_NODE_COUNT; ++i) {
      // only submit commands and input needs to be on main context
      {  // submit commands
        uint64_t value = MXC_NODE_SHARED[i].pendingTimelineSignal;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (value > MXC_NODE_SHARED[i].lastTimelineSignal) {
          MXC_NODE_SHARED[i].lastTimelineSignal = value;
          vkmSubmitCommandBuffer(MXC_NODE_SHARED[i].cmd, graphicsQueue, MXC_NODE_SHARED[i].nodeTimeline, value);
        }
      }

      // all below can go on other thread
      if (!MXC_NODE_SHARED[i].active || MXC_NODE_SHARED[i].currentTimelineSignal < 1)
        continue;

      // update node model mat... this should happen every frame so player can move it in comp
      MXC_NODE_SHARED[i].transform.rotation = QuatFromEuler(MXC_NODE_SHARED[i].transform.euler);
      MXC_NODE_SHARED[i].nodeSetState.model = Mat4FromTransform(MXC_NODE_SHARED[i].transform.position, MXC_NODE_SHARED[i].transform.rotation);

      {  // check frame available
        uint64_t value = MXC_NODE_SHARED[i].currentTimelineSignal;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (value > MXC_NODE_SHARED[i].lastTimelineSwap) {
          {  // update framebuffer for comp
            MXC_NODE_SHARED[i].lastTimelineSwap = value;
            const int nodeFramebufferIndex = !(value % VKM_SWAP_COUNT);
            if (MXC_NODE_SHARED[i].type == MXC_NODE_TYPE_INTERPROCESS) {
              //              CmdPipelineImageBarrier(cmd, &VKM_COLOR_IMAGE_BARRIER(VKM_IMAGE_BARRIER_EXTERNAL_ACQUIRE_GRAPHICS_ATTACH, VKM_IMAGE_BARRIER_SHADER_READ, MXC_NODE_SHARED[i].framebufferColorImages[nodeFramebufferIndex]));
              //              CmdPipelineImageBarrier(cmd, &VKM_COLOR_IMAGE_BARRIER(VKM_IMAGE_BARRIER_EXTERNAL_ACQUIRE_GRAPHICS_ATTACH, VKM_IMAGE_BARRIER_SHADER_READ, MXC_NODE_SHARED[i].framebufferNormalImages[nodeFramebufferIndex]));
              //              CmdPipelineImageBarrier(cmd, &VKM_COLOR_IMAGE_BARRIER(VKM_IMAGE_BARRIER_EXTERNAL_ACQUIRE_GRAPHICS_ATTACH, VKM_IMAGE_BARRIER_SHADER_READ, MXC_NODE_SHARED[i].framebufferGBufferImages[nodeFramebufferIndex]));
            }
            const VkWriteDescriptorSet writeSets[] = {
                SET_WRITE_COMP_NODE_COLOR(nodeSet, MXC_NODE_SHARED[i].framebufferColorImageViews[nodeFramebufferIndex]),
                SET_WRITE_COMP_NODE_NORMAL(nodeSet, MXC_NODE_SHARED[i].framebufferNormalImageViews[nodeFramebufferIndex]),
                SET_WRITE_COMP_NODE_GBUFFER(nodeSet, MXC_NODE_SHARED[i].framebufferGBufferImageViews[nodeFramebufferIndex]),
            };
            vkUpdateDescriptorSets(context.device, COUNT(writeSets), writeSets, 0, NULL);
          }
          {
            debugSwap = input.debugSwap;

            // move the global set state that was previously used to render into the node set state to use in comp
            memcpy(&MXC_NODE_SHARED[i].nodeSetState.view, (void*)&MXC_NODE_SHARED[i].globalSetState, sizeof(VkmGlobalSetState));
            MXC_NODE_SHARED[i].nodeSetState.ulUV = MXC_NODE_SHARED[i].lrUV;
            MXC_NODE_SHARED[i].nodeSetState.lrUV = MXC_NODE_SHARED[i].ulUV;

            if (debugSwap) {
              MXC_NODE_SHARED[i].nodeSetState.ulUV = (vec2){0, 0};
              MXC_NODE_SHARED[i].nodeSetState.lrUV = (vec2){1, 1};
            }

            memcpy(pNodeSetMapped, &MXC_NODE_SHARED[i].nodeSetState, sizeof(MxcNodeSetState));

            // calc framebuffersize
            const float radius = MXC_NODE_SHARED[i].radius;

            const vec4 ulModel = Vec4Rot(context.globalCameraTransform.rotation, (vec4){.x = -radius, .y = -radius, .w = 1});
            const vec4 ulWorld = Vec4MulMat4(MXC_NODE_SHARED[i].nodeSetState.model, ulModel);
            const vec4 ulClip = Vec4MulMat4(context.globalSetState.view, ulWorld);
            const vec3 ulNDC = Vec4WDivide(Vec4MulMat4(context.globalSetState.proj, ulClip));
            const vec2 ulUV = Vec2Clamp(UVFromNDC(ulNDC), 0.0f, 1.0f);


            const vec4 lrModel = Vec4Rot(context.globalCameraTransform.rotation, (vec4){.x = radius, .y = radius, .w = 1});
            const vec4 lrWorld = Vec4MulMat4(MXC_NODE_SHARED[i].nodeSetState.model, lrModel);
            const vec4 lrClip = Vec4MulMat4(context.globalSetState.view, lrWorld);
            const vec3 lrNDC = Vec4WDivide(Vec4MulMat4(context.globalSetState.proj, lrClip));
            const vec2 lrUV = Vec2Clamp(UVFromNDC(lrNDC), 0.0f, 1.0f);

            const vec2 diff = {.vec = lrUV.vec - ulUV.vec};

            __atomic_thread_fence(__ATOMIC_RELEASE);
            // write current global set state to node's global set state to use for next node render with new the framebuffer size
            memcpy((void*)&MXC_NODE_SHARED[i].globalSetState, &context.globalSetState, sizeof(VkmGlobalSetState) - sizeof(ivec2));
            MXC_NODE_SHARED[i].globalSetState.framebufferSize = (ivec2){diff.x * DEFAULT_WIDTH, diff.y * DEFAULT_HEIGHT};
            MXC_NODE_SHARED[i].ulUV = ulUV;
            MXC_NODE_SHARED[i].lrUV = lrUV;

            if (debugSwap) {
              MXC_NODE_SHARED[i].globalSetState.framebufferSize = (ivec2){DEFAULT_WIDTH, DEFAULT_HEIGHT};
              MXC_NODE_SHARED[i].ulUV = (vec2){0, 0};
              MXC_NODE_SHARED[i].lrUV = (vec2){1, 1};
            }
          }
        }
      }
    }

    // render could go on its own thread ? no it needs to wait for the node framebuffers to flip if needed... yes it can but node framebuffer flip also needs to be on render thread, input and node submit can be on main
    {  // Render Cycle
      compTimeline.value = compBaseCycleValue + MXC_CYCLE_RENDER;
      vkmTimelineSignal(context.device, &compTimeline);

      framebufferIndex = !framebufferIndex;

      vkmCmdResetBegin(cmd);
      vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_NONE, context.timeQueryPool, 0);

      vkmCmdBeginPass(cmd, stdRenderPass, VKM_PASS_CLEAR_COLOR, framebuffers[framebufferIndex]);

      vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodePipe);
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodePipeLayout, PIPE_SET_BASIC_COMP_GLOBAL_INDEX, 1, &globalSet, 0, NULL);

      for (int i = 0; i < MXC_NODE_COUNT; ++i) {
        if (!MXC_NODE_SHARED[i].active || MXC_NODE_SHARED[i].currentTimelineSignal < 1)
          continue;

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, nodePipeLayout, PIPE_SET_BASIC_COMP_NODE_INDEX, 1, &nodeSet, 0, NULL);

        vkCmdBindVertexBuffers(cmd, 0, 1, (const VkBuffer[]){quadBuffer}, (const VkDeviceSize[]){quadVertexOffset});
        vkCmdBindIndexBuffer(cmd, quadBuffer, quadIndexOffset, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cmd, quadIndexCount, 1, 0, 0, 0);
      }

      vkCmdEndRenderPass(cmd);

      vkResetQueryPool(device, context.timeQueryPool, 0, 2);
      vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, context.timeQueryPool, 1);

      {  // Blit Framebuffer
        vkAcquireNextImageKHR(device, swap.chain, UINT64_MAX, swap.acquireSemaphore, VK_NULL_HANDLE, &swap.swapIndex);
        const VkImage               swapImage = swap.images[swap.swapIndex];
        const VkImage               framebufferColorImage = frameBufferColorImages[framebufferIndex];
        const VkImageMemoryBarrier2 blitBarrier[] = {
            VKM_COLOR_IMAGE_BARRIER(VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VKM_TRANSFER_SRC_IMAGE_BARRIER, framebufferColorImage),
            VKM_COLOR_IMAGE_BARRIER(VKM_IMAGE_BARRIER_PRESENT, VKM_TRANSFER_DST_IMAGE_BARRIER, swapImage),
        };
        CmdPipelineImageBarriers2(cmd, 2, blitBarrier);

        if (debugSwap)
          vkmBlit(cmd, MXC_NODE_SHARED[0].framebufferColorImages[framebufferIndex], swapImage);
        else
          vkmBlit(cmd, framebufferColorImage, swapImage);

        const VkImageMemoryBarrier2 presentBarrier[] = {
            //        VKM_IMAGE_BARRIER(VKM_TRANSFER_SRC_IMAGE_BARRIER, VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, framebufferColorImage),
            VKM_COLOR_IMAGE_BARRIER(VKM_TRANSFER_DST_IMAGE_BARRIER, VKM_IMAGE_BARRIER_PRESENT, swapImage),
        };
        CmdPipelineImageBarriers2(cmd, 1, presentBarrier);
      }

      vkEndCommandBuffer(cmd);

      // signal input cycle on complete
      compBaseCycleValue += MXC_CYCLE_COUNT;
      vkmSubmitPresentCommandBuffer(cmd, graphicsQueue, &swap, compTimeline.semaphore, compBaseCycleValue + MXC_CYCLE_INPUT);


      uint64_t timestampsNS[2];
      vkGetQueryPoolResults(device, context.timeQueryPool, 0, 2, sizeof(uint64_t) * 2, timestampsNS, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
      double timestampsMS[2];
      for (uint32_t i = 0; i < 2; ++i) {
        timestampsMS[i] = (double)timestampsNS[i] / (double)1000000;  // ns to ms
      }
      timeQueryMs = timestampsMS[1] -  timestampsMS[0];
    }
  }

  CHECK_RUNNING;

  goto run_loop;
}