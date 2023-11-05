#pragma once

#include "../moxaic_logging.hpp"

#include "../moxaic_vulkan.hpp"
#include "../moxaic_vulkan_device.hpp"

#include <vulkan/vulkan.h>
#include <vector>

namespace Moxaic
{
    class VulkanDevice;

    class VulkanDescriptorShared
    {
    protected:
        inline static std::vector<VkDescriptorSetLayoutBinding> s_Bindings{};
        inline static std::vector<VkWriteDescriptorSet> s_Writes{};
    };

    template<typename T>
    class VulkanDescriptorBase : public VulkanDescriptorShared
    {
    public:
        VulkanDescriptorBase(const VulkanDevice &device)
                : k_Device(device) {}

        virtual ~VulkanDescriptorBase()
        {
            vkFreeDescriptorSets(k_Device.vkDevice(),
                                 k_Device.vkDescriptorPool(),
                                 1,
                                 &m_VkDescriptorSet);
        }

        inline VkDescriptorSetLayout vkDescriptorSetLayout() const { return s_VkDescriptorSetLayout; }
        inline auto vkDescriptorSet() const { return m_VkDescriptorSet; }
    protected:
        const VulkanDevice &k_Device;
        inline static VkDescriptorSetLayout s_VkDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSet m_VkDescriptorSet{VK_NULL_HANDLE};

        static bool initializeLayout() { return s_VkDescriptorSetLayout == VK_NULL_HANDLE; }

        MXC_RESULT CreateDescriptorSetLayout()
        {
            VkDescriptorSetLayoutCreateInfo layoutInfo = {};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.pNext = nullptr;
            layoutInfo.flags = 0;
            layoutInfo.bindingCount = s_Bindings.size();
            layoutInfo.pBindings = s_Bindings.data();
            VK_CHK(vkCreateDescriptorSetLayout(k_Device.vkDevice(),
                                               &layoutInfo,
                                               VK_ALLOC,
                                               &s_VkDescriptorSetLayout));
            s_Bindings.clear();
            return MXC_SUCCESS;
        }

        MXC_RESULT AllocateDescriptorSet()
        {
            const VkDescriptorSetAllocateInfo allocInfo = {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                    .pNext = nullptr,
                    .descriptorPool = k_Device.vkDescriptorPool(),
                    .descriptorSetCount = 1,
                    .pSetLayouts = &s_VkDescriptorSetLayout,
            };
            VK_CHK(vkAllocateDescriptorSets(k_Device.vkDevice(),
                                            &allocInfo,
                                            &m_VkDescriptorSet));
            return MXC_SUCCESS;
        }

        void PushBinding(VkDescriptorSetLayoutBinding binding)
        {
            binding.binding = s_Bindings.size();
            binding.descriptorCount = 1;
            s_Bindings.push_back(binding);
        }

        void PushWrite(VkDescriptorImageInfo imageInfo)
        {
            SDL_assert_always(m_VkDescriptorSet != nullptr);
            VkWriteDescriptorSet write = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = m_VkDescriptorSet,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &imageInfo
            };
            write.dstBinding = s_Writes.size();
            write.descriptorCount = write.descriptorCount == 0 ? 1 : write.descriptorCount;
            s_Writes.push_back(write);
        }

        void PushWrite(VkDescriptorBufferInfo bufferInfo)
        {
            SDL_assert_always(m_VkDescriptorSet != nullptr);
            VkWriteDescriptorSet write = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = nullptr,
                    .dstSet = m_VkDescriptorSet,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .pBufferInfo = &bufferInfo
            };
            write.dstBinding = s_Writes.size();
            write.descriptorCount = write.descriptorCount == 0 ? 1 : write.descriptorCount;
            s_Writes.push_back(write);
        }

        void WritePushedDescriptors()
        {
            VK_CHK_VOID(vkUpdateDescriptorSets(k_Device.vkDevice(),
                                               s_Writes.size(),
                                               s_Writes.data(),
                                               0,
                                               nullptr));
            s_Writes.clear();
        }
    };
}

