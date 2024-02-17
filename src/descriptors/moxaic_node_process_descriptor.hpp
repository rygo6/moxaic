#pragma once

#include "moxaic_vulkan_descriptor.hpp"
#include "moxaic_vulkan_texture.hpp"
#include "static_array.hpp"
#include "vulkan_mid.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace Moxaic::Vulkan
{
    class NodeProcessDescriptor : public VulkanDescriptorBase2<NodeProcessDescriptor>
    {
    public:
        constexpr static uint32_t SetIndex = 0;
        enum BindingIndex : uint32_t {
            SrcTexture,
            DstTexture,
        };
        constexpr static StaticArray LayoutBindings{
          Vkm::DescriptorSetLayoutBinding{
            .binding = SrcTexture,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          Vkm::DescriptorSetLayoutBinding{
            .binding = DstTexture,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
        };

        void SetSrcTextureDescriptorWrite(const VkImageView srcImageView,
                                          Vkm::DescriptorImageInfo* pImageInfo,
                                          Vkm::WriteDescriptorSet* pWriteDescriptorSet) const
        {
            new (pImageInfo) Vkm::DescriptorImageInfo{
              .sampler = device->VkMaxSamplerHandle,
              .imageView = srcImageView,
              .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
            EmplaceDescriptorWrite(SrcTexture, pImageInfo, pWriteDescriptorSet);
        }

        void SetDstTextureDescriptorWrite(const VkImageView dstImageView,
                                          Vkm::DescriptorImageInfo* pImageInfo,
                                          Vkm::WriteDescriptorSet* pWriteDescriptorSet) const
        {
            new (pImageInfo) VkDescriptorImageInfo{
              .imageView = dstImageView,
              .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
            EmplaceDescriptorWrite(DstTexture, pImageInfo, pWriteDescriptorSet);
        }
    };
}// namespace Moxaic::Vulkan
