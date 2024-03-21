/**********************************************************************************************
 *
 *   Mid Level Vulkan v1.0 - Highly Opiniated Vulkan Boilerplate API with Built-In Utilies
 *
 **********************************************************************************************/

#pragma once

#include <assert.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#ifdef WIN32
#include <windows.h>

#include <vulkan/vulkan_win32.h>

#define MVK_EXTERNAL_MEMORY_HANDLE VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR
#endif

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

typedef const char* VkString;
typedef uint32_t    VkCount;

// do I want to do something this to enforce literals on debug names?
// struct literal_name {
//   const VkString p{"N/A"};
//   constexpr      literal_name(VkString&& value) : p{value} {}
// };

struct ptr_void {
  const void* p{nullptr};

  template <typename T>
  constexpr ptr_void(T&& value) : p{&value} {}
  constexpr ptr_void() = default;
};

template <typename T>
struct ptr {
  const T* p{nullptr};

  constexpr ptr(T&& value) : p{&value} {}
  constexpr ptr(T* value) : p{value} {}
  constexpr ptr(std::initializer_list<T>&& l) : p{l.begin()} {}
  constexpr ptr() = default;

  constexpr operator const T*() const { return this; };
  constexpr operator T*() const { return this; };
};

template <typename T>
struct span {
  const VkCount count{0};
  const T*      p{nullptr};

  constexpr span(std::initializer_list<T>&& l) : count{VkCount(l.size())}, p{l.begin()} {}
  constexpr span() = default;
};

// Yes you need this for some reason. I really hate.
#pragma pack(push, 4)
template <typename T>
struct span_pack {
  const VkCount count{0};
  const T*      p{nullptr};

  constexpr span_pack(std::initializer_list<T>&& l) : count{VkCount(l.size())}, p{l.begin()} {}
  constexpr span_pack() = default;
};
#pragma pack(pop)

enum class Support : uint8_t {
  Optional,
  Yes,
  No,
};
VkString string_Support(Support support);

enum class Locality : uint8_t {
  Local,
  External,
  Imported,
};
VkString string_Locality(Locality locality);

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

struct ImageCreateInfo : VkStruct<VkImageCreateInfo> {
  VkStructureType       sType{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  ptr_void              pNext;
  VkImageCreateFlags    flags;
  VkImageType           imageType{VK_IMAGE_TYPE_2D};
  VkFormat              format;
  VkExtent3D            extent;
  uint32_t              mipLevels{1};
  uint32_t              arrayLayers{1};
  VkSampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT};
  VkImageTiling         tiling{VK_IMAGE_TILING_OPTIMAL};
  VkImageUsageFlags     usage;
  VkSharingMode         sharingMode{VK_SHARING_MODE_EXCLUSIVE};
  span<const uint32_t>  pQueueFamilyIndices;
  VkImageLayout         initialLayout{VK_IMAGE_LAYOUT_UNDEFINED};
};
static_assert(sizeof(ImageCreateInfo) == sizeof(VkImageCreateInfo));

struct ExternalMemoryImageCreateInfo : VkStruct<VkExternalMemoryImageCreateInfo> {
  VkStructureType                 sType{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};
  ptr_void                        pNext;
  VkExternalMemoryHandleTypeFlags handleTypes;
};
static_assert(sizeof(ExternalMemoryImageCreateInfo) == sizeof(VkExternalMemoryImageCreateInfo));

struct ImageSubresourceRange : VkStruct<VkImageSubresourceRange> {
  VkImageAspectFlags aspectMask{VK_IMAGE_ASPECT_COLOR_BIT};
  uint32_t           baseMipLevel;
  uint32_t           levelCount; // leave this zero, so you could set it to one to make imageview per mip
  uint32_t           baseArrayLayer;
  uint32_t           layerCount{1};
};
static_assert(sizeof(ImageSubresourceRange) == sizeof(VkImageSubresourceRange));

