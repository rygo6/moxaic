#pragma once

#include "../descriptors/moxaic_vulkan_descriptor.hpp"
#include "../utilities/static_array.hpp"
#include "../utilities/vulkan_medium.hpp"

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

        NodeProcessDescriptorLayout(const VkDevice deviceHandle, const VkSampler sampler)
        {
            VKM_ASSERT(Create(deviceHandle, sampler));
        }

        VkResult Create(const VkDevice deviceHandle, const VkSampler srcSampler)
        {
            const StaticArray layoutBindings{
              LayoutBindings[SrcTexture].WithSamplers(&srcSampler),
              LayoutBindings[DstTexture],
            };
            const Vkm::DescriptorSetLayoutCreateInfo createInfo{
              .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
              .bindingCount = layoutBindings.size(),
              .pBindings = layoutBindings.data(),
            };
            return Vkm::DescriptorSetLayout::Create(deviceHandle, Name, &createInfo);
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
                                               Vkm::DescriptorImageInfo* pImageInfo,
                                               Vkm::WriteDescriptorSet* pWriteDescriptorSet)
        {
            new (pImageInfo) Vkm::DescriptorImageInfo{
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
                                              const VkImageView srcImageView,
                                              const VkImageView dstImageView)
        {
            Vkm::DescriptorImageInfo imageInfos[2];
            Vkm::WriteDescriptorSet writes[2];
            NodeProcessDescriptorLayout::EmplaceSrcTextureImageInfo(srcImageView, &imageInfos[0], &writes[0]);
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

        NodeProcessPipeline(const NodeProcessPipelineLayout& layout,
                            const char* const shaderPath)
        {
            VKM_ASSERT(Create(layout, shaderPath));
        }

        VkResult Create(const NodeProcessPipelineLayout& layout,
                        const char* const shaderPath)
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
}// namespace Moxaic::Vulkan
