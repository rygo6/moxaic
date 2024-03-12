/**********************************************************************************************
 *
 *   Mid Level Vulkan v1.0 - Highly Opiniated Vulkan Boilerplate API with Built-In Utilies
 *
 **********************************************************************************************/

#pragma once

#include <assert.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#ifndef MVK_ASSERT
#define MVK_ASSERT(command) assert(command)
#endif

#ifndef MVK_LOG
#define MVK_LOG(...) printf(__VA_ARGS__)
#endif

#ifndef MVK_PFN_FUNCTIONS
#define MVK_PFN_FUNCTIONS                      \
  MVK_PFN_FUNCTION(SetDebugUtilsObjectNameEXT) \
  MVK_PFN_FUNCTION(CreateDebugUtilsMessengerEXT)
#endif

#define MVK_DEFAULT_IMAGE_LAYOUT VK_IMAGE_LAYOUT_GENERAL
#define MVK_DEFAULT_DESCRIPTOR_TYPE VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
#define MVK_DEFAULT_SHADER_STAGE VK_SHADER_STAGE_COMPUTE_BIT
#define MVK_DEFAULT_ALLOCATOR nullptr

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

namespace Mid::Vk {

struct {
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

struct ptr_void {
  const void* p{nullptr};

  template <typename T>
  constexpr ptr_void(T&& value) : p{&value} {}
  constexpr ptr_void() = default;
};

template <typename T>
struct ptr {
  const T* p{nullptr};

  constexpr ptr() = default;
  constexpr ptr(T&& value) : p{&value} {}
  constexpr ptr(T* value) : p{value} {}
  constexpr ptr(std::initializer_list<T> l) : p{l.begin()} {}

  constexpr operator const T*() const { return this; };
  constexpr operator T*() const { return this; };
};

template <typename T>
struct span {
  const VkCount count{0};
  const T*      p{nullptr};

  constexpr span(std::initializer_list<T> l) : count{VkCount(l.size())}, p{l.begin()} {}
  constexpr span() = default;
};

// Yes you need this for some reason. I really hate.
#pragma pack(push, 4)
template <typename T>
struct span_pack {
  const VkCount count{0};
  const T*      p{nullptr};

