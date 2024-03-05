/**********************************************************************************************
 *
 *   Mid Level Vulkan v0.0 - Vulkan boilerplate API
 *
 **********************************************************************************************/

#pragma once

#include <assert.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#define MVK_DEFAULT_IMAGE_LAYOUT VK_IMAGE_LAYOUT_GENERAL
#define MVK_DEFAULT_DESCRIPTOR_TYPE VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
#define MVK_DEFAULT_SHADER_STAGE VK_SHADER_STAGE_COMPUTE_BIT
#define MVK_DEFAULT_ALLOCATOR nullptr

#define MVK_ASSERT(command) assert(command)
#define MVK_LOG(...) printf(__VA_ARGS__)

#define VKM_CHECK(command)                         \
  {                                                \
    VkResult result = command;                     \
    if (result != VK_SUCCESS) [[unlikely]] {       \
      printf("VKCheck fail on command: %s - %s\n", \
             #command,                             \
             string_VkResult(result));             \
      return result;                               \
    }                                              \
  }

#define VKM_CHECK2(command)                        \
  {                                                \
    result = command;                              \
    if (result != VK_SUCCESS) [[unlikely]] {       \
      printf("VKCheck fail on command: %s - %s\n", \
             #command,                             \
             string_VkResult(result));             \
      return result;                               \
    }                                              \
  }

#define FORCE_INLINE inline __attribute__((always_inline))

namespace Mid::Vk {

struct {
#define MVK_PFN_FUNCTIONS                      \
  MVK_PFN_FUNCTION(SetDebugUtilsObjectNameEXT) \
  MVK_PFN_FUNCTION(CreateDebugUtilsMessengerEXT)

#define MVK_PFN_FUNCTION(func) PFN_vk##func func;
  MVK_PFN_FUNCTIONS
#undef MVK_PFN_FUNCTION
} PFN;

struct VkFormatAspect {
  VkFormat           format;
  VkImageAspectFlags aspectMask;
};

constexpr VkFormatAspect VkFormatR8B8G8A8UNorm{VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT};
constexpr VkFormatAspect VkFormatD32SFloat{VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT};

struct VkTexture {
  VkFormatAspect formatAspect;
  uint32_t       levelCount;
  uint32_t       layerCount;
  VkImage        image;
  VkImageView    view;
};

typedef const char* VkString;
typedef uint32_t    VkCount;

struct static_void_ptr {
  void* data{nullptr};

  template <typename T>
  constexpr static_void_ptr(T&& value) : data{&value} {}
  constexpr static_void_ptr() = default;
};

template <typename T>
struct static_ptr {
  const T* data{nullptr};

  constexpr static_ptr(T&& value) : data{&value} {}
  constexpr static_ptr(std::initializer_list<T> l) : data{l.begin()} {}
  constexpr static_ptr() = default;
};

template <typename T>
struct static_array_ptr {
  const VkCount count{0};
  const T*      data{nullptr};

  constexpr static_array_ptr(std::initializer_list<T> l) : count{VkCount(l.size())}, data{l.begin()} {}
  constexpr static_array_ptr() = default;
};

// Yes you need this for some reason. I really hate.
#pragma pack(push, 4)
template <typename T>
struct static_array_ptr_packed {
  const VkCount count{0};
  const T*      data{nullptr};

