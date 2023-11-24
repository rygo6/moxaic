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
    class StandardPipeline : public VulkanPipeline<StandardPipeline>
    {
    public:
        using VulkanPipeline::VulkanPipeline;

        MXC_RESULT Init(GlobalDescriptor const& globalDescriptor,
                        StandardMaterialDescriptor const& materialDescriptor,
                        ObjectDescriptor const& objectDescriptor)
        {
            // todo should this be ina  different method so I can call them all before trying make any descriptors???
            if (initializeLayout()) {
                StaticArray const setLayouts{
                  globalDescriptor.GetVkDescriptorSetLayout(),
                  materialDescriptor.GetVkDescriptorSetLayout(),
                  objectDescriptor.GetVkDescriptorSetLayout(),
                };
                MXC_CHK(CreateLayout(setLayouts));
            }

            VkShaderModule vertShader;
            MXC_CHK(CreateShaderModule("./shaders/shader_base.vert.spv",
                                       vertShader));
            VkShaderModule fragShader;
            MXC_CHK(CreateShaderModule("./shaders/shader_base.frag.spv",
                                       fragShader));
            StaticArray const stages{
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
            vkDestroyShaderModule(k_pDevice->GetVkDevice(), vertShader, VK_ALLOC);
            vkDestroyShaderModule(k_pDevice->GetVkDevice(), fragShader, VK_ALLOC);
            return MXC_SUCCESS;
        }

        void BindDescriptor(GlobalDescriptor const& descriptor) const
        {
            vkCmdBindDescriptorSets(k_pDevice->GetVkGraphicsCommandBuffer(),
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    s_vkPipelineLayout,
                                    0,
                                    1,
                                    &descriptor.GetVkDescriptorSet(),
                                    0,
                                    nullptr);
        }

        void BindDescriptor(StandardMaterialDescriptor const& descriptor) const
        {
            vkCmdBindDescriptorSets(k_pDevice->GetVkGraphicsCommandBuffer(),
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    s_vkPipelineLayout,
                                    1,
                                    1,
                                    &descriptor.GetVkDescriptorSet(),
                                    0,
                                    nullptr);
        }

        void BindDescriptor(ObjectDescriptor const& descriptor) const
        {
            vkCmdBindDescriptorSets(k_pDevice->GetVkGraphicsCommandBuffer(),
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    s_vkPipelineLayout,
                                    2,
                                    1,
                                    &descriptor.GetVkDescriptorSet(),
                                    0,
                                    nullptr);
        }
    };
}// namespace Moxaic::Vulkan
