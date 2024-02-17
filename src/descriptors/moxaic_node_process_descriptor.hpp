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

        void PushSrcDstTextureDescriptorWrite(VkCommandBuffer commandBuffer,
                                              const VkImageView srcImageView,
                                              const VkImageView dstImageView,
                                              VkPipelineLayout layout)
        {
            Vkm::WriteDescriptorSet writes[2];
            Vkm::DescriptorImageInfo imageInfos[2];
            EmplaceSrcTextureDescriptorWrite(srcImageView, &imageInfos[0], &writes[0]);
            EmplaceDstTextureDescriptorWrite(dstImageView, &imageInfos[1], &writes[1]);
            VkFunc.CmdPushDescriptorSetKHR(commandBuffer,
                                           VK_PIPELINE_BIND_POINT_COMPUTE,
                                           layout,
                                           SetIndex,
                                           2,
                                           (VkWriteDescriptorSet*) writes);
        }

        void EmplaceSrcTextureDescriptorWrite(const VkImageView srcImageView,
                                              Vkm::DescriptorImageInfo* pImageInfo,
                                              Vkm::WriteDescriptorSet* pWriteDescriptorSet) const
        {
            new (pImageInfo) Vkm::DescriptorImageInfo{
              .sampler = device->VkMaxSamplerHandle,
              .imageView = srcImageView,
              .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
            EmplaceDescriptorWrite(SrcTexture, pImageInfo, pWriteDescriptorSet);
        }

        void EmplaceDstTextureDescriptorWrite(const VkImageView dstImageView,
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
