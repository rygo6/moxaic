#pragma once

#include "moxaic_vulkan_pipeline.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"

#include "moxaic_global_descriptor.hpp"
#include "moxaic_mesh_node_descriptor.hpp"

#include "moxaic_logging.hpp"

#include "static_array.hpp"

#include <vulkan/vulkan.h>
#include <string>

namespace Moxaic::Vulkan
{
    class MeshNodePipeline : public VulkanPipeline<MeshNodePipeline>
    {
    public:
        using VulkanPipeline::VulkanPipeline;

        MXC_RESULT Init(const GlobalDescriptor& globalDescriptor,
                        const MeshNodeDescriptor& meshNodeDescriptor)
        {
            // todo should this be ina  different method so I can call them all before trying make any descriptors???
            if (initializeLayout()) {
                const StaticArray setLayouts{
                    globalDescriptor.vkDescriptorSetLayout(),
                    meshNodeDescriptor.vkDescriptorSetLayout(),
                };
                MXC_CHK(CreateLayout(setLayouts));
            }

            VkShaderModule taskShader;
            MXC_CHK(CreateShaderModule("./shaders/node_mesh.task.spv", taskShader));
            VkShaderModule meshShader;
            MXC_CHK(CreateShaderModule("./shaders/node_mesh.mesh.spv", meshShader));
            VkShaderModule fragShader;
            MXC_CHK(CreateShaderModule("./shaders/node_mesh.frag.spv", fragShader));
            const StaticArray stages{
                (VkPipelineShaderStageCreateInfo){
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_TASK_BIT_EXT,
                    .module = taskShader,
                    .pName = "main",
                },
                (VkPipelineShaderStageCreateInfo){
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_MESH_BIT_EXT,
                    .module = meshShader,
                    .pName = "main",
                },
                (VkPipelineShaderStageCreateInfo){
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                    .module = fragShader,
                    .pName = "main",
                }
            };
            MXC_CHK(CreateOpaquePipe(
                stages.size(),
                stages.data(),
                nullptr,
                nullptr,
                nullptr));
            vkDestroyShaderModule(k_Device.vkDevice(), taskShader, VK_ALLOC);
            vkDestroyShaderModule(k_Device.vkDevice(), meshShader, VK_ALLOC);
            vkDestroyShaderModule(k_Device.vkDevice(), fragShader, VK_ALLOC);
            return MXC_SUCCESS;
        }

        void BindDescriptor(const GlobalDescriptor& descriptor) const
        {
            vkCmdBindDescriptorSets(k_Device.vkGraphicsCommandBuffer(),
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    s_vkPipelineLayout,
                                    0,
                                    1,
                                    &descriptor.vkDescriptorSet(),
                                    0,
                                    nullptr);
        }

        void BindDescriptor(const MeshNodeDescriptor& descriptor) const
        {
            vkCmdBindDescriptorSets(k_Device.vkGraphicsCommandBuffer(),
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    s_vkPipelineLayout,
                                    1,
                                    1,
                                    &descriptor.vkDescriptorSet(),
                                    0,
                                    nullptr);
        }
    };
}
