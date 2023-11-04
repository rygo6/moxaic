#pragma once

#include "moxaic_vulkan_descriptor.hpp"

#include "../moxaic_vulkan_uniform.hpp"
#include "../moxaic_camera.hpp"

#include <glm/glm.hpp>

namespace Moxaic
{
    class GlobalDescriptor : public VulkanDescriptorBase<GlobalDescriptor>
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;

        struct Buffer
        {
            glm::mat4 view;
            glm::mat4 proj;
            glm::mat4 invView;
            glm::mat4 invProj;
            uint32_t width;
            uint32_t height;
        };

        inline Buffer& uniform() { return m_Uniform.Mapped(); }

        MXC_RESULT Init(Camera &camera, VkExtent2D dimensions)
        {
            if (initializeLayout()) {
                PushBinding((VkDescriptorSetLayoutBinding) {
                        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                                      VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT |
                                      VK_SHADER_STAGE_COMPUTE_BIT |
                                      VK_SHADER_STAGE_FRAGMENT_BIT |
                                      VK_SHADER_STAGE_MESH_BIT_EXT |
                                      VK_SHADER_STAGE_TASK_BIT_EXT,
                });
                MXC_CHK(CreateDescriptorSetLayout());
            };

            MXC_CHK(m_Uniform.Init(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   Locality::Local));
            uniform().width = dimensions.width;
            uniform().height = dimensions.height;
            uniform().proj = camera.projection();
            uniform().invProj = camera.inverseProjection();
            uniform().view = camera.view();
            uniform().invView = camera.inverseView();

            MXC_CHK(AllocateDescriptorSet());
            PushWrite((VkDescriptorBufferInfo) {
                    .buffer = m_Uniform.vkBuffer(),
                    .range = m_Uniform.BufferSize()
            });
            WritePushedDescriptors();

            return MXC_SUCCESS;
        }

    private:
        VulkanUniform<Buffer> m_Uniform{k_Device};
    };
}
