#include "comp_node.h"

#include <assert.h>
#include <stdatomic.h>
#include <vulkan/vk_enum_string_helper.h>

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

    CreateQuadMesh(0.5f, &pComp->quadMesh);

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
      swapBarrier[i] = VKM_IMAGE_BARRIER(VKM_IMAGE_BARRIER_UNDEFINED, VKM_IMAGE_BARRIER_PRESENT, VK_IMAGE_ASPECT_COLOR_BIT, pComp->swap.images[i]);
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
    pComp->standardRenderPass = context.standardRenderPass;
    pComp->standardPipelineLayout = context.standardPipe.pipelineLayout;
    pComp->standardPipeline = context.standardPipe.pipeline;
    pComp->globalSet = context.globalSet.set;
  }
}

// this should run on a different thread...
void mxcRunCompNode(const MxcBasicComp* pNode) {

  VkCommandBuffer cmd = pNode->cmd;
  VkRenderPass standardRenderPass = pNode->standardRenderPass;
  VkPipelineLayout standardPipelineLayout = pNode->standardPipelineLayout;
  VkPipeline standardPipeline = pNode->standardPipeline;
  VkDescriptorSet globalSet = pNode->globalSet;
  VkDescriptorSet checkerMaterialSet = pNode->checkerMaterialSet;
  VkDescriptorSet sphereObjectSet = pNode->sphereObjectSet;
  VkDevice device = pNode->device;
  VkmSwap swap = pNode->swap;

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
    frameBufferColorImages[i] = pNode->framebuffers[i].color.image;
  }

  // just making sure atomics are only using barriers, not locks
  for (int i = 0; i < MXC_NODE_COUNT; ++i) {
    assert(__atomic_always_lock_free(sizeof(MXC_NODE_SHARED[i].pendingTimelineSignal), &MXC_NODE_SHARED[i].pendingTimelineSignal));
    assert(__atomic_always_lock_free(sizeof(MXC_NODE_SHARED[i].currentTimelineSignal), &MXC_NODE_SHARED[i].currentTimelineSignal));
  }

  VKM_DEVICE_FUNC(CmdPipelineBarrier2);

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
      // only submit commands needs to be on main context
      {  // submit commands
        uint64_t value = MXC_NODE_SHARED[i].pendingTimelineSignal;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (value > MXC_NODE_SHARED[i].lastTimelineSignal) {
          MXC_NODE_SHARED[i].lastTimelineSignal = value;
          vkmSubmitCommandBuffer(MXC_NODE_SHARED[i].cmd, graphicsQueue, MXC_NODE_SHARED[i].nodeTimeline, value);
        }
      }

      if (!MXC_NODE_SHARED[i].active || MXC_NODE_SHARED[i].currentTimelineSignal < 1)
        continue;

      {  // check frame available
        uint64_t value = MXC_NODE_SHARED[i].currentTimelineSignal;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (value > MXC_NODE_SHARED[i].lastTimelineSwap) {
          {  // update framebuffer for comp
            MXC_NODE_SHARED[i].lastTimelineSwap = value;
            const int         nodeFramebufferIndex = !(value % VKM_SWAP_COUNT);
            const VkImageView nodeFramebufferColorImageView = MXC_NODE_SHARED[i].framebufferColorImageViews[nodeFramebufferIndex];
            const VkImage     nodeFramebufferColorImage = MXC_NODE_SHARED[i].framebufferColorImages[nodeFramebufferIndex];
            if (MXC_NODE_SHARED[i].type == MXC_NODE_TYPE_INTERPROCESS) {
              CmdPipelineImageBarrier(cmd, &VKM_IMAGE_BARRIER(VKM_IMAGE_BARRIER_EXTERNAL_ACQUIRE_GRAPHICS_ATTACH, VKM_IMAGE_BARRIER_SHADER_READ, VK_IMAGE_ASPECT_COLOR_BIT, nodeFramebufferColorImage));
            }
            vkmUpdateDescriptorSet(device, &VKM_SET_WRITE_STD_MATERIAL_IMAGE(checkerMaterialSet, nodeFramebufferColorImageView));
          }
          {  // update node state
            // move the global set state that was previously used to render into the node set state to use in comp
            memcpy((void*)&MXC_NODE_SHARED[i].nodeSetState, (void*)&MXC_NODE_SHARED[i].globalSetState, sizeof(context.globalSetState));
            // update model mat for node
            MXC_NODE_SHARED[i].nodeSetState.model = Mat4FromTransform(MXC_NODE_SHARED[i].transform.position, MXC_NODE_SHARED[i].transform.rotation);

            // calc framebuffersize
            const float radius = MXC_NODE_SHARED[i].radius;
            const vec4  ulModel = Vec4Rot(context.globalCameraTransform.rotation, (vec4){.x = -radius, .y = -radius, .w = 1});
            const vec4  ulWorld = Vec4MulMat4(MXC_NODE_SHARED[i].nodeSetState.model, ulModel);
            const vec4  ulClip = Vec4MulMat4(context.globalSetState.view, ulWorld);
            const vec3  ulNDC = Vec4WDivide(Vec4MulMat4(context.globalSetState.projection, ulClip));
            const vec2  ulUV = Vec2UVFromVec3NDC(ulNDC);
            const vec4  lrModel = Vec4Rot(context.globalCameraTransform.rotation, (vec4){.x = radius, .y = radius, .w = 1});
            const vec4  lrWorld = Vec4MulMat4(MXC_NODE_SHARED[i].nodeSetState.model, lrModel);
            const vec4  lrClip = Vec4MulMat4(context.globalSetState.view, lrWorld);
            const vec3  lrNDC = Vec4WDivide(Vec4MulMat4(context.globalSetState.projection, lrClip));
            const vec2  lrUV = Vec2UVFromVec3NDC(lrNDC);
            const vec2  diff = {.simd = lrUV.simd - ulUV.simd};

            __atomic_thread_fence(__ATOMIC_RELEASE);
            // write current global set state to node's global set state to use for next node render with new the framebuffer size
            MXC_NODE_SHARED[i].globalSetState = context.globalSetState;
            MXC_NODE_SHARED[i].globalSetState.framebufferSize = (ivec2){diff.x * DEFAULT_WIDTH, diff.y * DEFAULT_HEIGHT};
          }
        }
      }
    }

    // render could go on its own thread ? no it needs to wait for the node framebuffers to flip if needed
    {  // Render Cycle
      compTimeline.value = compBaseCycleValue + MXC_CYCLE_RENDER;
      vkmTimelineSignal(context.device, &compTimeline);

      framebufferIndex = !framebufferIndex;
      vkmCmdResetBegin(cmd);
      vkmCmdBeginPass(cmd, standardRenderPass, VKM_PASS_CLEAR_COLOR, framebuffers[framebufferIndex]);

      for (int i = 0; i < MXC_NODE_COUNT; ++i) {
        if (!MXC_NODE_SHARED[i].active || MXC_NODE_SHARED[i].currentTimelineSignal < 1)
          continue;

        // render quad
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, standardPipeline);
        // move to array
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, standardPipelineLayout, VKM_PIPE_SET_STD_GLOBAL_INDEX, 1, &globalSet, 0, NULL);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, standardPipelineLayout, VKM_PIPE_SET_STD_MATERIAL_INDEX, 1, &checkerMaterialSet, 0, NULL);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, standardPipelineLayout, VKM_PIPE_SET_STD_OBJECT_INDEX, 1, &sphereObjectSet, 0, NULL);

        vkCmdBindVertexBuffers(cmd, 0, 1, (const VkBuffer[]){quadBuffer}, (const VkDeviceSize[]){quadVertexOffset});
        vkCmdBindIndexBuffer(cmd, quadBuffer, quadIndexOffset, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cmd, quadIndexCount, 1, 0, 0, 0);
      }

      vkCmdEndRenderPass(cmd);

      {  // Blit Framebuffer
        vkAcquireNextImageKHR(device, swap.chain, UINT64_MAX, swap.acquireSemaphore, VK_NULL_HANDLE, &swap.swapIndex);
        const VkImage               swapImage = swap.images[swap.swapIndex];
        const VkImage               framebufferColorImage = frameBufferColorImages[framebufferIndex];
        const VkImageMemoryBarrier2 blitBarrier[] = {
            VKM_IMAGE_BARRIER(VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VKM_TRANSFER_SRC_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, framebufferColorImage),
            VKM_IMAGE_BARRIER(VKM_IMAGE_BARRIER_PRESENT, VKM_TRANSFER_DST_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, swapImage),
        };
        CmdPipelineImageBarriers(cmd, 2, blitBarrier);
        vkmBlit(cmd, framebufferColorImage, swapImage);
        const VkImageMemoryBarrier2 presentBarrier[] = {
            //        VKM_IMAGE_BARRIER(VKM_TRANSFER_SRC_IMAGE_BARRIER, VKM_COLOR_ATTACHMENT_IMAGE_BARRIER, VK_IMAGE_ASPECT_COLOR_BIT, framebufferColorImage),
            VKM_IMAGE_BARRIER(VKM_TRANSFER_DST_IMAGE_BARRIER, VKM_IMAGE_BARRIER_PRESENT, VK_IMAGE_ASPECT_COLOR_BIT, swapImage),
        };
        CmdPipelineImageBarriers(cmd, 1, presentBarrier);
      }

      vkEndCommandBuffer(cmd);

      // signal input cycle on complete
      compBaseCycleValue += MXC_CYCLE_COUNT;
      vkmSubmitPresentCommandBuffer(cmd, graphicsQueue, &swap, compTimeline.semaphore, compBaseCycleValue + MXC_CYCLE_INPUT);
    }
  }

  if (!isRunning) return;

  goto run_loop;
}