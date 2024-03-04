/***********************************************************************************
 *
 *   Mid-Level Vulkan Implementation
 *
 ************************************************************************************/

#include "mid_vulkan.hpp"

#include <string.h>

#define LOG(...) MVK_LOG(__VA_ARGS__)
#define ASSERT(command) MVK_ASSERT(command)

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

static VkBool32 DebugCallback(
    const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    const VkDebugUtilsMessageTypeFlagsEXT        messageType,
    const VkDebugUtilsMessengerCallbackDataEXT*  pCallbackData,
    void*                                        pUserData) {
  LOG("\n\n%s\n\n", pCallbackData->pMessage);
  return VK_FALSE;
}

Instance Instance::Create(const InstanceDesc&& desc) {
  LOG("# VkInstance... ");

  auto instance = Instance::Acquire();
  auto state = instance.state();

  state->applicationInfo = *desc.createInfo.pApplicationInfo.data;
  state->pInstanceAllocator = desc.pAllocator;

  LOG("Creating... ");
  const VkValidationFeaturesEXT validationFeatures{
      .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
      .pNext = nullptr,
      .enabledValidationFeatureCount = desc.validationFeatures.pEnabledValidationFeatures.count,
      .pEnabledValidationFeatures = desc.validationFeatures.pEnabledValidationFeatures.data,
      .disabledValidationFeatureCount = desc.validationFeatures.pDisabledValidationFeatures.count,
      .pDisabledValidationFeatures = desc.validationFeatures.pDisabledValidationFeatures.data,
  };
  VkInstanceCreateInfo createInfo = desc.createInfo;
  createInfo.pNext = &validationFeatures;
  for (int i = 0; i < desc.createInfo.pEnabledExtensionNames.count; ++i) {
    LOG("Loading %s... ", desc.createInfo.pEnabledExtensionNames.data[i]);
  }
  for (int i = 0; i < desc.createInfo.pEnabledLayerNames.count; ++i) {
    LOG("Loading %s... ", desc.createInfo.pEnabledLayerNames.data[i]);
  }
  state->result = vkCreateInstance(&createInfo, desc.pAllocator, instance.handle());
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

  LOG("Setting up DebugUtilsMessenger... ");
  VkDebugUtilsMessageSeverityFlagsEXT messageSeverity{};
  for (int i = 0; i < desc.debugUtilsMessageSeverityFlags.count; ++i)
    messageSeverity |= desc.debugUtilsMessageSeverityFlags.data[i];

  VkDebugUtilsMessageTypeFlagsEXT messageType{};
  for (int i = 0; i < desc.debugUtilsMessageTypeFlags.count; ++i)
    messageType |= desc.debugUtilsMessageTypeFlags.data[i];

  const VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .flags = 0,
      .messageSeverity = messageSeverity,
      .messageType = messageType,
      .pfnUserCallback = DebugCallback,
      .pUserData = nullptr,
  };
  state->result = PFN.CreateDebugUtilsMessengerEXT(
      instance,
      &debugUtilsMessengerCreateInfo,
      state->pInstanceAllocator,
      &state->debugUtilsMessengerEXT);
  CHECK_RESULT(instance);

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
  int size = (structSize - offsetof(VkBoolFeatureStruct, firstBool)) / sizeof(VkBool32);

  auto requiredBools = &requiredPtr->firstBool;
  auto supportedBools = &supportedPtr->firstBool;

  LOG("Checking %d bools... ", size);
  for (int i = 0; i < size; ++i) {
    if (requiredBools[i] && !supportedBools[i]) {
      LOG("Feature Index %d not supported on physicalDevice! Skipping!... ", i);
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
    printf("Checking %s... ", string_VkStructureType(supported[i]->sType) + strlen("VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_"));
    if (!CheckFeatureBools(required[i], supported[i], structSize[i])) {
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

  // should these be cpp structs!?
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

  state->physicalDeviceMeshShaderProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
  state->physicalDeviceMeshShaderProperties.pNext = nullptr;
  state->physicalDeviceSubgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
  state->physicalDeviceSubgroupProperties.pNext = &state->physicalDeviceMeshShaderProperties;
  state->physicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  state->physicalDeviceProperties.pNext = &state->physicalDeviceSubgroupProperties;

  const auto requiredApiVersion = state->instance.state()->applicationInfo.apiVersion;
  LOG("Required Vulkan API version %d.%d.%d.%d... ",
      VK_API_VERSION_VARIANT(requiredApiVersion),
      VK_API_VERSION_MAJOR(requiredApiVersion),
      VK_API_VERSION_MINOR(requiredApiVersion),
      VK_API_VERSION_PATCH(requiredApiVersion));

  int chosenDeviceIndex = -1;
  for (int deviceIndex = 0; deviceIndex < deviceCount; ++deviceIndex) {
    LOG("Getting Physical Device Index %d Properties... ");
    vkGetPhysicalDeviceProperties2(devices[deviceIndex], &state->physicalDeviceProperties);
    if (state->physicalDeviceProperties.properties.apiVersion < requiredApiVersion) {
      LOG("PhysicalDevice %s didn't support Vulkan API version %d.%d.%d.%d it only supports %d.%d.%d.%d... ",
          state->physicalDeviceProperties.properties.deviceName,
          VK_API_VERSION_VARIANT(requiredApiVersion),
          VK_API_VERSION_MAJOR(requiredApiVersion),
          VK_API_VERSION_MINOR(requiredApiVersion),
          VK_API_VERSION_PATCH(requiredApiVersion),
          VK_API_VERSION_VARIANT(state->physicalDeviceProperties.properties.apiVersion),
          VK_API_VERSION_MAJOR(state->physicalDeviceProperties.properties.apiVersion),
          VK_API_VERSION_MINOR(state->physicalDeviceProperties.properties.apiVersion),
          VK_API_VERSION_PATCH(state->physicalDeviceProperties.properties.apiVersion));
      continue;
    }

    printf("Getting %s Physical Device Features... ", state->physicalDeviceProperties.properties.deviceName);
    vkGetPhysicalDeviceFeatures2(devices[deviceIndex], &state->physicalDeviceFeatures);

    if (!CheckPhysicalDeviceFeatureBools(requiredCount, required, supported, structSize))
      continue;

    chosenDeviceIndex = deviceIndex;
    break;
  }

  if (chosenDeviceIndex == -1) {
    LOG("No suitable PhysicalDevice found!");
    state->result = VK_ERROR_INITIALIZATION_FAILED;
  } else {
    LOG("Picking Device %d %s... ", desc.preferredDeviceIndex, state->physicalDeviceProperties.properties.deviceName);
    *physicalDevice.handle() = devices[desc.preferredDeviceIndex];
    state->result = VK_SUCCESS;
  }
  CHECK_RESULT(physicalDevice);

  LOG("Getting Physical Device Memory Properties... ");
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
void ComputePipeline2::BindPipeline(const VkCommandBuffer commandBuffer) const {
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *this);
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