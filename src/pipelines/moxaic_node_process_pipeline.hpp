#pragma once

#include "moxaic_logging.hpp"
#include "moxaic_node_process_descriptor.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_pipeline.hpp"
#include "static_array.hpp"
#include <vulkan/vulkan.h>

namespace Moxaic::Vulkan
{
    // class NodeProcessPipeline : public ComputePipeline<NodeProcessPipeline>
    // {
    //     const char* shaderPath;
    //
    // public:
    //     constexpr static auto PipelineType{PipelineType::Compute};
    //     constexpr static uint32_t LocalSize = 32;
    //
    //     explicit NodeProcessPipeline(const Vulkan::Device* const device,
    //                                  const char* const shaderPath)
    //         : ComputePipeline(device),
    //           shaderPath(shaderPath) {}
    //
    //     static MXC_RESULT InitLayout(const Vulkan::Device& device)
    //     {
    //         MXC_LOG("InitLayout NodeProcessPipeline");
    //         assert(sharedVkPipelineLayout == VK_NULL_HANDLE);
    //         const StaticArray setLayouts{
    //           NodeProcessDescriptor::GetOrInitSharedVkDescriptorSetLayout(device),
    //         };
    //         MXC_CHK(CreateLayout(device, setLayouts));
    //         return MXC_SUCCESS;
    //     }
    //
    //     MXC_RESULT Init()
    //     {
    //         MXC_LOG("Init NodeProcessPipeline", shaderPath);
    //         VkShaderModule shader;
    //         MXC_CHK(CreateShaderModule(shaderPath,
    //                                    &shader));
    //         const VkPipelineShaderStageCreateInfo compStage{
    //           .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    //           .stage = VK_SHADER_STAGE_COMPUTE_BIT,
    //           .module = shader,
    //           .pName = "main",
    //         };
    //         MXC_CHK(CreateComputePipe(compStage, &vkPipeline, 0));
    //         vkDestroyShaderModule(Device->GetVkDevice(), shader, VK_ALLOC);
    //         return MXC_SUCCESS;
    //     }
    //
    //     static void BindDescriptor(const VkCommandBuffer commandBuffer,
    //                                const NodeProcessDescriptor& descriptor)
    //     {
    //         BindDescriptors(commandBuffer,
    //                         descriptor.VkDescriptorSetHandle,
    //                         NodeProcessDescriptor::SetIndex);
    //     }
    // };
}// namespace Moxaic::Vulkan
