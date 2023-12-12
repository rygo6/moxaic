#pragma once

#include "moxaic_vulkan_descriptor.hpp"

#include "moxaic_transform.hpp"
#include "moxaic_vulkan_uniform.hpp"
#include "static_array.hpp"

#include "glm/glm.hpp"

namespace Moxaic::Vulkan
{
    class ObjectDescriptor : public VulkanDescriptorBase<ObjectDescriptor>
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;

        constexpr static int SetIndex = 2;

        struct Buffer
        {
            glm::mat4 model;
        };

        static MXC_RESULT InitLayout(const Vulkan::Device& device)
        {
            MXC_LOG("Init ObjectDescriptor Layout");
            StaticArray bindings{
              (VkDescriptorSetLayoutBinding){
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                              VK_SHADER_STAGE_FRAGMENT_BIT},
            };
            MXC_CHK(CreateDescriptorSetLayout(device, bindings));
            return MXC_SUCCESS;
        }

        MXC_RESULT Init(const Transform& transform)
        {
            MXC_LOG("Init ObjectDescriptor");

            MXC_CHK(m_Uniform.Init(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   Vulkan::Locality::Local));
            m_Uniform.mapped().model = transform.ModelMatrix();

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

    private
        :
        Uniform<Buffer> m_Uniform{k_pDevice};
    };
}// namespace Moxaic::Vulkan
