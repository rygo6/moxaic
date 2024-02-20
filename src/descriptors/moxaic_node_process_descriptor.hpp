#pragma once

#include "moxaic_vulkan_descriptor.hpp"
#include "moxaic_vulkan_texture.hpp"
#include "static_array.hpp"
#include "vulkan_medium.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace Moxaic::Vulkan
{
    struct NodeProcessDescriptorLayout : Vkm::DescriptorSetLayout
    {
        constexpr static const char* Name{"NodeProcessDescriptorLayout"};
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
        constexpr static Vkm::DescriptorSetLayoutCreateInfo CreateInfo{
          .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
          .bindingCount = LayoutBindings.size(),
          .pBindings = LayoutBindings.data(),
        };

        NodeProcessDescriptorLayout(const VkDevice deviceHandle)
        {
            VKM_ASSERT(Create(deviceHandle));
        }

        VkResult Create(const VkDevice deviceHandle)
        {
            return Vkm::DescriptorSetLayout::Create(deviceHandle, Name, &CreateInfo);
        }

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
    };

    struct NodeProcessPipelineLayout : Vkm::PipelineLayout
    {
        constexpr static const char* Name{"NodeProcessPipelinLayout"};

        NodeProcessPipelineLayout(const NodeProcessDescriptorLayout& nodeProcessDescriptorLayout)
        {
            VKM_ASSERT(Create(nodeProcessDescriptorLayout));
        }

        VkResult Create(const NodeProcessDescriptorLayout& nodeProcessDescriptorLayout)
        {
            const StaticArray setLayouts{
              nodeProcessDescriptorLayout.handle,
            };
            const Vkm::PipelineLayoutCreateInfo createInfo{
              .setLayoutCount = setLayouts.size(),
              .pSetLayouts = setLayouts.data(),
            };
            return Vkm::PipelineLayout::Create(nodeProcessDescriptorLayout.deviceHandle, Name, &createInfo);
        }

        void PushSrcDstTextureDescriptorWrite(const VkCommandBuffer commandBuffer,
                                              const VkSampler srcSampler,
                                              const VkImageView srcImageView,
                                              const VkImageView dstImageView)
        {
            Vkm::DescriptorImageInfo imageInfos[2];
            Vkm::WriteDescriptorSet writes[2];
            NodeProcessDescriptorLayout::EmplaceSrcTextureImageInfo(srcImageView, srcSampler, &imageInfos[0], &writes[0]);
            NodeProcessDescriptorLayout::EmplaceDstTextureImageInfo(dstImageView, &imageInfos[1], &writes[1]);
            VkFunc.CmdPushDescriptorSetKHR(commandBuffer,
                                           VK_PIPELINE_BIND_POINT_COMPUTE,
                                           handle,
                                           NodeProcessDescriptorLayout::SetIndex,
                                           2,
                                           (VkWriteDescriptorSet*) writes);
        }
    };

    struct NodeProcessPipeline : Vkm::ComputePipeline
    {
        constexpr static const char* Name{"NodeProcessPipeline"};
        constexpr static int LocalSize{32};

        NodeProcessPipeline(const char* const shaderPath,
                            const NodeProcessPipelineLayout& layout)
        {
            VKM_ASSERT(Create(shaderPath, layout));
        }

        VkResult Create(const char* const shaderPath,
                        const NodeProcessPipelineLayout& layout)
        {
            Vkm::ShaderModule shader;
            VKM_CHECK(shader.Create(layout.deviceHandle, shaderPath));
            const Vkm::PipelineShaderStageCreateInfo stage{
              .stage = VK_SHADER_STAGE_COMPUTE_BIT,
              .module = shader,
              .pName = "main",
            };
            const Vkm::ComputePipelineCreateInfo createInfo{
              .stage = stage,
              .layout = layout,
            };
            return Vkm::ComputePipeline::Create(layout.deviceHandle, Name, &createInfo);
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
