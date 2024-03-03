/***********************************************************************************
 *
 *   Mid-Level Vulkan Implementation
 *
 ************************************************************************************/

#include "mid_vulkan.hpp"

#define LOG(...) printf(__VA_ARGS__)

#define CHECK_RESULT(handle)                                       \
  if (handle.state()->result != VK_SUCCESS) [[unlikely]] {         \
    LOG("Error! %s\n\n", string_VkResult(handle.state()->result)); \
    return handle;                                                 \
  }

using namespace Mid;
using namespace Mid::Vk;

static void SetDebugInfo(
    LogicalDevice      logicalDevice,
    const VkObjectType objectType,
    const uint64_t     objectHandle,
    const char*        name) {
  DebugUtilsObjectNameInfoEXT info{
      .objectType = objectType,
      .objectHandle = objectHandle,
      .pObjectName = name,
  };
  PFN.SetDebugUtilsObjectNameEXT(logicalDevice, (VkDebugUtilsObjectNameInfoEXT*)&info);
}

void ComputePipeline2::BindPipeline(const VkCommandBuffer commandBuffer) const {
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *this);
}

Instance Instance::Create(const InstanceDesc&& desc) {
  LOG("# VkInstance... ");

  auto instance = Instance::Acquire();
  auto state = instance.state();

  state->pInstanceAllocator = desc.pAllocator;

  LOG("Creating... ");
  state->result = vkCreateInstance(desc.createInfo.ptr(), desc.pAllocator, instance.handle());
  CHECK_RESULT(instance);

#define MVK_PFN_FUNCTION(func)                                                    \
  LOG("Loading PFN_%s... ", "vk" #func);                                          \
  PFN.func = (PFN_vk##func)vkGetInstanceProcAddr(*instance.handle(), "vk" #func); \
  if (PFN.SetDebugUtilsObjectNameEXT == nullptr) {                                \
    LOG("Fail! %s\n\n", #func);                                                   \
    return instance;                                                              \
  }
  MVK_PFN_FUNCTIONS
#undef MVK_PFN_FUNCTION

  LOG("%s\n\n", instance.ResultName());
  return instance;
}

struct VkBoolFeatureStruct {
  VkStructureType sType;
  void*           pNext;
  VkBool32        firstBool;
};

static bool CheckFeatureBools(
    const VkBoolFeatureStruct* requiredPtr,
    const VkBoolFeatureStruct* supportedPtr,
    const int                  structSize) {
  constexpr int sTypeOffset = sizeof(VkStructureType);
  constexpr int voidPtrOffset = sizeof(void*);

  auto requiredBools = &requiredPtr->firstBool;
  auto supportedBools = &supportedPtr->firstBool;
  int  size = (structSize - sTypeOffset - voidPtrOffset) / sizeof(VkBool32);
  LOG("Checking %d bools... ", size);
  for (int i = 0; i < size; ++i) {
    if (requiredBools[i] && !supportedBools[i]) {
      LOG("Index %d feature not supported on physicalDevice!", i);
      return false;
    }
  }
  return true;
}

static bool CheckPhysicalDeviceFeatureBools(
    int                         requiredCount,
    const VkBoolFeatureStruct** required,
    const VkBoolFeatureStruct** supported,
    const size_t*               structSize) {
  for (int i = 0; i < requiredCount; ++i) {
    printf("Checking %s... ", string_VkStructureType(supported[i]->sType));
    if (!CheckFeatureBools(required[i], supported[i], structSize[i])) {
      LOG("PhysicalDevicel didn't support necessary feature! Skipping!... ");
      return false;
    }
  }
  return true;
}

PhysicalDevice Instance::CreatePhysicalDevice(const PhysicalDeviceDesc&& desc) {
  LOG("# VkPhysicalDevice... ");

  auto physicalDevice = PhysicalDevice::Acquire();
  auto state = physicalDevice.state();

  LOG("Getting device count... ");
  uint32_t deviceCount = 0;
  state->result = vkEnumeratePhysicalDevices(*this, &deviceCount, nullptr);
  CHECK_RESULT(physicalDevice);

  LOG("%d devices found... ", deviceCount);
  state->result = deviceCount > 0 ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
  CHECK_RESULT(physicalDevice);

  LOG("Getting devices... ");
  VkPhysicalDevice devices[deviceCount];
  state->result = vkEnumeratePhysicalDevices(*this, &deviceCount, devices);
  CHECK_RESULT(physicalDevice);

  state->physicalDeviceGlobalPriorityQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_EXT;
  state->physicalDeviceGlobalPriorityQueryFeatures.pNext = nullptr;
  state->physicalDeviceRobustness2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
  state->physicalDeviceRobustness2Features.pNext = &state->physicalDeviceGlobalPriorityQueryFeatures;
  state->physicalDeviceMeshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
  state->physicalDeviceMeshShaderFeatures.pNext = &state->physicalDeviceRobustness2Features;
  state->physicalDeviceFeatures13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  state->physicalDeviceFeatures13.pNext = &state->physicalDeviceMeshShaderFeatures;
  state->physicalDeviceFeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  state->physicalDeviceFeatures12.pNext = &state->physicalDeviceFeatures13;
  state->physicalDeviceFeatures11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
  state->physicalDeviceFeatures11.pNext = &state->physicalDeviceFeatures12;
  state->physicalDeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  state->physicalDeviceFeatures.pNext = &state->physicalDeviceFeatures11;

  const VkBoolFeatureStruct* required[]{
      (VkBoolFeatureStruct*)&desc.physicalDeviceFeatures,
      (VkBoolFeatureStruct*)&desc.physicalDeviceFeatures11,
      (VkBoolFeatureStruct*)&desc.physicalDeviceFeatures12,
      (VkBoolFeatureStruct*)&desc.physicalDeviceFeatures13,
      (VkBoolFeatureStruct*)&desc.physicalDeviceMeshShaderFeatures,
      (VkBoolFeatureStruct*)&desc.physicalDeviceRobustness2Features,
      (VkBoolFeatureStruct*)&desc.physicalDeviceGlobalPriorityQueryFeatures,
  };
  const VkBoolFeatureStruct* supported[]{
      (VkBoolFeatureStruct*)&state->physicalDeviceFeatures,
      (VkBoolFeatureStruct*)&state->physicalDeviceFeatures11,
      (VkBoolFeatureStruct*)&state->physicalDeviceFeatures12,
      (VkBoolFeatureStruct*)&state->physicalDeviceFeatures13,
      (VkBoolFeatureStruct*)&state->physicalDeviceMeshShaderFeatures,
      (VkBoolFeatureStruct*)&state->physicalDeviceRobustness2Features,
      (VkBoolFeatureStruct*)&state->physicalDeviceGlobalPriorityQueryFeatures,
  };
  const size_t structSize[]{
      sizeof(desc.physicalDeviceFeatures),
      sizeof(desc.physicalDeviceFeatures11),
      sizeof(desc.physicalDeviceFeatures12),
      sizeof(desc.physicalDeviceFeatures13),
      sizeof(desc.physicalDeviceMeshShaderFeatures),
      sizeof(desc.physicalDeviceRobustness2Features),
      sizeof(desc.physicalDeviceGlobalPriorityQueryFeatures),
  };
  constexpr int requiredCount = sizeof(required) / sizeof(required[0]);

  int chosenDeviceIndex = -1;
  for (int deviceIndex = 0; deviceIndex < deviceCount; ++deviceIndex) {
    printf("Getting Physical Device Index %d Memory Properties... ", deviceIndex);
    vkGetPhysicalDeviceFeatures2(devices[deviceIndex], &state->physicalDeviceFeatures);

    if (!CheckPhysicalDeviceFeatureBools(requiredCount, required, supported, structSize))
      continue;

    chosenDeviceIndex = deviceIndex;
    break;
  }

  if (chosenDeviceIndex == -1) {
    LOG("No suitable PhysicalDevice found!");
    state->result = VK_ERROR_INITIALIZATION_FAILED;
  }
  else {
    LOG("Picking device %d... ", desc.deviceIndex);
    *physicalDevice.handle() = devices[desc.deviceIndex];
    state->result = VK_SUCCESS;
  }
  CHECK_RESULT(physicalDevice);

  LOG("Getting Physical Device Properties... ");
  state->physicalDeviceMeshShaderProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
  state->physicalDeviceMeshShaderProperties.pNext = nullptr;
  state->physicalDeviceSubgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
  state->physicalDeviceSubgroupProperties.pNext = &state->physicalDeviceMeshShaderProperties;
  VkPhysicalDeviceProperties2 physicalDeviceProperties2{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
      .pNext = &state->physicalDeviceSubgroupProperties,
  };
  vkGetPhysicalDeviceProperties2(physicalDevice, &physicalDeviceProperties2);
  state->physicalDeviceProperties = physicalDeviceProperties2.properties;

  printf("Getting Physical Device Memory Properties... ");
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &state->physicalDeviceMemoryProperties);

  LOG("%s\n\n", physicalDevice.ResultName());
  return physicalDevice;
}

// LogicalDevice Vk::PhysicalDevice::CreateLogicalDevice(
//     PhysicalDevice               physicalDevice,
//     const char*                  debugName,
//     const VkDeviceCreateInfo*    pCreateInfo,
//     const VkAllocationCallbacks* pAllocator) {
//   auto  handle = Acquire();
//   auto& state = handle.State();
//   state.physicalDevice = physicalDevice;
//   state.pDefaultAllocator = pAllocator;
//   state.result = vkCreateDevice(
//       physicalDevice,
//       pCreateInfo,
//       pAllocator,
//       &handle.Handle());
//   SetDebugInfo(handle, VK_OBJECT_TYPE_DEVICE, (uint64_t)handle.Handle(), debugName);
//   return handle;
// }

const VkAllocationCallbacks*
LogicalDevice::LogicalDeviceAllocator(
    const VkAllocationCallbacks* pAllocator) {
  return pAllocator == nullptr ? state()->pDefaultAllocator : pAllocator;
}

// PipelineLayout2 Vk::LogicalDevice::CreatePipelineLayout(
//     LogicalDevice                   logicalDevice,
//     const char*                     debugName,
//     const PipelineLayoutCreateInfo* pCreateInfo,
//     const VkAllocationCallbacks*    pAllocator) {
//   printf("PipelineLayout::Create %s\n", debugName);
//   auto  handle = Acquire();
//   auto& state = handle.State();
//   state.logicalDevice = logicalDevice;
//   state.pAllocator = logicalDevice.LogicalDeviceAllocator(pAllocator);
//   state.result = vkCreatePipelineLayout(
//       logicalDevice,
//       (VkPipelineLayoutCreateInfo*)pCreateInfo,
//       state.pAllocator,
//       &handle.Handle());
//   SetDebugInfo(logicalDevice, VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)handle.Handle(), debugName);
//   return handle;
// }
void Vk::PipelineLayout2::Destroy() {
  printf("PipelineLayout::Destroy\n");
  HandleBase::Release();
  vkDestroyPipelineLayout(state()->logicalDevice, *this, state()->pAllocator);
}

// ComputePipeline2 Vk::LogicalDevice::CreateComputePipeline(
//     LogicalDevice                    logicalDevice,
//     const char*                      debugName,
//     uint32_t                         createInfoCount,
//     const ComputePipelineCreateInfo* pCreateInfos,
//     const VkAllocationCallbacks*     pAllocator) {
//   printf("ComputePipeline::Create %s\n", debugName);
//   auto  handle = Acquire();
//   auto& state = handle.State();
//   state.logicalDevice = logicalDevice;
//   state.pAllocator = logicalDevice.LogicalDeviceAllocator(pAllocator);
//   state.result = vkCreateComputePipelines(
//       logicalDevice,
//       VK_NULL_HANDLE,
//       createInfoCount,
//       (VkComputePipelineCreateInfo*)pCreateInfos,
//       state.pAllocator,
//       &handle.Handle());
//   SetDebugInfo(logicalDevice, VK_OBJECT_TYPE_PIPELINE, (uint64_t)handle.Handle(), debugName);
//   return handle;
// }
void Vk::ComputePipeline2::Destroy() {
  printf("ComputePipeline2::Destroy\n");
  HandleBase::Release();
  vkDestroyPipeline(state()->logicalDevice, *this, state()->pAllocator);
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