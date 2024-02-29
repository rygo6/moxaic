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

#define VKM_ASSERT(command) assert(command == VK_SUCCESS)

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

namespace Mid::Vk {

struct {
#define MVK_PFN_FUNCTIONS \
  MVK_PFN_FUNCTION(SetDebugUtilsObjectNameEXT)

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

template <typename T>
struct Ptr {
  const T* data;

  constexpr Ptr(T&& value) : data{&value} {}
};

template <typename T>
struct Array {
  const uint32_t count;
  const T*       data;

  constexpr Array(std::initializer_list<T> l) : count{uint32_t(l.size())}, data{l.begin()} {}
};

template <typename T>
struct VkStruct {
  const T* ptr() const { return (T*)this; }
  operator const T&() const { return *(T*)this; };
};

struct ApplicationInfo : VkStruct<VkApplicationInfo> {
  VkStructureType sType{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  const void*     pNext{nullptr};
  const char*     pApplicationName{"Mid Vulkan Application"};
  uint32_t        applicationVersion{VK_MAKE_VERSION(1, 0, 0)};
  const char*     pEngineName{"Mid Vulkan"};
  uint32_t        engineVersion{VK_MAKE_VERSION(1, 0, 0)};
  uint32_t        apiVersion{VK_HEADER_VERSION_COMPLETE};
};

struct InstanceCreateInfo : VkStruct<VkInstanceCreateInfo> {
  VkStructureType       sType{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  const void*           pNext{nullptr};
  VkInstanceCreateFlags flags;
  Ptr<ApplicationInfo>  pApplicationInfo;
  Array<const char*>    pEnabledLayerNames;
  Array<const char*>    pEnabledExtensionNames;
};

struct ValidationFeatures : VkStruct<VkValidationFeaturesEXT> {
  VkStructureType                      sType{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
  const void*                          pNext{nullptr};
  Array<VkValidationFeatureEnableEXT>  pEnabledValidationFeatures;
  Array<VkValidationFeatureDisableEXT> pDisabledValidationFeatures;
};

struct DeviceCreateInfo : VkStruct<VkDeviceCreateInfo> {
  VkStructureType                sType{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  const void*                    pNext{nullptr};
  VkDeviceCreateFlags            flags{0};
  Array<VkDeviceQueueCreateInfo> queueCreateInfos;
  Array<const char*>             pEnabledLayerNames;
  Array<const char*>             pEnabledExtensionNames;
  Ptr<VkPhysicalDeviceFeatures>  pEnabledFeatures;
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

struct DebugUtilsObjectNameInfoEXT {
  const VkStructureType sType{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
  const void*           pNext{nullptr};
  const VkObjectType    objectType{};
  const uint64_t        objectHandle{NULL};
  const char*           pObjectName{"unnamed"};

  constexpr operator VkDebugUtilsObjectNameInfoEXT() const { return *(VkDebugUtilsObjectNameInfoEXT*)this; }
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

  void SetDebugInfo(VkDevice device) {
    const DebugUtilsObjectNameInfoEXT debugInfo{
        .objectType = VK_OBJECT_TYPE_SWAPCHAIN_KHR,
        .objectHandle = (uint64_t)handle,
        .pObjectName = "SwapChain",
    };
    PFN.SetDebugUtilsObjectNameEXT(device, (VkDebugUtilsObjectNameInfoEXT*)&debugInfo);
  }

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
    VKM_CHECK(vkCreateDescriptorSetLayout(vkDeviceHandle, &createInfo.vkHandle, MVK_DEFAULT_ALLOCATOR, &vkHandle));
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
    VKM_CHECK2(vkCreatePipelineLayout(device, (VkPipelineLayoutCreateInfo*)pCreateInfo, MVK_DEFAULT_ALLOCATOR, &handle));
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
    VKM_CHECK(vkCreateShaderModule(deviceHandle, (VkShaderModuleCreateInfo*)&createInfo, MVK_DEFAULT_ALLOCATOR, &handle));
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
#define MVK_HANDLE_COUNT 32
constexpr static MVK_HANDLE_INDEX_TYPE HandleIndexCapacity = (1 << 8 * sizeof(MVK_HANDLE_INDEX_TYPE)) - 1;
static_assert(MVK_HANDLE_COUNT <= 1 << (8 * sizeof(MVK_HANDLE_INDEX_TYPE)));

#define MVK_HANDLE_GENERATION_TYPE uint8_t
constexpr static MVK_HANDLE_GENERATION_TYPE HandleGenerationCount = (1 << 8 * sizeof(MVK_HANDLE_GENERATION_TYPE)) - 1;

#define MVK_HANDLE_METHOD inline __attribute__((always_inline))

template <typename T, T N>
struct Stack {
  T current{N - 1};
  T data[];

  T Pop() {
    assert(current != 0 && "Trying to pop stack below 0.");
    return data[current--];
  }

  void Push(T value) {
    assert(current != HandleIndexCapacity && "Tring to push stack above capacity.");
    data[++current] = value;
  }

  Stack() {
    assert(current <= HandleIndexCapacity && "Tring to create stack with capacity greater than type supports.");
    for (size_t i = 0; i < N; ++i) {
      data[i] = (T)i;
    }
  }
};

template <typename THandle, typename TState>
struct HandlePool {
  static inline MVK_HANDLE_GENERATION_TYPE                     generations[MVK_HANDLE_COUNT];
  static inline THandle                                        handles[MVK_HANDLE_COUNT];
  static inline TState                                         states[MVK_HANDLE_COUNT];
  static inline Stack<MVK_HANDLE_INDEX_TYPE, MVK_HANDLE_COUNT> freeIndexStack;
};

template <typename Derived, typename THandle, typename TState>
struct HandleBase {
  MVK_HANDLE_INDEX_TYPE      handleIndex;
  MVK_HANDLE_GENERATION_TYPE handleGeneration;

  THandle& Handle() const {
    assert(IsValid());
    return HandlePool<THandle, TState>::handles[handleIndex];
  }
  TState& State() {
    assert(IsValid());
    return HandlePool<THandle, TState>::states[handleIndex];
  }

  static Derived Acquire() {
    const auto index = HandlePool<THandle, TState>::freeIndexStack.Pop();
    const auto generation = HandlePool<THandle, TState>::generations[index];
    return Derived{index, generation};
  }

  void Release() const {
    const auto generation = HandlePool<THandle, TState>::generations[handleIndex];
    assert(generation != HandleGenerationCount && "Max handle generations reached.");
    ++HandlePool<THandle, TState>::generations[handleIndex];
    HandlePool<THandle, TState>::freeIndexStack.Push(handleIndex);
  }

  VkResult    Result() const { return HandlePool<THandle, TState>::states[handleIndex].result; }
  const char* ResultName() const { return string_VkResult(Result()); }

  bool IsValid() const {
    return HandlePool<THandle, TState>::generations[handleIndex] == handleGeneration;
  }

  operator THandle&() const {
    assert(IsValid());
    return Handle();
  }
};

/* Instance */
struct InstanceDesc {
  InstanceCreateInfo           createInfo;
  ValidationFeatures           validationFeatures;
  const VkAllocationCallbacks* pAllocator{};
};
struct InstanceState {
  const VkAllocationCallbacks* pInstanceAllocator{nullptr};
  VkResult                     result{VK_NOT_READY};
};

struct PhysicalDevice;
struct Instance : HandleBase<Instance, VkInstance, InstanceState> {
  static Instance Create(const InstanceDesc&& desc);

  void Destroy();

  PhysicalDevice CreatePhysicalDevice(const auto&& desc);

  const VkAllocationCallbacks* InstanceAllocator(const VkAllocationCallbacks* pAllocator);
};

/* PhysicalDevice */
struct PhysicalDeviceDesc {
  const char* debugName{"PhysicalDevice"};
};
struct PhysicalDeviceState {
  VkPhysicalDeviceProperties2             physicalDeviceProperties{};
  VkPhysicalDeviceMeshShaderPropertiesEXT pysicalDeviceMeshShaderProperties{};
  VkPhysicalDeviceSubgroupProperties      physicalDeviceSubgroupProperties{};
  VkPhysicalDeviceMemoryProperties        physicalDeviceMemoryProperties{};
  const VkAllocationCallbacks*            pDefaultAllocator{nullptr};
  VkResult                                result{VK_NOT_READY};
};

struct LogicalDevice;
struct PhysicalDevice : HandleBase<PhysicalDevice, VkPhysicalDevice, PhysicalDeviceState> {
  static PhysicalDevice Create(
      VkInstance  instance,
      const char* debugName);
  void Destroy();

  LogicalDevice CreateLogicalDevice(const auto&& desc);
};

/* LogicalDevice */
struct ComputePipeline2;
struct PipelineLayout2;

struct LocalDeviceDesc {
  const char*            debugName{"Device"};
  PhysicalDevice         physicalDevice{};
  DeviceCreateInfo       createInfo;
  VkAllocationCallbacks* pAllocator{nullptr};
};
struct LogicalDeviceState {
  PhysicalDevice               physicalDevice{};
  const VkAllocationCallbacks* pDefaultAllocator{nullptr};
  VkResult                     result{VK_NOT_READY};
};

struct LogicalDevice : HandleBase<LogicalDevice, VkDevice, LogicalDeviceState> {
  static LogicalDevice Create(
      PhysicalDevice               physicalDevice,
      const char*                  debugName,
      const VkDeviceCreateInfo*    pCreateInfo,
      const VkAllocationCallbacks* pAllocator);
  void Destroy();

  ComputePipeline2 CreateComputePipeline(const auto&& desc);
  PipelineLayout2  CreatePipelineLayout(const auto&& desc);

  const VkAllocationCallbacks* LogicalDeviceAllocator(const VkAllocationCallbacks* pAllocator);
};
// MVK_HANDLE_METHOD LogicalDevice PhysicalDevice::CreateLogicalDevice(const auto&& desc) {
//   return Create(desc.physicalDevice, desc.debugName, &desc.createInfo, desc.pAllocator);
// }

struct GenericHandleState {
  LogicalDevice                logicalDevice;
  const VkAllocationCallbacks* pAllocator{nullptr};
  VkResult                     result{VK_NOT_READY};
};

/* PipelineLayout */
struct PipelineLayoutDesc {
  const char*                  debugName{"PipelineLayout"};
  PipelineLayoutCreateInfo     createInfo{};
  const VkAllocationCallbacks* pAllocator{nullptr};
};

struct PipelineLayout2 : HandleBase<PipelineLayout2, VkPipelineLayout, GenericHandleState> {
  static PipelineLayout2 Create(
      LogicalDevice                   logicalDevice,
      const char*                     debugName,
      const PipelineLayoutCreateInfo* pCreateInfo,
      const VkAllocationCallbacks*    pAllocator);
  void Destroy();
};
MVK_HANDLE_METHOD PipelineLayout2 LogicalDevice::CreatePipelineLayout(const auto&& desc) {
  return PipelineLayout2::Create(*this, desc.debugName, &desc.createInfo, desc.pAllocator);
}

/* ComputePipeline */
template <uint32_t N0>
struct ComputePipelineDesc {
  const char*                  debugName{"ComputePipeline"};
  uint32_t                     createInfoCount{N0};
  ComputePipelineCreateInfo    createInfos[N0]{};
  const VkAllocationCallbacks* pAllocator{nullptr};
};

struct ComputePipeline2 : HandleBase<ComputePipeline2, VkPipeline, GenericHandleState> {
  static ComputePipeline2 Create(
      LogicalDevice                    logicalDevice,
      const char*                      debugName,
      uint32_t                         createInfoCount,
      const ComputePipelineCreateInfo* pCreateInfos,
      const VkAllocationCallbacks*     pAllocator);
  void Destroy();

  void BindPipeline(VkCommandBuffer commandBuffer) const;
};
MVK_HANDLE_METHOD ComputePipeline2 LogicalDevice::CreateComputePipeline(const auto&& desc) {
  return ComputePipeline2::Create(*this, desc.debugName, desc.createInfoCount, desc.createInfos, desc.pAllocator);
}

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
