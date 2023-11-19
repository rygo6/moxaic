#pragma once

#include "moxaic_vulkan_descriptor.hpp"

#include "../moxaic_vulkan_texture.hpp"

namespace Moxaic::Vulkan
{
    class MaterialDescriptor : public VulkanDescriptorBase<MaterialDescriptor>
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;

        MXC_RESULT Init(const Texture &texture)
        {
            MXC_LOG("Init MaterialDescriptor");

            if (initializeLayout()) {
                std::array bindings{
                        (VkDescriptorSetLayoutBinding) {
                                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                        }
                };
                MXC_CHK(CreateDescriptorSetLayout(bindings.size(),
                                                  bindings.data()));
            }

            MXC_CHK(AllocateDescriptorSet());
            const VkDescriptorImageInfo imageInfo{
                    .sampler = k_Device.vkLinearSampler(),
                    .imageView = texture.vkImageView,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
            std::array writes{
                    (VkWriteDescriptorSet) {
                            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            .pImageInfo = &imageInfo
                    },
            };
            WriteDescriptors(writes.size(),
                             writes.data());

            return MXC_SUCCESS;
        }
    };
}
