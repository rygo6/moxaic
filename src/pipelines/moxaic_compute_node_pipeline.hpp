#pragma once

#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_pipeline.hpp"

#include "moxaic_compute_node_descriptor.hpp"
#include "moxaic_global_descriptor.hpp"

#include "moxaic_logging.hpp"

#include "static_array.hpp"

#include <string>
#include <vulkan/vulkan.h>

namespace Moxaic::Vulkan
{
    class ComputeNodePipeline : public ComputePipeline<ComputeNodePipeline>
    {
    public:
        using ComputePipeline::ComputePipeline;

        constexpr static uint32_t LocalSize = 32;

        static MXC_RESULT InitLayout(const Vulkan::Device& device)
        {
            SDL_assert(s_vkPipelineLayout == VK_NULL_HANDLE);
            const StaticArray setLayouts{
              GlobalDescriptor::GetOrInitVkDescriptorSetLayout(device),
              ComputeNodeDescriptor::GetOrInitVkDescriptorSetLayout(device),
            };
            MXC_CHK(CreateLayout(device, setLayouts));
            return MXC_SUCCESS;
        }

        MXC_RESULT Init(const char* shaderPath)
        {
            MXC_LOG("Init ComputeNodePipeline", shaderPath);

            VkShaderModule shader;
            MXC_CHK(CreateShaderModule(shaderPath,
                                       &shader));
            const VkPipelineShaderStageCreateInfo compStage{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = VK_SHADER_STAGE_COMPUTE_BIT,
              .module = shader,
              .pName = "main",
            };
            MXC_CHK(CreateComputePipe(compStage));
            vkDestroyShaderModule(k_pDevice->  GetVkDevice(), shader, VK_ALLOC);

            return MXC_SUCCESS;
        }

        static void BindDescriptor(const VkCommandBuffer commandBuffer,
                                   const GlobalDescriptor& descriptor)
        {
            BindDescriptorS(commandBuffer,
                            descriptor.GetVkDescriptorSet(),
                            GlobalDescriptor::SetIndex);
        }

        static void BindDescriptor(const VkCommandBuffer commandBuffer,
                                   const ComputeNodeDescriptor& descriptor)
        {
            BindDescriptorS(commandBuffer,
                            descriptor.GetVkDescriptorSet(),
                            ComputeNodeDescriptor::SetIndex);
        }
    };
}// namespace Moxaic::Vulkan
