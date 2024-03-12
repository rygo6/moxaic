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

/* FreeStack */
template <typename TIndex, size_t Capacity>
struct FreeIndexStack {
  constexpr static TIndex LastIndex = (1 << 8 * sizeof(TIndex)) - 1;
  static_assert(Capacity <= LastIndex + 1);

  TIndex current{Capacity - 1};
  TIndex data[Capacity]{};

  FreeIndexStack() {
    ASSERT(current <= LastIndex && "Trying to create handle stack with capacity greater than type supports.");
    for (size_t i = 0; i < Capacity; ++i) {
      data[i] = (TIndex)i;
    }
  }

  TIndex Pop() {
    ASSERT(current >= 0 && "Trying to pop handle stack below 0.");
    return data[current--];
  }

  void Push(TIndex value) {
    ASSERT(current != LastIndex && "Tring to push handle stack above capacity.");
    data[++current] = value;
  }
};

/* ArenaPool */
template <typename TBufferIndex, size_t SlotMaxSize, size_t BufferCapacity>
struct ArenaPool {
  constexpr static size_t LastBufferIndex = (1ULL << 8 * sizeof(TBufferIndex)) - 1;
  static_assert(BufferCapacity <= LastBufferIndex + 1);

  size_t       slotCount{};
  uint8_t      buffer[BufferCapacity]{};
  TBufferIndex bufferIndex{};
  TBufferIndex freeSlotIndices[SlotMaxSize]{};

  ArenaPool() {
    for (size_t i = 0; i < SlotMaxSize; ++i)
      freeSlotIndices[i] = LastBufferIndex;
  }

  TBufferIndex Pop(size_t size) {
    ASSERT(size < SlotMaxSize && "Trying to pop state size larger than MVK_STATE_MAX_SIZE.");

    slotCount++;
    LOG("Popping slot %llu buffer %d... ", slotCount, bufferIndex);

    if (freeSlotIndices[size] == LastBufferIndex) {
      ASSERT(bufferIndex + size < BufferCapacity && "Trying to pop beyond MVK_STATE_CAPACITY.");
      LOG("New slot at %d... ", bufferIndex);
      TBufferIndex poppedIndex = bufferIndex;
      bufferIndex += size;
      return poppedIndex;
    }

    TBufferIndex* pFreeSlotIndex = &freeSlotIndices[size];
    TBufferIndex* pPriorFreeSlotIndex = pFreeSlotIndex;

    while (*pFreeSlotIndex != LastBufferIndex) {
      LOG("Checking slot at %d... ", *pFreeSlotIndex);
      pPriorFreeSlotIndex = pFreeSlotIndex;
      pFreeSlotIndex = (TBufferIndex*)(buffer + *pFreeSlotIndex);
    }
    *pPriorFreeSlotIndex = LastBufferIndex;
    LOG("Popped slot index %d... ", *pFreeSlotIndex);
    return *pFreeSlotIndex;
  }

  void Push(TBufferIndex index, size_t size) {
    ASSERT(size < SlotMaxSize && "Trying to push state size larger than MVK_STATE_MAX_SIZE.");
    ASSERT(index < BufferCapacity && "Trying to push state after buffer range.");

    slotCount--;
    LOG("Pushing slot %llu buffer %d... ", slotCount, index);

    void* bufferPtr = buffer + index;
    *(TBufferIndex*)bufferPtr = LastBufferIndex;

    if (freeSlotIndices[size] == LastBufferIndex) {
      LOG("Pushed first free slot index... ");
      freeSlotIndices[size] = index;
      return;
    }

    TBufferIndex* pFreeSlotIndex = &freeSlotIndices[size];
    while (*pFreeSlotIndex != LastBufferIndex) {
      LOG("Checking slot at %d... ", *pFreeSlotIndex);
      pFreeSlotIndex = (TBufferIndex*)(buffer + *pFreeSlotIndex);
    }
    LOG("Pushing to index %d... ", index);
    *pFreeSlotIndex = index;
  }
};

/* HandleBase */
constexpr static size_t           MaxHandleGenerations = (1 << 8 * sizeof(HandleGeneration));
constexpr static HandleGeneration HandleGenerationLastIndex = MaxHandleGenerations - 1;
constexpr static size_t           MaxHandles = (1 << 8 * sizeof(HandleIndex));
constexpr static HandleIndex      HandleLastIndex = MaxHandles - 1;

