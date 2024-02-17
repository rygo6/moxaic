/**********************************************************************************************
*
*   Vulkan Mid v0.0 - Vulkan boilerplate API
*
**********************************************************************************************/

#pragma once

#include <vulkan/vulkan.h>

#define VKM_DEFAULT_IMAGE_LAYOUT VK_IMAGE_LAYOUT_GENERAL
#define VKM_DEFAULT_DESCRIPTOR_TYPE VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
#define VKM_DEFAULT_SHADER_STAGE VK_SHADER_STAGE_FRAGMENT_BIT

namespace Vkm
{
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

    constexpr VkDescriptorSetLayoutBinding defaultBinding{
      .binding{},
      .descriptorType{},
      .descriptorCount{},
      .stageFlags{},
      .pImmutableSamplers{},
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

    struct DescriptorSetAllocateInfo
    {
        const VkStructureType sType{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        const void* pNext{nullptr};
        const VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
        const uint32_t descriptorSetCount{1};
        const VkDescriptorSetLayout* pSetLayouts{nullptr};

        constexpr operator VkDescriptorSetAllocateInfo() const { return *(VkDescriptorSetAllocateInfo*) this; }
    };

    //------------------------------------------------------------------------------------
    // Functions Declaration
    //------------------------------------------------------------------------------------


    // void glGenerateTextureMipmap(	GLuint texture);

    void CmdPipelineImageBarrier2(
      VkCommandBuffer commandBuffer,
      uint32_t imageMemoryBarrierCount,
      const VkImageMemoryBarrier2* pImageMemoryBarriers);

    VkResult CreateDescriptorSetLayout(
      VkDevice device,
      uint32_t bindingsCount,
      const DescriptorSetLayoutBinding* bindings,
      VkDescriptorSetLayout* pSetLayout);

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

VkResult Vkm::CreateDescriptorSetLayout(
  const VkDevice device,
  const uint32_t bindingsCount,
  const Vkm::DescriptorSetLayoutBinding* pBindings,
  VkDescriptorSetLayout* const pSetLayout)
{
    const VkDescriptorSetLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .bindingCount = bindingsCount,
      .pBindings = (VkDescriptorSetLayoutBinding*) pBindings,
    };
    return vkCreateDescriptorSetLayout(device,
                                       &layoutInfo,
                                       nullptr,
                                       pSetLayout);
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
