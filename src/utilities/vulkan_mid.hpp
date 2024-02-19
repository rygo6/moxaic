/**********************************************************************************************
*
*   Vulkan Mid v0.0 - Vulkan boilerplate API
*
**********************************************************************************************/

#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#define VKM_DEFAULT_IMAGE_LAYOUT VK_IMAGE_LAYOUT_GENERAL
#define VKM_DEFAULT_DESCRIPTOR_TYPE VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
#define VKM_DEFAULT_SHADER_STAGE VK_SHADER_STAGE_COMPUTE_BIT
#define VKM_ALLOCATOR nullptr

#define VKM_CHECK(command)                               \
    {                                                    \
        VkResult result = command;                       \
        if (result != VK_SUCCESS) [[unlikely]] {         \
            printf("VKCheck fail on command: %s - %s\n", \
                   #command,                             \
                   string_VkResult(result));             \
            return result;                               \
        }                                                \
    }

namespace Vkm
{
    struct
    {
#define VK_FUNCS                           \
    VK_FUNC(SetDebugUtilsObjectNameEXT)

#define VK_FUNC(func) PFN_vk##func func;
        VK_FUNCS
#undef VK_FUNC
    } PFN;

    struct VkFormatAspect
    {
        VkFormat format;
        VkImageAspectFlags aspectMask;
    };

    constexpr VkFormatAspect VkFormatR8B8G8A8UNorm{VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT};
    constexpr VkFormatAspect VkFormatD32SFloat{VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT};

    struct VkTexture
    {
        VkFormatAspect formatAspect;
        uint32_t levelCount;
        uint32_t layerCount;
        VkImage image;
        VkImageView view;
    };

    struct DescriptorSetLayoutBinding
    {
        const uint32_t binding{0};
        const VkDescriptorType descriptorType{VKM_DEFAULT_DESCRIPTOR_TYPE};
        const uint32_t descriptorCount{1};
        const VkShaderStageFlags stageFlags{VKM_DEFAULT_SHADER_STAGE};
        const VkSampler* pImmutableSamplers{VK_NULL_HANDLE};

        constexpr operator VkDescriptorSetLayoutBinding() const { return *(VkDescriptorSetLayoutBinding*) this; }
    };

    struct DescriptorImageInfo
    {
        const VkSampler sampler{VK_NULL_HANDLE};
        const VkImageView imageView{VK_NULL_HANDLE};
        const VkImageLayout imageLayout{VKM_DEFAULT_IMAGE_LAYOUT};

        constexpr operator VkDescriptorImageInfo() const { return *(VkDescriptorImageInfo*) this; }
    };

    struct WriteDescriptorSet
    {
        const VkStructureType sType{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        const void* pNext{nullptr};
        const VkDescriptorSet dstSet{VK_NULL_HANDLE};
        const uint32_t dstBinding{0};
        const uint32_t dstArrayElement{0};
        const uint32_t descriptorCount{1};
        const VkDescriptorType descriptorType{VKM_DEFAULT_DESCRIPTOR_TYPE};
        const DescriptorImageInfo* pImageInfo{nullptr};
        const VkDescriptorBufferInfo* pBufferInfo{nullptr};
        const VkBufferView* pTexelBufferView{nullptr};

        constexpr operator VkWriteDescriptorSet() const { return *(VkWriteDescriptorSet*) this; }
    };

    struct DescriptorSetLayoutCreateInfo
    {
        const VkStructureType sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        const void* pNext{nullptr};
        const VkDescriptorSetLayoutCreateFlags flags{0};
        const uint32_t bindingCount{1};
        const DescriptorSetLayoutBinding* pBindings{nullptr};

        constexpr operator VkDescriptorSetLayoutCreateInfo() const { return *(VkDescriptorSetLayoutCreateInfo*) this; }
    };

    struct DescriptorSetAllocateInfo
    {
        const VkStructureType sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        const void* pNext{nullptr};
        const VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
        const uint32_t descriptorSetCount{1};
        const VkDescriptorSetLayout* pSetLayouts{nullptr};

        constexpr operator VkDescriptorSetAllocateInfo() const { return *(VkDescriptorSetAllocateInfo*) this; }
    };

    struct DebugUtilsObjectNameInfoEXT
    {
        const VkStructureType sType{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT};
        const void* pNext{nullptr};
        const VkObjectType objectType{};
        const uint64_t objectHandle{};
        const char* pObjectName{"unnamed"};