typedef uint32_t        PoolIndex;
constexpr static size_t StatePoolCapacity = 1024 * 512; // 512kb
constexpr static size_t StateMaxSize = 1024 * 4;          // 4kb

static size_t                                                 handleCount{};
static Handle                                                 handles[MaxHandles]{};
static HandleGeneration                                       generations[MaxHandles]{};
static FreeIndexStack<HandleIndex, MaxHandles>                freeHandleIndexStack{};
static PoolIndex                                              statePoolIndices[MaxHandles]{};
static ArenaPool<PoolIndex, StateMaxSize, StatePoolCapacity> statePool{};

#define HANDLE_TEMPLATE template <typename Derived, typename THandle, typename TState>

HANDLE_TEMPLATE
THandle* HandleBase<Derived, THandle, TState>::pHandle() {
  ASSERT(IsValid() && "Trying to get handle with wrong generation!");
  return (THandle*)&handles[handleIndex];
}

HANDLE_TEMPLATE
TState* HandleBase<Derived, THandle, TState>::pState() {
  ASSERT(IsValid() && "Trying to get state with wrong generation!");
  PoolIndex poolIndex = statePoolIndices[handleIndex];
  return (TState*)(statePool.buffer + poolIndex);
}

HANDLE_TEMPLATE
const THandle& HandleBase<Derived, THandle, TState>::handle() const {
  ASSERT(IsValid() && "Trying to get handle with wrong generation!");
  return *(THandle*)&handles[handleIndex];
}

HANDLE_TEMPLATE
const TState& HandleBase<Derived, THandle, TState>::state() const {
  ASSERT(IsValid() && "Trying to get state with wrong generation!");
  PoolIndex poolIndex = statePoolIndices[handleIndex];
  return *(TState*)(statePool.buffer + poolIndex);
}

HANDLE_TEMPLATE
Derived HandleBase<Derived, THandle, TState>::Acquire() {
  const auto handleIndex = freeHandleIndexStack.Pop();
  const auto generation = generations[handleIndex];
  const auto poolIndex = statePool.Pop(sizeof(TState));

  statePoolIndices[handleIndex] = poolIndex;
  new (statePool.buffer + poolIndex) TState;

  handleCount++;
  MVK_LOG("Acquiring handle index %d generation %d total %llu... ", handleIndex, generation, handleCount);

  return Derived{handleIndex, generation};
}

HANDLE_TEMPLATE
void HandleBase<Derived, THandle, TState>::Release() const {
  const auto generation = generations[handleIndex];
  ASSERT(generation != HandleGenerationLastIndex && "Max handle generations reached.");
  ++generations[handleIndex];

  handleCount--;
  MVK_LOG("Releasing handle index %d generation %d total %llu... ", handleIndex, generation, handleCount);

  freeHandleIndexStack.Push(handleIndex);

  PoolIndex poolIndex = statePoolIndices[handleIndex];
  statePool.Push(poolIndex, sizeof(TState));
}

HANDLE_TEMPLATE
VkResult HandleBase<Derived, THandle, TState>::Result() const {
  return state().result;
}

HANDLE_TEMPLATE
bool HandleBase<Derived, THandle, TState>::IsValid() const {
  return generations[handleIndex] == handleGeneration;
}

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