  constexpr static_array_ptr_packed(std::initializer_list<T> l) : count{VkCount(l.size())}, data{l.begin()} {}
  constexpr static_array_ptr_packed() = default;
};
#pragma pack(pop)

template <typename T>
struct VkStruct {
  T* ptr() const { return (T*)this; }
  operator const T&() const { return *(T*)this; };
};

struct ApplicationInfo : VkStruct<VkApplicationInfo> {
  VkStructureType sType{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  static_void_ptr pNext;
  VkString        pApplicationName{"Mid Vulkan Application"};
  uint32_t        applicationVersion;
  VkString        pEngineName{"Mid Vulkan"};
  uint32_t        engineVersion;
  uint32_t        apiVersion{VK_MAKE_API_VERSION(0, 1, 3, 0)};
};
static_assert(sizeof(ApplicationInfo) == sizeof(VkApplicationInfo));

struct InstanceCreateInfo : VkStruct<VkInstanceCreateInfo> {
  VkStructureType             sType{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  static_void_ptr             pNext;
  VkInstanceCreateFlags       flags;
  static_ptr<ApplicationInfo> pApplicationInfo;
  static_array_ptr<VkString>  pEnabledLayerNames;
  static_array_ptr<VkString>  pEnabledExtensionNames;
};
static_assert(sizeof(InstanceCreateInfo) == sizeof(VkInstanceCreateInfo));

struct ValidationFeatures : VkStruct<VkValidationFeaturesEXT> {
  VkStructureType                                 sType{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
  static_void_ptr                                 pNext;
  static_array_ptr<VkValidationFeatureEnableEXT>  pEnabledValidationFeatures;
  static_array_ptr<VkValidationFeatureDisableEXT> pDisabledValidationFeatures;
};
static_assert(sizeof(ValidationFeatures) == sizeof(VkValidationFeaturesEXT));

struct DeviceQueueCreateInfo : VkStruct<VkDeviceQueueCreateInfo> {
  VkStructureType          sType{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  static_void_ptr          pNext;
  VkDeviceQueueCreateFlags flags;
  uint32_t                 queueFamilyIndex;
  uint32_t                 queueCount{1};
  static_ptr<float>        pQueuePriorities;
};
static_assert(sizeof(DeviceQueueCreateInfo) == sizeof(VkDeviceQueueCreateInfo));

struct DeviceCreateInfo : VkStruct<VkDeviceCreateInfo> {
  VkStructureType                                sType{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  static_void_ptr                                pNext;
  VkDeviceCreateFlags                            flags;
  static_array_ptr_packed<DeviceQueueCreateInfo> pQueueCreateInfos;
  static_array_ptr<VkString>                     pEnabledLayerNames;
  static_array_ptr<VkString>                     pEnabledExtensionNames;
  static_ptr<VkPhysicalDeviceFeatures>           pEnabledFeatures;
};
// I really hate this, I think I am going to just make simpliffied C++ structs that you copy
static_assert(sizeof(DeviceCreateInfo) == sizeof(VkDeviceCreateInfo));
static_assert(offsetof(DeviceCreateInfo, sType) == offsetof(VkDeviceCreateInfo, sType));
static_assert(offsetof(DeviceCreateInfo, pNext) == offsetof(VkDeviceCreateInfo, pNext));
static_assert(offsetof(DeviceCreateInfo, flags) == offsetof(VkDeviceCreateInfo, flags));
static_assert(offsetof(DeviceCreateInfo, pQueueCreateInfos) == offsetof(VkDeviceCreateInfo, queueCreateInfoCount));
static_assert(offsetof(DeviceCreateInfo, pEnabledLayerNames) == offsetof(VkDeviceCreateInfo, enabledLayerCount));
static_assert(offsetof(DeviceCreateInfo, pEnabledExtensionNames) == offsetof(VkDeviceCreateInfo, enabledExtensionCount));
static_assert(offsetof(DeviceCreateInfo, pEnabledFeatures) == offsetof(VkDeviceCreateInfo, pEnabledFeatures));

struct DeviceQueueGlobalPriorityCreateInfo : VkStruct<VkDeviceQueueGlobalPriorityCreateInfoKHR> {
  VkStructureType          sType{VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT};
  static_void_ptr          pNext;
  VkQueueGlobalPriorityKHR globalPriority;
};

struct DescriptorSetLayoutBinding {
  uint32_t           binding{0};
  VkDescriptorType   descriptorType{MVK_DEFAULT_DESCRIPTOR_TYPE};
  uint32_t           descriptorCount{1};
  VkShaderStageFlags stageFlags{MVK_DEFAULT_SHADER_STAGE};
  const VkSampler*   pImmutableSamplers{VK_NULL_HANDLE};

  DescriptorSetLayoutBinding WithSamplers(const VkSampler* pImmutableSamplers) const {
    return {
        .binding = binding,
        .descriptorType = descriptorType,
        .descriptorCount = descriptorCount,
        .stageFlags = stageFlags,
        .pImmutableSamplers = pImmutableSamplers};
  }

