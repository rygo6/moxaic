#pragma once

#include "moxaic_vulkan_descriptor.hpp"

#include "../moxaic_vulkan_uniform.hpp"

#include "glm/glm.hpp"

namespace Moxaic
{
    class ObjectDescriptor : public VulkanDescriptorBase<ObjectDescriptor>
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;
        MXC_RESULT Init();

        struct Buffer
        {
            glm::mat4 model;
        };

    private:
        VulkanUniform<Buffer> m_Uniform{k_Device};
    };
}