Instance Instance::Create(InstanceDesc&& desc) {
  LOG("# %s... ", desc.debugName);

  auto instance = Instance::Acquire();
  auto state = instance.pState();

  state->applicationInfo = *desc.createInfo.pApplicationInfo.p;
  state->pAllocator = desc.pAllocator;

  LOG("Creating... ");
  ASSERT(desc.createInfo.pNext.p == nullptr && "Chaining onto VkInstanceCreateInfo.pNext not supported.");
  desc.createInfo.pNext = desc.validationFeatures;
  for (int i = 0; i < desc.createInfo.pEnabledExtensionNames.count; ++i) {
    LOG("Loading %s... ", desc.createInfo.pEnabledExtensionNames.p[i]);
  }
  for (int i = 0; i < desc.createInfo.pEnabledLayerNames.count; ++i) {
    LOG("Loading %s... ", desc.createInfo.pEnabledLayerNames.p[i]);
  }
  state->result = vkCreateInstance(&desc.createInfo.vk(), desc.pAllocator, instance.pHandle());
  CHECK_RESULT(instance);

#define MVK_PFN_FUNCTION(func)                                                       \
  LOG("Loading PFN_%s... ", "vk" #func);                                             \
  PFN.func = (PFN_vk##func)vkGetInstanceProcAddr(*instance.pHandle(), "vk" #func);   \
  state->result = PFN.func == nullptr ? VK_ERROR_INITIALIZATION_FAILED : VK_SUCCESS; \
  CHECK_RESULT(instance);

  MVK_PFN_FUNCTIONS
#undef MVK_PFN_FUNCTION

  LOG("Setting up DebugUtilsMessenger... ");
  if (desc.debugUtilsMessengerCreateInfo.pfnUserCallback == nullptr)
    desc.debugUtilsMessengerCreateInfo.pfnUserCallback = DebugCallback;

  auto t = instance.handle();

  state->result = PFN.CreateDebugUtilsMessengerEXT(
      instance.handle(),
      &desc.debugUtilsMessengerCreateInfo.vk(),
      state->pAllocator,
      &state->debugUtilsMessengerEXT);
  CHECK_RESULT(instance);

  LOG("%s\n\n", string_VkResult(instance.Result()));
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
  LOG("# %s... ", desc.debugName);

  auto physicalDevice = PhysicalDevice::Acquire();
  auto state = physicalDevice.pState();

  state->instance = *this;

  LOG("Getting device count... ");
  uint32_t deviceCount = 0;
  state->result = vkEnumeratePhysicalDevices(handle(), &deviceCount, nullptr);
  CHECK_RESULT(physicalDevice);

  LOG("%d devices found... ", deviceCount);
  state->result = deviceCount > 0 ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
  CHECK_RESULT(physicalDevice);

  LOG("Getting devices... ");
  VkPhysicalDevice devices[deviceCount];
  state->result = vkEnumeratePhysicalDevices(handle(), &deviceCount, devices);
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

  const auto requiredApiVersion = state->instance.pState()->applicationInfo.apiVersion;
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
    LOG("Picking Device %d %s... ", chosenDeviceIndex, state->physicalDeviceProperties.properties.deviceName);
    *physicalDevice.pHandle() = devices[chosenDeviceIndex];
    state->result = VK_SUCCESS;
  }
  CHECK_RESULT(physicalDevice);

  // Once device is chosen the properties in state should represent what is actually enabled on physicalDevice so copy them over
  for (int i = 0; i < requiredCount; ++i) {
    supported[i] = required[i];
  }

  LOG("Getting Physical Device Memory Properties... ");
  vkGetPhysicalDeviceMemoryProperties(physicalDevice.handle(), &state->physicalDeviceMemoryProperties);

  LOG("Getting queue family count... ");
  vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice.handle(), &state->queueFamilyCount, nullptr);

  LOG("%d queue families found... ", state->queueFamilyCount);
  state->result = state->queueFamilyCount > 0 ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
  CHECK_RESULT(physicalDevice);

  LOG("Getting queue families... ");
  for (uint32_t i = 0; i < state->queueFamilyCount; ++i) {
    state->queueFamilyGlobalPriorityProperties[i] = {
        .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_GLOBAL_PRIORITY_PROPERTIES_EXT};
    state->queueFamilyProperties[i] = {
        .sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2,
        .pNext = &state->queueFamilyGlobalPriorityProperties[i],
    };
  }
  vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice.handle(), &state->queueFamilyCount, state->queueFamilyProperties);

  LOG("%s\n\n", string_VkResult(physicalDevice.Result()));
  return physicalDevice;
}

void PhysicalDevice::Destroy() {
  ASSERT(*pHandle() != VK_NULL_HANDLE && "Trying to destroy null PhysicalDevice handle!");
  ASSERT(pState()->result != VK_SUCCESS && "Trying to destroy non-succesfully created PhysicalDevice handle!");
  // you don't destroy physical device handles, so we just release...
  Release();
}

