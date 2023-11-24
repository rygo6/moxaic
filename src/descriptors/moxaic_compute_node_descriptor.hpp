#pragma once

#include "moxaic_global_descriptor.hpp"
#include "moxaic_vulkan_descriptor.hpp"
#include "moxaic_vulkan_framebuffer.hpp"
#include "static_array.hpp"

namespace Moxaic::Vulkan
{
    class ComputeNodeDescriptor : public VulkanDescriptorBase<ComputeNodeDescriptor>
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;

        static MXC_RESULT InitLayout(Vulkan::Device const& device)
        {
            SDL_assert(s_VkDescriptorSetLayout == VK_NULL_HANDLE);
            StaticArray bindings{
              (VkDescriptorSetLayoutBinding){
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .stageFlags = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              },
              (VkDescriptorSetLayoutBinding){
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              },
              (VkDescriptorSetLayoutBinding){
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              },
              (VkDescriptorSetLayoutBinding){
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              },
              (VkDescriptorSetLayoutBinding){
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              },
            };
            MXC_CHK(CreateDescriptorSetLayout(device, bindings));
            return MXC_SUCCESS;
        }

        MXC_RESULT Init(GlobalDescriptor::Buffer const& buffer, Framebuffer const& framebuffer)
        {
            MXC_LOG("Init ComputeNodeDescriptor");
            SDL_assert(s_VkDescriptorSetLayout != VK_NULL_HANDLE);
            SDL_assert(m_VkDescriptorSet == nullptr);

            MXC_CHK(m_Uniform.Init(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   Vulkan::Locality::Local));
            m_Buffer = buffer;
            Update();

            MXC_CHK(AllocateDescriptorSet());
            StaticArray writes{
              (VkWriteDescriptorSet){
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = StaticRef((VkDescriptorBufferInfo){
                  .buffer = m_Uniform.vkBuffer(),
                  .range = m_Uniform.Size()})},
              (VkWriteDescriptorSet){
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = k_pDevice->GetVkLinearSampler(),
                  .imageView = framebuffer.colorTexture().vkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL})},
              (VkWriteDescriptorSet){
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = k_pDevice->GetVkLinearSampler(),
                  .imageView = framebuffer.normalTexture().vkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL})},
              (VkWriteDescriptorSet){
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = k_pDevice->GetVkLinearSampler(),
                  .imageView = framebuffer.gBufferTexture().vkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL})},
              (VkWriteDescriptorSet){
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = k_pDevice->GetVkLinearSampler(),
                  .imageView = framebuffer.depthTexture().vkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL})},
            };
            WriteDescriptors(writes);

            return MXC_SUCCESS;
        }

        void Update()
        {
            m_Uniform.CopyBuffer(m_Buffer);
        }

        void Update(GlobalDescriptor::Buffer const& buffer)
        {
            m_Buffer = buffer;
            Update();
        }

    private:
        GlobalDescriptor::Buffer m_Buffer{};// is there a case where I wouldn't want a local copy!?
        Uniform<GlobalDescriptor::Buffer> m_Uniform{*k_pDevice};
    };
}// namespace Moxaic::Vulkan
