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

        struct UniformBuffer
        {
            glm::mat4 view;
            glm::mat4 proj;
            glm::mat4 viewProj;
            glm::mat4 invView;
            glm::mat4 invProj;
            glm::mat4 invViewProj;
            glm::ivec2 screenSize;
        };

        static MXC_RESULT InitLayout(const Vulkan::Device& device)
        {
            // MXC_LOG("Init GlobalDescriptor Layout");
            StaticArray bindings{
              VkDescriptorSetLayoutBinding{
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
            name = "Global";
            // MXC_LOG("Init GlobalDescriptor");
            MXC_CHK(uniform.Init( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 Vulkan::Locality::Local));
            localBuffer.screenSize.x = dimensions.width;
            localBuffer.screenSize.y = dimensions.height;
            localBuffer.proj = camera.GetProjection();
            localBuffer.view = camera.GetView();
            localBuffer.viewProj = camera.GetViewProjection();
            localBuffer.invView = camera.GetInverseView();
            localBuffer.invProj = camera.GetInverseProjection();
            localBuffer.invViewProj = camera.GetInverseViewProjection();
            WriteLocalBuffer();

            MXC_CHK(AllocateDescriptorSet());
            StaticArray writes{
              VkWriteDescriptorSet{
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = StaticRef{VkDescriptorBufferInfo{
                  .buffer = uniform.GetVkBuffer(),
                  .range = uniform.Size()}}},
            };
            WriteDescriptors(writes);

            return MXC_SUCCESS;
        }

        /// Bypass local buffer copy
        void WriteBuffer(const UniformBuffer& buffer)
        {
            uniform.CopyBuffer(buffer);
        }

        void WriteLocalBuffer()
        {
            uniform.CopyBuffer(localBuffer);
        }

        void SetLocalBufferView(const Camera& camera)
        {
            localBuffer.view = camera.GetView();
            localBuffer.invView = camera.GetInverseView();
            localBuffer.viewProj = camera.GetViewProjection();
            localBuffer.invViewProj = camera.GetInverseViewProjection();
        }

        UniformBuffer localBuffer{};

    private:
        Buffer<UniformBuffer> uniform{device};
    };
}// namespace Moxaic::Vulkan
