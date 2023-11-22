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

        struct Buffer
        {
            glm::mat4 view;
            glm::mat4 proj;
            glm::mat4 invView;
            glm::mat4 invProj;
            uint32_t width;
            uint32_t height;
        };

        MXC_RESULT Init(const Camera& camera, const VkExtent2D& dimensions)
        {
            MXC_LOG("Init GlobalDescriptor");
            SDL_assert(m_VkDescriptorSet == nullptr);

            // todo should this be ina  different method so I can call them all before trying make any descriptors???
            if (initializeLayout()) {
                StaticArray bindings{
                  (VkDescriptorSetLayoutBinding){
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                                  VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT |
                                  VK_SHADER_STAGE_COMPUTE_BIT |
                                  VK_SHADER_STAGE_FRAGMENT_BIT |
                                  VK_SHADER_STAGE_MESH_BIT_EXT |
                                  VK_SHADER_STAGE_TASK_BIT_EXT,
                  },
                };
                MXC_CHK(CreateDescriptorSetLayout(bindings));
            }

            MXC_CHK(m_Uniform.Init(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   Vulkan::Locality::Local));
            m_Buffer.width = dimensions.width;
            m_Buffer.height = dimensions.height;
            m_Buffer.proj = camera.projection();
            m_Buffer.invProj = camera.inverseProjection();
            m_Buffer.view = camera.view();
            m_Buffer.invView = camera.inverseView();
            Update();

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

        void Update()
        {
            m_Uniform.CopyBuffer(m_Buffer);
        }

        void Update(const Buffer& buffer)
        {
            m_Buffer = buffer;
            Update();
        }

        void UpdateView(const Camera& camera)
        {
            m_Buffer.view = camera.view();
            m_Buffer.invView = camera.inverseView();
            Update();
        }

        const auto& buffer() const { return m_Buffer; }

    private:
        Buffer m_Buffer{};// is there a case where I wouldn't want a local copy!?
        Uniform<Buffer> m_Uniform{k_Device};
    };
}// namespace Moxaic::Vulkan
