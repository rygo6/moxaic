#pragma once

#include "moxaic_compute_composite_descriptor.hpp"
#include "moxaic_global_descriptor.hpp"
#include "moxaic_logging.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_pipeline.hpp"
#include "static_array.hpp"
#include "mid_vulkan.hpp"
#include <vulkan/vulkan.h>

namespace Moxaic::Vulkan
{
    class ComputeCompositePipeline : public ComputePipeline<ComputeCompositePipeline>
    {
    public:
        constexpr static auto PipelineType{PipelineType::Compute};
        constexpr static uint32_t LocalSize = 32;

        explicit ComputeCompositePipeline(const Vulkan::Device* const device)
            : ComputePipeline(device) {}

        static MXC_RESULT InitLayout(const Vulkan::Device& device)
        {
            MXC_LOG("InitLayout ComputeNodePipeline");
            assert(sharedVkPipelineLayout == VK_NULL_HANDLE);
            const StaticArray setLayouts{
              GlobalDescriptor::GetOrInitSharedVkDescriptorSetLayout(device),
              ComputeCompositeDescriptor::GetOrInitSharedVkDescriptorSetLayout(device),
            };
            MXC_CHK(CreateLayout(device, setLayouts));
            return MXC_SUCCESS;
        }

        MXC_RESULT Init(const char* shaderPath)
        {
            MXC_LOG("Init ComputeNodePipeline", shaderPath);
            VkShaderModule shader;
            MXC_CHK(CreateShaderModule(shaderPath,
                                       &shader));
            const VkPipelineShaderStageCreateInfo compStage{
              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = VK_SHADER_STAGE_COMPUTE_BIT,
              .module = shader,
              .pName = "main",
            };
            MXC_CHK(CreateComputePipe(compStage));
            vkDestroyShaderModule(Device->GetVkDevice(), shader, VK_ALLOC);

            Mid::Vk::Device device;
            auto pipeline = Mid::Vk::DeferDestroy(device.CreateComputePipeline(Mid::Vk::ComputePipeline2::Desc{
              .name = "ComputeCompositePipeline",
              .createInfos{
                {
                  .stage = {
                    .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                    .module = shader,
                    .pName = "main"},
                  .layout = sharedVkPipelineLayout,
                },
              },
            }));

            return MXC_SUCCESS;
        }

        static void BindDescriptor(const VkCommandBuffer commandBuffer,
                                   const GlobalDescriptor& descriptor)
        {
            BindDescriptors(commandBuffer,
                            descriptor.GetVkDescriptorSet(),
                            GlobalDescriptor::SetIndex);
        }

        static void BindDescriptor(const VkCommandBuffer commandBuffer,
                                   const ComputeCompositeDescriptor& descriptor)
        {
            BindDescriptors(commandBuffer,
                            descriptor.GetVkDescriptorSet(),
                            ComputeCompositeDescriptor::SetIndex);
        }
    };
}// namespace Moxaic::Vulkan
