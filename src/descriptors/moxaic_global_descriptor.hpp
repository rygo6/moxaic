#pragma once

#include "moxaic_vulkan_descriptor.hpp"

#include "../moxaic_vulkan_uniform.hpp"
#include "../moxaic_camera.hpp"

#include <glm/glm.hpp>

namespace Moxaic::Vulkan
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

        inline MXC_RESULT Init(Camera &camera, VkExtent2D dimensions)
        {
            MXC_LOG("Init GlobalDescriptor");

            if (initializeLayout()) {
                std::array bindings{
                        (VkDescriptorSetLayoutBinding) {
                                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                                              VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT |
                                              VK_SHADER_STAGE_COMPUTE_BIT |
                                              VK_SHADER_STAGE_FRAGMENT_BIT |
                                              VK_SHADER_STAGE_MESH_BIT_EXT |
                                              VK_SHADER_STAGE_TASK_BIT_EXT,
                        }
                };
                MXC_CHK(CreateDescriptorSetLayout(bindings.size(),
                                                  bindings.data()));
            };

            MXC_CHK(m_Uniform.Init(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   Vulkan::Locality::Local));
            m_Uniform.mapped().width = dimensions.width;
            m_Uniform.mapped().height = dimensions.height;
            m_Uniform.mapped().proj = camera.projection();
            m_Uniform.mapped().invProj = camera.inverseProjection();
            m_Uniform.mapped().view = camera.view();
            m_Uniform.mapped().invView = camera.inverseView();

            MXC_CHK(AllocateDescriptorSet());
            const VkDescriptorBufferInfo globalUBOInfo{
                    .buffer = m_Uniform.vkBuffer(),
                    .range = m_Uniform.Size()
            };
            std::array writes{
                    (VkWriteDescriptorSet) {
                            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            .pBufferInfo = &globalUBOInfo
                    },
            };
            WriteDescriptors(writes.size(),
                             writes.data());

            return MXC_SUCCESS;
        }

        inline void Update(const Buffer &buffer)
        {
            m_Uniform.CopyBuffer(buffer);
        }

        inline void UpdateView(Camera &camera)
        {
            m_Uniform.mapped().view = camera.view();
            m_Uniform.mapped().invView = camera.inverseView();
        }

    private:
        Uniform<Buffer> m_Uniform{k_Device};
    };
}
