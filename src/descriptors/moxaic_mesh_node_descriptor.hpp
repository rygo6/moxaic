#pragma once

#include "moxaic_global_descriptor.hpp"
#include "moxaic_vulkan_descriptor.hpp"
#include "moxaic_vulkan_framebuffer.hpp"
#include "static_array.hpp"

namespace Moxaic::Vulkan
{
    class MeshNodeDescriptor : public VulkanDescriptorBase<MeshNodeDescriptor>
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;

        constexpr static int SetIndex = 1;

        static MXC_RESULT InitLayout(const Vulkan::Device& device)
        {
            MXC_LOG("Init MeshNodeDescriptor Layout");
            StaticArray bindings{
              (VkDescriptorSetLayoutBinding){
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_TASK_BIT_EXT |
                              VK_SHADER_STAGE_MESH_BIT_EXT,
              },
              (VkDescriptorSetLayoutBinding){
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_TASK_BIT_EXT |
                              VK_SHADER_STAGE_MESH_BIT_EXT,
              },
              (VkDescriptorSetLayoutBinding){
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_TASK_BIT_EXT |
                              VK_SHADER_STAGE_MESH_BIT_EXT,
              },
              (VkDescriptorSetLayoutBinding){
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_TASK_BIT_EXT |
                              VK_SHADER_STAGE_MESH_BIT_EXT,
              },
            };
            MXC_CHK(CreateDescriptorSetLayout(device, bindings));
            return MXC_SUCCESS;
        }

        MXC_RESULT Init(const GlobalDescriptor::UniformBuffer& buffer, const Framebuffer& framebuffer)
        {
            name = "MeshNode";
            MXC_LOG("Init MaterialDescriptor");
            SDL_assert(sharedVkDescriptorSetLayout != VK_NULL_HANDLE);
            SDL_assert(vkDescriptorSet == nullptr);

            MXC_CHK(m_Uniform.Init(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   Vulkan::Locality::Local));
            m_LocalBuffer = buffer;
            WriteLocalBuffer();

            MXC_CHK(AllocateDescriptorSet());
            StaticArray writes{
              VkWriteDescriptorSet{
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = StaticRef{VkDescriptorBufferInfo{
                  .buffer = m_Uniform.GetVkBuffer(),
                  .range = m_Uniform.Size()}}},
              VkWriteDescriptorSet{
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef{VkDescriptorImageInfo{
                  .sampler = device->GetVkLinearSampler(),
                  .imageView = framebuffer.GetColorTexture().VkImageViewHandle,
                  .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}}},
              VkWriteDescriptorSet{
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef{VkDescriptorImageInfo{
                  .sampler = device->GetVkLinearSampler(),
                  .imageView = framebuffer.GetNormalTexture().VkImageViewHandle,
                  .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}}},
              VkWriteDescriptorSet{
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef{VkDescriptorImageInfo{
                  .sampler = device->GetVkLinearSampler(),
                  .imageView = framebuffer.GetGBufferTexture().VkImageViewHandle,
                  .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}}},
            };
            WriteDescriptors(writes);

            return MXC_SUCCESS;
        }

        void WriteLocalBuffer()
        {
            m_Uniform.CopyBuffer(m_LocalBuffer);
        }

        void SetLocalBuffer(const GlobalDescriptor::UniformBuffer& buffer)
        {
            m_LocalBuffer = buffer;
        }

    private:
        GlobalDescriptor::UniformBuffer m_LocalBuffer{};// is there a case where I wouldn't want a local copy!?
        Buffer<GlobalDescriptor::UniformBuffer> m_Uniform{device};
    };
}// namespace Moxaic::Vulkan
