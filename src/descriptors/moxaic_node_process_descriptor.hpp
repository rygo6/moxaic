#pragma once

#include "moxaic_vulkan_descriptor.hpp"
#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_texture.hpp"
#include "static_array.hpp"
#include "vulkan_mid.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace Moxaic::Vulkan
{
    class NodeProcessDescriptor : public VulkanDescriptorBase<NodeProcessDescriptor>
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;
        constexpr static uint32_t SetIndex = 0;

        enum BindingIndex : uint32_t {
            SrcTexture,
            DstTexture,
        };

        constexpr static Vkm::Array LayoutBindings{
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

        static MXC_RESULT InitLayout(const Vulkan::Device& device)
        {
            MXC_LOG("InitLayout NodeProcessDescriptor");
            VK_CHK(Vkm::CreateDescriptorSetLayout(device.GetVkDevice(),
                                                  LayoutBindings.Size,
                                                  LayoutBindings.Data,
                                                  &sharedVkDescriptorSetLayout));
            return MXC_SUCCESS;
        }

        MXC_RESULT Init(const Device* const device)
        {
            MXC_LOG("Init NodeProcessDescriptor");
            this->device = device;
            MXC_CHK(AllocateDescriptorSet());
            return MXC_SUCCESS;
        }

        MXC_RESULT Init()
        {
            MXC_LOG("Init NodeProcessDescriptor");
            MXC_CHK(AllocateDescriptorSet());
            return MXC_SUCCESS;
        }

        void SetDescriptorWrite(const BindingIndex binding,
                        const VkDescriptorImageInfo* pImageInfo,
                        VkWriteDescriptorSet* pWriteDescriptorSet) const
        {
            *pWriteDescriptorSet = VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = vkDescriptorSet,
                .dstBinding = LayoutBindings[binding].binding,
                .descriptorCount = LayoutBindings[binding].descriptorCount,
                .descriptorType = LayoutBindings[binding].descriptorType,
                .pImageInfo = pImageInfo,
              };
        }

        void SetSrcTextureDescriptorWrite(const VkImageView srcImageView,
                                          VkDescriptorImageInfo* pImageInfo,
                                          VkWriteDescriptorSet* pWriteDescriptorSet) const
        {
            *pImageInfo = VkDescriptorImageInfo{
                .sampler = device->VkMaxSamplerHandle,
                .imageView = srcImageView,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
            SetDescriptorWrite(SrcTexture, pImageInfo, pWriteDescriptorSet);
        }

        void SetDstTextureDescriptorWrite(const VkImageView dstImageView,
                                          VkDescriptorImageInfo* pImageInfo,
                                          VkWriteDescriptorSet* pWriteDescriptorSet) const
        {
            *pImageInfo = VkDescriptorImageInfo{
                .imageView = dstImageView,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
            SetDescriptorWrite(DstTexture, pImageInfo, pWriteDescriptorSet);
        }

        MXC_RESULT WriteSrcTexture(const VkImageView& depthImageView) const
        {
            const Vkm::Array writes{
              Vkm::WriteDescriptorSet(
                vkDescriptorSet,
                LayoutBindings[SrcTexture],
                VkDescriptorImageInfo{
                  .sampler = device->VkMaxSamplerHandle,
                  .imageView = depthImageView,
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
                }),
            };
            Vkm::WriteDescriptors(device->GetVkDevice(), vkDescriptorSet, writes);
            return MXC_SUCCESS;
        }

        MXC_RESULT WriteDstTexture(const VkImageView gBufferMip) const
        {
            StaticArray writes{
              (VkWriteDescriptorSet){
                .dstBinding = BindingIndex::DstTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .imageView = gBufferMip,
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
            };
            WriteDescriptors(writes);
            return MXC_SUCCESS;
        }
    };
}// namespace Moxaic::Vulkan
