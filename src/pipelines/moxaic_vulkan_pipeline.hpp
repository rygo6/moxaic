#pragma once

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
    static MXC_RESULT AllocReadFile(const char *filename,
                                    uint32_t *length,
                                    char **ppContents)
    {
        FILE *file = fopen(filename, "rb");
        if (file == NULL) {
            MXC_LOG("File can't be opened!", filename);
            return MXC_FAIL;
        }
        fseek(file, 0, SEEK_END);
        *length = ftell(file);
        rewind(file);
        *ppContents = (char *) calloc(1 + *length, sizeof(char));
        size_t readCount = fread(*ppContents, *length, 1, file);
        if (readCount == 0) {
            MXC_LOG("Failed to read file!", filename);
            return MXC_FAIL;
        }
        fclose(file);
        return MXC_SUCCESS;
    }

    template<typename T>
    class VulkanPipeline
    {
    public:
        VulkanPipeline(const VulkanDevice &device)
                : k_Device(device) {}

        virtual ~VulkanPipeline()
        {
            vkDestroyPipeline(k_Device.vkDevice(),
                              m_VkPipeline,
                              VK_ALLOC);
        }

        inline VkPipelineLayout vkPipelineLayout() const { return s_VkPipelineLayout; }
        inline auto vkPipeline() const { return m_VkPipeline; }
    protected:
        const VulkanDevice &k_Device;
        inline static VkPipelineLayout s_VkPipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_VkPipeline{VK_NULL_HANDLE};

        static bool initializeLayout() { return s_VkPipelineLayout == VK_NULL_HANDLE; }

        MXC_RESULT CreateShaderModule(const char *pShaderPath,
                                      VkShaderModule &shaderModule)
        {
            uint32_t codeLength;
            char *pShaderCode;
            MXC_CHK(AllocReadFile(pShaderPath,
                                  &codeLength,
                                  &pShaderCode));
            const VkShaderModuleCreateInfo createInfo{
                    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = 0,
                    .codeSize = codeLength,
                    .pCode = (const uint32_t *) pShaderCode,
            };
            VK_CHK(vkCreateShaderModule(k_Device.vkDevice(),
                                        &createInfo,
                                        VK_ALLOC,
                                        &shaderModule));
            free(pShaderCode);
            return MXC_SUCCESS;
        }

        MXC_RESULT CreateVertexInputOpaquePipe(VkPrimitiveTopology topology,
                                               uint32_t stageCount,
                                               const VkPipelineShaderStageCreateInfo *pStages,
                                               const VkPipelineTessellationStateCreateInfo *pTessellationState)
        {
            MXC_LOG_FUNCTION();
            // Vertex Input
            std::array vertexBindingDescriptions{
                    (VkVertexInputBindingDescription) {
                            .binding = 0,
                            .stride = sizeof(Vertex),
                            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                    },
            };
            std::array vertexAttributeDescriptions{
                    (VkVertexInputAttributeDescription) {
                            .location = 0,
                            .binding = 0,
                            .format = VK_FORMAT_R32G32B32_SFLOAT,
                            .offset = offsetof(Vertex, pos),
                    },
                    (VkVertexInputAttributeDescription) {
                            .location = 1,
                            .binding = 0,
                            .format = VK_FORMAT_R32G32B32_SFLOAT,
                            .offset = offsetof(Vertex, normal),
                    },
                    (VkVertexInputAttributeDescription) {
                            .location = 2,
                            .binding = 0,
                            .format = VK_FORMAT_R32G32_SFLOAT,
                            .offset = offsetof(Vertex, uv),
                    }
            };
            const VkPipelineVertexInputStateCreateInfo vertexInputState{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                    .vertexBindingDescriptionCount = vertexBindingDescriptions.size(),
                    .pVertexBindingDescriptions = vertexBindingDescriptions.data(),
                    .vertexAttributeDescriptionCount = vertexAttributeDescriptions.size(),
                    .pVertexAttributeDescriptions = vertexAttributeDescriptions.data()
            };
            const VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                    .topology = topology,
                    .primitiveRestartEnable = VK_FALSE,
            };
            MXC_CHK(CreateOpaquePipe(topology,
                                     stageCount,
                                     pStages,
                                     &vertexInputState,
                                     &inputAssemblyState,
                                     pTessellationState));
            return MXC_SUCCESS;
        }

        MXC_RESULT CreateOpaquePipe(VkPrimitiveTopology topology,
                                    uint32_t stageCount,
                                    const VkPipelineShaderStageCreateInfo *pStages,
                                    const VkPipelineVertexInputStateCreateInfo *pVertexInputState,
                                    const VkPipelineInputAssemblyStateCreateInfo *pInputAssemblyState,
                                    const VkPipelineTessellationStateCreateInfo *pTessellationState)
        {
            MXC_LOG_FUNCTION();
            SDL_assert(s_VkPipelineLayout != nullptr);
            // Fragment
            const std::array pipelineColorBlendAttachmentStates{
                    (VkPipelineColorBlendAttachmentState) {
                            .blendEnable = VK_FALSE,
                            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                              VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT |
                                              VK_COLOR_COMPONENT_A_BIT,

                    },
                    (VkPipelineColorBlendAttachmentState) {
                            .blendEnable = VK_FALSE,
                            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                              VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT |
                                              VK_COLOR_COMPONENT_A_BIT,

                    },
                    (VkPipelineColorBlendAttachmentState) {
                            .blendEnable = VK_FALSE,
                            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                              VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT |
                                              VK_COLOR_COMPONENT_A_BIT,

                    }
            };
            const VkPipelineColorBlendStateCreateInfo colorBlendState{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                    .pNext = NULL,
                    .logicOpEnable = VK_FALSE,
                    .logicOp = VK_LOGIC_OP_COPY,
                    .attachmentCount = pipelineColorBlendAttachmentStates.size(),
                    .pAttachments = pipelineColorBlendAttachmentStates.data(),
                    .blendConstants {0.0f, 0.0f, 0.0f, 0.0f}
            };
            const VkPipelineDepthStencilStateCreateInfo depthStencilState{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                    .depthTestEnable = VK_TRUE,
                    .depthWriteEnable =  VK_TRUE,
                    .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL, // vktut has VK_COMPARE_OP_LESS ?
                    .depthBoundsTestEnable = VK_FALSE,
                    .stencilTestEnable = VK_FALSE,
//            .minDepthBounds = 0.0f,
//            .maxDepthBounds = 1.0f,
            };

            // Rasterizing
            const VkPipelineRasterizationStateCreateInfo rasterizationState{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                    .pNext = NULL,
                    .flags = 0,
                    .depthClampEnable = VK_FALSE,
                    .rasterizerDiscardEnable = VK_FALSE,
#ifdef FBR_DEBUG_WIREFRAME
                    .polygonMode = pVulkan->isChild ? VK_POLYGON_MODE_FILL : VK_POLYGON_MODE_LINE,
#else
                    .polygonMode = VK_POLYGON_MODE_FILL,
#endif
                    .cullMode = VK_CULL_MODE_NONE,
                    .frontFace = VK_FRONT_FACE_CLOCKWISE,
                    .depthBiasEnable = VK_FALSE,
                    .depthBiasConstantFactor = 0,
                    .depthBiasClamp = 0,
                    .depthBiasSlopeFactor = 0,
                    .lineWidth = 1.0f,
            };
            const VkPipelineMultisampleStateCreateInfo multisampleState{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
                    .sampleShadingEnable = VK_FALSE,

            };

            // Viewport/Scissor
            const VkPipelineViewportStateCreateInfo viewportState{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                    .viewportCount = 1,
                    .scissorCount = 1,
            };
            const VkDynamicState dynamicStates[]{
                    VK_DYNAMIC_STATE_VIEWPORT,
                    VK_DYNAMIC_STATE_SCISSOR
            };
            const VkPipelineDynamicStateCreateInfo dynamicState{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                    .dynamicStateCount = 2,
                    .pDynamicStates = dynamicStates,
            };

            const VkPipelineRobustnessCreateInfoEXT pipelineRobustnessCreateInfo{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_ROBUSTNESS_CREATE_INFO_EXT,
                    .pNext = NULL,
                    .storageBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DEVICE_DEFAULT_EXT,
//            .storageBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT,
                    .uniformBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DEVICE_DEFAULT_EXT,
//            .uniformBuffers = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT,
                    .vertexInputs = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_DEVICE_DEFAULT_EXT,
//            .vertexInputs = VK_PIPELINE_ROBUSTNESS_BUFFER_BEHAVIOR_ROBUST_BUFFER_ACCESS_2_EXT,
                    .images = VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_DEVICE_DEFAULT_EXT,
//            .images = VK_PIPELINE_ROBUSTNESS_IMAGE_BEHAVIOR_ROBUST_IMAGE_ACCESS_2_EXT,
            };

            // Create
            const VkGraphicsPipelineCreateInfo pipelineInfo{
                    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                    .pNext = &pipelineRobustnessCreateInfo,
                    .flags = 0,
                    .stageCount = stageCount,
                    .pStages = pStages,
                    .pVertexInputState = pVertexInputState,
                    .pInputAssemblyState = pInputAssemblyState,
                    .pTessellationState = pTessellationState,
                    .pViewportState = &viewportState,
                    .pRasterizationState = &rasterizationState,
                    .pMultisampleState = &multisampleState,
                    .pDepthStencilState = &depthStencilState,
                    .pColorBlendState = &colorBlendState,
                    .pDynamicState = &dynamicState,
                    .layout = s_VkPipelineLayout,
                    .renderPass = k_Device.vkRenderPass(),
                    .subpass = 0,
                    .basePipelineHandle = VK_NULL_HANDLE,
                    .basePipelineIndex = 0,
            };
            VK_CHK(vkCreateGraphicsPipelines(k_Device.vkDevice(),
                                             VK_NULL_HANDLE,
                                             1,
                                             &pipelineInfo,
                                             VK_ALLOC,
                                             &m_VkPipeline));

            return MXC_SUCCESS;
        }
    };
}
