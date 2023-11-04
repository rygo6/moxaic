#pragma once

#include "moxaic_vulkan_descriptor.hpp"

#include "../moxaic_vulkan_texture.hpp"

namespace Moxaic
{
    class MaterialDescriptor : public VulkanDescriptorBase<MaterialDescriptor>
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;

        MXC_RESULT Init(VulkanTexture texture)
        {
            if (initializeLayout()) {
                PushBinding((VkDescriptorSetLayoutBinding) {
                        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                });
                MXC_CHK(CreateDescriptorSetLayout());
            }
            return MXC_SUCCESS;
        }
    };
}
