#pragma once

#include "moxaic_vulkan_pipeline.hpp"

#include "../descriptors/moxaic_global_descriptor.hpp"
#include "../descriptors/moxaic_material_descriptor.hpp"
#include "../descriptors/moxaic_object_descriptor.hpp"

#include "../moxaic_logging.hpp"

#include "../moxaic_vulkan.hpp"
#include "../moxaic_vulkan_device.hpp"
#include "../moxaic_vulkan_mesh.hpp"

#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <string>

namespace Moxaic
{
    class StandardPipeline : VulkanPipeline
    {
    public:
        using VulkanPipeline::VulkanPipeline;

        MXC_RESULT Init(const GlobalDescriptor &globalDescriptor,
                        const MaterialDescriptor &materialDescriptor,
                        const ObjectDescriptor &objectDescriptor)
        {
            if (initializeLayout()) {
                const std::array setLayouts{
                        globalDescriptor.vkDescriptorSetLayout(),
                        materialDescriptor.vkDescriptorSetLayout(),
                        objectDescriptor.vkDescriptorSetLayout(),
                };
                const VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
                        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                        .pNext = nullptr,
                        .flags = 0,
                        .setLayoutCount = setLayouts.size(),
                        .pSetLayouts = setLayouts.data(),
                        .pushConstantRangeCount  = 0,
                        .pPushConstantRanges = nullptr,
                };
                VK_CHK(vkCreatePipelineLayout(k_Device.vkDevice(),
                                              &pipelineLayoutCreateInfo,
                                              VK_ALLOC,
                                              &s_VkPipelineLayout));
            }

            VkShaderModule vertShaderModule;
            MXC_CHK(CreateShaderModule("./shaders/vert.spv",
                                       vertShaderModule));
            VkShaderModule fragShaderModule;
            MXC_CHK(CreateShaderModule("./shaders/frag.spv",
                                       fragShaderModule));
            const std::array stages{
                    (VkPipelineShaderStageCreateInfo) {
                            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                            .stage = VK_SHADER_STAGE_VERTEX_BIT,
                            .module = vertShaderModule,
                            .pName = "main",
                    },
                    (VkPipelineShaderStageCreateInfo) {
                            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                            .module = fragShaderModule,
                            .pName = "main",
                    }
            };
            MXC_CHK(CreateVertexInputOpaquePipe(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                                stages.size(),
                                                stages.data(),
                                                nullptr));
            vkDestroyShaderModule(k_Device.vkDevice(), vertShaderModule, VK_ALLOC);
            vkDestroyShaderModule(k_Device.vkDevice(), fragShaderModule, VK_ALLOC);
        }
    };
}
