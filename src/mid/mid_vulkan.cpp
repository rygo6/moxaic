/***********************************************************************************
 *
 *   Mid-Level Vulkan Implementation
 *
 ************************************************************************************/

#include "mid_vulkan.hpp"

using namespace Mid;
using namespace Mid::Vk;

void SetDebugInfo(
    LogicalDevice      logicalDevice,
    const VkObjectType objectType,
    const uint64_t     objectHandle,
    const char*        name) {
  // DebugUtilsObjectNameInfoEXT info{
  //     .objectType = objectType,
  //     .objectHandle = objectHandle,
  //     .pObjectName = name,
  // };
  // PFN.SetDebugUtilsObjectNameEXT(logicalDevice, (VkDebugUtilsObjectNameInfoEXT*)&info);
}

void ComputePipeline2::BindPipeline(const VkCommandBuffer commandBuffer) const {
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, Handle());
}

LogicalDevice Vk::LogicalDevice::Create(
    VkPhysicalDevice             physicalDevice,
    const char*                  debugName,
    const VkDeviceCreateInfo*    pCreateInfo,
    const VkAllocationCallbacks* pAllocator) {
  auto  handle = Acquire();
  auto& state = handle.State();
  state.physicalDevice = physicalDevice;
  state.pDefaultAllocator = pAllocator;
  state.result = vkCreateDevice(
      physicalDevice,
      pCreateInfo,
      pAllocator,
      &handle.Handle());
  SetDebugInfo(handle, VK_OBJECT_TYPE_DEVICE, (uint64_t)handle.Handle(), debugName);
  return handle;
}

const VkAllocationCallbacks*
LogicalDevice::DefaultAllocator(
    const VkAllocationCallbacks* pAllocator) {
  return pAllocator == nullptr ? State().pDefaultAllocator : pAllocator;
}

PipelineLayout2 Vk::PipelineLayout2::Create(
    LogicalDevice                   logicalDevice,
    const char*                     debugName,
    const PipelineLayoutCreateInfo* pCreateInfo,
    const VkAllocationCallbacks*    pAllocator) {
  printf("PipelineLayout::Create %s\n", debugName);
  auto  handle = Acquire();
  auto& state = handle.State();
  state.logicalDevice = logicalDevice;
  state.pAllocator = logicalDevice.DefaultAllocator(pAllocator);
  state.result = vkCreatePipelineLayout(
      logicalDevice,
      (VkPipelineLayoutCreateInfo*)pCreateInfo,
      state.pAllocator,
      &handle.Handle());
  SetDebugInfo(logicalDevice, VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)handle.Handle(), debugName);
  return handle;
}
void Vk::PipelineLayout2::Destroy() {
  printf("PipelineLayout::Destroy\n");
  HandleBase::Release();
  vkDestroyPipelineLayout(State().logicalDevice, Handle(), State().pAllocator);
}

ComputePipeline2 Vk::ComputePipeline2::Create(
    LogicalDevice                    logicalDevice,
    const char*                      debugName,
    uint32_t                         createInfoCount,
    const ComputePipelineCreateInfo* pCreateInfos,
    const VkAllocationCallbacks*     pAllocator) {
  printf("ComputePipeline::Create %s\n", debugName);
  auto  handle = Acquire();
  auto& state = handle.State();
  state.logicalDevice = logicalDevice;
  state.pAllocator = logicalDevice.DefaultAllocator(pAllocator);
  state.result = vkCreateComputePipelines(
      logicalDevice,
      VK_NULL_HANDLE,
      createInfoCount,
      (VkComputePipelineCreateInfo*)pCreateInfos,
      state.pAllocator,
      &handle.Handle());
  SetDebugInfo(logicalDevice, VK_OBJECT_TYPE_PIPELINE, (uint64_t)handle.Handle(), debugName);
  return handle;
}
void Vk::ComputePipeline2::Destroy() {
  printf("ComputePipeline2::Destroy\n");
  HandleBase::Release();
  vkDestroyPipeline(State().logicalDevice, Handle(), State().pAllocator);
}

void Vk::CmdPipelineImageBarrier2(
    VkCommandBuffer              commandBuffer,
    uint32_t                     imageMemoryBarrierCount,
    const VkImageMemoryBarrier2* pImageMemoryBarriers) {
  const VkDependencyInfo toComputeDependencyInfo = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .imageMemoryBarrierCount = imageMemoryBarrierCount,
      .pImageMemoryBarriers = pImageMemoryBarriers,
  };
  vkCmdPipelineBarrier2(commandBuffer, &toComputeDependencyInfo);
}