#pragma once

#include "moxaic_global_descriptor.hpp"
#include "moxaic_vulkan_descriptor.hpp"
#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_texture.hpp"
#include "static_array.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace Moxaic::Vulkan
{
    class NodeProcessDescriptor : public VulkanDescriptorBase<NodeProcessDescriptor>
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;
        constexpr static int SetIndex = 0;

        enum Indices : uint32_t {
            DepthTexture,
            GBufferTexture,
        };

        static MXC_RESULT InitLayout(const Vulkan::Device& device)
        {
            MXC_LOG("InitLayout NodeProcessDescriptor");
            StaticArray bindings{
              (VkDescriptorSetLayoutBinding){
                .binding = Indices::DepthTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
              },
              (VkDescriptorSetLayoutBinding){
                .binding = Indices::GBufferTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
              },
            };
            MXC_CHK(CreateDescriptorSetLayout(device, bindings));
            return MXC_SUCCESS;
        }

        MXC_RESULT Init()
        {
            MXC_LOG("Init NodeProcessDescriptor");
            MXC_CHK(AllocateDescriptorSet());
            return MXC_SUCCESS;
        }

        MXC_RESULT WriteFramebuffer(const Framebuffer& framebuffer) const
        {
            StaticArray writes{
              (VkWriteDescriptorSet){
                .dstBinding = Indices::DepthTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = Device->VkNearestSamplerHandle,
                  .imageView = framebuffer.GetDepthTexture().VkImageViewHandle,
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
              (VkWriteDescriptorSet){
                .dstBinding = Indices::GBufferTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .imageView = framebuffer.GetGBufferTexture().VkImageViewHandle,
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
            };
            WriteDescriptors(writes);
            return MXC_SUCCESS;
        }
    };
}// namespace Moxaic::Vulkan
