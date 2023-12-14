#pragma once

#include "moxaic_camera.hpp"
#include "moxaic_vulkan_descriptor.hpp"
#include "moxaic_vulkan_uniform.hpp"
#include "static_array.hpp"

#include <glm/glm.hpp>

namespace Moxaic::Vulkan
{
    class GlobalDescriptor : public VulkanDescriptorBase<GlobalDescriptor>
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;

        constexpr static int SetIndex = 0;

        struct Buffer
        {
            glm::mat4 view;
            glm::mat4 proj;
            glm::mat4 viewProj;
            glm::mat4 invView;
            glm::mat4 invProj;
            glm::mat4 invViewProj;
            uint32_t width;
            uint32_t height;
        };

        static MXC_RESULT InitLayout(const Vulkan::Device& device)
        {
            MXC_LOG("Init GlobalDescriptor Layout");
            StaticArray bindings{
              (VkDescriptorSetLayoutBinding){
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                              VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT |
                              VK_SHADER_STAGE_COMPUTE_BIT |
                              VK_SHADER_STAGE_FRAGMENT_BIT |
                              VK_SHADER_STAGE_MESH_BIT_EXT |
                              VK_SHADER_STAGE_TASK_BIT_EXT},
            };
            MXC_CHK(CreateDescriptorSetLayout(device, bindings));
            return MXC_SUCCESS;
        }

        MXC_RESULT Init(const Camera& camera, const VkExtent2D& dimensions)
        {
            MXC_LOG("Init GlobalDescriptor");

            MXC_CHK(m_Uniform.Init(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   Vulkan::Locality::Local));
            m_LocalBuffer.width = dimensions.width;
            m_LocalBuffer.height = dimensions.height;
            m_LocalBuffer.proj = camera.projection();
            m_LocalBuffer.view = camera.view();
            m_LocalBuffer.viewProj = camera.viewProjection();
            m_LocalBuffer.invView = camera.inverseView();
            m_LocalBuffer.invProj = camera.inverseProjection();
            m_LocalBuffer.invViewProj = camera.inverseViewProjection();
            WriteLocalBuffer();

            MXC_CHK(AllocateDescriptorSet());
            StaticArray writes{
              (VkWriteDescriptorSet){
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = StaticRef((VkDescriptorBufferInfo){
                  .buffer = m_Uniform.vkBuffer(),
                  .range = m_Uniform.Size()})},
            };
            WriteDescriptors(writes);

            return MXC_SUCCESS;
        }

        /// Bypass local buffer copy
        void WriteBuffer(const Buffer& buffer)
        {
            m_Uniform.CopyBuffer(buffer);
        }

        void WriteLocalBuffer()
        {
            m_Uniform.CopyBuffer(m_LocalBuffer);
        }

        void SetLocalBufferView(const Camera& camera)
        {
            m_LocalBuffer.view = camera.view();
            m_LocalBuffer.invView = camera.inverseView();
            m_LocalBuffer.viewProj = camera.viewProjection();
            m_LocalBuffer.invViewProj = camera.inverseViewProjection();
        }

        MXC_PTR_ACCESS(LocalBuffer);
        MXC_GETSET(LocalBuffer);

    private:
        Buffer m_LocalBuffer{};// is there a case where I wouldn't want a local copy!?
        Uniform<Buffer> m_Uniform{k_pDevice};
    };
}// namespace Moxaic::Vulkan
