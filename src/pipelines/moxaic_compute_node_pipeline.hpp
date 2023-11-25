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
    class ComputeNodePipeline : public VulkanComputePipeline<ComputeNodePipeline>
    {
    public:
        using VulkanComputePipeline::VulkanComputePipeline;

        static MXC_RESULT InitLayout(Vulkan::Device const& device)
        {
            SDL_assert(s_vkPipelineLayout == VK_NULL_HANDLE);
            StaticArray const setLayouts{
              GlobalDescriptor::GetOrInitVkDescriptorSetLayout(device),
              ComputeNodeDescriptor::GetOrInitVkDescriptorSetLayout(device),
            };
            MXC_CHK(CreateLayout(device, setLayouts));
            return MXC_SUCCESS;
        }

        MXC_RESULT Init()
        {
            MXC_LOG("Init ComputeNodePipeline");

            VkShaderModule compShader;
            MXC_CHK(CreateShaderModule("./shaders/compute_node.comp.spv",
                                       &compShader));
            VkPipelineShaderStageCreateInfo const stage{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = VK_SHADER_STAGE_COMPUTE_BIT,
              .module = compShader,
              .pName = "main",
            };
            MXC_CHK(CreateComputePipe(stage));
            vkDestroyShaderModule(k_pDevice->GetVkDevice(), compShader, VK_ALLOC);
            return MXC_SUCCESS;
        }

        void BindDescriptor(GlobalDescriptor const& descriptor) const
        {
            VulkanComputePipeline::BindDescriptor(descriptor.GetVkDescriptorSet(),
                                                  GlobalDescriptor::SetIndex);
        }

        void BindDescriptor(ComputeNodeDescriptor const& descriptor) const
        {
            VulkanComputePipeline::BindDescriptor(descriptor.GetVkDescriptorSet(),
                                                  ComputeNodeDescriptor::SetIndex);
        }
    };
}// namespace Moxaic::Vulkan