  constexpr span_pack(std::initializer_list<T> l) : count{VkCount(l.size())}, p{l.begin()} {}
  constexpr span_pack() = default;
};
#pragma pack(pop)

enum class Support {
  Optional,
  Yes,
  No,
};
VkString string_Support(Support support);

template <typename T>
struct VkStruct {
  const T& vk() const { return *(T*)this; }
};

struct ApplicationInfo : VkStruct<VkApplicationInfo> {
  VkStructureType sType{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  ptr_void        pNext;
  VkString        pApplicationName{"Mid Vulkan Application"};
  uint32_t        applicationVersion;
  VkString        pEngineName{"Mid Vulkan"};
  uint32_t        engineVersion;
  uint32_t        apiVersion{VK_MAKE_API_VERSION(0, 1, 3, 0)};
};
static_assert(sizeof(ApplicationInfo) == sizeof(VkApplicationInfo));

struct InstanceCreateInfo : VkStruct<VkInstanceCreateInfo> {
  VkStructureType       sType{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  ptr_void              pNext;
  VkInstanceCreateFlags flags;
  ptr<ApplicationInfo>  pApplicationInfo;
  span<VkString>        pEnabledLayerNames;
  span<VkString>        pEnabledExtensionNames;
};
static_assert(sizeof(InstanceCreateInfo) == sizeof(VkInstanceCreateInfo));

struct ValidationFeatures : VkStruct<VkValidationFeaturesEXT> {
  VkStructureType                     sType{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
  ptr_void                            pNext;
  span<VkValidationFeatureEnableEXT>  pEnabledValidationFeatures;
  span<VkValidationFeatureDisableEXT> pDisabledValidationFeatures;
};
static_assert(sizeof(ValidationFeatures) == sizeof(VkValidationFeaturesEXT));

struct DebugUtilsMessengerCreateInfo : VkStruct<VkDebugUtilsMessengerCreateInfoEXT> {
  VkStructureType                      sType{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
  ptr_void                             pNext;
  VkDebugUtilsMessengerCreateFlagsEXT  flags;
  VkDebugUtilsMessageSeverityFlagsEXT  messageSeverity;
  VkDebugUtilsMessageTypeFlagsEXT      messageType;
  PFN_vkDebugUtilsMessengerCallbackEXT pfnUserCallback;
  ptr_void                             pUserData;
};
static_assert(sizeof(DebugUtilsMessengerCreateInfo) == sizeof(VkDebugUtilsMessengerCreateInfoEXT));

struct DeviceQueueCreateInfo : VkStruct<VkDeviceQueueCreateInfo> {
  VkStructureType          sType{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  ptr_void                 pNext;
  VkDeviceQueueCreateFlags flags;
  uint32_t                 queueFamilyIndex;
  uint32_t                 queueCount{1};
  ptr<float>               pQueuePriorities;
};
static_assert(sizeof(DeviceQueueCreateInfo) == sizeof(VkDeviceQueueCreateInfo));

struct DeviceCreateInfo : VkStruct<VkDeviceCreateInfo> {
  VkStructureType                  sType{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  ptr_void                         pNext;
  VkDeviceCreateFlags              flags;
  span_pack<DeviceQueueCreateInfo> pQueueCreateInfos;
  span<VkString>                   pEnabledLayerNames;
  span<VkString>                   pEnabledExtensionNames;
  ptr<VkPhysicalDeviceFeatures>    pEnabledFeatures;
};
static_assert(sizeof(DeviceCreateInfo) == sizeof(VkDeviceCreateInfo));

struct DeviceQueueGlobalPriorityCreateInfo : VkStruct<VkDeviceQueueGlobalPriorityCreateInfoKHR> {
  VkStructureType          sType{VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT};
  ptr_void                 pNext;
  VkQueueGlobalPriorityKHR globalPriority;
};
static_assert(sizeof(DeviceQueueGlobalPriorityCreateInfo) == sizeof(VkDeviceQueueGlobalPriorityCreateInfoKHR));

struct AttachmentDescription : VkStruct<VkAttachmentDescription> {
  VkAttachmentDescriptionFlags flags;
  VkFormat                     format;
  VkSampleCountFlagBits        samples{VK_SAMPLE_COUNT_1_BIT};
  VkAttachmentLoadOp           loadOp{VK_ATTACHMENT_LOAD_OP_CLEAR};
  VkAttachmentStoreOp          storeOp{VK_ATTACHMENT_STORE_OP_STORE};
  VkAttachmentLoadOp           stencilLoadOp{VK_ATTACHMENT_LOAD_OP_DONT_CARE};
  VkAttachmentStoreOp          stencilStoreOp{VK_ATTACHMENT_STORE_OP_DONT_CARE};
  VkImageLayout                initialLayout{VK_IMAGE_LAYOUT_UNDEFINED};
  VkImageLayout                finalLayout{VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL};
};
static_assert(sizeof(AttachmentDescription) == sizeof(VkAttachmentDescription));

struct AttachmentReference : VkStruct<VkAttachmentReference> {
  uint32_t      attachment;
  VkImageLayout layout{VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL};
};
static_assert(sizeof(AttachmentReference) == sizeof(VkAttachmentReference));

struct SubpassDescription : VkStruct<VkSubpassDescription> {
  VkSubpassDescriptionFlags flags;
  VkPipelineBindPoint       pipelineBindPoint{VK_PIPELINE_BIND_POINT_GRAPHICS};
  span<AttachmentReference> pInputAttachments;
  span<AttachmentReference> pColorAttachments;
  ptr<AttachmentReference>  pResolveAttachments;
  ptr<AttachmentReference>  pDepthStencilAttachment;
  span<uint32_t>            pPreserveAttachments;
};
static_assert(sizeof(SubpassDescription) == sizeof(VkSubpassDescription));

struct SubpassDependency : VkStruct<VkSubpassDependency> {
  // needs defaults, but ive not used yet
  uint32_t             srcSubpass;
  uint32_t             dstSubpass;
  VkPipelineStageFlags srcStageMask;
  VkPipelineStageFlags dstStageMask;
  VkAccessFlags        srcAccessMask;
  VkAccessFlags        dstAccessMask;
  VkDependencyFlags    dependencyFlags;
};
static_assert(sizeof(SubpassDependency) == sizeof(VkSubpassDependency));

struct RenderPassCreateInfo : VkStruct<VkRenderPassCreateInfo> {
  VkStructureType                  sType{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  ptr_void                         pNext;
  VkRenderPassCreateFlags          flags;
  span_pack<AttachmentDescription> pAttachments;
  span<SubpassDescription>         pSubpasses;
  span<SubpassDependency>          pDependencies;
};
static_assert(sizeof(RenderPassCreateInfo) == sizeof(VkRenderPassCreateInfo));

struct CommandPoolCreateInfo : VkStruct<VkCommandPoolCreateInfo> {
  VkStructureType          sType{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  ptr_void                 pNext;
  VkCommandPoolCreateFlags flags;
  uint32_t                 queueFamilyIndex;
};
static_assert(sizeof(CommandPoolCreateInfo) == sizeof(VkCommandPoolCreateInfo));

struct CommandBufferAllocateInfo : VkStruct<VkCommandBufferAllocateInfo> {
  VkStructureType      sType{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  ptr_void             pNext;
  VkCommandPool        commandPool{VK_NULL_HANDLE};
  VkCommandBufferLevel level{VK_COMMAND_BUFFER_LEVEL_PRIMARY};
  uint32_t             commandBufferCount{1};
};
static_assert(sizeof(CommandBufferAllocateInfo) == sizeof(VkCommandBufferAllocateInfo));

struct DescriptorSetLayoutBinding : VkStruct<VkDescriptorSetLayoutBinding> {
  uint32_t           binding;
  VkDescriptorType   descriptorType{MVK_DEFAULT_DESCRIPTOR_TYPE};
  uint32_t           descriptorCount{1};
  VkShaderStageFlags stageFlags{MVK_DEFAULT_SHADER_STAGE};
  ptr<VkSampler>     pImmutableSamplers;