        constexpr operator VkDebugUtilsObjectNameInfoEXT() const { return *(VkDebugUtilsObjectNameInfoEXT*) this; }
    };

    struct PipelineLayoutCreateInfo
    {
        const VkStructureType sType{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        const void* pNext{nullptr};
        const VkPipelineLayoutCreateFlags flags{};
        const uint32_t setLayoutCount{1};
        const VkDescriptorSetLayout* pSetLayouts{nullptr};
        const uint32_t pushConstantRangeCount{0};
        const VkPushConstantRange* pPushConstantRanges{nullptr};

        constexpr operator VkPipelineLayoutCreateInfo() const { return *(VkPipelineLayoutCreateInfo*) this; }
    };

    struct PipelineShaderStageCreateInfo
    {
        const VkStructureType sType{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        const void* const pNext{nullptr};
        const VkPipelineShaderStageCreateFlags flags{};
        const VkShaderStageFlagBits stage{VKM_DEFAULT_SHADER_STAGE};
        const VkShaderModule module{VK_NULL_HANDLE};
        const char* const pName{nullptr};
        const VkSpecializationInfo* pSpecializationInfo{nullptr};

        constexpr operator VkPipelineShaderStageCreateInfo() const { return *(VkPipelineShaderStageCreateInfo*) this; }
    };

    struct ComputePipelineCreateInfo
    {
        const VkStructureType sType{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        const void* const pNext{nullptr};
        const VkPipelineCreateFlags flags{0};
        const PipelineShaderStageCreateInfo stage{};
        const VkPipelineLayout layout{VK_NULL_HANDLE};
        const VkPipeline basePipelineHandle{VK_NULL_HANDLE};
        const int32_t basePipelineIndex{0};

        constexpr operator VkComputePipelineCreateInfo() const { return *(VkComputePipelineCreateInfo*) this; }
    };

    struct Error
    {
        const char* command;
        const VkResult result;
    };

    // Handles

    struct SwapChain
    {
        const VkSwapchainKHR handle{VK_NULL_HANDLE};
        const VkDevice deviceHandle{VK_NULL_HANDLE};

        void SetDebugInfo(VkDevice device)
        {
            const DebugUtilsObjectNameInfoEXT debugInfo{
              .objectType = VK_OBJECT_TYPE_SWAPCHAIN_KHR,
              .objectHandle = (uint64_t) handle,
              .pObjectName = "SwapChain",
            };
            PFN.SetDebugUtilsObjectNameEXT(device, (VkDebugUtilsObjectNameInfoEXT*) &debugInfo);
        }

        ~SwapChain()
        {
            if (handle != VK_NULL_HANDLE)
                vkDestroySwapchainKHR(deviceHandle, handle, VKM_ALLOCATOR);
        }

        constexpr operator VkSwapchainKHR() const { return handle; }
    };

    struct DescriptorSetLayout
    {
        VkDescriptorSetLayout handle{VK_NULL_HANDLE};
        VkDevice deviceHandle{VK_NULL_HANDLE};

        VkResult Create(const VkDevice device,
                        const char* const name,
                        const Vkm::DescriptorSetLayoutCreateInfo* const pCreateInfo)
        {
            deviceHandle = device;
            VKM_CHECK(vkCreateDescriptorSetLayout(device, (VkDescriptorSetLayoutCreateInfo*) pCreateInfo, VKM_ALLOCATOR, &handle));
            const DebugUtilsObjectNameInfoEXT debugInfo{
              .objectType = VK_OBJECT_TYPE_SWAPCHAIN_KHR,
              .objectHandle = (uint64_t) handle,
              .pObjectName = name,
            };
            return PFN.SetDebugUtilsObjectNameEXT(device, (VkDebugUtilsObjectNameInfoEXT*) &debugInfo);
        }

        ~DescriptorSetLayout()
        {
            if (handle != VK_NULL_HANDLE)
                vkDestroyDescriptorSetLayout(deviceHandle, handle, VKM_ALLOCATOR);
        }

        constexpr operator VkDescriptorSetLayout() const { return handle; }
    };

    struct DescriptorSet
    {
        const VkDescriptorSet handle{VK_NULL_HANDLE};
        const VkDevice deviceHandle{VK_NULL_HANDLE};
        const VkDescriptorPool descriptorPoolHandle{VK_NULL_HANDLE};