  constexpr operator VkDescriptorSetLayoutBinding() const { return *(VkDescriptorSetLayoutBinding*)this; }
};

struct DescriptorImageInfo {
  const VkSampler     sampler{VK_NULL_HANDLE};
  const VkImageView   imageView{VK_NULL_HANDLE};
  const VkImageLayout imageLayout{MVK_DEFAULT_IMAGE_LAYOUT};

  constexpr operator VkDescriptorImageInfo() const { return *(VkDescriptorImageInfo*)this; }
};

struct WriteDescriptorSet {
  const VkStructureType         sType{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  const void*                   pNext{nullptr};
  const VkDescriptorSet         dstSet{VK_NULL_HANDLE};
  const uint32_t                dstBinding{0};
  const uint32_t                dstArrayElement{0};
  const uint32_t                descriptorCount{1};
  const VkDescriptorType        descriptorType{MVK_DEFAULT_DESCRIPTOR_TYPE};
  const DescriptorImageInfo*    pImageInfo{nullptr};
  const VkDescriptorBufferInfo* pBufferInfo{nullptr};
  const VkBufferView*           pTexelBufferView{nullptr};

  constexpr operator VkWriteDescriptorSet() const { return *(VkWriteDescriptorSet*)this; }
};

struct DescriptorSetLayoutCreateInfo {
  const VkStructureType                  sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  const void*                            pNext{nullptr};
  const VkDescriptorSetLayoutCreateFlags flags{0};
  const uint32_t                         bindingCount{1};
  const DescriptorSetLayoutBinding*      pBindings{nullptr};

  const VkDescriptorSetLayoutCreateInfo& vkHandle{*(VkDescriptorSetLayoutCreateInfo*)this};
  constexpr                              operator VkDescriptorSetAllocateInfo() const { return *(VkDescriptorSetAllocateInfo*)this; }
};

struct DescriptorSetAllocateInfo {
  const VkStructureType        sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  const void*                  pNext{nullptr};
  const VkDescriptorPool       descriptorPool{VK_NULL_HANDLE};
  const uint32_t               descriptorSetCount{1};
  const VkDescriptorSetLayout* pSetLayouts{nullptr};

  constexpr operator VkDescriptorSetAllocateInfo() const { return *(VkDescriptorSetAllocateInfo*)this; }
};

struct PipelineLayoutCreateInfo {
  const VkStructureType             sType{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  const void*                       pNext{nullptr};
  const VkPipelineLayoutCreateFlags flags{};
  const uint32_t                    setLayoutCount{1};
  const VkDescriptorSetLayout*      pSetLayouts{nullptr};
  const uint32_t                    pushConstantRangeCount{0};
  const VkPushConstantRange*        pPushConstantRanges{nullptr};

  constexpr operator VkPipelineLayoutCreateInfo() const { return *(VkPipelineLayoutCreateInfo*)this; }
};

struct PipelineShaderStageCreateInfo {
  const VkStructureType                  sType{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
  const void* const                      pNext{nullptr};
  const VkPipelineShaderStageCreateFlags flags{};
  const VkShaderStageFlagBits            stage{MVK_DEFAULT_SHADER_STAGE};
  const VkShaderModule                   module{VK_NULL_HANDLE};
  const char* const                      pName{"main"};
  const VkSpecializationInfo*            pSpecializationInfo{nullptr};

  constexpr operator VkPipelineShaderStageCreateInfo() const { return *(VkPipelineShaderStageCreateInfo*)this; }
};

struct ComputePipelineCreateInfo {
  const VkStructureType               sType{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
  const void* const                   pNext{nullptr};
  const VkPipelineCreateFlags         flags{0};
  const PipelineShaderStageCreateInfo stage{};
  const VkPipelineLayout              layout{VK_NULL_HANDLE};
  const VkPipeline                    basePipelineHandle{VK_NULL_HANDLE};
  const int32_t                       basePipelineIndex{0};

  constexpr operator VkComputePipelineCreateInfo() const { return *(VkComputePipelineCreateInfo*)this; }
};

struct ShaderModuleCreateInfo {
  const VkStructureType     sType{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  const void*               pNext{nullptr};
  VkShaderModuleCreateFlags flags{0};
  size_t                    codeSize{0};
  const uint32_t*           pCode{nullptr};

  constexpr operator VkShaderModuleCreateInfo() const { return *(VkShaderModuleCreateInfo*)this; }
};

// Handles

#define MVK_HANDLE_CONSTRUCTOR(name) \
  name() = default;                  \
  name(const name&) = delete;        \
  name& operator=(const name&) = delete;

struct SwapChain {
  MVK_HANDLE_CONSTRUCTOR(SwapChain)