  DescriptorSetLayoutBinding WithSamplers(VkSampler* pImmutableSamplers) const {
    return {
        .binding = binding,
        .descriptorType = descriptorType,
        .descriptorCount = descriptorCount,
        .stageFlags = stageFlags,
        .pImmutableSamplers = pImmutableSamplers};
  }
};
static_assert(sizeof(DescriptorSetLayoutBinding) == sizeof(VkDescriptorSetLayoutBinding));

struct QueryPoolCreateInfo : VkStruct<VkQueryPoolCreateInfo> {
  VkStructureType               sType{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
  ptr_void                      pNext;
  VkQueryPoolCreateFlags        flags;
  VkQueryType                   queryType;
  uint32_t                      queryCount;
  VkQueryPipelineStatisticFlags pipelineStatistics;
};
static_assert(sizeof(QueryPoolCreateInfo) == sizeof(VkQueryPoolCreateInfo));

struct DescriptorPoolCreateInfo : VkStruct<VkDescriptorPoolCreateInfo> {
  VkStructureType             sType{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  ptr_void                    pNext;
  VkDescriptorPoolCreateFlags flags{VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT};
  uint32_t                    maxSets;
  span<VkDescriptorPoolSize>  pPoolSizes;
};
static_assert(sizeof(DescriptorPoolCreateInfo) == sizeof(VkDescriptorPoolCreateInfo));

struct SamplerCreateInfo : VkStruct<VkSamplerCreateInfo> {
  VkStructureType      sType{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  ptr_void             pNext;
  VkSamplerCreateFlags flags;
  VkFilter             magFilter{VK_FILTER_LINEAR};
  VkFilter             minFilter{VK_FILTER_LINEAR};
  VkSamplerMipmapMode  mipmapMode{VK_SAMPLER_MIPMAP_MODE_LINEAR};
  VkSamplerAddressMode addressModeU{VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};
  VkSamplerAddressMode addressModeV{VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};
  VkSamplerAddressMode addressModeW{VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};
  float                mipLodBias;
  VkBool32             anisotropyEnable;
  float                maxAnisotropy;
  VkBool32             compareEnable;
  VkCompareOp          compareOp;
  float                minLod;
  float                maxLod{16.0f};
  VkBorderColor        borderColor{VK_BORDER_COLOR_INT_OPAQUE_BLACK};
  VkBool32             unnormalizedCoordinates;
};
static_assert(sizeof(DescriptorPoolCreateInfo) == sizeof(VkDescriptorPoolCreateInfo));

struct SamplerReductionModeCreateInfo : VkStruct<VkSamplerReductionModeCreateInfo> {
  VkStructureType        sType{VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT};
  ptr_void               pNext;
  VkSamplerReductionMode reductionMode;
};
static_assert(sizeof(SamplerReductionModeCreateInfo) == sizeof(VkSamplerReductionModeCreateInfo));

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

struct DescriptorSetLayoutCreateInfo : VkStruct<VkDescriptorSetLayoutCreateInfo> {
  VkStructureType                       sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  ptr_void                              pNext;
  VkDescriptorSetLayoutCreateFlags      flags;
  span_pack<DescriptorSetLayoutBinding> pBindings;
};
static_assert(sizeof(DescriptorSetLayoutCreateInfo) == sizeof(VkDescriptorSetLayoutCreateInfo));

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
                                          &createInfo.vk(),
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
    VKM_CHECK(vkCreatePipelineLayout(device,
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

typedef uint64_t Handle;
typedef uint16_t HandleIndex; // 65,536 vulkan handles. Could you possibly use more?
typedef uint8_t  HandleGeneration; // Only 256 generations. They roll over. I guess it's like a generation ring.

template <typename Derived, typename THandle, typename TState>
struct HandleBase {
  HandleIndex      handleIndex{};
  HandleGeneration handleGeneration{};

  THandle*       pHandle();
  TState*        pState();
  const THandle& handle() const;
  const TState&  state() const;
  static Derived Acquire();
  void           Release() const;
  VkResult       Result() const;
  bool           IsValid() const;
};

struct PhysicalDevice;
struct PhysicalDeviceDesc;

/* Instance */
struct InstanceDesc {
  VkString                      debugName{"Instance"};
  InstanceCreateInfo            createInfo;
  ValidationFeatures            validationFeatures;
  DebugUtilsMessengerCreateInfo debugUtilsMessengerCreateInfo;
  const VkAllocationCallbacks*  pAllocator{nullptr};
};
struct InstanceState {
  ApplicationInfo              applicationInfo;
  VkDebugUtilsMessengerEXT     debugUtilsMessengerEXT;
  const VkAllocationCallbacks* pAllocator{nullptr};
  VkResult                     result{VK_NOT_READY};
};
struct Instance : HandleBase<Instance, VkInstance, InstanceState> {
  const VkAllocationCallbacks* DefaultAllocator(const VkAllocationCallbacks* pAllocator);
  static Instance              Create(InstanceDesc&& desc);
  void                         Destroy();
  PhysicalDevice               CreatePhysicalDevice(const PhysicalDeviceDesc&& desc);
};

struct LogicalDevice;
struct LogicalDeviceDesc;
struct QueueIndexDesc;

/* PhysicalDevice */
struct PhysicalDeviceDesc {
  VkString                                       debugName{"PhysicalDevice"};
  VkPhysicalDeviceFeatures2                      physicalDeviceFeatures;
  VkPhysicalDeviceVulkan11Features               physicalDeviceFeatures11;
  VkPhysicalDeviceVulkan12Features               physicalDeviceFeatures12;
  VkPhysicalDeviceVulkan13Features               physicalDeviceFeatures13;
  VkPhysicalDeviceMeshShaderFeaturesEXT          physicalDeviceMeshShaderFeatures;
  VkPhysicalDeviceRobustness2FeaturesEXT         physicalDeviceRobustness2Features;
  VkPhysicalDeviceGlobalPriorityQueryFeaturesEXT physicalDeviceGlobalPriorityQueryFeatures;
};
struct PhysicalDeviceState {
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
  constexpr static VkCount                       QueueFamilyCountCapacity{6};
  VkCount                                        queueFamilyCount;
  VkQueueFamilyProperties2                       queueFamilyProperties[QueueFamilyCountCapacity];
  VkQueueFamilyGlobalPriorityPropertiesEXT       queueFamilyGlobalPriorityProperties[QueueFamilyCountCapacity];
  VkResult                                       result{VK_NOT_READY};
};
struct PhysicalDevice : HandleBase<PhysicalDevice, VkPhysicalDevice, PhysicalDeviceState> {
  void          Destroy();
  LogicalDevice CreateLogicalDevice(LogicalDeviceDesc&& desc);
  uint32_t      FindQueueIndex(QueueIndexDesc&& desc);
};

struct ComputePipelineDesc;
struct ComputePipeline2;
struct PipelineLayoutDesc;
struct PipelineLayout2;
struct RenderPassDesc;
struct RenderPass;
struct CommandPoolDesc;
struct CommandPool;
struct QueueDesc;
struct Queue;
struct QueryPoolDesc;
struct QueryPool;
struct DescriptorPoolDesc;
struct DescriptorPool;
struct SamplerDesc;
struct Sampler;

/* LogicalDevice */
struct LogicalDeviceDesc {
  VkString               debugName{"LogicalDevice"};
  DeviceCreateInfo       createInfo;
  VkAllocationCallbacks* pAllocator{nullptr};
};
struct LogicalDeviceState {
  PhysicalDevice               physicalDevice;
  const VkAllocationCallbacks* pAllocator{nullptr};
  VkResult                     result{VK_NOT_READY};
};
struct LogicalDevice : HandleBase<LogicalDevice, VkDevice, LogicalDeviceState> {
  void Destroy();

  RenderPass       CreateRenderPass(RenderPassDesc&& desc) const;
  Queue            GetQueue(QueueDesc&& desc) const;
  CommandPool      CreateCommandPool(CommandPoolDesc&& desc) const;
  QueryPool        CreateQueryPool(QueryPoolDesc&& desc) const;
  DescriptorPool   CreateDescriptorPool(DescriptorPoolDesc&& desc) const;
  Sampler          CreateSampler(SamplerDesc&& desc);
  ComputePipeline2 CreateComputePipeline(ComputePipelineDesc&& desc);
  PipelineLayout2  CreatePipelineLayout(PipelineLayoutDesc&& desc);

  const VkAllocationCallbacks* DefaultAllocator(const VkAllocationCallbacks* pAllocator) const;
};

/* Queue */
struct QueueIndexDesc {
  Support graphics{Support::Optional};
  Support compute{Support::Optional};
  Support transfer{Support::Optional};
  Support globalPriority{Support::Optional};

  VkSurfaceKHR present{VK_NULL_HANDLE};
};
struct QueueDesc {
  const char* debugName{"Queue"};
  uint32_t    queueIndex;
};
struct QueueState {
  LogicalDevice logicalDevice;
};
struct Queue : HandleBase<Queue, VkQueue, QueueState> {
  void Destroy();
};

/* RenderPass */
struct RenderPassDesc {
  const char*            debugName{"RenderPass"};
  RenderPassCreateInfo   createInfo;
  VkAllocationCallbacks* pAllocator{nullptr};
};
struct RenderPassState {
  LogicalDevice                logicalDevice;
  const VkAllocationCallbacks* pAllocator{nullptr};
  VkResult                     result{VK_NOT_READY};
};
struct RenderPass : HandleBase<RenderPass, VkRenderPass, RenderPassState> {
  void Destroy();
};

/* CommandPool */
struct CommandBufferDesc;
struct CommandBuffer;
struct CommandPoolDesc {
  const char*                  debugName{"CommandPool"};
  CommandPoolCreateInfo        createInfo;
  const VkAllocationCallbacks* pAllocator{nullptr};
};
struct CommandPoolState {
  LogicalDevice                logicalDevice;
  const VkAllocationCallbacks* pAllocator{nullptr};
  VkResult                     result{VK_NOT_READY};
};
struct CommandPool : HandleBase<CommandPool, VkCommandPool, CommandPoolState> {
  CommandBuffer AllocateCommandBuffer(CommandBufferDesc&& desc);
  void          Destroy();
};

/* CommandBuffer */
struct CommandBufferDesc {
  const char*               debugName{"CommandBuffer"};
  CommandBufferAllocateInfo allocateInfo;
};
struct CommandBufferState {
  CommandPool commandPool;
  VkResult    result{VK_NOT_READY};
};
struct CommandBuffer : HandleBase<CommandBuffer, VkCommandBuffer, CommandBufferState> {
  void Destroy();
};

/* QueryPool */
struct QueryPoolDesc {
  const char*                  debugName{"QueryPool"};
  QueryPoolCreateInfo          createInfo;
  const VkAllocationCallbacks* pAllocator{nullptr};
};
struct QueryPoolState {
  LogicalDevice                logicalDevice;
  const VkAllocationCallbacks* pAllocator{nullptr};
  VkResult                     result{VK_NOT_READY};
};
struct QueryPool : HandleBase<QueryPool, VkQueryPool, QueryPoolState> {
  void Destroy();
};

/* DescriptorPool */
struct DescriptorPoolDesc {
  const char*                  debugName{"DescriptorPool"};
  DescriptorPoolCreateInfo     createInfo;
  const VkAllocationCallbacks* pAllocator{nullptr};
};
struct DescriptorPoolState {
  LogicalDevice                logicalDevice;
  const VkAllocationCallbacks* pAllocator{nullptr};
  VkResult                     result{VK_NOT_READY};
};
struct DescriptorPool : HandleBase<DescriptorPool, VkDescriptorPool, DescriptorPoolState> {
  void Destroy();
};

/* Sampler */
struct SamplerDesc {
  const char*                         debugName{"Sampler"};
  SamplerCreateInfo                   createInfo;
  ptr<SamplerReductionModeCreateInfo> pReductionModeCreateInfo;
  const VkAllocationCallbacks*        pAllocator{nullptr};
};
struct SamplerState {
  LogicalDevice                logicalDevice;
  const VkAllocationCallbacks* pAllocator{nullptr};
  VkResult                     result{VK_NOT_READY};
};
struct Sampler : HandleBase<Sampler, VkSampler, SamplerState> {
  void Destroy();
};

/* PipelineLayout */
struct PipelineLayoutDesc {
  const char*                  debugName{"PipelineLayout"};
  PipelineLayoutCreateInfo     createInfo;
  const VkAllocationCallbacks* pAllocator{nullptr};
};
struct PipelineLayoutState {
  LogicalDevice                logicalDevice;
  const VkAllocationCallbacks* pAllocator{nullptr};
  VkResult                     result{VK_NOT_READY};
};
struct PipelineLayout2 : HandleBase<PipelineLayout2, VkPipelineLayout, PipelineLayoutState> {
  void Destroy();
};

/* ComputePipeline */
struct ComputePipelineDesc {
  const char*                     debugName{"ComputePipeline"};
  span<ComputePipelineCreateInfo> createInfos;
  const VkAllocationCallbacks*    pAllocator{nullptr};
};
struct ComputePipelineState {
  LogicalDevice                logicalDevice;
  const VkAllocationCallbacks* pAllocator{nullptr};
  VkResult                     result{VK_NOT_READY};
};
struct ComputePipeline2 : HandleBase<ComputePipeline2, VkPipeline, ComputePipelineState> {
  void Destroy();
  void BindPipeline(VkCommandBuffer commandBuffer);
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
