#pragma once

#include "moxaic_vulkan_descriptor.hpp"

#include "../moxaic_vulkan_uniform.hpp"
#include "../moxaic_transform.hpp"

#include "glm/glm.hpp"

namespace Moxaic
{
    class ObjectDescriptor : public VulkanDescriptorBase<ObjectDescriptor>
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;
        MXC_RESULT Init(const Transform &transform)
        {
            if (initializeLayout()) {
                PushBinding((VkDescriptorSetLayoutBinding) {
                        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                                      VK_SHADER_STAGE_FRAGMENT_BIT,
                });
                MXC_CHK(CreateDescriptorSetLayout());
            }
            MXC_CHK(m_Uniform.Init(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   Locality::Local));
            MXC_CHK(AllocateDescriptorSet());
            PushWrite((VkDescriptorBufferInfo) {
                    .buffer = m_Uniform.vkBuffer(),
                    .range = m_Uniform.BufferSize()
            });
            WritePushedDescriptors();

            return MXC_SUCCESS;
        }

        struct Buffer
        {
            glm::mat4 model;
        };

    private:
        VulkanUniform<Buffer> m_Uniform{k_Device};
    };
}