  const VkSwapchainKHR handle{VK_NULL_HANDLE};
  const VkDevice       deviceHandle{VK_NULL_HANDLE};

  ~SwapChain() {
    if (handle != VK_NULL_HANDLE)
      vkDestroySwapchainKHR(deviceHandle, handle, MVK_DEFAULT_ALLOCATOR);
  }

  constexpr operator VkSwapchainKHR() const { return handle; }
};

struct DescriptorSetLayout {
  MVK_HANDLE_CONSTRUCTOR(DescriptorSetLayout)

  VkDescriptorSetLayout vkHandle{VK_NULL_HANDLE};
  VkDevice              vkDeviceHandle{VK_NULL_HANDLE};

  VkResult Create(
      const VkDevice                        vkDeviceHandle,
      const char* const                     name,
      const DescriptorSetLayoutCreateInfo&& createInfo) {
    this->vkDeviceHandle = vkDeviceHandle;
    VKM_CHECK(vkCreateDescriptorSetLayout(vkDeviceHandle,
                                          &createInfo.vkHandle,
                                          MVK_DEFAULT_ALLOCATOR,
                                          &vkHandle));
    // const DebugUtilsObjectNameInfoEXT debugInfo{
    //   .objectType = VK_OBJECT_TYPE_SWAPCHAIN_KHR,
    //   .objectHandle = (uint64_t) handle,
    //   .pObjectName = name,
    // };
    // VKM_CHECK(PFN.SetDebugUtilsObjectNameEXT(deviceHandle, (VkDebugUtilsObjectNameInfoEXT*) &debugInfo));
    return VK_SUCCESS;
  }

  ~DescriptorSetLayout() {
    if (vkHandle != VK_NULL_HANDLE)
      vkDestroyDescriptorSetLayout(vkDeviceHandle, vkHandle, MVK_DEFAULT_ALLOCATOR);
  }

  constexpr operator const VkDescriptorSetLayout&() const { return vkHandle; }
};

struct DescriptorSet {
  MVK_HANDLE_CONSTRUCTOR(DescriptorSet)

  VkDescriptorSet  handle{VK_NULL_HANDLE};
  VkDevice         deviceHandle{VK_NULL_HANDLE};
  VkDescriptorPool descriptorPoolHandle{VK_NULL_HANDLE};
  VkResult         result{VK_NOT_READY};

  // void SetDebugInfo(
  //   VkDevice device,
  //   const char* const name)
  // {
  //     const DebugUtilsObjectNameInfoEXT debugInfo{
  //       .objectType = VK_OBJECT_TYPE_SWAPCHAIN_KHR,
  //       .objectHandle = (uint64_t) handle,
  //       .pObjectName = name,
  //     };
  //     PFN.SetDebugUtilsObjectNameEXT(device, (VkDebugUtilsObjectNameInfoEXT*) &debugInfo);
  // }

  ~DescriptorSet() {
    if (handle != VK_NULL_HANDLE)
      vkFreeDescriptorSets(deviceHandle, descriptorPoolHandle, 1, &handle);
  }

  constexpr operator const VkDescriptorSet&() const { return handle; }
};

struct PipelineLayout {
  MVK_HANDLE_CONSTRUCTOR(PipelineLayout)

  VkPipelineLayout handle{VK_NULL_HANDLE};
  VkDevice         deviceHandle{VK_NULL_HANDLE};
  VkResult         result{VK_NOT_READY};

