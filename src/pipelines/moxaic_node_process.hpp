#pragma once

#include "../descriptors/moxaic_vulkan_descriptor.hpp"
#include "../utilities/static_array.hpp"
#include "../utilities/vulkan_medium.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace Moxaic::Vulkan
{
    struct NodeProcessDescriptorLayout : MVk::DescriptorSetLayout
    {
        constexpr static const char* Name{"NodeProcessDescriptorLayout"};
        constexpr static uint32_t SetIndex{0};
        enum BindingIndex : uint32_t {
            SrcTexture,
            DstTexture,
        };
        constexpr static StaticArray LayoutBindings{
          MVk::DescriptorSetLayoutBinding{
            .binding = SrcTexture,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
          MVk::DescriptorSetLayoutBinding{
            .binding = DstTexture,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          },
        };

        VkResult Create(const VkDevice deviceHandle, const VkSampler srcSampler)
        {
            const StaticArray layoutBindings{
              LayoutBindings[SrcTexture].WithSamplers(&srcSampler),
              LayoutBindings[DstTexture],
            };
            VKM_CHECK(MVk::DescriptorSetLayout::Create(
              deviceHandle,
              Name,
              {
                .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
                .bindingCount = layoutBindings.size(),
                .pBindings = layoutBindings.data(),
              }));
            return VK_SUCCESS;
        }

        static void EmplaceDescriptorWrite(const uint32_t bindingIndex,
                                           const MVk::DescriptorImageInfo* pImageInfo,
                                           MVk::WriteDescriptorSet* pWriteDescriptorSet)
        {
            new (pWriteDescriptorSet) MVk::WriteDescriptorSet{
              .dstBinding = NodeProcessDescriptorLayout::LayoutBindings[bindingIndex].binding,
              .descriptorCount = NodeProcessDescriptorLayout::LayoutBindings[bindingIndex].descriptorCount,
              .descriptorType = NodeProcessDescriptorLayout::LayoutBindings[bindingIndex].descriptorType,
              .pImageInfo = pImageInfo,
            };
        }

        static void EmplaceSrcTextureImageInfo(const VkImageView srcImageView,
                                               MVk::DescriptorImageInfo* pImageInfo,
                                               MVk::WriteDescriptorSet* pWriteDescriptorSet)
        {
            new (pImageInfo) MVk::DescriptorImageInfo{
              .imageView = srcImageView,
              .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
            EmplaceDescriptorWrite(NodeProcessDescriptorLayout::BindingIndex::SrcTexture, pImageInfo, pWriteDescriptorSet);
        }

        static void EmplaceDstTextureImageInfo(const VkImageView dstImageView,
                                               MVk::DescriptorImageInfo* pImageInfo,
                                               MVk::WriteDescriptorSet* pWriteDescriptorSet)
        {
            new (pImageInfo) VkDescriptorImageInfo{
              .imageView = dstImageView,
              .imageLayout = VK_IMAGE_LAYOUT_GENERAL};
            EmplaceDescriptorWrite(NodeProcessDescriptorLayout::BindingIndex::DstTexture, pImageInfo, pWriteDescriptorSet);
        }
    };

    struct NodeProcessPipelineLayout : MVk::PipelineLayout
    {
        constexpr static const char* Name{"NodeProcessPipelinLayout"};

        VkResult Create(const NodeProcessDescriptorLayout& nodeProcessDescriptorLayout)
        {
            const StaticArray setLayouts{
              nodeProcessDescriptorLayout.vkHandle,
            };
            const MVk::PipelineLayoutCreateInfo createInfo{
              .setLayoutCount = setLayouts.size(),
              .pSetLayouts = setLayouts.data(),
            };
            VKM_CHECK(MVk::PipelineLayout::Create(nodeProcessDescriptorLayout.vkDeviceHandle, Name, &createInfo));
            return VK_SUCCESS;
        }

        void PushSrcDstTextureDescriptorWrite(const VkCommandBuffer commandBuffer,
                                              const VkImageView srcImageView,
                                              const VkImageView dstImageView)
        {
            MVk::DescriptorImageInfo imageInfos[2];
            MVk::WriteDescriptorSet writes[2];
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

    struct NodeProcessPipeline : MVk::ComputePipeline
    {
        constexpr static const char* Name{"NodeProcessPipeline"};
        constexpr static int LocalSize{32};

        VkResult Create(const NodeProcessPipelineLayout& layout,
                        const char* const pShaderPath)
        {
            MVk::ShaderModule shader;
            VKM_CHECK(shader.Create(layout.deviceHandle, pShaderPath));
            const MVk::PipelineShaderStageCreateInfo stage{
              .stage = VK_SHADER_STAGE_COMPUTE_BIT,
              .module = shader,
            };
            const MVk::ComputePipelineCreateInfo createInfo{
              .stage = stage,
              .layout = layout,
            };
            VKM_CHECK(MVk::ComputePipeline::Create(layout.deviceHandle, Name, &createInfo));
            return VK_SUCCESS;
        }
    };
}// namespace Moxaic::Vulkan
