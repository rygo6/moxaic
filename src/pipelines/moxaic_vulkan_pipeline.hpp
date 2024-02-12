#pragma once

#include "main.hpp"

#include "moxaic_logging.hpp"

#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_mesh.hpp"

#include "moxaic_vulkan_pipeline.hpp"

#include <vulkan/vulkan.h>

namespace Moxaic::Vulkan
{
    static MXC_RESULT ReadFile(const char* filename,
                               uint32_t* length,
                               char** ppContents)
    {
        FILE* file = fopen(filename, "rb");
        if (file == nullptr) {
            MXC_LOG("File can't be opened!", filename);
            return MXC_FAIL;
        }
        fseek(file, 0, SEEK_END);
        *length = ftell(file);
        rewind(file);
        *ppContents = static_cast<char*>(calloc(1 + *length, sizeof(char)));
        const size_t readCount = fread(*ppContents, *length, 1, file);
        if (readCount == 0) {
            MXC_LOG("Failed to read file!", filename);
            return MXC_FAIL;
        }
        fclose(file);
        return MXC_SUCCESS;
    }

    template<typename Derived>
    class Pipeline
    {
    protected:
        const Device* const Device;

    public:
        MXC_NO_VALUE_PASS(Pipeline)

        explicit Pipeline(const Vulkan::Device* const pDevice)
            : Device(pDevice) {}

        ~Pipeline()
        {
            vkDestroyPipeline(Device->GetVkDevice(),
                              vkPipeline,
                              VK_ALLOC);
        }

    protected:
        // at some point layout will need to be a map on the device to support multiple devices
        inline static VkPipelineLayout sharedVkPipelineLayout = VK_NULL_HANDLE;
        VkPipeline vkPipeline{VK_NULL_HANDLE};

        static void CheckLayoutInitialized(const Vulkan::Device& device)
        {
            if (sharedVkPipelineLayout == VK_NULL_HANDLE)
                Derived::InitLayout(device);
        }

        template<uint32_t N>
        static MXC_RESULT CreateLayout(const Vulkan::Device& device,
                                       const StaticArray<VkDescriptorSetLayout, N>& setLayouts)
        {
            SDL_assert(sharedVkPipelineLayout == VK_NULL_HANDLE);
            const VkPipelineLayoutCreateInfo createInfo{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
              .pNext = nullptr,
              .flags = 0,
              .setLayoutCount = setLayouts.size(),
              .pSetLayouts = setLayouts.data(),
              .pushConstantRangeCount = 0,
              .pPushConstantRanges = nullptr,
            };
            VK_CHK(vkCreatePipelineLayout(device.GetVkDevice(),
                                          &createInfo,
                                          VK_ALLOC,
                                          &sharedVkPipelineLayout));
            return MXC_SUCCESS;
        }

        MXC_RESULT CreateShaderModule(const char* const pShaderPath,
                                      VkShaderModule* pShaderModule) const
        {
            uint32_t codeLength;
            char* pShaderCode;
            MXC_CHK(ReadFile(pShaderPath,
                             &codeLength,
                             &pShaderCode));
            const VkShaderModuleCreateInfo createInfo{
              .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
              .pNext = nullptr,
              .flags = 0,
              .codeSize = codeLength,
              .pCode = reinterpret_cast<const uint32_t*>(pShaderCode),
            };
            VK_CHK(vkCreateShaderModule(Device->GetVkDevice(),
                                        &createInfo,
                                        VK_ALLOC,
                                        pShaderModule));
            free(pShaderCode);
            return MXC_SUCCESS;
        }