  VkResult Create(
      const VkDevice                        device,
      const char* const                     name,
      const PipelineLayoutCreateInfo* const pCreateInfo) {
    deviceHandle = device;
    VKM_CHECK2(vkCreatePipelineLayout(device,
                                      (VkPipelineLayoutCreateInfo*)pCreateInfo,
                                      MVK_DEFAULT_ALLOCATOR,
                                      &handle));
    // const DebugUtilsObjectNameInfoEXT debugInfo{
    //   .objectType = VK_OBJECT_TYPE_PIPELINE_LAYOUT,
    //   .objectHandle = (uint64_t) handle,
    //   .pObjectName = name,
    // };
    // VKM_CHECK(PFN.SetDebugUtilsObjectNameEXT(device, (VkDebugUtilsObjectNameInfoEXT*) &debugInfo));
    return VK_SUCCESS;
  }

  ~PipelineLayout() {
    if (handle != VK_NULL_HANDLE)
      vkDestroyPipelineLayout(deviceHandle, handle, MVK_DEFAULT_ALLOCATOR);
  }

  constexpr operator const VkPipelineLayout&() const { return handle; }
};

inline VkResult
ReadFile(const char* filename,
         uint32_t*   length,
         char**      ppContents) {
  FILE* file = fopen(filename, "rb");
  if (file == nullptr) {
    printf("File can't be opened! %s\n", filename);
    return VK_ERROR_INVALID_SHADER_NV;
  }
  fseek(file, 0, SEEK_END);
  *length = ftell(file);
  rewind(file);
  *ppContents = (char*)calloc(1 + *length, sizeof(char));
  const size_t readCount = fread(*ppContents, *length, 1, file);
  if (readCount == 0) {
    printf("Failed to read file! %s\n", filename);
    return VK_ERROR_INVALID_SHADER_NV;
  }
  fclose(file);
  return VK_SUCCESS;
}

struct ShaderModule {
  MVK_HANDLE_CONSTRUCTOR(ShaderModule)

  VkShaderModule handle{VK_NULL_HANDLE};
  VkDevice       deviceHandle{VK_NULL_HANDLE};

  VkResult Create(
      const VkDevice    deviceHandle,
      const char* const pShaderPath) {
    this->deviceHandle = deviceHandle;
    uint32_t codeLength;
    char*    pShaderCode;
    VKM_CHECK(ReadFile(pShaderPath, &codeLength, &pShaderCode));
    const ShaderModuleCreateInfo createInfo{
        .codeSize = codeLength,
        .pCode = (uint32_t*)pShaderCode,
    };
    VKM_CHECK(vkCreateShaderModule(deviceHandle,
                                   (VkShaderModuleCreateInfo*)&createInfo,
                                   MVK_DEFAULT_ALLOCATOR,
                                   &handle));
    // const DebugUtilsObjectNameInfoEXT debugInfo{
    //   .objectType = VK_OBJECT_TYPE_SHADER_MODULE,
    //   .objectHandle = (uint64_t) handle,
    //   .pObjectName = pShaderPath,
    // };
    // VKM_CHECK(PFN.SetDebugUtilsObjectNameEXT(device, (VkDebugUtilsObjectNameInfoEXT*) &debugInfo));
    free(pShaderCode);
    return VK_SUCCESS;
  }

  ~ShaderModule() {
    if (handle != VK_NULL_HANDLE)
      vkDestroyShaderModule(deviceHandle, handle, MVK_DEFAULT_ALLOCATOR);
  }

  constexpr operator VkShaderModule() const { return handle; }
};

struct ComputePipeline {
  MVK_HANDLE_CONSTRUCTOR(ComputePipeline)

  VkPipeline handle{VK_NULL_HANDLE};
  VkDevice   deviceHandle{VK_NULL_HANDLE};

  VkResult Create(
      const VkDevice                         device,
      const char* const                      name,
      const ComputePipelineCreateInfo* const pCreateInfo) {
    deviceHandle = device;
    VKM_CHECK(vkCreateComputePipelines(device,
                                       VK_NULL_HANDLE,
                                       1,
                                       (VkComputePipelineCreateInfo*)pCreateInfo,
                                       MVK_DEFAULT_ALLOCATOR,
                                       &handle));
    // const DebugUtilsObjectNameInfoEXT debugInfo{
    //   .objectType = VK_OBJECT_TYPE_PIPELINE,
    //   .objectHandle = (uint64_t) handle,
    //   .pObjectName = name,
    // };
    // VKM_CHECK(PFN.SetDebugUtilsObjectNameEXT(device, (VkDebugUtilsObjectNameInfoEXT*) &debugInfo));
    return VK_SUCCESS;
  }

