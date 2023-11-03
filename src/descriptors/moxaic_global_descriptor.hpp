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

        inline void Update(Camera &camera, VkExtent2D dimensions)
        {
            m_Uniform.Mapped().width = dimensions.width;
            m_Uniform.Mapped().height = dimensions.height;
            m_Uniform.Mapped().proj = camera.projection();
            m_Uniform.Mapped().invProj = camera.inverseProjection();
            m_Uniform.Mapped().view = camera.view();
            m_Uniform.Mapped().invView = camera.inverseView();
        }

    private:
        VulkanUniform<Buffer> m_Uniform{k_Device};

        inline MXC_RESULT SetupDescriptorSetLayout() override
        {
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
            return MXC_SUCCESS;
        }
        inline MXC_RESULT SetupDescriptorSet() override
        {
            MXC_CHK(m_Uniform.Init(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   Locality::Local));
            PushWrite((VkDescriptorBufferInfo) {
                    .buffer = m_Uniform.vkBuffer(),
                    .range = m_Uniform.BufferSize()
            });
            WritePushedDescriptors();
            return MXC_SUCCESS;
        }
    };
}
