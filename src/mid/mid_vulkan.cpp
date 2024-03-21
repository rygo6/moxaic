/***********************************************************************************
 *
 *   Mid-Level Vulkan Implementation
 *
 ************************************************************************************/

#include "mid_vulkan.hpp"

#include "moxaic_vulkan.hpp"

#define LOG(...) MVK_LOG(__VA_ARGS__)
#define ASSERT(command) MVK_ASSERT(command)

#define CHECK_RESULT(handle)                                        \
  if (handle.pState()->result != VK_SUCCESS) [[unlikely]] {         \
    LOG("Error! %s\n\n", string_VkResult(handle.pState()->result)); \
    return handle;                                                  \
  }

#define CHECK_RESULT_VOID(handle)                                   \
  if (handle.pState()->result != VK_SUCCESS) [[unlikely]] {         \
    LOG("Error! %s\n\n", string_VkResult(handle.pState()->result)); \
    return;                                                         \
  }

using namespace Mid;
using namespace Mid::Vk;

static struct {
#define MVK_PFN_FUNCTION(func) PFN_vk##func func;
  MVK_PFN_FUNCTIONS
#undef MVK_PFN_FUNCTION
} PFN;

/* FreeStack */
template <typename TIndex, size_t Capacity>
struct FreeIndexStack {
  constexpr static TIndex LastIndex = (1ULL << 8 * sizeof(TIndex)) - 1;
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
constexpr static size_t           MaxHandleGenerations = (1ULL << 8 * sizeof(HandleGeneration));
constexpr static HandleGeneration HandleGenerationLastIndex = MaxHandleGenerations - 1;
constexpr static size_t           MaxHandles = (1ULL << 8 * sizeof(HandleIndex));
constexpr static HandleIndex      HandleLastIndex = MaxHandles - 1;

typedef uint32_t        PoolIndex;
constexpr static size_t StatePoolCapacity = 1024 * 512; // 512kb
constexpr static size_t StateMaxSize = 1024 * 4;        // 4kb

static FreeIndexStack<HandleIndex, MaxHandles> freeHandleIndexStack{};

// Hot Data. Stored in a linear pool.
static Handle           handles[MaxHandles]{};
static HandleGeneration generations[MaxHandles]{};

// Cold Data. Stored in a packaed ArenaPool. Types of different sizes share the same pool, claiming their slot on a first come first serve basis.
static PoolIndex                                             statePoolIndices[MaxHandles]{};
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

  MVK_LOG("Acquiring handle index %d generation %d total %llu... ", handleIndex, generation);

  return Derived{handleIndex, generation};
}

