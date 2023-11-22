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

        struct Buffer
        {
            glm::mat4 model;
        };

        MXC_RESULT Init(const Transform& transform)
        {
            MXC_LOG("Init ObjectDescriptor");
            SDL_assert(m_VkDescriptorSet == nullptr);

            // todo should this be ina  different method so I can call them all before trying make any descriptors???
            if (initializeLayout()) {
                StaticArray bindings{
                  (VkDescriptorSetLayoutBinding){
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                                  VK_SHADER_STAGE_FRAGMENT_BIT,
                  },
                };
                MXC_CHK(CreateDescriptorSetLayout(bindings));
            }

            MXC_CHK(m_Uniform.Init(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   Vulkan::Locality::Local));
            m_Uniform.mapped().model = transform.modelMatrix();

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
        Uniform<Buffer> m_Uniform{k_Device};
    };
}// namespace Moxaic::Vulkan