        void SetDebugInfo(VkDevice device, const char* const name)
        {
            const DebugUtilsObjectNameInfoEXT debugInfo{
              .objectType = VK_OBJECT_TYPE_SWAPCHAIN_KHR,
              .objectHandle = (uint64_t) handle,
              .pObjectName = name,
            };
            PFN.SetDebugUtilsObjectNameEXT(device, (VkDebugUtilsObjectNameInfoEXT*) &debugInfo);
        }

        ~DescriptorSet()
        {
            if (handle != VK_NULL_HANDLE)
                vkFreeDescriptorSets(deviceHandle, descriptorPoolHandle, 1, &handle);
        }

        constexpr operator VkDescriptorSet() const { return handle; }
    };

    struct PipelineLayout
    {
        VkPipelineLayout handle{VK_NULL_HANDLE};
        VkDevice deviceHandle{VK_NULL_HANDLE};

        VkResult Create(const VkDevice device,
                        const char* const name,
                        const Vkm::PipelineLayoutCreateInfo* const pCreateInfo)
        {
            deviceHandle = device;
            VKM_CHECK(vkCreatePipelineLayout(device,
                                             (VkPipelineLayoutCreateInfo*) pCreateInfo,
                                             VKM_ALLOCATOR,
                                             &handle));
            const DebugUtilsObjectNameInfoEXT debugInfo{
              .objectType = VK_OBJECT_TYPE_PIPELINE_LAYOUT,
              .objectHandle = (uint64_t) handle,
              .pObjectName = name,
            };
            return PFN.SetDebugUtilsObjectNameEXT(device,
                                                  (VkDebugUtilsObjectNameInfoEXT*) &debugInfo);
        }

        ~PipelineLayout()
        {
            if (handle != VK_NULL_HANDLE)
                vkDestroyPipelineLayout(deviceHandle, handle, VKM_ALLOCATOR);
        }

        constexpr operator VkPipelineLayout() const { return handle; }
    };

    inline VkResult ReadFile(const char* filename,
                             uint32_t* length,
                             char** ppContents)
    {
        FILE* file;
        if (!fopen_s(&file, filename, "rb")) {
            printf("File can't be opened! %s\n", filename);
            return VK_ERROR_INVALID_SHADER_NV;
        }
        fseek(file, 0, SEEK_END);
        *length = ftell(file);
        rewind(file);
        *ppContents = (char*) calloc(1 + *length, sizeof(char));
        const size_t readCount = fread_s(*ppContents, *length, *length, 1, file);
        if (readCount == 0) {
            printf("Failed to read file! %s\n", filename);
            return VK_ERROR_INVALID_SHADER_NV;
        }
        fclose(file);
        return VK_SUCCESS;
    }

    struct ShaderModule
    {
        VkShaderModule handle{VK_NULL_HANDLE};
        VkDevice deviceHandle{VK_NULL_HANDLE};

        VkResult Create(const VkDevice device,
                        const char* const pShaderPath)
        {
            deviceHandle = device;
            uint32_t codeLength;
            char* pShaderCode;
            VKM_CHECK(ReadFile(pShaderPath,
                               &codeLength,
                               &pShaderCode));
            const VkShaderModuleCreateInfo createInfo{
              .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
              .pNext = nullptr,
              .flags = 0,
              .codeSize = codeLength,
              .pCode = (const uint32_t*) pShaderCode,
            };
            VKM_CHECK(vkCreateShaderModule(device,
                                           &createInfo,
                                           VKM_ALLOCATOR,
                                           &handle));
            const DebugUtilsObjectNameInfoEXT debugInfo{
              .objectType = VK_OBJECT_TYPE_SHADER_MODULE,
              .objectHandle = (uint64_t) handle,
              .pObjectName = pShaderPath,
            };
            VKM_CHECK(PFN.SetDebugUtilsObjectNameEXT(device, (VkDebugUtilsObjectNameInfoEXT*) &debugInfo));
            free(pShaderCode);
            return VK_SUCCESS;
        }

        ~ShaderModule()
        {
            if (handle != VK_NULL_HANDLE)
                vkDestroyShaderModule(deviceHandle, handle, VKM_ALLOCATOR);
        }

        constexpr operator VkShaderModule() const { return handle; }
    };

    struct ComputePipeline
    {
        VkPipeline handle{VK_NULL_HANDLE};
        VkDevice deviceHandle{VK_NULL_HANDLE};

