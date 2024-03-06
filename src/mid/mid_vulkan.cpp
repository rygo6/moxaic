/***********************************************************************************
 *
 *   Mid-Level Vulkan Implementation
 *
 ************************************************************************************/

#include "mid_vulkan.hpp"

#include "moxaic_vulkan.hpp"

#include <string.h>

#define LOG(...) MVK_LOG(__VA_ARGS__)
#define ASSERT(command) MVK_ASSERT(command)

#define CHECK_RESULT(handle)                                        \
  if (handle.pState()->result != VK_SUCCESS) [[unlikely]] {         \
    LOG("Error! %s\n\n", string_VkResult(handle.pState()->result)); \
    return handle;                                                  \
  }

using namespace Mid;
using namespace Mid::Vk;

VkString Vk::string_Support(Support support) {
  switch (support) {
  case Support::Optional:
    return "Optional";
  case Support::Yes:
    return "Yes";
  case Support::No:
    return "No";
  default:
    ASSERT(false);
  }
}

static VkResult SetDebugInfo(
    VkDevice           logicalDevice,
    const VkObjectType objectType,
    const uint64_t     objectHandle,
    const char*        name) {
  VkDebugUtilsObjectNameInfoEXT info{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .objectType = objectType,
      .objectHandle = objectHandle,
      .pObjectName = name,
  };
  return PFN.SetDebugUtilsObjectNameEXT(logicalDevice, &info);
}

static VkBool32 DebugCallback(
    const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    const VkDebugUtilsMessageTypeFlagsEXT        messageType,
    const VkDebugUtilsMessengerCallbackDataEXT*  pCallbackData,
    void*                                        pUserData) {
  LOG("\n\n%s\n\n", pCallbackData->pMessage);
  if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    assert(false && "Validation Error! Exiting!");
  }
  return VK_FALSE;
}

const VkAllocationCallbacks* Instance::DefaultAllocator(const VkAllocationCallbacks* pAllocator) {
  return pAllocator == nullptr ? pState()->pAllocator : pAllocator;
}