  void BindPipeline(const VkCommandBuffer commandBuffer) const {
    vkCmdBindPipeline(commandBuffer,
                      VK_PIPELINE_BIND_POINT_COMPUTE,
                      handle);
  }

  ~ComputePipeline() {
    if (handle != VK_NULL_HANDLE)
      vkDestroyPipeline(deviceHandle, handle, MVK_DEFAULT_ALLOCATOR);
  }

  constexpr operator VkPipeline() const { return handle; }
};

#define MVK_HANDLE_INDEX_TYPE uint8_t
#define MVK_HANDLE_COUNT 16
constexpr static MVK_HANDLE_INDEX_TYPE HandleLastIndex = (1 << 8 * sizeof(MVK_HANDLE_INDEX_TYPE)) - 1;
static_assert(MVK_HANDLE_COUNT <= 1 << (8 * sizeof(MVK_HANDLE_INDEX_TYPE)));

#define MVK_HANDLE_GENERATION_TYPE uint8_t
constexpr static MVK_HANDLE_GENERATION_TYPE HandleGenerationCount = (1 << 8 * sizeof(MVK_HANDLE_GENERATION_TYPE)) - 1;

template <typename T, T N>
struct HandleStack {
  T current{N - 1};
  T data[N]{};

  T Pop() {
    MVK_ASSERT(current >= 0 && "Trying to pop handle stack below 0.");
    return data[current--];
  }

  void Push(T value) {
    MVK_ASSERT(current != HandleLastIndex && "Tring to push handle stack above capacity.");
    data[++current] = value;
  }

  HandleStack() {
    MVK_ASSERT(current <= HandleLastIndex && "Trying to create handle stack with capacity greater than type supports.");
    for (size_t i = 0; i < N; ++i) {
      data[i] = (T)i;
    }
  }
};

template <typename THandle, MVK_HANDLE_INDEX_TYPE N>
static inline THandle handles[N]{};

template <typename THandle, MVK_HANDLE_INDEX_TYPE N>
static inline MVK_HANDLE_GENERATION_TYPE generations[N]{};

template <typename TState, MVK_HANDLE_INDEX_TYPE N>
static inline TState states[N]{};

template <typename THandle, MVK_HANDLE_INDEX_TYPE N>
static inline HandleStack<MVK_HANDLE_INDEX_TYPE, N> freeIndexStack;

template <typename Derived, typename THandle, typename TState, MVK_HANDLE_INDEX_TYPE TCapacity>
struct HandleBase {
  MVK_HANDLE_INDEX_TYPE      handleIndex{};
  MVK_HANDLE_GENERATION_TYPE handleGeneration{};

  THandle* handle() const {
    MVK_ASSERT(IsValid());
    return &handles<THandle, TCapacity>[handleIndex];
  }
  TState* state() {
    MVK_ASSERT(IsValid());
    return &states<TState, TCapacity>[handleIndex];
  }

  static Derived Acquire() {
    const auto index = freeIndexStack<THandle, TCapacity>.Pop();
    const auto generation = generations<THandle, TCapacity>[index];
    MVK_LOG("Acquiring index %d generation %d... ", index, generation);
    return Derived{index, generation};
  }

  void Release() const {
    const auto generation = generations<THandle, TCapacity>[handleIndex];
    MVK_ASSERT(generation != HandleGenerationCount && "Max handle generations reached.");
    ++generations<THandle, TCapacity>[handleIndex];
    freeIndexStack<THandle, TCapacity>.Push(handleIndex);
  }

  VkResult       Result() const { return states<TState, TCapacity>[handleIndex].result; }
  const VkString ResultName() const { return string_VkResult(Result()); }

