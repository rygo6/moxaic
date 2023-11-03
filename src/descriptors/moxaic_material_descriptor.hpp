#pragma once

#include "moxaic_vulkan_descriptor.hpp"

namespace Moxaic
{
    class MaterialDescriptor : public VulkanDescriptorBase<MaterialDescriptor>
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;
        void Update(VulkanTexture texture);

    private:

        inline MXC_RESULT SetupDescriptorSetLayout() override
        {
            PushBinding((VkDescriptorSetLayoutBinding) {
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            });
            MXC_CHK(CreateDescriptorSetLayout());
            return MXC_SUCCESS;

        }
        inline MXC_RESULT SetupDescriptorSet() override
        {
            return MXC_SUCCESS;
        }
    };
}