HANDLE_TEMPLATE
void HandleBase<Derived, THandle, TState>::Release() const {
  const auto generation = generations[handleIndex];
  ASSERT(generation != HandleGenerationLastIndex && "Max handle generations reached.");
  ++generations[handleIndex];

  MVK_LOG("Releasing handle index %d generation %d total %llu... ", handleIndex, generation);

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

VkString Vk::string_Support(const Support support) {
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
VkString Vk::string_Locality(const Locality locality) {
  switch (locality) {
  case Locality::Local:
    return "Local";
  case Locality::External:
    return "External";
  case Locality::Imported:
    return "Imported";
  default:
    ASSERT(false);
  }
}

#define DESTROY_ASSERT                                                             \
  ASSERT(handle() != VK_NULL_HANDLE && "Trying to destroy null Instance handle!"); \
  ASSERT(Result() != VK_SUCCESS && "Trying to destroy non-succesfully created Instance handle!");

constexpr bool checkStringLiteral(VkString str) {
  return std::is_constant_evaluated();
}

static VkResult SetDebugInfo(
    const VkDevice     logicalDevice,
    const VkObjectType objectType,
    const uint64_t     objectHandle,
    const VkString     name) {
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

struct VkNextStruct {
  VkStructureType sType;
  void*           pNext;
};
void SetNextChainEnd(VkNextStruct* pNextStruct, VkNextStruct* pEndChainStruct) {
  while (pNextStruct->pNext != nullptr) {
    pNextStruct = (VkNextStruct*)pNextStruct->pNext;
  }
  pNextStruct->pNext = pEndChainStruct;
}

/* Instance */
template struct HandleBase<Instance, VkInstance, InstanceState>;

Instance Instance::Create(InstanceDesc&& desc) {
  LOG("# %s... ", desc.debugName);

  auto instance = Instance::Acquire();
  auto s = instance.pState();

  s->applicationInfo = *desc.createInfo.pApplicationInfo.p;
  s->pAllocator = desc.pAllocator;

  LOG("Creating... ");

  SetNextChainEnd((VkNextStruct*)&desc.createInfo, (VkNextStruct*)&desc.validationFeatures);

  for (int i = 0; i < desc.createInfo.pEnabledExtensionNames.count; ++i) {
    LOG("Loading %s... ", desc.createInfo.pEnabledExtensionNames.p[i]);
  }
  for (int i = 0; i < desc.createInfo.pEnabledLayerNames.count; ++i) {
    LOG("Loading %s... ", desc.createInfo.pEnabledLayerNames.p[i]);
  }
  s->result = vkCreateInstance(&desc.createInfo.vk(), desc.pAllocator, instance.pHandle());
  CHECK_RESULT(instance);

#define MVK_PFN_FUNCTION(func)                                                     \
  LOG("Loading PFN_%s... ", "vk" #func);                                           \
  PFN.func = (PFN_vk##func)vkGetInstanceProcAddr(*instance.pHandle(), "vk" #func); \
  s->result = PFN.func == nullptr ? VK_ERROR_INITIALIZATION_FAILED : VK_SUCCESS;   \
  CHECK_RESULT(instance);

  MVK_PFN_FUNCTIONS
#undef MVK_PFN_FUNCTION

  LOG("Setting up DebugUtilsMessenger... ");
  if (desc.debugUtilsMessengerCreateInfo.pfnUserCallback == nullptr)
    desc.debugUtilsMessengerCreateInfo.pfnUserCallback = DebugCallback;

  s->result = PFN.CreateDebugUtilsMessengerEXT(
      instance.handle(),
      &desc.debugUtilsMessengerCreateInfo.vk(),
      s->pAllocator,
      &s->debugUtilsMessengerEXT);
  CHECK_RESULT(instance);

  LOG("%s\n\n", string_VkResult(instance.Result()));
  return instance;
}

void Instance::Destroy() {
  DESTROY_ASSERT
  vkDestroyInstance(*pHandle(), pState()->pAllocator);
  Release();
}

Surface Instance::CreateSurface(SurfaceDesc&& desc) {
  LOG("# %s... ", desc.debugName);

  auto surface = Surface::Acquire();
  auto s = surface.pState();

  s->pAllocator = DefaultAllocator(desc.pAllocator);

  s->instance = *this;
  s->result = VK_SUCCESS;

  LOG("%s\n\n", string_VkResult(surface.Result()));
  return surface;
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

PhysicalDevice Instance::CreatePhysicalDevice(const PhysicalDeviceDesc&& desc) const {
  LOG("# %s... ", desc.debugName);

  auto physicalDevice = PhysicalDevice::Acquire();
  auto s = physicalDevice.pState();

  s->instance = *this;

  LOG("Getting device count... ");
  uint32_t deviceCount = 0;
  s->result = vkEnumeratePhysicalDevices(handle(), &deviceCount, nullptr);
  CHECK_RESULT(physicalDevice);

  LOG("%d devices found... ", deviceCount);
  s->result = deviceCount > 0 ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
  CHECK_RESULT(physicalDevice);

  LOG("Getting devices... ");
  VkPhysicalDevice devices[deviceCount];
  s->result = vkEnumeratePhysicalDevices(handle(), &deviceCount, devices);
  CHECK_RESULT(physicalDevice);

  // This searches for a physical device that had all the features which were enabled on the instance.

  // I may not want to store these since technically there are stored on the CPU, but in vulkan driver

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

    LOG("Getting %s Physical Device Features... ", s->physicalDeviceProperties.properties.deviceName);
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
    LOG("Picking Device %d %s... ", chosenDeviceIndex, s->physicalDeviceProperties.properties.deviceName);
    *physicalDevice.pHandle() = devices[chosenDeviceIndex];
    s->result = VK_SUCCESS;
  }
  CHECK_RESULT(physicalDevice);

  // Once device is chosen the properties in state should represent what is actually enabled on physicalDevice so copy them over
  // no dont save these, you can requery them
  for (int i = 0; i < requiredCount; ++i) {
    supported[i] = required[i];
  }

  LOG("Getting Physical Device Memory Properties... ");
  vkGetPhysicalDeviceMemoryProperties(physicalDevice.handle(), &s->physicalDeviceMemoryProperties);

  LOG("Getting queue family count... ");
  vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice.handle(), &s->queueFamilyCount, nullptr);

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
  vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice.handle(), &s->queueFamilyCount, s->queueFamilyProperties);

  LOG("%s\n\n", string_VkResult(physicalDevice.Result()));
  return physicalDevice;
}

const VkAllocationCallbacks* Instance::DefaultAllocator(const VkAllocationCallbacks* pAllocator) {
  return pAllocator == nullptr ? pState()->pAllocator : pAllocator;
}

/* PhysicalDevice */
template struct HandleBase<PhysicalDevice, VkPhysicalDevice, PhysicalDeviceState>;

void PhysicalDevice::Destroy() {
  ASSERT(*pHandle() != VK_NULL_HANDLE && "Trying to destroy null PhysicalDevice handle!");
  ASSERT(pState()->result != VK_SUCCESS && "Trying to destroy non-succesfully created PhysicalDevice handle!");
  // you don't destroy physical device handles, so we just release...
  Release();
}

LogicalDevice PhysicalDevice::CreateLogicalDevice(LogicalDeviceDesc&& desc) {
  LOG("# %s... ", desc.debugName);

  auto logicalDevice = LogicalDevice::Acquire();
  auto s = logicalDevice.pState();

  s->pAllocator = pState()->instance.DefaultAllocator(desc.pAllocator);
  s->physicalDevice = *this;

  LOG("Creating... ");
  ASSERT(desc.createInfo.pNext.p == nullptr &&
         "Chaining onto VkDeviceCreateInfo.pNext not supported.");
  ASSERT(desc.createInfo.pEnabledLayerNames.p == nullptr &&
         "VkDeviceCreateInfo.ppEnabledLayerNames obsolete.");
  ASSERT(desc.createInfo.pEnabledFeatures.p == nullptr &&
         "LogicalDeviceDesc.createInfo.pEnabledFeatures should be nullptr. "
         "Internally VkPhysicalDeviceFeatures2 is used from PhysicalDevice.");
  SetNextChainEnd((VkNextStruct*)&desc.createInfo, (VkNextStruct*)&pState()->physicalDeviceFeatures);
  s->result = vkCreateDevice(
      handle(),
      &desc.createInfo.vk(),
      s->pAllocator,
      logicalDevice.pHandle());
  CHECK_RESULT(logicalDevice);

  LOG("Setting LogicalDevice DebugName... ");
  s->result = SetDebugInfo(
      logicalDevice.handle(),
      VK_OBJECT_TYPE_DEVICE,
      (uint64_t)logicalDevice.handle(),
      desc.debugName);
  CHECK_RESULT(logicalDevice);

  // Set debug on physical device and instance now that we can...
  LOG("Setting PhysicalDevice DebugName... ");
  s->result = SetDebugInfo(
      logicalDevice.handle(),
      VK_OBJECT_TYPE_PHYSICAL_DEVICE,
      (uint64_t)handle(),
      s->physicalDevice.pState()->physicalDeviceProperties.properties.deviceName);
  CHECK_RESULT(logicalDevice);

  LOG("Setting Instance DebugName... ");
  auto instance = s->physicalDevice.pState()->instance;
  s->result = SetDebugInfo(
      logicalDevice.handle(),
      VK_OBJECT_TYPE_INSTANCE,
      (uint64_t)instance.handle(),
      instance.pState()->applicationInfo.pApplicationName);
  CHECK_RESULT(logicalDevice);

  LOG("%s\n\n", string_VkResult(logicalDevice.Result()));
  return logicalDevice;
}

uint32_t PhysicalDevice::FindQueueIndex(QueueIndexDesc&& desc) {
  LOG("# Finding queue family: graphics=%s compute=%s transfer=%s globalPriority=%s present=%s... ",
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

/* Surface */
template struct HandleBase<Surface, VkSurfaceKHR, SurfaceState>;

void Surface::Destroy() const {
  DESTROY_ASSERT
  vkDestroySurfaceKHR(state().instance.handle(), handle(), state().pAllocator);
  Release();
}

/* LogicalDevice */
template struct HandleBase<LogicalDevice, VkDevice, LogicalDeviceState>;

void LogicalDevice::Destroy() const {
  DESTROY_ASSERT
  vkDestroyDevice(handle(), state().pAllocator);
  Release();
}

template <typename T, typename THandle, typename TInfo>
inline T CreateHandle(
    const LogicalDevice          device,
    const VkString               debugName,
    const VkObjectType           objectType,
    const TInfo*                 pInfo,
    const VkAllocationCallbacks* pAllocator,
    VkResult                     (*vkFunc)(VkDevice, const TInfo*, const VkAllocationCallbacks*, THandle*)) {
  LOG("# %s... ", debugName);

  auto handle = T::Acquire();
  auto s = handle.pState();

  s->pAllocator = device.DefaultAllocator(pAllocator);
  s->logicalDevice = device;

  LOG("Creating... ");
  s->result = vkFunc(
      device.handle(),
      pInfo,
      s->pAllocator,
      handle.pHandle());
  CHECK_RESULT(handle);

  LOG("Setting DebugName... ");
  s->result = SetDebugInfo(
      device.handle(),
      objectType,
      (uint64_t)*handle.pHandle(),
      debugName);
  CHECK_RESULT(handle);

  LOG("%s\n\n", string_VkResult(handle.Result()));
  return handle;
}

RenderPass LogicalDevice::CreateRenderPass(RenderPassDesc&& desc) const {
  return CreateHandle<RenderPass, VkRenderPass, VkRenderPassCreateInfo>(
      *this,
      desc.debugName,
      VK_OBJECT_TYPE_RENDER_PASS,
      &desc.createInfo.vk(),
      desc.pAllocator,
      vkCreateRenderPass);
}

Queue LogicalDevice::GetQueue(QueueDesc&& desc) const {
  LOG("# %s... ", desc.debugName);

  auto queue = Queue::Acquire();
  auto s = queue.pState();

  s->logicalDevice = *this;

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

  s->result = VK_SUCCESS;

  LOG("DONE\n\n");
  return queue;
}

CommandPool LogicalDevice::CreateCommandPool(CommandPoolDesc&& desc) const {
  return CreateHandle<CommandPool, VkCommandPool, VkCommandPoolCreateInfo>(
      *this,
      desc.debugName,
      VK_OBJECT_TYPE_COMMAND_POOL,
      &desc.createInfo.vk(),
      desc.pAllocator,
      vkCreateCommandPool);
}

QueryPool LogicalDevice::CreateQueryPool(QueryPoolDesc&& desc) const {
  return CreateHandle<QueryPool, VkQueryPool, VkQueryPoolCreateInfo>(
      *this,
      desc.debugName,
      VK_OBJECT_TYPE_QUERY_POOL,
      &desc.createInfo.vk(),
      desc.pAllocator,
      vkCreateQueryPool);
}

DescriptorPool LogicalDevice::CreateDescriptorPool(DescriptorPoolDesc&& desc) const {
  return CreateHandle<DescriptorPool, VkDescriptorPool, VkDescriptorPoolCreateInfo>(
      *this,
      desc.debugName,
      VK_OBJECT_TYPE_DESCRIPTOR_POOL,
      &desc.createInfo.vk(),
      desc.pAllocator,
      vkCreateDescriptorPool);
}

Sampler LogicalDevice::CreateSampler(SamplerDesc&& desc) const {
  // todo step through pnext and apply to end
  desc.createInfo.pNext.p = desc.pReductionModeCreateInfo.p;
  return CreateHandle<Sampler, VkSampler, VkSamplerCreateInfo>(
      *this,
      desc.debugName,
      VK_OBJECT_TYPE_SAMPLER,
      &desc.createInfo.vk(),
      desc.pAllocator,
      vkCreateSampler);
}

static VkResult MemoryTypeFromProperties(
    const VkPhysicalDeviceMemoryProperties& physicalDeviceMemoryProperties,
    const VkMemoryRequirements&             memoryRequirements,
    const VkMemoryPropertyFlags             memoryPropertyFlags,
    uint32_t*                               pMemoryTypeIndex) {
  LOG("With properties... ");
  int  index = 0;
  auto writeRequiredMemoryProperties = memoryPropertyFlags;
  while (writeRequiredMemoryProperties) {
    if (writeRequiredMemoryProperties & 1) {
      LOG(" %s ", string_VkMemoryPropertyFlagBits((VkMemoryPropertyFlagBits)(1U << index)));
    }
    ++index;
    writeRequiredMemoryProperties >>= 1;
  }
  LOG("... ");

  for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++) {
    if ((memoryRequirements.memoryTypeBits & 1 << i) &&
        ((physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & memoryPropertyFlags) == memoryPropertyFlags)) {
      LOG("Found index %d... ", i);
      *pMemoryTypeIndex = i;
      return VK_SUCCESS;
    }
  }

  LOG("Can't find... ");
  return VK_ERROR_INITIALIZATION_FAILED;
}

Image LogicalDevice::CreateImage(ImageDesc&& desc) const {
  LOG("# %s... ", desc.debugName);

  auto image = Image::Acquire();
  auto s = image.pState();

  auto locality = Locality::Local;
  if (desc.pExternalMemoryImageCreateInfo != nullptr) {
    ASSERT(desc.pImportMemoryWin32HandleInfo == nullptr && "Cannot have pImportMemoryWin32HandleInfo if pExternalMemoryImageCreateInfo is set.");
    SetNextChainEnd((VkNextStruct*)&desc.createInfo, (VkNextStruct*)&desc.pExternalMemoryImageCreateInfo);
    locality = Locality::External;
  }
  if (desc.pImportMemoryWin32HandleInfo != nullptr) {
    ASSERT(desc.pExternalMemoryImageCreateInfo == nullptr && "Cannot have pExternalMemoryImageCreateInfo if pImportMemoryWin32HandleInfo is set.");
    SetNextChainEnd((VkNextStruct*)&desc.createInfo, (VkNextStruct*)&desc.pImportMemoryWin32HandleInfo);
    locality = Locality::Imported;
  }

  s->pAllocator = DefaultAllocator(desc.pAllocator);
  s->logicalDevice = *this;
  s->locality = locality;
  s->imageType = desc.createInfo.imageType;
  s->format = desc.createInfo.format;
  s->extent = desc.createInfo.extent;
  s->mipLevels = desc.createInfo.mipLevels;
  s->arrayLayers = desc.createInfo.arrayLayers;
  s->samples = desc.createInfo.samples;
  s->usage = desc.createInfo.usage;
  s->memoryPropertyFlags = desc.memoryPropertyFlags;

  LOG("Creating... ");
  s->result = vkCreateImage(
      handle(),
      &desc.createInfo.vk(),
      s->pAllocator,
      image.pHandle());
  CHECK_RESULT(image);

  LOG("Getting memory requirements... ");
  vkGetImageMemoryRequirements(
      handle(),
      image.handle(),
      &s->memoryRequirements);

  LOG("Getting memory type... ");
  s->result = MemoryTypeFromProperties(
      s->logicalDevice.state().physicalDevice.state().physicalDeviceMemoryProperties,
      s->memoryRequirements,
      s->memoryPropertyFlags,
      &s->memoryTypeIndex);
  CHECK_RESULT(image);

  LOG("Setting DebugName... ");
  s->result = SetDebugInfo(
      handle(),
      VK_OBJECT_TYPE_IMAGE,
      (uint64_t)*image.pHandle(),
      desc.debugName);
  CHECK_RESULT(image);

  LOG("%s\n\n", string_VkResult(image.Result()));
  return image;
}

DeviceMemory LogicalDevice::AllocateMemory(DeviceMemoryDesc&& desc) const {
  LOG("# %s... ", desc.debugName);

  auto deviceMemory = DeviceMemory::Acquire();
  auto s = deviceMemory.pState();

  s->pAllocator = DefaultAllocator(desc.pAllocator);
  s->logicalDevice = *this;

  LOG("Creating... ");
  s->result = vkAllocateMemory(
      handle(),
      &desc.allocateInfo.vk(),
      s->pAllocator,
      deviceMemory.pHandle());
  CHECK_RESULT(deviceMemory);

  LOG("Setting DebugName... ");
  s->result = SetDebugInfo(
      handle(),
      VK_OBJECT_TYPE_DEVICE_MEMORY,
      (uint64_t)*deviceMemory.pHandle(),
      desc.debugName);
  CHECK_RESULT(deviceMemory);

  s->allocationSize = desc.allocateInfo.allocationSize;
  s->memoryTypeIndex = desc.allocateInfo.memoryTypeIndex;

  LOG("%s\n\n", string_VkResult(deviceMemory.Result()));
  return deviceMemory;
}

template <typename T, typename TBits>
static void LogFlagBits(
    T        flags,
    VkString (*stringFunc)(TBits)) {
  int index = 0;
  while (flags) {
    if (flags & 1) {
      LOG(" %s ", stringFunc((TBits)(1U << index)));
    }
    ++index;
    flags >>= 1;
  }
}

Swapchain LogicalDevice::CreateSwapchain(SwapchainDesc&& desc) const {
  LOG("# %s... ", desc.debugName);

  auto swapchain = Swapchain::Acquire();
  auto s = swapchain.pState();

  s->pAllocator = DefaultAllocator(desc.pAllocator);
  s->logicalDevice = *this;

  {
    LOG("Getting present mode count... ");
    uint32_t presentModeCount;
    s->result = vkGetPhysicalDeviceSurfacePresentModesKHR(
        state().physicalDevice.handle(),
        desc.createInfo.surface,
        &presentModeCount,
        nullptr);
    CHECK_RESULT(swapchain);

    LOG("Found %d... Getting present modes... ", presentModeCount);
    VkPresentModeKHR presentModes[presentModeCount];
    s->result = vkGetPhysicalDeviceSurfacePresentModesKHR(
        state().physicalDevice.handle(),
        desc.createInfo.surface,
        &presentModeCount,
        (VkPresentModeKHR*)&presentModes);
    CHECK_RESULT(swapchain);

    LOG("Checking for present mode %s... ",
        string_VkPresentModeKHR(desc.createInfo.presentMode));
    bool foundPresentMode = false;
    for (uint32_t i = 0; i < presentModeCount; ++i) {
      LOG("%s... ", string_VkPresentModeKHR(presentModes[i]));
      if (presentModes[i] == desc.createInfo.presentMode) {
        foundPresentMode = true;
        break;
      }
    }

    if (!foundPresentMode) {
      s->result = VK_ERROR_INITIALIZATION_FAILED;
      LOG("Present mode unavailable on PhysicalDevice... ");
      CHECK_RESULT(swapchain);
    }
  }

  {
    LOG("Getting SurfaceFormat count... ");
    uint32_t formatCount;
    s->result = vkGetPhysicalDeviceSurfaceFormatsKHR(
        state().physicalDevice.handle(),
        desc.createInfo.surface,
        &formatCount,
        nullptr);
    CHECK_RESULT(swapchain);

    LOG("Found %d... Getting SurfaceFormats... ", formatCount);
    VkSurfaceFormatKHR formats[formatCount];
    s->result = vkGetPhysicalDeviceSurfaceFormatsKHR(
        state().physicalDevice.handle(),
        desc.createInfo.surface,
        &formatCount,
        (VkSurfaceFormatKHR*)&formats);
    CHECK_RESULT(swapchain);

    LOG("Checking for SurfaceFormat %s %s... ",
        string_VkFormat(desc.createInfo.imageFormat),
        string_VkColorSpaceKHR(desc.createInfo.imageColorSpace));
    bool foundFormat = false;
    for (uint32_t i = 0; i < formatCount; ++i) {
      LOG("%s %s... ", string_VkFormat(formats[i].format),
          string_VkColorSpaceKHR(formats[i].colorSpace));
      if (formats[i].format == desc.createInfo.imageFormat) {
        foundFormat = true;
        desc.createInfo.imageColorSpace = formats[i].colorSpace;
        break;
      }
    }

    if (!foundFormat) {
      s->result = VK_ERROR_INITIALIZATION_FAILED;
      LOG("SurfaceFormat unavailable PhysicalDevice... ");
      CHECK_RESULT(swapchain);
    }
  }

  LOG("Getting SurfaceCapabilities... ");
  VkSurfaceCapabilitiesKHR capabilities;
  s->result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      state().physicalDevice.handle(),
      desc.createInfo.surface,
      &capabilities);
  CHECK_RESULT(swapchain);

  LOG("Checking minImageCount %d maxImageCount %d... ", capabilities.minImageCount, capabilities.maxImageCount);
  if (desc.createInfo.minImageCount < capabilities.minImageCount ||
      desc.createInfo.minImageCount > capabilities.maxImageCount) {
    LOG("MinImageCount %d not supported ... ", desc.createInfo.minImageCount);
    s->result = VK_ERROR_INITIALIZATION_FAILED;
    CHECK_RESULT(swapchain);
  }

  LOG("Checking imageExtent %d %d... ",
      desc.createInfo.imageExtent.width,
      desc.createInfo.imageExtent.height);
  if (desc.createInfo.imageExtent.width < capabilities.minImageExtent.width ||
      desc.createInfo.imageExtent.height < capabilities.minImageExtent.height ||
      desc.createInfo.imageExtent.width > capabilities.maxImageExtent.width ||
      desc.createInfo.imageExtent.height > capabilities.maxImageExtent.height) {
    LOG("Error! Supported minImageExtent %d %d maxImageExtent %d %d... ",
        capabilities.minImageExtent.width,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.width,
        capabilities.maxImageExtent.height);
    s->result = VK_ERROR_INITIALIZATION_FAILED;
    CHECK_RESULT(swapchain);
  }

  LOG("Checking imageUsage... ");
  LogFlagBits<VkImageUsageFlags, VkImageUsageFlagBits>(desc.createInfo.imageUsage, string_VkImageUsageFlagBits);
  if ((desc.createInfo.imageUsage & capabilities.supportedUsageFlags) != desc.createInfo.imageUsage) {
    LOG("Error! Supported imageUsage ... ");
    LogFlagBits<VkImageUsageFlags, VkImageUsageFlagBits>(capabilities.supportedUsageFlags, string_VkImageUsageFlagBits);
    s->result = VK_ERROR_INITIALIZATION_FAILED;
    CHECK_RESULT(swapchain);
  }

  LOG("Checking preTransform %s... ", string_VkSurfaceTransformFlagBitsKHR(desc.createInfo.preTransform));
  if ((desc.createInfo.preTransform & capabilities.supportedTransforms) != desc.createInfo.preTransform) {
    LOG("Error! Supported transforms... ");
    LogFlagBits<VkSurfaceTransformFlagsKHR, VkSurfaceTransformFlagBitsKHR>(capabilities.supportedTransforms, string_VkSurfaceTransformFlagBitsKHR);
    s->result = VK_ERROR_INITIALIZATION_FAILED;
    CHECK_RESULT(swapchain);
  }

  LOG("Checking compositeAlpha %s... ", string_VkCompositeAlphaFlagBitsKHR(desc.createInfo.compositeAlpha));
  if ((desc.createInfo.compositeAlpha & capabilities.supportedCompositeAlpha) != desc.createInfo.compositeAlpha) {
    LOG("Error! compositeAlphas... ");
    LogFlagBits<VkCompositeAlphaFlagsKHR, VkCompositeAlphaFlagBitsKHR>(capabilities.supportedCompositeAlpha, string_VkCompositeAlphaFlagBitsKHR);
    s->result = VK_ERROR_INITIALIZATION_FAILED;
    CHECK_RESULT(swapchain);
  }

  LOG("Creating... ");
  s->result = vkCreateSwapchainKHR(
      handle(),
      &desc.createInfo.vk(),
      s->pAllocator,
      swapchain.pHandle());
  CHECK_RESULT(swapchain);

  LOG("Setting DebugName... ");
  s->result = SetDebugInfo(
      handle(),
      VK_OBJECT_TYPE_SWAPCHAIN_KHR,
      (uint64_t)*swapchain.pHandle(),
      desc.debugName);
  CHECK_RESULT(swapchain);

  LOG("%s\n\n", string_VkResult(swapchain.Result()));
  return swapchain;
}

const VkAllocationCallbacks* LogicalDevice::DefaultAllocator(
    const VkAllocationCallbacks* pAllocator) const {
  return pAllocator == nullptr ? state().pAllocator : pAllocator;
}

/* Queue */
template struct HandleBase<Queue, VkQueue, QueueState>;

void Queue::Destroy() const {
  DESTROY_ASSERT
  Release();
}

/* CommandPool */
template struct HandleBase<CommandPool, VkCommandPool, CommandPoolState>;

CommandBuffer CommandPool::AllocateCommandBuffer(CommandBufferDesc&& desc) const {
  LOG("# AllocateCommandBuffer... ");

  auto commandBuffer = CommandBuffer::Acquire();
  auto s = commandBuffer.pState();

  s->commandPool = *this;

  LOG("Allocating... ");
  ASSERT(desc.allocateInfo.commandBufferCount == 1 &&
         "Can only allocate one command buffer with CommandPool::AllocateCommandBuffer!");
  ASSERT(desc.allocateInfo.commandPool == VK_NULL_HANDLE &&
         "CommandPool::AllocateCommandBuffer overrides allocateInfo.commandPool!");
  desc.allocateInfo.commandPool = handle();
  auto logicalDevice = state().logicalDevice;
  s->result = vkAllocateCommandBuffers(
      logicalDevice.handle(),
      &desc.allocateInfo.vk(),
      commandBuffer.pHandle());
  CHECK_RESULT(commandBuffer);

  LOG("Setting DebugName... ");
  s->result = SetDebugInfo(
      logicalDevice.handle(),
      VK_OBJECT_TYPE_COMMAND_BUFFER,
      (uint64_t)commandBuffer.handle(),
      desc.debugName);
  CHECK_RESULT(commandBuffer);

  LOG("%s\n\n", string_VkResult(commandBuffer.Result()));
  return commandBuffer;
}

void CommandPool::Destroy() const {
  DESTROY_ASSERT
  vkDestroyCommandPool(state().logicalDevice.handle(), handle(), state().pAllocator);
  Release();
}

// void DeviceMemory::BindImage(Image image, VkDeviceSize memoryOffset) {
// thinking BindImage like this should be on image, as it only binds one image
// and multiple BindImage2 can be on the device memory
// also because it being on the image then makes more sense to put the fail state on the image
// because a binding failure does means the image isnt ready, not the device memory
// assert(false);
// LOG("# Binding image at offset %d... ", memoryOffset);
//
// auto s = image.pState();
//
// ASSERT(s->memoryTypeIndex == state().memoryTypeIndex && "MemoryTypeIndex of Image and DeviceMemory do not match.");
// ASSERT(s->memoryRequirements.size <= state().allocationSize - state().currentBindOffset && "DeviceMemory allocationSize after currentBindOffset not large enough.");
//
// // should result also go in the device memory and I check device memory?
// s->result = vkBindImageMemory(state().logicalDevice.handle(), image.handle(), handle(), memoryOffset);
// CHECK_RESULT_VOID(image);
//
// s->deviceMemory = *this;
// s->deviceMemoryOffset = pState()->currentBindOffset;
//
// pState()->currentBindOffset += s->memoryRequirements.size;
//
// LOG("%s\n\n", string_VkResult(image.Result()));
// }

template struct HandleBase<Image, VkImage, ImageState>;

void Image::BindImage(DeviceMemory deviceMemory, VkDeviceSize memoryOffset) {
  LOG("# Binding image at offset %d... ", memoryOffset);

  Image image = *this;
  auto  s = pState();
  auto  ds = deviceMemory.pState();

  ASSERT(s->memoryTypeIndex == ds->memoryTypeIndex && "MemoryTypeIndex of Image and DeviceMemory do not match.");
  ASSERT(s->memoryRequirements.size <= ds->allocationSize - ds->currentBindOffset && "DeviceMemory allocationSize after currentBindOffset not large enough.");

  // should result also go in the device memory and I check device memory?
  s->result = vkBindImageMemory(state().logicalDevice.handle(), handle(), deviceMemory.handle(), memoryOffset);
  CHECK_RESULT_VOID(image);

  s->deviceMemory = deviceMemory;
  s->deviceMemoryOffset = ds->currentBindOffset;

  ds->currentBindOffset += s->memoryRequirements.size;

  LOG("%s\n\n", string_VkResult(image.Result()));
}

template struct HandleBase<ImageView, VkImageView, ImageViewState>;

ImageView Image::CreateImageView(ImageViewDesc&& desc) const {
  desc.createInfo.image = handle();
  desc.createInfo.format = state().format;

  // if mip level count not set, then presume it's for the whole image and set to image mip level
  auto levelCount = desc.createInfo.subresourceRange.levelCount;
  desc.createInfo.subresourceRange.levelCount = levelCount == 0 ? state().mipLevels : levelCount;

  return CreateHandle<ImageView, VkImageView, VkImageViewCreateInfo>(
      state().logicalDevice,
      desc.debugName,
      VK_OBJECT_TYPE_IMAGE_VIEW,
      &desc.createInfo.vk(),
      desc.pAllocator,
      vkCreateImageView);
}

template struct HandleBase<Swapchain, VkSwapchainKHR, SwapchainState>;

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