  bool IsValid() const {
    return generations<THandle, TCapacity>[handleIndex] == handleGeneration;
  }

  operator THandle() const {
    MVK_ASSERT(IsValid());
    return *handle();
  }
};

struct PhysicalDevice;
struct PhysicalDeviceDesc;

/* Instance */
struct InstanceDesc {
  InstanceCreateInfo                                       createInfo;
  ValidationFeatures                                       validationFeatures;
  static_array_ptr<VkDebugUtilsMessageSeverityFlagBitsEXT> debugUtilsMessageSeverityFlags;
  static_array_ptr<VkDebugUtilsMessageTypeFlagBitsEXT>     debugUtilsMessageTypeFlags;
  const VkAllocationCallbacks*                             pAllocator{nullptr};
};
struct InstanceState {
  ApplicationInfo              applicationInfo;
  VkDebugUtilsMessengerEXT     debugUtilsMessengerEXT;
  const VkAllocationCallbacks* pAllocator{nullptr};
  VkResult                     result{VK_NOT_READY};
};
struct Instance : HandleBase<Instance, VkInstance, InstanceState, 1> {
  const VkAllocationCallbacks* DefaultAllocator(const VkAllocationCallbacks* pAllocator);
  static Instance              Create(const InstanceDesc&& desc);
  void                         Destroy();
  PhysicalDevice               CreatePhysicalDevice(const PhysicalDeviceDesc&& desc);
};

struct LogicalDevice;
struct LogicalDeviceDesc;

/* PhysicalDevice */
enum class Support {
  Optional,
  Yes,
  No,
};
VkString string_Support(Support support);

struct PhysicalDeviceDesc {
  uint32_t                                       preferredDeviceIndex;
  VkPhysicalDeviceFeatures2                      physicalDeviceFeatures;
  VkPhysicalDeviceVulkan11Features               physicalDeviceFeatures11;
  VkPhysicalDeviceVulkan12Features               physicalDeviceFeatures12;
  VkPhysicalDeviceVulkan13Features               physicalDeviceFeatures13;
  VkPhysicalDeviceMeshShaderFeaturesEXT          physicalDeviceMeshShaderFeatures;
  VkPhysicalDeviceRobustness2FeaturesEXT         physicalDeviceRobustness2Features;
  VkPhysicalDeviceGlobalPriorityQueryFeaturesEXT physicalDeviceGlobalPriorityQueryFeatures;
};
struct FindQueueDesc {
  Support      graphics;
  Support      compute;
  Support      transfer;
  Support      globalPriority;
  VkSurfaceKHR present;
};

struct PhysicalDeviceState {
  constexpr static VkCount QueueFamilyCountCapacity{6};