struct ImageViewCreateInfo : VkStruct<VkImageViewCreateInfo> {
  VkStructureType        sType{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  ptr_void               pNext;
  VkImageViewCreateFlags flags;
  VkImage                image;
  VkImageViewType        viewType{VK_IMAGE_VIEW_TYPE_2D};
  VkFormat               format;
  VkComponentMapping     components;
  ImageSubresourceRange  subresourceRange;
};
static_assert(sizeof(ImageViewCreateInfo) == sizeof(VkImageViewCreateInfo));

struct MemoryAllocateInfo : VkStruct<VkMemoryAllocateInfo> {
  VkStructureType sType{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  ptr_void        pNext;
  VkDeviceSize    allocationSize;
  uint32_t        memoryTypeIndex;
};
static_assert(sizeof(MemoryAllocateInfo) == sizeof(VkMemoryAllocateInfo));

struct ExportMemoryAllocateInfo : VkStruct<VkExportMemoryAllocateInfo> {
  VkStructureType                 sType{VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO};
  ptr_void                        pNext;
  VkExternalMemoryHandleTypeFlags handleTypes{MVK_EXTERNAL_MEMORY_HANDLE};
};
static_assert(sizeof(ExportMemoryAllocateInfo) == sizeof(VkExportMemoryAllocateInfo));

#ifdef WIN32
struct ImportMemoryWin32HandleInfo : VkStruct<VkImportMemoryWin32HandleInfoKHR> {
  VkStructureType                    sType{VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR};
  ptr_void                           pNext;
  VkExternalMemoryHandleTypeFlagBits handleType{MVK_EXTERNAL_MEMORY_HANDLE};
  HANDLE                             handle;
  LPCWSTR                            name;
};
static_assert(sizeof(ImportMemoryWin32HandleInfo) == sizeof(VkImportMemoryWin32HandleInfoKHR));
#endif

struct SwapchainCreateInfo : VkStruct<VkSwapchainCreateInfoKHR> {
  VkStructureType               sType{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  ptr_void                      pNext;
  VkSwapchainCreateFlagsKHR     flags;
  VkSurfaceKHR                  surface;
  uint32_t                      minImageCount{2};
  VkFormat                      imageFormat;
  VkColorSpaceKHR               imageColorSpace;
  VkExtent2D                    imageExtent;
  uint32_t                      imageArrayLayers{1};
  VkImageUsageFlags             imageUsage{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};
  VkSharingMode                 imageSharingMode{VK_SHARING_MODE_EXCLUSIVE};
  span<const uint32_t>          pQueueFamilyIndices;
  VkSurfaceTransformFlagBitsKHR preTransform{VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR};
  VkCompositeAlphaFlagBitsKHR   compositeAlpha{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR};
  VkPresentModeKHR              presentMode{VK_PRESENT_MODE_FIFO_KHR};
  VkBool32                      clipped{VK_TRUE};
  VkSwapchainKHR                oldSwapchain;
};
static_assert(sizeof(SwapchainCreateInfo) == sizeof(VkSwapchainCreateInfoKHR));

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

template <typename T>
struct DeferredHandle {
  T data;
    DeferredHandle(T&& data) : data(data) {}
  ~ DeferredHandle() { data.Destroy(); }
};

typedef uint64_t Handle;
typedef uint16_t HandleIndex;      // 65,536 vulkan handles. Could you possibly use more?
typedef uint8_t  HandleGeneration; // Only 256 generations. They roll over. I guess it's like a generation ring.

template <typename Derived, typename THandle, typename TState>
struct HandleBase {
  HandleIndex      handleIndex;
  HandleGeneration handleGeneration;

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
struct Surface;
struct SurfaceDesc;

/* Instance */
struct InstanceDesc {
  VkString                      debugName{"Instance"};
  InstanceCreateInfo            createInfo;
  ValidationFeatures            validationFeatures;
  DebugUtilsMessengerCreateInfo debugUtilsMessengerCreateInfo;
  const VkAllocationCallbacks*  pAllocator;
};
struct InstanceState {
  ApplicationInfo              applicationInfo;
  VkDebugUtilsMessengerEXT     debugUtilsMessengerEXT;
  const VkAllocationCallbacks* pAllocator;
  VkResult                     result{VK_NOT_READY};
};
struct Instance : HandleBase<Instance, VkInstance, InstanceState> {
  static Instance Create(InstanceDesc&& desc);
  void            Destroy();
  Surface         CreateSurface(SurfaceDesc&& desc);
  PhysicalDevice  CreatePhysicalDevice(const PhysicalDeviceDesc&& desc) const;

