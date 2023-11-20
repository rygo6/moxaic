#pragma once

#include "moxaic_vulkan_descriptor.hpp"
#include "moxaic_vulkan_texture.hpp"

namespace Moxaic::Vulkan
{
    class StandardMaterialDescriptor : public VulkanDescriptorBase<StandardMaterialDescriptor>
    {
    public:
        using VulkanDescriptorBase::VulkanDescriptorBase;

        MXC_RESULT Init(const Texture &texture)
        {
            MXC_LOG("Init MaterialDescriptor");
            SDL_assert(m_VkDescriptorSet == nullptr);

            // todo should this be ina  different method so I can call them all before trying make any descriptors???
            if (initializeLayout()) {
                StaticArray bindings{
                        (VkDescriptorSetLayoutBinding) {
                                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                        }
                };
                MXC_CHK(CreateDescriptorSetLayout(bindings.size(),
                                                  bindings.data()));
            }

            MXC_CHK(AllocateDescriptorSet());
            const VkDescriptorImageInfo imageInfo{
                    .sampler = k_Device.vkLinearSampler(),
                    .imageView = texture.vkImageView(),
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
            StaticArray writes{
                    (VkWriteDescriptorSet) {
                            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            .pImageInfo = &imageInfo
                    },
            };
            WriteDescriptors(writes.size(),
                             writes.data());

            return MXC_SUCCESS;
        }
    };
}
