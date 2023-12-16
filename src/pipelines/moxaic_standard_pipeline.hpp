#pragma once

#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_pipeline.hpp"

#include "moxaic_global_descriptor.hpp"
#include "moxaic_material_descriptor.hpp"
#include "moxaic_object_descriptor.hpp"

#include "moxaic_logging.hpp"

#include "static_array.hpp"

#include <string>
#include <vulkan/vulkan.h>

namespace Moxaic::Vulkan
{
    class StandardPipeline : public GraphicsPipeline<StandardPipeline>
    {
    public:
        using GraphicsPipeline::GraphicsPipeline;

        static MXC_RESULT InitLayout(const Vulkan::Device& device)
        {
            const StaticArray setLayouts{
              GlobalDescriptor::GetOrInitVkDescriptorSetLayout(device),
              StandardMaterialDescriptor::GetOrInitVkDescriptorSetLayout(device),
              ObjectDescriptor::GetOrInitVkDescriptorSetLayout(device),
            };
            MXC_CHK(CreateLayout(device, setLayouts));
            return MXC_SUCCESS;
        }

        MXC_RESULT Init()
        {
            MXC_LOG("Init StandardPipeline");

            VkShaderModule vertShader;
            MXC_CHK(CreateShaderModule("./shaders/basic_material.vert.spv",
                                       &vertShader));
            VkShaderModule fragShader;
            MXC_CHK(CreateShaderModule("./shaders/basic_material.frag.spv",
                                       &fragShader));
            const StaticArray stages{
              (VkPipelineShaderStageCreateInfo){
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vertShader,
                .pName = "main",
              },
              (VkPipelineShaderStageCreateInfo){
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = fragShader,
                .pName = "main",
              },
            };
            MXC_CHK(CreateVertexInputOpaquePipe(stages.size(),
                                                stages.data(),
                                                nullptr));
            vkDestroyShaderModule(k_pDevice->  GetVkDevice(), vertShader, VK_ALLOC);
            vkDestroyShaderModule(k_pDevice->  GetVkDevice(), fragShader, VK_ALLOC);
            return MXC_SUCCESS;
        }

        void BindDescriptor(const VkCommandBuffer commandBuffer,
                            const GlobalDescriptor& descriptor) const
        {
            GraphicsPipeline::BindDescriptor(commandBuffer,
                                       descriptor.GetVkDescriptorSet(),
                                       GlobalDescriptor::SetIndex);
        }

        void BindDescriptor(const VkCommandBuffer commandBuffer,
                            const StandardMaterialDescriptor& descriptor) const
        {
            GraphicsPipeline::BindDescriptor(commandBuffer,
                                       descriptor.GetVkDescriptorSet(),
                                       StandardMaterialDescriptor::SetIndex);
        }

        void BindDescriptor(const VkCommandBuffer commandBuffer,
                            const ObjectDescriptor& descriptor) const
        {
            GraphicsPipeline::BindDescriptor(commandBuffer,
                                       descriptor.GetVkDescriptorSet(),
                                       ObjectDescriptor::SetIndex);
        }
    };
}// namespace Moxaic::Vulkan
