#pragma once

#include "moxaic_vulkan_descriptor.hpp"
#include "moxaic_vulkan_texture.hpp"
#include "static_array.hpp"

namespace Moxaic::Vulkan
{
    class StandardMaterialDescriptor : public VulkanDescriptorBase<StandardMaterialDescriptor>
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;

        constexpr static int SetIndex = 1;

        static MXC_RESULT InitLayout(const Vulkan::Device& device)
        {
            MXC_LOG("Init StandardMaterialDescriptor Layout");
            StaticArray bindings{
              (VkDescriptorSetLayoutBinding){
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
            };
            MXC_CHK(CreateDescriptorSetLayout(device, bindings));
            return MXC_SUCCESS;
        }

        MXC_RESULT Init(const Texture& texture)
        {
            MXC_LOG("Init MaterialDescriptor");

            MXC_CHK(AllocateDescriptorSet());
            StaticArray writes{
              (VkWriteDescriptorSet){
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = StaticRef((VkDescriptorImageInfo){
                  .sampler = Device->GetVkLinearSampler(),
                  .imageView = texture.GetVkImageView(),
                  .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                })},
            };
            WriteDescriptors(writes);

            return MXC_SUCCESS;
        }
    };
}// namespace Moxaic::Vulkan
