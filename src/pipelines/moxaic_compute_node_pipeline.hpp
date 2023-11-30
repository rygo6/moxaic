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

        MXC_RESULT Init()
        {
            MXC_LOG("Init ComputeNodePipeline");

            VkShaderModule clearShader;
            MXC_CHK(CreateShaderModule("./shaders/compute_node_pre.comp.spv",
                                       &clearShader));
            const VkPipelineShaderStageCreateInfo clearStage{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = VK_SHADER_STAGE_COMPUTE_BIT,
              .module = clearShader,
              .pName = "main",
            };
            MXC_CHK(CreateComputePipe(clearStage, &m_VkPrePipeline));
            vkDestroyShaderModule(k_pDevice->GetVkDevice(), clearShader, VK_ALLOC);

            VkShaderModule compShader;
            MXC_CHK(CreateShaderModule("./shaders/compute_node.comp.spv",
                                       &compShader));
            const VkPipelineShaderStageCreateInfo compStage{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = VK_SHADER_STAGE_COMPUTE_BIT,
              .module = compShader,
              .pName = "main",
            };
            MXC_CHK(CreateComputePipe(compStage));
            vkDestroyShaderModule(k_pDevice->GetVkDevice(), compShader, VK_ALLOC);

            return MXC_SUCCESS;
        }

        void BindDescriptor(const VkCommandBuffer commandBuffer,
                            const GlobalDescriptor& descriptor) const
        {
            ComputePipeline::BindDescriptor(commandBuffer,
                                            descriptor.GetVkDescriptorSet(),
                                            GlobalDescriptor::SetIndex);
        }

        void BindDescriptor(const VkCommandBuffer commandBuffer,
                            const ComputeNodeDescriptor& descriptor) const
        {
            ComputePipeline::BindDescriptor(commandBuffer,
                                            descriptor.GetVkDescriptorSet(),
                                            ComputeNodeDescriptor::SetIndex);
        }

        void BindComputeClearPipeline(const VkCommandBuffer commandBuffer) const
        {
            vkCmdBindPipeline(commandBuffer,
                              VK_PIPELINE_BIND_POINT_COMPUTE,
                              m_VkPrePipeline);
        }

    private:
        VkPipeline m_VkPrePipeline{VK_NULL_HANDLE};
    };
}// namespace Moxaic::Vulkan