Instance Instance::Create(const InstanceDesc&& desc) {
  LOG("# VkInstance... ");

  auto instance = Instance::Acquire();
  auto s = instance.pState();

  s->applicationInfo = *desc.createInfo.pApplicationInfo.data;
  s->pAllocator = desc.pAllocator;

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
  ASSERT(createInfo.pNext == nullptr && "Chaining onto VkInstanceCreateInfo.pNext not supported.");
  createInfo.pNext = &validationFeatures;
  for (int i = 0; i < desc.createInfo.pEnabledExtensionNames.count; ++i) {
    LOG("Loading %s... ", desc.createInfo.pEnabledExtensionNames.data[i]);
  }
  for (int i = 0; i < desc.createInfo.pEnabledLayerNames.count; ++i) {
    LOG("Loading %s... ", desc.createInfo.pEnabledLayerNames.data[i]);
  }
  s->result = vkCreateInstance(&createInfo, desc.pAllocator, instance.pHandle());
  CHECK_RESULT(instance);

#define MVK_PFN_FUNCTION(func)                                                     \
  LOG("Loading PFN_%s... ", "vk" #func);                                           \
  PFN.func = (PFN_vk##func)vkGetInstanceProcAddr(*instance.pHandle(), "vk" #func); \
  if (PFN.SetDebugUtilsObjectNameEXT == nullptr) {                                 \
    LOG("Fail! %s\n\n", #func);                                                    \
    return instance;                                                               \
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
  s->result = PFN.CreateDebugUtilsMessengerEXT(
      instance,
      &debugUtilsMessengerCreateInfo,
      s->pAllocator,
      &s->debugUtilsMessengerEXT);
  CHECK_RESULT(instance);

  LOG("%s\n\n", instance.ResultName());
  return instance;
}
void Instance::Destroy() {
  ASSERT(*pHandle() != VK_NULL_HANDLE && "Trying to destroy null Instance handle!");
  ASSERT(pState()->result != VK_SUCCESS && "Trying to destroy non-succesfully created Instance handle!");
  vkDestroyInstance(*pHandle(), pState()->pAllocator);
  Release();
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
  int  size = (structSize - offsetof(VkBoolFeatureStruct, firstBool)) / sizeof(VkBool32);
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
  auto s = physicalDevice.pState();

  LOG("Getting device count... ");
  uint32_t deviceCount = 0;
  s->result = vkEnumeratePhysicalDevices(*this, &deviceCount, nullptr);
  CHECK_RESULT(physicalDevice);

  LOG("%d devices found... ", deviceCount);
  s->result = deviceCount > 0 ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
  CHECK_RESULT(physicalDevice);

  LOG("Getting devices... ");
  VkPhysicalDevice devices[deviceCount];
  s->result = vkEnumeratePhysicalDevices(*this, &deviceCount, devices);
  CHECK_RESULT(physicalDevice);

  // should these be cpp structs!?
  s->physicalDeviceGlobalPriorityQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GLOBAL_PRIORITY_QUERY_FEATURES_EXT;
  s->physicalDeviceGlobalPriorityQueryFeatures.pNext = nullptr;
  s->physicalDeviceRobustness2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
  s->physicalDeviceRobustness2Features.pNext = &s->physicalDeviceGlobalPriorityQueryFeatures;
  s->physicalDeviceMeshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
  s->physicalDeviceMeshShaderFeatures.pNext = &s->physicalDeviceRobustness2Features;
  s->physicalDeviceFeatures13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  s->physicalDeviceFeatures13.pNext = &s->physicalDeviceMeshShaderFeatures;
  s->physicalDeviceFeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  s->physicalDeviceFeatures12.pNext = &s->physicalDeviceFeatures13;
  s->physicalDeviceFeatures11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
  s->physicalDeviceFeatures11.pNext = &s->physicalDeviceFeatures12;
  s->physicalDeviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  s->physicalDeviceFeatures.pNext = &s->physicalDeviceFeatures11;

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
      (VkBoolFeatureStruct*)&s->physicalDeviceFeatures,
      (VkBoolFeatureStruct*)&s->physicalDeviceFeatures11,
      (VkBoolFeatureStruct*)&s->physicalDeviceFeatures12,
      (VkBoolFeatureStruct*)&s->physicalDeviceFeatures13,
      (VkBoolFeatureStruct*)&s->physicalDeviceMeshShaderFeatures,
      (VkBoolFeatureStruct*)&s->physicalDeviceRobustness2Features,
      (VkBoolFeatureStruct*)&s->physicalDeviceGlobalPriorityQueryFeatures,
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

  s->physicalDeviceMeshShaderProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
  s->physicalDeviceMeshShaderProperties.pNext = nullptr;
  s->physicalDeviceSubgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
  s->physicalDeviceSubgroupProperties.pNext = &s->physicalDeviceMeshShaderProperties;
  s->physicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  s->physicalDeviceProperties.pNext = &s->physicalDeviceSubgroupProperties;

  const auto requiredApiVersion = s->instance.pState()->applicationInfo.apiVersion;
  LOG("Required Vulkan API version %d.%d.%d.%d... ",
      VK_API_VERSION_VARIANT(requiredApiVersion),
      VK_API_VERSION_MAJOR(requiredApiVersion),
      VK_API_VERSION_MINOR(requiredApiVersion),
      VK_API_VERSION_PATCH(requiredApiVersion));

  int chosenDeviceIndex = -1;
  for (int deviceIndex = 0; deviceIndex < deviceCount; ++deviceIndex) {
    LOG("Getting Physical Device Index %d Properties... ");
    vkGetPhysicalDeviceProperties2(devices[deviceIndex], &s->physicalDeviceProperties);
    if (s->physicalDeviceProperties.properties.apiVersion < requiredApiVersion) {
      LOG("PhysicalDevice %s didn't support Vulkan API version %d.%d.%d.%d it only supports %d.%d.%d.%d... ",
          s->physicalDeviceProperties.properties.deviceName,
          VK_API_VERSION_VARIANT(requiredApiVersion),
          VK_API_VERSION_MAJOR(requiredApiVersion),
          VK_API_VERSION_MINOR(requiredApiVersion),
          VK_API_VERSION_PATCH(requiredApiVersion),
          VK_API_VERSION_VARIANT(s->physicalDeviceProperties.properties.apiVersion),
          VK_API_VERSION_MAJOR(s->physicalDeviceProperties.properties.apiVersion),
          VK_API_VERSION_MINOR(s->physicalDeviceProperties.properties.apiVersion),
          VK_API_VERSION_PATCH(s->physicalDeviceProperties.properties.apiVersion));
      continue;
    }

    printf("Getting %s Physical Device Features... ", s->physicalDeviceProperties.properties.deviceName);
    vkGetPhysicalDeviceFeatures2(devices[deviceIndex], &s->physicalDeviceFeatures);

    if (!CheckPhysicalDeviceFeatureBools(requiredCount, required, supported, structSize))
      continue;

    chosenDeviceIndex = deviceIndex;
    break;
  }

  if (chosenDeviceIndex == -1) {
    LOG("No suitable PhysicalDevice found!");
    s->result = VK_ERROR_INITIALIZATION_FAILED;
  } else {
    LOG("Picking Device %d %s... ", desc.preferredDeviceIndex, s->physicalDeviceProperties.properties.deviceName);
    *physicalDevice.pHandle() = devices[desc.preferredDeviceIndex];
    s->result = VK_SUCCESS;
  }
  CHECK_RESULT(physicalDevice);

  // Once device is chosen the properties in state should represent what is actually enabled on physicalDevice so copy them over
  for (int i = 0; i < requiredCount; ++i) {
    supported[i] = required[i];
  }

  LOG("Getting Physical Device Memory Properties... ");
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &s->physicalDeviceMemoryProperties);

  LOG("Getting queue family count... ");
  vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &s->queueFamilyCount, nullptr);

  LOG("%d queue families found... ", s->queueFamilyCount);
  s->result = s->queueFamilyCount > 0 ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
  CHECK_RESULT(physicalDevice);

  LOG("Getting queue families... ");
  for (uint32_t i = 0; i < s->queueFamilyCount; ++i) {
    s->queueFamilyGlobalPriorityProperties[i] = {
        .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES_EXT};
    s->queueFamilyProperties[i] = {
        .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2,
        .pNext = &s->queueFamilyGlobalPriorityProperties[i],
    };
  }
  vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &s->queueFamilyCount, s->queueFamilyProperties);

  LOG("%s\n\n", physicalDevice.ResultName());
  return physicalDevice;
}

void PhysicalDevice::Destroy() {
  ASSERT(*pHandle() != VK_NULL_HANDLE && "Trying to destroy null PhysicalDevice handle!");
  ASSERT(pState()->result != VK_SUCCESS && "Trying to destroy non-succesfully created PhysicalDevice handle!");
  // you don't destroy physical device handles, so we just release...
  Release();
}

LogicalDevice PhysicalDevice::CreateLogicalDevice(const LogicalDeviceDesc&& desc) {
  LOG("# CreateLogicalDevice... ");

  auto logicalDevice = LogicalDevice::Acquire();
  auto s = logicalDevice.pState();

  s->pAllocator = pState()->instance.DefaultAllocator(desc.pAllocator);
  s->physicalDevice = *this;

  LOG("Creating... ");
  VkDeviceCreateInfo createInfo = desc.createInfo;
  ASSERT(createInfo.pNext == nullptr &&
         "Chaining onto VkDeviceCreateInfo.pNext not supported.");
  ASSERT(createInfo.ppEnabledLayerNames == nullptr &&
         "VkDeviceCreateInfo.ppEnabledLayerNames obsolete.");
  ASSERT(createInfo.pEnabledFeatures == nullptr &&
         "LogicalDeviceDesc.createInfo.pEnabledFeatures should be nullptr. "
         "Internally VkPhysicalDeviceFeatures2 is used from PhysicalDevice.");
  createInfo.pNext = &pState()->physicalDeviceFeatures;
  s->result = vkCreateDevice(
      *this,
      &createInfo,
      s->pAllocator,
      logicalDevice.pHandle());
  CHECK_RESULT(logicalDevice);

  LOG("Setting LogicalDevice DebugName... ");
  s->result = SetDebugInfo(
      logicalDevice,
      VK_OBJECT_TYPE_DEVICE,
      (uint64_t)(VkDevice)logicalDevice,
      desc.debugName);
  CHECK_RESULT(logicalDevice);

  // Set debug on physical device and instance now that we can...
  LOG("Setting PhysicalDevice DebugName... ");
  s->result = SetDebugInfo(
      logicalDevice,
      VK_OBJECT_TYPE_PHYSICAL_DEVICE,
      (uint64_t)(VkPhysicalDevice) * this,
      s->physicalDevice.pState()->physicalDeviceProperties.properties.deviceName);
  CHECK_RESULT(logicalDevice);

  LOG("Setting Instance DebugName... ");
  auto instance = s->physicalDevice.pState()->instance;
  s->result = SetDebugInfo(
      logicalDevice,
      VK_OBJECT_TYPE_INSTANCE,
      (uint64_t)(VkInstance)instance,
      instance.pState()->applicationInfo.pApplicationName);
  CHECK_RESULT(logicalDevice);

  LOG("%s\n\n", logicalDevice.ResultName());
  return logicalDevice;
}
uint32_t PhysicalDevice::FindQueueIndex(
    const Support      graphics,
    const Support      compute,
    const Support      transfer,
    const Support      globalPriority,
    const VkSurfaceKHR present) {
  LOG("Finding queue family: graphics=%s compute=%s transfer=%s globalPriority=%s present=%s... ",
      string_Support(graphics),
      string_Support(compute),
      string_Support(transfer),
      string_Support(globalPriority),
      present == nullptr ? "No" : "Yes");

  const auto s = pState();
  for (uint32_t i = 0; i < s->queueFamilyCount; ++i) {
    const bool graphicsSupport = s->queueFamilyProperties[i].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
    const bool computeSupport = s->queueFamilyProperties[i].queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT;
    const bool transferSupport = s->queueFamilyProperties[i].queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT;
    const bool globalPrioritySupport = s->queueFamilyGlobalPriorityProperties[i].priorityCount > 0;

    if (graphics == Support::Yes && !graphicsSupport)
      continue;
    if (graphics == Support::No && graphicsSupport)
      continue;

    if (compute == Support::Yes && !computeSupport)
      continue;
    if (compute == Support::No && computeSupport)
      continue;

    if (transfer == Support::Yes && !transferSupport)
      continue;
    if (transfer == Support::No && transferSupport)
      continue;

    if (globalPriority == Support::Yes && !globalPrioritySupport)
      continue;
    if (globalPriority == Support::No && globalPrioritySupport)
      continue;

    if (present != nullptr) {
      VkBool32 presentSupport = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(*this, i, present, &presentSupport);
      if (!presentSupport)
        continue;
    }

    LOG("Found queue family index %d\n\n", i);
    return i;
  }

  LOG("Failed to find Queue Family!\n\n");
  return -1;
}

void LogicalDevice::Destroy() {
  ASSERT(*pHandle() != VK_NULL_HANDLE && "Trying to destroy null Instance handle!");
  ASSERT(pState()->result != VK_SUCCESS && "Trying to destroy non-succesfully created Instance handle!");
  vkDestroyDevice(*pHandle(), pState()->pAllocator);
  Release();
}

template<typename T>
static T CreateGeneric(LogicalDevice device, const auto&& desc) {
  LOG("# CreateCommandPool... ");

  auto handle = T::Acquire();
  auto s = handle.pState();

  s->pAllocator = DefaultAllocator(desc.pAllocator);
  s->logicalDevice = device;

  LOG("Creating... ");
  s->result = vkCreateCommandPool(device,
                                 desc.createInfo.p(),
                                 s->pAllocator,
                                 &handle.pHandle());
  CHECK_RESULT(handle);

  LOG("Setting DebugName... ");
  s->result = SetDebugInfo(
      device,
      VK_OBJECT_TYPE_RENDER_PASS,
      (uint64_t)*handle.pHandle(),
      desc.debugName);
  CHECK_RESULT(handle);

  LOG("%s\n\n", handle.ResultName());
  return handle;
}

RenderPass LogicalDevice::CreateRenderPass(const RenderPassDesc&& desc) {
  LOG("# CreateRenderPass... ");

  auto renderPass = RenderPass::Acquire();
  auto s = renderPass.pState();

  s->pAllocator = DefaultAllocator(desc.pAllocator);
  s->logicalDevice = *this;

  LOG("Creating... ");
  s->result = vkCreateRenderPass(*pHandle(),
                                 desc.createInfo.p(),
                                 s->pAllocator,
                                 renderPass.pHandle());
  CHECK_RESULT(renderPass);

  LOG("Setting DebugName... ");
  s->result = SetDebugInfo(
      *pHandle(),
      VK_OBJECT_TYPE_RENDER_PASS,
      (uint64_t)*renderPass.pHandle(),
      desc.debugName);
  CHECK_RESULT(renderPass);

  LOG("%s\n\n", renderPass.ResultName());
  return renderPass;
}

CommandPool LogicalDevice::CreateCommandPool(const CommandPoolDesc&& desc) {
  LOG("# CreateCommandPool... ");

  auto commandPool = CommandPool::Acquire();
  auto s = commandPool.pState();

  s->pAllocator = DefaultAllocator(desc.pAllocator);
  s->logicalDevice = *this;

  LOG("Creating... ");
  s->result = vkCreateCommandPool(*pHandle(),
                                 desc.createInfo.p(),
                                 s->pAllocator,
                                 commandPool.pHandle());
  CHECK_RESULT(commandPool);

  LOG("Setting DebugName... ");
  s->result = SetDebugInfo(
      *pHandle(),
      VK_OBJECT_TYPE_RENDER_PASS,
      (uint64_t)*commandPool.pHandle(),
      desc.debugName);
  CHECK_RESULT(commandPool);

  LOG("%s\n\n", commandPool.ResultName());
  return commandPool;
}

const VkAllocationCallbacks* LogicalDevice::DefaultAllocator(
    const VkAllocationCallbacks* pAllocator) {
  return pAllocator == nullptr ? pState()->pAllocator : pAllocator;
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
  Release();
  vkDestroyPipelineLayout(pState()->logicalDevice, *this, pState()->pAllocator);
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
  vkDestroyPipeline(pState()->logicalDevice, *this, pState()->pAllocator);
}
void ComputePipeline2::BindPipeline(const VkCommandBuffer commandBuffer) {
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, *pHandle());
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