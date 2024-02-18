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

        struct UniformBuffer
        {
            glm::mat4 model;
        };

        static MXC_RESULT InitLayout(const Vulkan::Device& device)
        {
            // MXC_LOG("Init ObjectDescriptor Layout");
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
            name = "Object";
            // MXC_LOG("Init ObjectDescriptor");
            MXC_CHK(uniform.Init(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   Vulkan::Locality::Local));
            uniform.mappedBuffer->model = transform.ModelMatrix();

            MXC_CHK(AllocateDescriptorSet());
            StaticArray writes{
              (VkWriteDescriptorSet){
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = StaticRef((VkDescriptorBufferInfo){
                  .buffer = uniform.GetVkBuffer(),
                  .range = uniform.Size()})},
            };
            WriteDescriptors(writes);

            return MXC_SUCCESS;
        }

    private:
        Buffer<UniformBuffer> uniform{device};
    };
}// namespace Moxaic::Vulkan
