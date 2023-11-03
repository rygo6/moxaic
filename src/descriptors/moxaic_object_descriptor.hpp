#pragma once

#include "moxaic_vulkan_descriptor.hpp"

namespace Moxaic
{
    class ObjectDescriptor : public VulkanDescriptorBase<ObjectDescriptor>
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;
//        void Update(Transform transform);

        struct Buffer
        {
            glm::mat4 model;
        };

    private:
        VulkanUniform<Buffer> m_Uniform{k_Device};

        MXC_RESULT SetupDescriptorSetLayout() override;
        MXC_RESULT SetupDescriptorSet() override;
    };
}
