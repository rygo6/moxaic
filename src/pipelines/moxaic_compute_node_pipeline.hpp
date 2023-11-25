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
    class ComputeNodePipeline : public VulkanPipeline<ComputeNodePipeline>
    {
    public:
        using VulkanPipeline::VulkanPipeline;

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
            CreateComputePipe(stage);
            vkDestroyShaderModule(k_pDevice->GetVkDevice(), compShader, VK_ALLOC);
            return MXC_SUCCESS;
        }

        void BindDescriptor(GlobalDescriptor const& descriptor) const
        {
            vkCmdBindDescriptorSets(k_pDevice->GetVkGraphicsCommandBuffer(),
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    s_vkPipelineLayout,
                                    GlobalDescriptor::SetIndex,
                                    1,
                                    &descriptor.GetVkDescriptorSet(),
                                    0,
                                    nullptr);
        }

        void BindDescriptor(ComputeNodeDescriptor const& descriptor) const
        {
            vkCmdBindDescriptorSets(k_pDevice->GetVkGraphicsCommandBuffer(),
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    s_vkPipelineLayout,
                                    ComputeNodeDescriptor::SetIndex,
                                    1,
                                    &descriptor.GetVkDescriptorSet(),
                                    0,
                                    nullptr);
        }
    };
}// namespace Moxaic::Vulkan