  const VkAllocationCallbacks* DefaultAllocator(const VkAllocationCallbacks* pAllocator);
};

struct LogicalDevice;
struct LogicalDeviceDesc;
struct QueueIndexDesc;
struct SwapchainDesc;
struct Swapchain;

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

/* Surface */
struct SurfaceDesc {
  VkString                     debugName{"PhysicalDevice"};
  const VkAllocationCallbacks* pAllocator;
};
struct SurfaceState {
  Instance                     instance;
  const VkAllocationCallbacks* pAllocator;
  VkResult                     result{VK_NOT_READY};
};
struct Surface : HandleBase<Surface, VkSurfaceKHR, SurfaceState> {
  void Destroy() const;
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
struct ImageDesc;
struct Image;
struct DeviceMemoryDesc;
struct DeviceMemory;

/* LogicalDevice */
struct LogicalDeviceDesc {
  VkString               debugName{"LogicalDevice"};
  DeviceCreateInfo       createInfo;
  VkAllocationCallbacks* pAllocator;
};
struct LogicalDeviceState {
  PhysicalDevice               physicalDevice;
  const VkAllocationCallbacks* pAllocator;
  VkResult                     result{VK_NOT_READY};

  // could do something like creating lists of things created to then destroy
  // LogicalDevice* pDestroyLogicalDevice;
  // RenderPass*    pDestroyRenderPass;
  // CommandPool*   pDestroyCommandPool;
};
struct LogicalDevice : HandleBase<LogicalDevice, VkDevice, LogicalDeviceState> {
  void Destroy() const;

  RenderPass     CreateRenderPass(RenderPassDesc&& desc) const;
  Queue          GetQueue(QueueDesc&& desc) const;
  CommandPool    CreateCommandPool(CommandPoolDesc&& desc) const;
  QueryPool      CreateQueryPool(QueryPoolDesc&& desc) const;
  DescriptorPool CreateDescriptorPool(DescriptorPoolDesc&& desc) const;
  Sampler        CreateSampler(SamplerDesc&& desc) const;
  Image          CreateImage(ImageDesc&& desc) const;
  DeviceMemory   AllocateMemory(DeviceMemoryDesc&& desc) const;

  Swapchain CreateSwapchain(SwapchainDesc&& desc) const;

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
  VkResult      result{VK_NOT_READY};
};
struct Queue : HandleBase<Queue, VkQueue, QueueState> {
  void Destroy() const;
};

/* RenderPass */
struct RenderPassDesc {
  const char*            debugName{"RenderPass"};
  RenderPassCreateInfo   createInfo;
  VkAllocationCallbacks* pAllocator;
};
struct RenderPassState {
  LogicalDevice                logicalDevice;
  const VkAllocationCallbacks* pAllocator;
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
  const VkAllocationCallbacks* pAllocator;
};
struct CommandPoolState {
  LogicalDevice                logicalDevice;
  const VkAllocationCallbacks* pAllocator;
  VkResult                     result{VK_NOT_READY};
};
struct CommandPool : HandleBase<CommandPool, VkCommandPool, CommandPoolState> {
  CommandBuffer AllocateCommandBuffer(CommandBufferDesc&& desc) const;
  void          Destroy() const;
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
  const VkAllocationCallbacks* pAllocator;
};
struct QueryPoolState {
  LogicalDevice                logicalDevice;
  const VkAllocationCallbacks* pAllocator;
  VkResult                     result{VK_NOT_READY};
};
struct QueryPool : HandleBase<QueryPool, VkQueryPool, QueryPoolState> {
  void Destroy();
};

/* DescriptorPool */
struct DescriptorPoolDesc {
  const char*                  debugName{"DescriptorPool"};
  DescriptorPoolCreateInfo     createInfo;
  const VkAllocationCallbacks* pAllocator;
};
struct DescriptorPoolState {
  LogicalDevice                logicalDevice;
  const VkAllocationCallbacks* pAllocator{};
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
  const VkAllocationCallbacks*        pAllocator;
};
struct SamplerState {
  LogicalDevice                logicalDevice;
  const VkAllocationCallbacks* pAllocator;
  VkResult                     result{VK_NOT_READY};
};
struct Sampler : HandleBase<Sampler, VkSampler, SamplerState> {
  void Destroy();
};

/* DeviceMemory */
struct DeviceMemoryDesc {
  const char*                  debugName{"DeviceMemory"};
  MemoryAllocateInfo           allocateInfo;
  Image*                       pBindImage;
  const VkAllocationCallbacks* pAllocator;
};
struct DeviceMemoryState {
  LogicalDevice                logicalDevice;
  VkDeviceSize                 currentBindOffset;
  VkDeviceSize                 allocationSize;
  uint32_t                     memoryTypeIndex;
  const VkAllocationCallbacks* pAllocator;
  VkResult                     result{VK_NOT_READY};
};
struct DeviceMemory : HandleBase<DeviceMemory, VkDeviceMemory, DeviceMemoryState> {
  // void BindImage(Image image, VkDeviceSize memoryOffset = 0);
  void Destroy();
};

struct ImageViewDesc;
struct ImageView;

/* Image */
struct ImageDesc {
  const char*                    debugName{"Image"};
  ImageCreateInfo                createInfo;
  VkMemoryPropertyFlags          memoryPropertyFlags{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
  ExternalMemoryImageCreateInfo* pExternalMemoryImageCreateInfo;
  ImportMemoryWin32HandleInfo*   pImportMemoryWin32HandleInfo;
  const VkAllocationCallbacks*   pAllocator;
};
struct ImageState {
  LogicalDevice logicalDevice;

