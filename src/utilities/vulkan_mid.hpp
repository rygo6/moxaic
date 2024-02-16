/**********************************************************************************************
*
*   Vulkan Mid v0.0 - Vulkan boilerplate API
*
**********************************************************************************************/

#pragma once

#include <vulkan/vulkan.h>

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

    struct VkWriteImageDescriptorSet
    {
        uint32_t dstBinding;
        uint32_t dstArrayElement;
        uint32_t descriptorCount;
        VkDescriptorType descriptorType;
        VkSampler sampler;
        VkImageView imageView;
        VkImageLayout imageLayout;
    };

    struct DescriptorSetLayoutBinding
    {
        const uint32_t binding{0};
        const VkDescriptorType descriptorType{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER};
        const uint32_t descriptorCount{1};
        const VkShaderStageFlags stageFlags{VK_SHADER_STAGE_FRAGMENT_BIT};
        const VkSampler* pImmutableSamplers{VK_NULL_HANDLE};

        // constexpr operator VkDescriptorSetLayoutBinding() const
        // {
        //     return (VkDescriptorSetLayoutBinding){
        //       .binding = binding,
        //       .descriptorType = descriptorType,
        //       .descriptorCount = descriptorCount,
        //       .stageFlags = stageFlags,
        //       .pImmutableSamplers = pImmutableSamplers,
        //     };
        // }
    };

    // struct WriteDescriptorSet
    // {
    //     constexpr WriteDescriptorSet(const VkDescriptorSet dstSet,
    //                                  const DescriptorSetLayoutBinding& layoutBinding,
    //                                  const VkDescriptorImageInfo& imageInfo)
    //         : dstSet(dstSet),
    //           dstBinding(layoutBinding.binding),
    //           descriptorCount(layoutBinding.descriptorCount),
    //           descriptorType(layoutBinding.descriptorType),
    //           pImageInfo(&imageInfo)
    //     {
    //     }
    //
    //     constexpr WriteDescriptorSet(const VkDescriptorSet dstSet,
    //                                  const DescriptorSetLayoutBinding& layoutBinding,
    //                                  const VkDescriptorBufferInfo& bufferInfo)
    //         : dstSet(dstSet),
    //           dstBinding(layoutBinding.binding),
    //           descriptorCount(layoutBinding.descriptorCount),
    //           descriptorType(layoutBinding.descriptorType),
    //           pBufferInfo(&bufferInfo)
    //     {
    //     }
    //
    //     const VkStructureType sType{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    //     const void* pNext{nullptr};
    //     const VkDescriptorSet dstSet{VK_NULL_HANDLE};
    //     const uint32_t dstBinding{0};
    //     const uint32_t dstArrayElement{0};
    //     const uint32_t descriptorCount{1};
    //     const VkDescriptorType descriptorType{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER};
    //     const VkDescriptorImageInfo* pImageInfo{nullptr};
    //     const VkDescriptorBufferInfo* pBufferInfo{nullptr};
    //     const VkBufferView* pTexelBufferView{nullptr};
    //
    //     // constexpr operator VkWriteDescriptorSet() const
    //     // {
    //     //     return (VkWriteDescriptorSet){
    //     //       .sType = sType,
    //     //       .pNext = pNext,
    //     //       .dstBinding = dstBinding,
    //     //       .dstArrayElement = dstArrayElement,
    //     //       .descriptorCount = descriptorCount,
    //     //       .descriptorType = descriptorType,
    //     //       .pImageInfo = pImageInfo,
    //     //       .pBufferInfo = pBufferInfo,
    //     //       .pTexelBufferView = pTexelBufferView,
    //     //     };
    //     // }
    // };

    struct WriteDescriptorSet
    {
        const DescriptorSetLayoutBinding layoutBinding{};
        const VkDescriptorSet dstSet{VK_NULL_HANDLE};
        const uint32_t dstArrayElement{0};
        const VkDescriptorImageInfo imageInfo{nullptr};
        const VkDescriptorBufferInfo bufferInfo{nullptr};

        WriteDescriptorSet WriteDescriptorSet::operator=(const WriteDescriptorSet&)
        {
            return WriteDescriptorSet{
              .layoutBinding = layoutBinding,
              .dstSet = dstSet,
              .dstArrayElement = dstArrayElement,
              .imageInfo = VkDescriptorImageInfo{
                .sampler = imageInfo.sampler,
                .imageLayout = imageInfo.imageLayout,
                .imageView = imageInfo.imageView,
              },
              .bufferInfo = VkDescriptorBufferInfo{
                .buffer = bufferInfo.buffer,
                .offset = bufferInfo.offset,
                .range = bufferInfo.range,
              },
            };
        }

        constexpr operator VkWriteDescriptorSet() const
        {
            return VkWriteDescriptorSet{
              .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              .pNext = nullptr,
              .dstSet = dstSet,
              .dstBinding = layoutBinding.binding,
              .dstArrayElement = dstArrayElement,
              .descriptorCount = layoutBinding.descriptorCount,
              .descriptorType = layoutBinding.descriptorType,
              .pImageInfo = &imageInfo,
              .pBufferInfo = &bufferInfo,
              .pTexelBufferView = nullptr,
            };
        }
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

    void WriteDescriptors(
      VkDevice device,
      size_t writeCount,
      const WriteDescriptorSet* pWrites,
      VkDescriptorSet dstSet);

    void WriteDescriptors(
      VkDevice device,
      size_t writeCount,
      const WriteDescriptorSet* pWrites);

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

void Vkm::WriteDescriptors(
  const VkDevice device,
  const size_t writeCount,
  const Vkm::WriteDescriptorSet* pWrites,
  const VkDescriptorSet dstSet)
{
    VkWriteDescriptorSet descriptorWrites[writeCount];
    for (int i = 0; i < writeCount; ++i) {
        descriptorWrites[i] = VkWriteDescriptorSet{
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .pNext = nullptr,
          .dstSet = dstSet,
          .dstBinding = pWrites[i].layoutBinding.binding,
          .dstArrayElement = pWrites[i].dstArrayElement,
          .descriptorCount = pWrites[i].layoutBinding.descriptorCount,
          .descriptorType = pWrites[i].layoutBinding.descriptorType,
          .pImageInfo = &pWrites[i].imageInfo,
          .pBufferInfo = &pWrites[i].bufferInfo,
          .pTexelBufferView = nullptr,
        };
    }
    vkUpdateDescriptorSets(device,
                           writeCount,
                           descriptorWrites,
                           0,
                           nullptr);
}

void Vkm::WriteDescriptors(
  const VkDevice device,
  const size_t writeCount,
  const Vkm::WriteDescriptorSet* pWrites)
{
    VkWriteDescriptorSet descriptorWrites[writeCount];
    for (int i = 0; i < writeCount; ++i) {
        descriptorWrites[i] = pWrites[i];
    }
    vkUpdateDescriptorSets(device,
                           writeCount,
                           descriptorWrites,
                           0,
                           nullptr);
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