  Instance                                       instance;
  VkPhysicalDeviceProperties2                    physicalDeviceProperties;
  VkPhysicalDeviceSubgroupProperties             physicalDeviceSubgroupProperties;
  VkPhysicalDeviceMeshShaderPropertiesEXT        physicalDeviceMeshShaderProperties;
  VkPhysicalDeviceMemoryProperties               physicalDeviceMemoryProperties;
  VkPhysicalDeviceFeatures2                      physicalDeviceFeatures;
  VkPhysicalDeviceVulkan11Features               physicalDeviceFeatures11;
  VkPhysicalDeviceVulkan12Features               physicalDeviceFeatures12;
  VkPhysicalDeviceVulkan13Features               physicalDeviceFeatures13;
  VkPhysicalDeviceMeshShaderFeaturesEXT          physicalDeviceMeshShaderFeatures;
  VkPhysicalDeviceRobustness2FeaturesEXT         physicalDeviceRobustness2Features;
  VkPhysicalDeviceGlobalPriorityQueryFeaturesEXT physicalDeviceGlobalPriorityQueryFeatures;
  VkCount                                        queueFamilyCount;
  VkQueueFamilyProperties2                       queueFamilyProperties[QueueFamilyCountCapacity];
  VkQueueFamilyGlobalPriorityPropertiesEXT       queueFamilyGlobalPriorityProperties[QueueFamilyCountCapacity];
  VkResult                                       result{VK_NOT_READY};
};
struct PhysicalDevice : HandleBase<PhysicalDevice, VkPhysicalDevice, PhysicalDeviceState, 1> {
  void          Destroy();
  LogicalDevice CreateLogicalDevice(const LogicalDeviceDesc&& desc);
  uint32_t      FindQueueIndex(const FindQueueDesc&& desc);
};

struct ComputePipeline2;
struct ComputePipelineDesc;
struct PipelineLayout2;
struct PipelineLayoutDesc;

/* LogicalDevice */
struct LogicalDeviceDesc {
  const char*            debugName{"LogicalDevice"};
  DeviceCreateInfo       createInfo;
  VkAllocationCallbacks* pAllocator{nullptr};
};
struct LogicalDeviceState {
  PhysicalDevice               physicalDevice;
  const VkAllocationCallbacks* pAllocator{nullptr};
  VkResult                     result{VK_NOT_READY};
};
struct LogicalDevice : HandleBase<LogicalDevice, VkDevice, LogicalDeviceState, 1> {
  void                         Destroy();
  ComputePipeline2             CreateComputePipeline(const ComputePipelineDesc&& desc);
  PipelineLayout2              CreatePipelineLayout(const PipelineLayoutDesc&& desc);
  const VkAllocationCallbacks* DefaultAllocator(const VkAllocationCallbacks* pAllocator);
};

struct GenericHandleState {
  LogicalDevice                logicalDevice;
  const VkAllocationCallbacks* pAllocator{nullptr};
  VkResult                     result{VK_NOT_READY};
};

/* PipelineLayout */
struct PipelineLayoutDesc {
  const char*                  debugName{"PipelineLayout"};
  PipelineLayoutCreateInfo     createInfo;
  const VkAllocationCallbacks* pAllocator{nullptr};
};
struct PipelineLayout2 : HandleBase<PipelineLayout2, VkPipelineLayout, GenericHandleState, 32> {
  void Destroy();
};

/* ComputePipeline */
struct ComputePipelineDesc {
  const char*                                 debugName{"ComputePipeline"};
  static_array_ptr<ComputePipelineCreateInfo> createInfos;
  const VkAllocationCallbacks*                pAllocator{nullptr};
};
struct ComputePipeline2 : HandleBase<ComputePipeline2, VkPipeline, GenericHandleState, 32> {
  void Destroy();
  void BindPipeline(VkCommandBuffer commandBuffer) const;
};

template <typename T>
struct DeferDestroy {
  T create;
  ~ DeferDestroy() { create.Destroy(); }
};

//------------------------------------------------------------------------------------
// Functions Declaration
//------------------------------------------------------------------------------------

// void glGenerateTextureMipmap(	GLuint texture);

void CmdPipelineImageBarrier2(
    VkCommandBuffer              commandBuffer,
    uint32_t                     imageMemoryBarrierCount,
    const VkImageMemoryBarrier2* pImageMemoryBarriers);

inline VkResult
CreateDescriptorSetLayout(
    const VkDevice                       device,
    const DescriptorSetLayoutCreateInfo* pCreateInfo,
    VkDescriptorSetLayout*               pSetLayout) {
  return vkCreateDescriptorSetLayout(device, (VkDescriptorSetLayoutCreateInfo*)pCreateInfo, MVK_DEFAULT_ALLOCATOR, pSetLayout);
}

inline void
UpdateDescriptorSets(
    VkDevice                   device,
    uint32_t                   descriptorWriteCount,
    const WriteDescriptorSet*  pDescriptorWrites,
    uint32_t                   descriptorCopyCount = 0,
    const VkCopyDescriptorSet* pDescriptorCopies = nullptr) {
  vkUpdateDescriptorSets(device, descriptorWriteCount, (VkWriteDescriptorSet*)pDescriptorWrites, 0, nullptr);
}

inline VkResult
AllocateDescriptorSets(
    VkDevice                         device,
    const DescriptorSetAllocateInfo* pAllocateInfo,
    VkDescriptorSet*                 pDescriptorSets) {
  return vkAllocateDescriptorSets(device, (VkDescriptorSetAllocateInfo*)pAllocateInfo, pDescriptorSets);
}

} // namespace Mid::Vk