        VkResult Create(const VkDevice device,
                        const char* const name,
                        const Vkm::ComputePipelineCreateInfo* const pCreateInfo)
        {
            deviceHandle = device;
            VKM_CHECK(vkCreateComputePipelines(device,
                                               VK_NULL_HANDLE,
                                               1,
                                               (VkComputePipelineCreateInfo*) pCreateInfo,
                                               VKM_ALLOCATOR,
                                               &handle));
            const DebugUtilsObjectNameInfoEXT debugInfo{
              .objectType = VK_OBJECT_TYPE_PIPELINE,
              .objectHandle = (uint64_t) handle,
              .pObjectName = name,
            };
            VKM_CHECK(PFN.SetDebugUtilsObjectNameEXT(device, (VkDebugUtilsObjectNameInfoEXT*) &debugInfo));
            return VK_SUCCESS;
        }

        ~ComputePipeline()
        {
            if (handle != VK_NULL_HANDLE)
                vkDestroyPipeline(deviceHandle, handle, VKM_ALLOCATOR);
        }

        constexpr operator VkPipeline() const { return handle; }
    };

    //------------------------------------------------------------------------------------
    // Functions Declaration
    //------------------------------------------------------------------------------------


    // void glGenerateTextureMipmap(	GLuint texture);

    void CmdPipelineImageBarrier2(
      VkCommandBuffer commandBuffer,
      uint32_t imageMemoryBarrierCount,
      const VkImageMemoryBarrier2* pImageMemoryBarriers);

    inline VkResult CreateDescriptorSetLayout(
      const VkDevice device,
      const Vkm::DescriptorSetLayoutCreateInfo* pCreateInfo,
      VkDescriptorSetLayout* pSetLayout)
    {
        return vkCreateDescriptorSetLayout(device, (VkDescriptorSetLayoutCreateInfo*) pCreateInfo, VKM_ALLOCATOR, pSetLayout);
    }

    inline void UpdateDescriptorSets(
      VkDevice device,
      uint32_t descriptorWriteCount,
      const WriteDescriptorSet* pDescriptorWrites,
      uint32_t descriptorCopyCount = 0,
      const VkCopyDescriptorSet* pDescriptorCopies = nullptr)
    {
        vkUpdateDescriptorSets(device, descriptorWriteCount, (VkWriteDescriptorSet*) pDescriptorWrites, 0, nullptr);
    }

    inline VkResult AllocateDescriptorSets(
      VkDevice device,
      const DescriptorSetAllocateInfo* pAllocateInfo,
      VkDescriptorSet* pDescriptorSets)
    {
        return vkAllocateDescriptorSets(device, (VkDescriptorSetAllocateInfo*) pAllocateInfo, pDescriptorSets);
    }

}// namespace Vkm

/***********************************************************************************
*
*   VULKAN MEDIUM IMPLEMENTATION
*
************************************************************************************/

#if defined(VULKAN_MEDIUM_IMPLEMENTATION)

void Vkm::CmdPipelineImageBarrier2(
  VkCommandBuffer commandBuffer,
  uint32_t imageMemoryBarrierCount,
  const VkImageMemoryBarrier2* pImageMemoryBarriers)
{
    const VkDependencyInfo toComputeDependencyInfo = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .imageMemoryBarrierCount = imageMemoryBarrierCount,
      .pImageMemoryBarriers = pImageMemoryBarriers,
    };
    vkCmdPipelineBarrier2(commandBuffer, &toComputeDependencyInfo);
}

VkResult vkCreateImageView2D(
  VkDevice device,
  VkImage image,
  VkFormat format,
  VkImageAspectFlags aspectMask,
  const VkAllocationCallbacks* pAllocator,
  VkImageView* pView)
{
    const VkImageViewCreateInfo imageViewCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .subresourceRange = {
        .aspectMask = aspectMask,
        .levelCount = 1,
        .layerCount = 1,
      },
    };
    return vkCreateImageView(device,
                             &imageViewCreateInfo,
                             pAllocator,
                             pView);
}

VkResult vkCreateMipImageView2D(
  VkDevice device,
  VkImage image,
  VkFormat format,
  VkImageAspectFlags aspectMask,
  uint32_t mipLevelCount,
  uint32_t mipLevel,
  const VkAllocationCallbacks* pAllocator,
  VkImageView* pView)
{
    const VkImageViewCreateInfo imageViewCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .subresourceRange = {
        .aspectMask = aspectMask,
        .baseMipLevel = mipLevel,
        .levelCount = mipLevelCount,
        .layerCount = 1,
      },
    };
    return vkCreateImageView(device,
                             &imageViewCreateInfo,
                             pAllocator,
                             pView);
}

#endif