        MXC_RESULT CreateVertexInputOpaquePipe(const uint32_t stageCount,
                                               const VkPipelineShaderStageCreateInfo* pStages,
                                               const VkPipelineTessellationStateCreateInfo* pTessellationState)
        {
            // Vertex Input
            constexpr StaticArray vertexBindingDescriptions{
              (VkVertexInputBindingDescription){
                .binding = 0,
                .stride = sizeof(Vertex),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
              },
            };
            const StaticArray vertexAttributeDescriptions{
              (VkVertexInputAttributeDescription){
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(Vertex, pos),
              },
              (VkVertexInputAttributeDescription){
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(Vertex, normal),
              },
              (VkVertexInputAttributeDescription){
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(Vertex, uv),
              }};
            const VkPipelineVertexInputStateCreateInfo vertexInputState{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
              .vertexBindingDescriptionCount = vertexBindingDescriptions.size(),
              .pVertexBindingDescriptions = vertexBindingDescriptions.data(),
              .vertexAttributeDescriptionCount = vertexAttributeDescriptions.size(),
              .pVertexAttributeDescriptions = vertexAttributeDescriptions.data()};
            constexpr VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
              .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
              .primitiveRestartEnable = VK_FALSE,
            };
            MXC_CHK(CreateOpaquePipe(
              stageCount,
              pStages,
              &vertexInputState,
              &inputAssemblyState,
              pTessellationState));
            return MXC_SUCCESS;
        }

        MXC_RESULT CreateOpaquePipe(const uint32_t stageCount,
                                    const VkPipelineShaderStageCreateInfo* pStages,
                                    const VkPipelineVertexInputStateCreateInfo* pVertexInputState,
                                    const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState,
                                    const VkPipelineTessellationStateCreateInfo* pTessellationState)
        {
            SDL_assert(vkPipeline == nullptr);
            CheckLayoutInitialized(*Device);
            // Fragment
            constexpr StaticArray pipelineColorBlendAttachmentStates{
              // Color
              (VkPipelineColorBlendAttachmentState){
                .blendEnable = VK_FALSE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                  VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT |
                                  VK_COLOR_COMPONENT_A_BIT,

              },
              // normal
              (VkPipelineColorBlendAttachmentState){
                .blendEnable = VK_FALSE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                  VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT |
                                  VK_COLOR_COMPONENT_A_BIT,

              },
              // gbuffer
              // (VkPipelineColorBlendAttachmentState){
              //   .blendEnable = VK_FALSE,
              //   .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
              //                     VK_COLOR_COMPONENT_G_BIT |
              //                     VK_COLOR_COMPONENT_B_BIT |
              //                     VK_COLOR_COMPONENT_A_BIT,
              //
              // },
            };
            const VkPipelineColorBlendStateCreateInfo colorBlendState{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
              .pNext = nullptr,
              .logicOpEnable = VK_FALSE,
              .logicOp = VK_LOGIC_OP_COPY,
              .attachmentCount = pipelineColorBlendAttachmentStates.size(),
              .pAttachments = pipelineColorBlendAttachmentStates.data(),
              .blendConstants{0.0f, 0.0f, 0.0f, 0.0f}};
            constexpr VkPipelineDepthStencilStateCreateInfo depthStencilState{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
              .depthTestEnable = VK_TRUE,
              .depthWriteEnable = VK_TRUE,
              .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,// vktut has VK_COMPARE_OP_LESS ?
              .depthBoundsTestEnable = VK_FALSE,
              .stencilTestEnable = VK_FALSE,
              .minDepthBounds = 0.0f,
              .maxDepthBounds = 1.0f,
            };

            // Rasterizing
            constexpr VkPipelineRasterizationStateCreateInfo rasterizationState{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
              .pNext = nullptr,
              .flags = 0,
              .depthClampEnable = VK_FALSE,
              .rasterizerDiscardEnable = VK_FALSE,
#ifdef MXC_DEBUG_WIREFRAME
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
            constexpr VkPipelineMultisampleStateCreateInfo multisampleState{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
              .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
              .sampleShadingEnable = VK_FALSE,

            };

            // Viewport/Scissor
            constexpr VkPipelineViewportStateCreateInfo viewportState{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
              .viewportCount = 1,
              .scissorCount = 1,
            };
            constexpr StaticArray dynamicStates{
              (VkDynamicState) VK_DYNAMIC_STATE_VIEWPORT,
              (VkDynamicState) VK_DYNAMIC_STATE_SCISSOR};
            const VkPipelineDynamicStateCreateInfo dynamicState{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
              .dynamicStateCount = dynamicStates.size(),
              .pDynamicStates = dynamicStates.data(),
            };

            constexpr VkPipelineRobustnessCreateInfoEXT pipelineRobustnessCreateInfo{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_ROBUSTNESS_CREATE_INFO_EXT,
              .pNext = nullptr,
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
              .layout = sharedVkPipelineLayout,
              .renderPass = Device->GetVkRenderPass(),
              .subpass = 0,
              .basePipelineHandle = VK_NULL_HANDLE,
              .basePipelineIndex = 0,
            };
            VK_CHK(vkCreateGraphicsPipelines(Device->GetVkDevice(),
                                             VK_NULL_HANDLE,
                                             1,
                                             &pipelineInfo,
                                             VK_ALLOC,
                                             &vkPipeline));

            return MXC_SUCCESS;
        }

        MXC_RESULT CreateComputePipe(const VkPipelineShaderStageCreateInfo& stage,
                                     VkPipeline* pVkPipeline)
        {
            CheckLayoutInitialized(*Device);
            const VkComputePipelineCreateInfo pipelineInfo{
              .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
              .pNext = nullptr,
              .flags = 0,
              .stage = stage,
              .layout = sharedVkPipelineLayout,
              .basePipelineHandle = nullptr,
              .basePipelineIndex = 0,
            };
            VK_CHK(vkCreateComputePipelines(Device->GetVkDevice(),
                                            VK_NULL_HANDLE,
                                            1,
                                            &pipelineInfo,
                                            VK_ALLOC,
                                            pVkPipeline));
            return MXC_SUCCESS;
        }

        MXC_RESULT CreateComputePipe(const VkPipelineShaderStageCreateInfo& stage)
        {
            return CreateComputePipe(stage, &vkPipeline);
        }
    };

