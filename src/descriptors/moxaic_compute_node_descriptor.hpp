#pragma once

#include "moxaic_global_descriptor.hpp"
#include "moxaic_vulkan_descriptor.hpp"
#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_texture.hpp"
#include "static_array.hpp"

#include <vulkan/vulkan.h>

namespace Moxaic::Vulkan
{
    class ComputeNodeDescriptor : public VulkanDescriptorBase<ComputeNodeDescriptor>
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;

        constexpr static int SetIndex = 1;

        static MXC_RESULT InitLayout(const Vulkan::Device& device)
        {
            MXC_LOG("Init ComputeNodeDescriptor Layout");
            StaticArray bindings{
              // node ubo
              (VkDescriptorSetLayoutBinding){
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
              },
              // color
              (VkDescriptorSetLayoutBinding){
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
              },
              // normal
              (VkDescriptorSetLayoutBinding){
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
              },
              // gbuffer
              (VkDescriptorSetLayoutBinding){
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
              },
              // depth
              (VkDescriptorSetLayoutBinding){
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
              },
              // output atomic color
              (VkDescriptorSetLayoutBinding){
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
              },
              // output swap/final
              (VkDescriptorSetLayoutBinding){
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
              },
            };
            MXC_CHK(CreateDescriptorSetLayout(device, bindings));
            return MXC_SUCCESS;
        }

        MXC_RESULT Init(const GlobalDescriptor::Buffer& buffer,
                        const Framebuffer& framebuffer,
                        const Texture& outputAtomicTexture,
                        const VkImageView outputColorImage)
        {
            MXC_LOG("Init ComputeNodeDescriptor");

            MXC_CHK(m_Uniform.Init(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   Vulkan::Locality::Local));
            m_LocalBuffer = buffer;
            WriteLocalBuffer();

            MXC_CHK(AllocateDescriptorSet());
            StaticArray writes{
              // node ubo
              (VkWriteDescriptorSet){
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = StaticRef((VkDescriptorBufferInfo){
                  .buffer = m_Uniform.vkBuffer(),
                  .range = m_Uniform.Size()})},
              //' color
              (VkWriteDescriptorSet){
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = k_pDevice->GetVkLinearSampler(),
                  .imageView = framebuffer.GetColorTexture().GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
              // normal
              (VkWriteDescriptorSet){
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = k_pDevice->GetVkLinearSampler(),
                  .imageView = framebuffer.GetNormalTexture().GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
              // gbuffer
              (VkWriteDescriptorSet){
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = k_pDevice->GetVkLinearSampler(),
                  .imageView = framebuffer.GetGBufferTexture().GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
              // depth
              (VkWriteDescriptorSet){
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = k_pDevice->GetVkLinearSampler(),
                  .imageView = framebuffer.GetGBufferTexture().GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
              // output atomic image
              (VkWriteDescriptorSet){
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .imageView = outputAtomicTexture.GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
              // output swap/final
              (VkWriteDescriptorSet){
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .imageView = outputColorImage,
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
            };
            WriteDescriptors(writes);
            return MXC_SUCCESS;
        }

        MXC_RESULT WriteFramebuffer(const Framebuffer& framebuffer)
        {
            StaticArray writes{
              (VkWriteDescriptorSet){
                .dstBinding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = k_pDevice->GetVkLinearSampler(),
                  .imageView = framebuffer.GetColorTexture().GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
              (VkWriteDescriptorSet){
                .dstBinding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = k_pDevice->GetVkLinearSampler(),
                  .imageView = framebuffer.GetNormalTexture().GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
              (VkWriteDescriptorSet){
                .dstBinding = 3,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = k_pDevice->GetVkLinearSampler(),
                  .imageView = framebuffer.GetGBufferTexture().GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
              (VkWriteDescriptorSet){
                .dstBinding = 4,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = k_pDevice->GetVkLinearSampler(),
                  .imageView = framebuffer.GetDepthTexture().GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
            };
            WriteDescriptors(writes);
            return MXC_SUCCESS;
        }

        MXC_RESULT WriteOutputColorImage(const VkImageView outputColorImage)
        {
            StaticArray writes{
              (VkWriteDescriptorSet){
                .dstBinding = 6,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .imageView = outputColorImage,
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
            };
            WriteDescriptors(writes);
            return MXC_SUCCESS;
        }

        void WriteLocalBuffer()
        {
            m_Uniform.CopyBuffer(m_LocalBuffer);
        }

        void SetLocalBuffer(const GlobalDescriptor::Buffer& buffer)
        {
            m_LocalBuffer = buffer;
        }

    private:
        GlobalDescriptor::Buffer m_LocalBuffer{};// is there a case where I wouldn't want a local copy!?
        Uniform<GlobalDescriptor::Buffer> m_Uniform{*k_pDevice};
    };
}// namespace Moxaic::Vulkan