LogicalDevice PhysicalDevice::CreateLogicalDevice(LogicalDeviceDesc&& desc) {
  LOG("# %s... ", desc.debugName);

  auto logicalDevice = LogicalDevice::Acquire();
  auto state = logicalDevice.pState();

  state->pAllocator = pState()->instance.DefaultAllocator(desc.pAllocator);
  state->physicalDevice = *this;

  LOG("Creating... ");
  ASSERT(desc.createInfo.pNext.p == nullptr &&
         "Chaining onto VkDeviceCreateInfo.pNext not supported.");
  ASSERT(desc.createInfo.pEnabledLayerNames.p == nullptr &&
         "VkDeviceCreateInfo.ppEnabledLayerNames obsolete.");
  ASSERT(desc.createInfo.pEnabledFeatures.p == nullptr &&
         "LogicalDeviceDesc.createInfo.pEnabledFeatures should be nullptr. "
         "Internally VkPhysicalDeviceFeatures2 is used from PhysicalDevice.");
  desc.createInfo.pNext.p = &pState()->physicalDeviceFeatures;
  state->result = vkCreateDevice(
      handle(),
      &desc.createInfo.vk(),
      state->pAllocator,
      logicalDevice.pHandle());
  CHECK_RESULT(logicalDevice);

  LOG("Setting LogicalDevice DebugName... ");
  state->result = SetDebugInfo(
      logicalDevice.handle(),
      VK_OBJECT_TYPE_DEVICE,
      (uint64_t)logicalDevice.handle(),
      desc.debugName);
  CHECK_RESULT(logicalDevice);

  // Set debug on physical device and instance now that we can...
  LOG("Setting PhysicalDevice DebugName... ");
  state->result = SetDebugInfo(
      logicalDevice.handle(),
      VK_OBJECT_TYPE_PHYSICAL_DEVICE,
      (uint64_t)handle(),
      state->physicalDevice.pState()->physicalDeviceProperties.properties.deviceName);
  CHECK_RESULT(logicalDevice);

  LOG("Setting Instance DebugName... ");
  auto instance = state->physicalDevice.pState()->instance;
  state->result = SetDebugInfo(
      logicalDevice.handle(),
      VK_OBJECT_TYPE_INSTANCE,
      (uint64_t)instance.handle(),
      instance.pState()->applicationInfo.pApplicationName);
  CHECK_RESULT(logicalDevice);

  LOG("%s\n\n", string_VkResult(logicalDevice.Result()));
  return logicalDevice;
}