  DeviceMemory deviceMemory;
  VkDeviceSize deviceMemoryOffset;

  Locality locality;

  VkImageType           imageType;
  VkFormat              format;
  VkExtent3D            extent;
  uint32_t              mipLevels;
  uint32_t              arrayLayers;
  VkSampleCountFlagBits samples;
  VkImageUsageFlags     usage;

  VkMemoryRequirements  memoryRequirements;
  VkMemoryPropertyFlags memoryPropertyFlags;
  uint32_t              memoryTypeIndex;

  VkExternalMemoryHandleTypeFlags externalMemoryHandleTypeFlags;
  HANDLE                          externalHandle;

  const VkAllocationCallbacks* pAllocator;
  VkResult                     result{VK_NOT_READY};
};
struct Image : HandleBase<Image, VkImage, ImageState> {
  void      BindImage(DeviceMemory deviceMemory, VkDeviceSize memoryOffset = 0);
  ImageView CreateImageView(ImageViewDesc&& imageViewDesc) const;
  void      Destroy();
};

/* Image */
struct ImageViewDesc {
  const char*                  debugName{"ImageView"};
  ImageViewCreateInfo          createInfo;
  const VkAllocationCallbacks* pAllocator;
};
struct ImageViewState {
  LogicalDevice                logicalDevice;
  Image                        image;
  const VkAllocationCallbacks* pAllocator;
  VkResult                     result{VK_NOT_READY};
};
struct ImageView : HandleBase<ImageView, VkImageView, ImageViewState> {
  void Destroy();
};

/* Swapchain */
struct SwapchainDesc {
  const char*                  debugName{"Swapchain"};
  SwapchainCreateInfo          createInfo;
  const VkAllocationCallbacks* pAllocator;
};
struct SwapchainState {
  LogicalDevice                logicalDevice;
  Image                        image;
  const VkAllocationCallbacks* pAllocator;
  VkResult                     result{VK_NOT_READY};
};
struct Swapchain : HandleBase<Swapchain, VkSwapchainKHR, SwapchainState> {
  void Destroy();
};

// struct VkFormatAspect {
//   VkFormat           format;
//   VkImageAspectFlags aspectMask;
// };
// constexpr VkFormatAspect VkFormatR8B8G8A8UNorm{VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT};
// constexpr VkFormatAspect VkFormatD32SFloat{VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT};
// struct VkTexture {
//   VkFormatAspect formatAspect;
//   uint32_t       levelCount;
//   uint32_t       layerCount;
//   VkImage        image;
//   VkImageView    view;
// };

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
