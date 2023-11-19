#pragma once

#include "main.hpp"
#include "moxaic_logging.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"

#include <vulkan/vulkan.h>
#include <vector>

namespace Moxaic::Vulkan
{
    class Device;

    template<typename T>
    class VulkanDescriptorBase
    {
        MXC_NO_VALUE_PASS(VulkanDescriptorBase)

    public:
        explicit VulkanDescriptorBase(const Device&device)
            : k_Device(device)
        {
        }

        virtual ~VulkanDescriptorBase()
        {
            vkFreeDescriptorSets(k_Device.vkDevice(),
                                 k_Device.vkDescriptorPool(),
                                 1,
                                 &m_VkDescriptorSet);
        }

        static VkDescriptorSetLayout vkDescriptorSetLayout() { return s_VkDescriptorSetLayout; }
        const auto& vkDescriptorSet() const { return m_VkDescriptorSet; }

    protected:
        const Device&k_Device;
        inline static VkDescriptorSetLayout s_VkDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSet m_VkDescriptorSet{VK_NULL_HANDLE};

        static bool initializeLayout() { return s_VkDescriptorSetLayout == VK_NULL_HANDLE; }

        MXC_RESULT CreateDescriptorSetLayout(const uint32_t bindingCount,
                                             VkDescriptorSetLayoutBinding* pBindings) const
        {
            for (int i = 0; i < bindingCount; ++i) {
                pBindings[i].binding = i;
                pBindings[i].descriptorCount = pBindings[i].descriptorCount == 0 ? 1 : pBindings[i].descriptorCount;
            }
            const VkDescriptorSetLayoutCreateInfo layoutInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .bindingCount = bindingCount,
                .pBindings = pBindings,
            };
            VK_CHK(vkCreateDescriptorSetLayout(k_Device.vkDevice(),
                &layoutInfo,
                VK_ALLOC,
                &s_VkDescriptorSetLayout));
            return MXC_SUCCESS;
        }

        MXC_RESULT AllocateDescriptorSet()
        {
            const VkDescriptorSetAllocateInfo allocInfo{
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

        void WriteDescriptors(const uint32_t descriptorWriteCount,
                              VkWriteDescriptorSet* pDescriptorWrites) const
        {
            for (int i = 0; i < descriptorWriteCount; ++i) {
                pDescriptorWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                pDescriptorWrites[i].dstSet = m_VkDescriptorSet;
                pDescriptorWrites[i].dstBinding = i;
                pDescriptorWrites[i].descriptorCount = pDescriptorWrites[i].descriptorCount == 0
                                                           ? 1
                                                           : pDescriptorWrites[i].descriptorCount;
            }
            VK_CHK_VOID(vkUpdateDescriptorSets(k_Device.vkDevice(),
                descriptorWriteCount,
                pDescriptorWrites,
                0,
                nullptr));
        }
    };
}
