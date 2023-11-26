#pragma once

#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_pipeline.hpp"

#include "moxaic_global_descriptor.hpp"
#include "moxaic_mesh_node_descriptor.hpp"

#include "moxaic_logging.hpp"

#include "static_array.hpp"

#include <string>
#include <vulkan/vulkan.h>

namespace Moxaic::Vulkan
{
    class MeshNodePipeline : public GraphicsPipeline<MeshNodePipeline>
    {
    public:
        using GraphicsPipeline::GraphicsPipeline;

        static MXC_RESULT InitLayout(const Vulkan::Device& device)
        {
            SDL_assert(s_vkPipelineLayout == VK_NULL_HANDLE);
            const StaticArray setLayouts{
              GlobalDescriptor::GetOrInitVkDescriptorSetLayout(device),
              MeshNodeDescriptor::GetOrInitVkDescriptorSetLayout(device),
            };
            MXC_CHK(CreateLayout(device, setLayouts));
            return MXC_SUCCESS;
        }

        MXC_RESULT Init()
        {
            MXC_LOG("Init MeshNodePipeline");

            VkShaderModule taskShader;
            MXC_CHK(CreateShaderModule("./shaders/mesh_node.task.spv",
                                       &taskShader));
            VkShaderModule meshShader;
            MXC_CHK(CreateShaderModule("./shaders/mesh_node.mesh.spv",
                                       &meshShader));
            VkShaderModule fragShader;
            MXC_CHK(CreateShaderModule("./shaders/mesh_node.frag.spv",
                                       &fragShader));
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
              },
            };
            MXC_CHK(CreateOpaquePipe(stages.size(),
                                     stages.data(),
                                     nullptr,
                                     nullptr,
                                     nullptr));
            vkDestroyShaderModule(k_pDevice->GetVkDevice(), taskShader, VK_ALLOC);
            vkDestroyShaderModule(k_pDevice->GetVkDevice(), meshShader, VK_ALLOC);
            vkDestroyShaderModule(k_pDevice->GetVkDevice(), fragShader, VK_ALLOC);
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
                            const MeshNodeDescriptor& descriptor) const
        {
            GraphicsPipeline::BindDescriptor(commandBuffer,
                                                   descriptor.GetVkDescriptorSet(),
                                                   MeshNodeDescriptor::SetIndex);
        }
    };
}// namespace Moxaic::Vulkan
