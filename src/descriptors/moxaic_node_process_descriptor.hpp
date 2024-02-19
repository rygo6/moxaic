#pragma once

#include "moxaic_vulkan_descriptor.hpp"
#include "moxaic_vulkan_texture.hpp"
#include "static_array.hpp"
#include "vulkan_mid.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace Moxaic::Vulkan
{
    class NodeProcessDescriptorLayout
    {
        MXC_NO_VALUE_PASS(NodeProcessDescriptorLayout);
        Vkm::DescriptorSetLayout descriptorSetLayout{};

    public:
        const VkDescriptorSetLayout& Handle{descriptorSetLayout};
        const char* const Name{"NodeProcessDescriptorLayout"};
        constexpr static uint32_t SetIndex = 0;
        enum BindingIndex : uint32_t {
            SrcTexture,
            DstTexture,
        };
        constexpr static StaticArray LayoutBindings{
          Vkm::DescriptorSetLayoutBinding{
            .binding = SrcTexture,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          Vkm::DescriptorSetLayoutBinding{
            .binding = DstTexture,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
        };

        VkResult Init(const Vulkan::Device& device)
        {
            const Vkm::DescriptorSetLayoutCreateInfo createInfo{
              .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
              .bindingCount = LayoutBindings.size(),
              .pBindings = LayoutBindings.data(),
            };
            return descriptorSetLayout.Create(device.VkDeviceHandle, Name, &createInfo);
        }
    };

    class NodeProcessPipelineLayout
    {
        MXC_NO_VALUE_PASS(NodeProcessPipelineLayout);
        Vkm::PipelineLayout pipelineLayout{};

    public:
        const VkPipelineLayout& Handle{pipelineLayout};
        const char* const Name{"NodeProcessPipelinLayout"};

        VkResult Init(const Vulkan::Device& device,
                      const NodeProcessDescriptorLayout& nodeProcessDescriptorLayout)
        {
            const StaticArray setLayouts{
              nodeProcessDescriptorLayout.Handle,
            };
            const Vkm::PipelineLayoutCreateInfo createInfo{
              .setLayoutCount = setLayouts.size(),
              .pSetLayouts = setLayouts.data(),
            };
            return pipelineLayout.Create(device.VkDeviceHandle, Name, &createInfo);
        }
    };

    class NodeProcessPipeline2
    {
        MXC_NO_VALUE_PASS(NodeProcessPipeline2);
        Vkm::ComputePipeline pipeline{};

    public:
        const VkPipeline& Handle{pipeline.handle};
        const char* const Name{"NodeProcessPipeline"};

        VkResult Init(const Vulkan::Device& device,
                      const char* const shaderPath,
                      const NodeProcessPipelineLayout& layout)
        {
            Vkm::ShaderModule shader;
            shader.Create(device.VkDeviceHandle, shaderPath);
            const Vkm::ComputePipelineCreateInfo createInfo{
              .stage{
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = shader,
                .pName = "main",
              },
              .layout = layout.Handle,
            };
            return pipeline.Create(device.VkDeviceHandle, Name, &createInfo);
        }
    };

    class NodeProcessPushDescriptor
    {
        static void EmplaceDescriptorWrite(const uint32_t bindingIndex,
                                           const Vkm::DescriptorImageInfo* pImageInfo,
                                           Vkm::WriteDescriptorSet* pWriteDescriptorSet)
        {
            new (pWriteDescriptorSet) Vkm::WriteDescriptorSet{
              .dstBinding = NodeProcessDescriptorLayout::LayoutBindings[bindingIndex].binding,
              .descriptorCount = NodeProcessDescriptorLayout::LayoutBindings[bindingIndex].descriptorCount,
              .descriptorType = NodeProcessDescriptorLayout::LayoutBindings[bindingIndex].descriptorType,
              .pImageInfo = pImageInfo,
            };
        }

        static void EmplaceSrcTextureImageInfo(const VkImageView srcImageView,
                                               const VkSampler srcSampler,
                                               Vkm::DescriptorImageInfo* pImageInfo,
                                               Vkm::WriteDescriptorSet* pWriteDescriptorSet)
        {
            new (pImageInfo) Vkm::DescriptorImageInfo{
              .sampler = srcSampler,
              .imageView = srcImageView,
              .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
            EmplaceDescriptorWrite(NodeProcessDescriptorLayout::BindingIndex::SrcTexture, pImageInfo, pWriteDescriptorSet);
        }

        static void EmplaceDstTextureImageInfo(const VkImageView dstImageView,
                                               Vkm::DescriptorImageInfo* pImageInfo,
                                               Vkm::WriteDescriptorSet* pWriteDescriptorSet)
        {
            new (pImageInfo) VkDescriptorImageInfo{
              .imageView = dstImageView,
              .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
            EmplaceDescriptorWrite(NodeProcessDescriptorLayout::BindingIndex::DstTexture, pImageInfo, pWriteDescriptorSet);
        }

    public:
        static void PushSrcDstTextureDescriptorWrite(VkCommandBuffer commandBuffer,
                                                     const VkSampler srcSampler,
                                                     const VkImageView srcImageView,
                                                     const VkImageView dstImageView,
                                                     const NodeProcessPipelineLayout& layout)
        {
            Vkm::DescriptorImageInfo imageInfos[2];
            Vkm::WriteDescriptorSet writes[2];
            EmplaceSrcTextureImageInfo(srcImageView, srcSampler, &imageInfos[0], &writes[0]);
            EmplaceDstTextureImageInfo(dstImageView, &imageInfos[1], &writes[1]);
            VkFunc.CmdPushDescriptorSetKHR(commandBuffer,
                                           VK_PIPELINE_BIND_POINT_COMPUTE,
                                           layout.Handle,
                                           NodeProcessDescriptorLayout::SetIndex,
                                           2,
                                           (VkWriteDescriptorSet*) writes);
        }
    };


    class NodeProcessDescriptor : public VulkanDescriptorBase2<NodeProcessDescriptor>
    {
    public:
        constexpr static uint32_t SetIndex = 0;
        enum BindingIndex : uint32_t {
            SrcTexture,
            DstTexture,
        };
        constexpr static StaticArray LayoutBindings{
          Vkm::DescriptorSetLayoutBinding{
            .binding = SrcTexture,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          Vkm::DescriptorSetLayoutBinding{
            .binding = DstTexture,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
        };

        static void EmplaceSrcTextureImageInfo(const VkImageView srcImageView,
                                               const VkDescriptorSet dstSet,
                                               Vkm::DescriptorImageInfo* pImageInfo,
                                               Vkm::WriteDescriptorSet* pWriteDescriptorSet)
        {
            new (pImageInfo) Vkm::DescriptorImageInfo{
              .sampler = device->VkMaxSamplerHandle,
              .imageView = srcImageView,
              .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
            EmplaceDescriptorWrite(SrcTexture, dstSet, pImageInfo, pWriteDescriptorSet);
        }

        static void EmplaceDstTextureImageInfo(const VkImageView dstImageView,
                                               const VkDescriptorSet dstSet,
                                               Vkm::DescriptorImageInfo* pImageInfo,
                                               Vkm::WriteDescriptorSet* pWriteDescriptorSet)
        {
            new (pImageInfo) VkDescriptorImageInfo{
              .imageView = dstImageView,
              .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
            EmplaceDescriptorWrite(DstTexture, dstSet, pImageInfo, pWriteDescriptorSet);
        }

        static void PushSrcDstTextureDescriptorWrite(VkCommandBuffer commandBuffer,
                                                     const VkImageView srcImageView,
                                                     const VkImageView dstImageView,
                                                     VkPipelineLayout layout)
        {
            Vkm::DescriptorImageInfo imageInfos[2];
            Vkm::WriteDescriptorSet writes[2];
            EmplaceSrcTextureImageInfo(srcImageView, VK_NULL_HANDLE, &imageInfos[0], &writes[0]);
            EmplaceDstTextureImageInfo(dstImageView, VK_NULL_HANDLE, &imageInfos[1], &writes[1]);
            VkFunc.CmdPushDescriptorSetKHR(commandBuffer,
                                           VK_PIPELINE_BIND_POINT_COMPUTE,
                                           layout,
                                           SetIndex,
                                           2,
                                           (VkWriteDescriptorSet*) writes);
        }

        void EmplaceSrcTextureDescriptorWrite(const VkImageView srcImageView,
                                              Vkm::DescriptorImageInfo* pImageInfo,
                                              Vkm::WriteDescriptorSet* pWriteDescriptorSet)
        {
            EmplaceDescriptorWrite(SrcTexture, vkDescriptorSetHandle, pImageInfo, pWriteDescriptorSet);
        }

        void EmplaceDstTextureDescriptorWrite(const VkImageView dstImageView,
                                              Vkm::DescriptorImageInfo* pImageInfo,
                                              Vkm::WriteDescriptorSet* pWriteDescriptorSet)
        {
            EmplaceDescriptorWrite(DstTexture, vkDescriptorSetHandle, pImageInfo, pWriteDescriptorSet);
        }
    };
}// namespace Moxaic::Vulkan
