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
    class MeshNodePipeline : public VulkanGraphicsPipeline<MeshNodePipeline>
    {
    public:
        using VulkanGraphicsPipeline::VulkanGraphicsPipeline;

        static MXC_RESULT InitLayout(Vulkan::Device const& device)
        {
            SDL_assert(s_vkPipelineLayout == VK_NULL_HANDLE);
            StaticArray const setLayouts{
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
            StaticArray const stages{
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

        void BindDescriptor(MeshNodeDescriptor const& descriptor) const
        {
            vkCmdBindDescriptorSets(k_pDevice->GetVkGraphicsCommandBuffer(),
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    s_vkPipelineLayout,
                                    MeshNodeDescriptor::SetIndex,
                                    1,
                                    &descriptor.GetVkDescriptorSet(),
                                    0,
                                    nullptr);
        }
    };
}// namespace Moxaic::Vulkan