    template<typename Derived>
    class GraphicsPipeline : public Pipeline<Derived>
    {
    public:
        using Pipeline<Derived>::Pipeline;
        void BindPipeline(const VkCommandBuffer commandBuffer) const
        {
            vkCmdBindPipeline(commandBuffer,
                              VK_PIPELINE_BIND_POINT_GRAPHICS,
                              this->vkPipeline);
        }

    protected:
        void BindDescriptor(const VkCommandBuffer commandBuffer,
                            const VkDescriptorSet& descriptorSet,
                            const int& setIndex) const
        {
            vkCmdBindDescriptorSets(commandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    this->sharedVkPipelineLayout,
                                    setIndex,
                                    1,
                                    &descriptorSet,
                                    0,
                                    nullptr);
        }
    };

    template<typename Derived>
    class ComputePipeline : public Pipeline<Derived>
    {
    public:
        using Pipeline<Derived>::Pipeline;
        void BindPipeline(const VkCommandBuffer commandBuffer) const
        {
            vkCmdBindPipeline(commandBuffer,
                              VK_PIPELINE_BIND_POINT_COMPUTE,
                              this->vkPipeline);
        }

    protected:
        void BindDescriptor(const VkCommandBuffer commandBuffer,
                            const VkDescriptorSet& descriptorSet,
                            const int& setIndex) const
        {
            vkCmdBindDescriptorSets(commandBuffer,
                                    VK_PIPELINE_BIND_POINT_COMPUTE,
                                    this->sharedVkPipelineLayout,
                                    setIndex,
                                    1,
                                    &descriptorSet,
                                    0,
                                    nullptr);
        }

        static void BindDescriptors(const VkCommandBuffer commandBuffer,
                                    const VkDescriptorSet& descriptorSet,
                                    const int& setIndex)
        {
            vkCmdBindDescriptorSets(commandBuffer,
                                    VK_PIPELINE_BIND_POINT_COMPUTE,
                                    Pipeline<Derived>::sharedVkPipelineLayout,
                                    setIndex,
                                    1,
                                    &descriptorSet,
                                    0,
                                    nullptr);
        }
    };

}// namespace Moxaic::Vulkan
