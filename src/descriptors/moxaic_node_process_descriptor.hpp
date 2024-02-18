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

        static void EmplaceSrcTextureImageInfo(const VkImageView srcImageView,
                                               const VkDescriptorSet dstSet,
                                               Vkm::DescriptorImageInfo* pImageInfo,
                                               Vkm::WriteDescriptorSet* pWriteDescriptorSet)
        {
            new (pImageInfo) Vkm::DescriptorImageInfo{
              .sampler = device->VkMaxSamplerHandle,
              .imageView = srcImageView,
              .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
            EmplaceDescriptorWrite(SrcTexture, dstSet, pImageInfo, pWriteDescriptorSet);
        }

        static void EmplaceDstTextureImageInfo(const VkImageView dstImageView,
                                               const VkDescriptorSet dstSet,
                                               Vkm::DescriptorImageInfo* pImageInfo,
                                               Vkm::WriteDescriptorSet* pWriteDescriptorSet)
        {
            new (pImageInfo) VkDescriptorImageInfo{
              .imageView = dstImageView,
              .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
            EmplaceDescriptorWrite(DstTexture, dstSet, pImageInfo, pWriteDescriptorSet);
        }

        static void PushSrcDstTextureDescriptorWrite(VkCommandBuffer commandBuffer,
                                                     const VkImageView srcImageView,
                                                     const VkImageView dstImageView,
                                                     VkPipelineLayout layout)
        {
            Vkm::DescriptorImageInfo imageInfos[2];
            Vkm::WriteDescriptorSet writes[2];
            EmplaceSrcTextureImageInfo(srcImageView, VK_NULL_HANDLE, &imageInfos[0], &writes[0]);
            EmplaceDstTextureImageInfo(dstImageView, VK_NULL_HANDLE, &imageInfos[1], &writes[1]);
            VkFunc.CmdPushDescriptorSetKHR(commandBuffer,
                                           VK_PIPELINE_BIND_POINT_COMPUTE,
                                           layout,
                                           SetIndex,
                                           2,
                                           (VkWriteDescriptorSet*) writes);
        }

        void EmplaceSrcTextureDescriptorWrite(const VkImageView srcImageView,
                                              Vkm::DescriptorImageInfo* pImageInfo,
                                              Vkm::WriteDescriptorSet* pWriteDescriptorSet)
        {
            EmplaceDescriptorWrite(SrcTexture, vkDescriptorSetHandle, pImageInfo, pWriteDescriptorSet);
        }

        void EmplaceDstTextureDescriptorWrite(const VkImageView dstImageView,
                                              Vkm::DescriptorImageInfo* pImageInfo,
                                              Vkm::WriteDescriptorSet* pWriteDescriptorSet)
        {
            EmplaceDescriptorWrite(DstTexture, vkDescriptorSetHandle, pImageInfo, pWriteDescriptorSet);
        }
    };
}// namespace Moxaic::Vulkan
