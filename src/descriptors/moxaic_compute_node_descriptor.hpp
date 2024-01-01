#pragma once

#include "moxaic_global_descriptor.hpp"
#include "moxaic_vulkan_descriptor.hpp"
#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_texture.hpp"
#include "static_array.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace Moxaic::Vulkan
{
    class ComputeNodeDescriptor : public VulkanDescriptorBase<ComputeNodeDescriptor>
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;

        constexpr static int SetIndex = 1;

        enum Indices : uint32_t {
            NodeUB0,
            ColorTexture,
            NormalTexture,
            GBufferTexture,
            DepthTexture,
            OutputAveragedAtomicTexture,
            OutputAtomicTexture,
            OutputFinalTexture,
            OutputAtomictiles,
        };

        struct UniformBuffer
        {
            glm::mat4 view;
            glm::mat4 proj;
            glm::mat4 viewProj;
            glm::mat4 invView;
            glm::mat4 invProj;
            glm::mat4 invViewProj;
            glm::uint32_t width;
            glm::uint32_t height;
            glm::float32_t planeZDepth;
        };

        struct Tile
        {
            glm::uint16_t x;// how to make float16!?
            glm::uint16_t y;
            glm::uint8_t size;
            glm::uint8_t id;
        };

        struct StorageBuffer
        {
            uint32_t atomicTileCount;
            Tile tiles[32 * 32];
        };

        static MXC_RESULT InitLayout(const Vulkan::Device& device)
        {
            // MXC_LOG("Init ComputeNodeDescriptor Layout");
            StaticArray bindings{
              (VkDescriptorSetLayoutBinding){
                .binding = Indices::NodeUB0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
              },
              (VkDescriptorSetLayoutBinding){
                .binding = Indices::ColorTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
              },
              (VkDescriptorSetLayoutBinding){
                .binding = Indices::NormalTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
              },
              (VkDescriptorSetLayoutBinding){
                .binding = Indices::GBufferTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
              },
              (VkDescriptorSetLayoutBinding){
                .binding = Indices::DepthTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
              },
              (VkDescriptorSetLayoutBinding){
                .binding = Indices::OutputAveragedAtomicTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
              },
              (VkDescriptorSetLayoutBinding){
                .binding = Indices::OutputAtomicTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
              },
              (VkDescriptorSetLayoutBinding){
                .binding = Indices::OutputFinalTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
              },
              (VkDescriptorSetLayoutBinding){
                .binding = Indices::OutputAtomictiles,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
              },
            };
            MXC_CHK(CreateDescriptorSetLayout(device, bindings));
            return MXC_SUCCESS;
        }

        MXC_RESULT Init(const UniformBuffer& buffer,
                        const Framebuffer& framebuffer,
                        const Texture& outputAveragedAtomicTexture,
                        const Texture& outputAtomicTexture,
                        const VkImageView outputColorImage)
        {
            // MXC_LOG("Init ComputeNodeDescriptor");
            MXC_CHK(uniform.Init(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 Vulkan::Locality::Local));
            localUniformBuffer = buffer;
            WriteLocalBuffer();

            MXC_CHK(storage.Init(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                 Vulkan::Locality::Local));

            MXC_CHK(AllocateDescriptorSet());
            StaticArray writes{
              (VkWriteDescriptorSet){
                .dstBinding = Indices::NodeUB0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = StaticRef((VkDescriptorBufferInfo){
                  .buffer = uniform.GetVkBuffer(),
                  .range = uniform.Size()})},
              (VkWriteDescriptorSet){
                .dstBinding = Indices::ColorTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = Device->GetVkLinearSampler(),
                  .imageView = framebuffer.GetColorTexture().GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
              (VkWriteDescriptorSet){
                .dstBinding = Indices::NormalTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = Device->GetVkLinearSampler(),
                  .imageView = framebuffer.GetNormalTexture().GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
              (VkWriteDescriptorSet){
                .dstBinding = Indices::GBufferTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = Device->GetVkLinearSampler(),
                  .imageView = framebuffer.GetGBufferTexture().GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
              (VkWriteDescriptorSet){
                .dstBinding = Indices::DepthTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = Device->GetVkLinearSampler(),
                  .imageView = framebuffer.GetGBufferTexture().GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
              (VkWriteDescriptorSet){
                .dstBinding = Indices::OutputAveragedAtomicTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .imageView = outputAveragedAtomicTexture.GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
              (VkWriteDescriptorSet){
                .dstBinding = Indices::OutputAtomicTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .imageView = outputAtomicTexture.GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
              (VkWriteDescriptorSet){
                .dstBinding = Indices::OutputFinalTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .imageView = outputColorImage,
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
              (VkWriteDescriptorSet){
                .dstBinding = Indices::OutputAtomictiles,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo = StaticRef((VkDescriptorBufferInfo){
                  .buffer = storage.GetVkBuffer(),
                  .range = storage.Size()})},
            };
            WriteDescriptors(writes);
            return MXC_SUCCESS;
        }

        MXC_RESULT WriteFramebuffer(const Framebuffer& framebuffer)
        {
            StaticArray writes{
              (VkWriteDescriptorSet){
                .dstBinding = Indices::ColorTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = Device->GetVkLinearSampler(),
                  .imageView = framebuffer.GetColorTexture().GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
              (VkWriteDescriptorSet){
                .dstBinding = Indices::NormalTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = Device->GetVkLinearSampler(),
                  .imageView = framebuffer.GetNormalTexture().GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
              (VkWriteDescriptorSet){
                .dstBinding = Indices::GBufferTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = Device->GetVkLinearSampler(),
                  .imageView = framebuffer.GetGBufferTexture().GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
              (VkWriteDescriptorSet){
                .dstBinding = Indices::DepthTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = Device->GetVkLinearSampler(),
                  .imageView = framebuffer.GetDepthTexture().GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
            };
            WriteDescriptors(writes);
            return MXC_SUCCESS;
        }

        MXC_RESULT WriteOutputAveragedAtomicTexture(const Texture& outputAveragedAtomicTexture)
        {
            StaticArray writes{
              (VkWriteDescriptorSet){
                .dstBinding = Indices::OutputAveragedAtomicTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .imageView = outputAveragedAtomicTexture.GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
            };
            WriteDescriptors(writes);
            return MXC_SUCCESS;
        }

        MXC_RESULT WriteOutputAtomicTexture(const Texture& outputAtomicTexture)
        {
            StaticArray writes{
              (VkWriteDescriptorSet){
                .dstBinding = Indices::OutputAtomicTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .imageView = outputAtomicTexture.GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
            };
            WriteDescriptors(writes);
            return MXC_SUCCESS;
        }

        MXC_RESULT WriteOutputColorImage(const VkImageView outputColorImage)
        {
            StaticArray writes{
              (VkWriteDescriptorSet){
                .dstBinding = Indices::OutputFinalTexture,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .imageView = outputColorImage,
                  .imageLayout = VK_IMAGE_LAYOUT_GENERAL})},
            };
            WriteDescriptors(writes);
            return MXC_SUCCESS;
        }

        void WriteLocalBuffer()
        {
            uniform.CopyBuffer(localUniformBuffer);
        }

        UniformBuffer localUniformBuffer{};

    private:
        Buffer<UniformBuffer> uniform{Device};
        Buffer<StorageBuffer> storage{Device};
    };
}// namespace Moxaic::Vulkan
