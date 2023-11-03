#pragma once

#include "../moxaic_logging.hpp"

#include "../moxaic_vulkan.hpp"
#include "../moxaic_vulkan_device.hpp"

#include <vulkan/vulkan.h>
#include <vector>

namespace Moxaic
{
    class VulkanDevice;

    namespace
    {
        thread_local inline std::vector<VkDescriptorSetLayoutBinding> g_Bindings;
        thread_local inline std::vector<VkWriteDescriptorSet> g_Writes;
    }

    template<typename T>
    class VulkanDescriptorBase
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

        MXC_RESULT Init()
        {
            if (vkLayout() == VK_NULL_HANDLE) {
                MXC_CHK(SetupDescriptorSetLayout());
            };
            MXC_CHK(AllocateDescriptorSet());
            MXC_CHK(SetupDescriptorSet());
            return MXC_SUCCESS;
        }

        inline VkDescriptorSetLayout vkLayout() const { return s_VkDescriptorSetLayout; }
        inline auto vkSet() const { return m_VkDescriptorSet; }
    protected:
        const VulkanDevice &k_Device;
        inline static VkDescriptorSetLayout s_VkDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSet m_VkDescriptorSet{VK_NULL_HANDLE};

        virtual MXC_RESULT SetupDescriptorSetLayout() = 0;
        virtual MXC_RESULT SetupDescriptorSet() = 0;

        MXC_RESULT CreateDescriptorSetLayout()
        {
            VkDescriptorSetLayoutCreateInfo layoutInfo = {};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.pNext = nullptr;
            layoutInfo.flags = 0;
            layoutInfo.bindingCount = g_Bindings.size();
            layoutInfo.pBindings = g_Bindings.data();
            VK_CHK(vkCreateDescriptorSetLayout(k_Device.vkDevice(),
                                               &layoutInfo,
                                               VK_ALLOC,
                                               &s_VkDescriptorSetLayout));
            g_Bindings.clear();
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
            binding.binding = g_Bindings.size();
            binding.descriptorCount = 1;
            g_Bindings.push_back(binding);
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
            write.dstBinding = g_Writes.size();
            write.descriptorCount = write.descriptorCount == 0 ? 1 : write.descriptorCount;
            g_Writes.push_back(write);
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
            write.dstBinding = g_Writes.size();
            write.descriptorCount = write.descriptorCount == 0 ? 1 : write.descriptorCount;
            g_Writes.push_back(write);
        }

        void WritePushedDescriptors()
        {
            VK_CHK_VOID(vkUpdateDescriptorSets(k_Device.vkDevice(),
                                               g_Writes.size(),
                                               g_Writes.data(),
                                               0,
                                               nullptr));
            g_Writes.clear();
        }
    };
}