uint32_t PhysicalDevice::FindQueueIndex(QueueIndexDesc&& desc) {
  LOG("Finding queue family: graphics=%s compute=%s transfer=%s globalPriority=%s present=%s... ",
      string_Support(desc.graphics),
      string_Support(desc.compute),
      string_Support(desc.transfer),
      string_Support(desc.globalPriority),
      desc.present == nullptr ? "No" : "Yes");

  const auto state = pState();
  for (uint32_t i = 0; i < state->queueFamilyCount; ++i) {
    const bool graphicsSupport = state->queueFamilyProperties[i].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
    const bool computeSupport = state->queueFamilyProperties[i].queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT;
    const bool transferSupport = state->queueFamilyProperties[i].queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT;
    const bool globalPrioritySupport = state->queueFamilyGlobalPriorityProperties[i].priorityCount > 0;

    if (desc.graphics == Support::Yes && !graphicsSupport)
      continue;
    if (desc.graphics == Support::No && graphicsSupport)
      continue;

    if (desc.compute == Support::Yes && !computeSupport)
      continue;
    if (desc.compute == Support::No && computeSupport)
      continue;

    if (desc.transfer == Support::Yes && !transferSupport)
      continue;
    if (desc.transfer == Support::No && transferSupport)
      continue;

    if (desc.globalPriority == Support::Yes && !globalPrioritySupport)
      continue;
    if (desc.globalPriority == Support::No && globalPrioritySupport)
      continue;

    if (desc.present != nullptr) {
      VkBool32 presentSupport = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(handle(), i, desc.present, &presentSupport);
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
  vkDestroyDevice(handle(), pState()->pAllocator);
  Release();
}

template <typename T, typename THandle, typename TInfo>
static T CreateGeneric(
    LogicalDevice device,
    VkObjectType  objectType,
    VkResult      (*vkCreateFunc)(VkDevice, const TInfo*, const VkAllocationCallbacks*, THandle*),
    auto&&        desc) {
  LOG("# %s... ", desc.debugName);

  auto handle = T::Acquire();
  auto state = handle.pState();

  state->pAllocator = device.DefaultAllocator(desc.pAllocator);
  state->logicalDevice = device;

  LOG("Creating... ");
  state->result = vkCreateFunc(
      device.handle(),
      &desc.createInfo.vk(),
      state->pAllocator,
      handle.pHandle());
  CHECK_RESULT(handle);

  LOG("Setting DebugName... ");
  state->result = SetDebugInfo(
      device.handle(),
      objectType,
      (uint64_t)*handle.pHandle(),
      desc.debugName);
  CHECK_RESULT(handle);

  LOG("%s\n\n", string_VkResult(handle.Result()));
  return handle;
}

RenderPass LogicalDevice::CreateRenderPass(RenderPassDesc&& desc) const {
  return CreateGeneric<RenderPass, VkRenderPass, VkRenderPassCreateInfo>(
      *this,
      VK_OBJECT_TYPE_RENDER_PASS,
      vkCreateRenderPass,
      desc);
}

Queue LogicalDevice::GetQueue(QueueDesc&& desc) const {
  LOG("# %s... ", desc.debugName);

  auto queue = Queue::Acquire();
  auto state = queue.pState();

  state->logicalDevice = *this;

  LOG("Getting... ");
  vkGetDeviceQueue(
      handle(),
      desc.queueIndex,
      0,
      queue.pHandle());

  LOG("Setting DebugName... ");
  SetDebugInfo(
      handle(),
      VK_OBJECT_TYPE_QUEUE,
      (uint64_t)queue.handle(),
      desc.debugName);

  LOG("DONE\n\n");
  return queue;
}

CommandPool LogicalDevice::CreateCommandPool(CommandPoolDesc&& desc) const {
  return CreateGeneric<CommandPool, VkCommandPool, VkCommandPoolCreateInfo>(
      *this,
      VK_OBJECT_TYPE_COMMAND_POOL,
      vkCreateCommandPool,
      desc);
}

QueryPool LogicalDevice::CreateQueryPool(QueryPoolDesc&& desc) const {
  return CreateGeneric<QueryPool, VkQueryPool, VkQueryPoolCreateInfo>(
      *this,
      VK_OBJECT_TYPE_QUERY_POOL,
      vkCreateQueryPool,
      desc);
}

DescriptorPool LogicalDevice::CreateDescriptorPool(DescriptorPoolDesc&& desc) const {
  return CreateGeneric<DescriptorPool, VkDescriptorPool, VkDescriptorPoolCreateInfo>(
      *this,
      VK_OBJECT_TYPE_DESCRIPTOR_POOL,
      vkCreateDescriptorPool,
      desc);
}

Sampler LogicalDevice::CreateSampler(SamplerDesc&& desc) const {
  // todo step through pnext and apply to end
  desc.createInfo.pNext.p = desc.pReductionModeCreateInfo.p;
  return CreateGeneric<Sampler, VkSampler, VkSamplerCreateInfo>(
      *this,
      VK_OBJECT_TYPE_SAMPLER,
      vkCreateSampler,
      desc);
}

const VkAllocationCallbacks* LogicalDevice::DefaultAllocator(
    const VkAllocationCallbacks* pAllocator) const {
  return pAllocator == nullptr ? state().pAllocator : pAllocator;
}

CommandBuffer CommandPool::AllocateCommandBuffer(CommandBufferDesc&& desc) {
  LOG("# AllocateCommandBuffer... ");

  auto commandBuffer = CommandBuffer::Acquire();
  auto state = commandBuffer.pState();

  state->commandPool = *this;

  LOG("Allocating... ");
  ASSERT(desc.allocateInfo.commandBufferCount == 1 &&
         "Can only allocate one command buffer with CommandPool::AllocateCommandBuffer!");
  ASSERT(desc.allocateInfo.commandPool == VK_NULL_HANDLE &&
         "CommandPool::AllocateCommandBuffer overrides allocateInfo.commandPool!");
  desc.allocateInfo.commandPool = handle();
  auto logicalDevice = pState()->logicalDevice;
  state->result = vkAllocateCommandBuffers(
      logicalDevice.handle(),
      &desc.allocateInfo.vk(),
      commandBuffer.pHandle());
  CHECK_RESULT(commandBuffer);

  LOG("Setting DebugName... ");
  state->result = SetDebugInfo(
      logicalDevice.handle(),
      VK_OBJECT_TYPE_COMMAND_BUFFER,
      (uint64_t)commandBuffer.handle(),
      desc.debugName);
  CHECK_RESULT(commandBuffer);

  LOG("%s\n\n", string_VkResult(commandBuffer.Result()));
  return commandBuffer;
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
  vkDestroyPipelineLayout(state().logicalDevice.handle(), handle(), pState()->pAllocator);
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
  vkDestroyPipeline(state().logicalDevice.handle(), handle(), pState()->pAllocator);
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