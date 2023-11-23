#pragma once

#include "main.hpp"
#include "moxaic_logging.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"

#include <vector>
#include <vulkan/vulkan.h>

#include "static_array.hpp"

namespace Moxaic::Vulkan
{
    class Device;

    template<typename T>
    class VulkanDescriptorBase
    {
    public:
        MXC_NO_VALUE_PASS(VulkanDescriptorBase)

        explicit VulkanDescriptorBase(const Device& device)
            : k_Device(device) {}

        virtual ~VulkanDescriptorBase()
        {
            vkFreeDescriptorSets(k_Device.GetVkDevice(),
                                 k_Device.GetVkDescriptorPool(),
                                 1,
                                 &m_VkDescriptorSet);
        }

        const static VkDescriptorSetLayout& vkDescriptorSetLayout() { return s_VkDescriptorSetLayout; }
        const auto& vkDescriptorSet() const { return m_VkDescriptorSet; }

    protected:
        const Device& k_Device;
        inline static VkDescriptorSetLayout s_VkDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSet m_VkDescriptorSet{VK_NULL_HANDLE};

        static bool initializeLayout() { return s_VkDescriptorSetLayout == VK_NULL_HANDLE; }

        template<uint32_t N>
        MXC_RESULT CreateDescriptorSetLayout(StaticArray<VkDescriptorSetLayoutBinding, N>& bindings) const
        {
            for (int i = 0; i < bindings.size(); ++i) {
                bindings[i].binding = i;
                bindings[i].descriptorCount = bindings[i].descriptorCount == 0 ? 1 : bindings[i].descriptorCount;
            }
            const VkDescriptorSetLayoutCreateInfo layoutInfo{
              .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
              .pNext = nullptr,
              .flags = 0,
              .bindingCount = bindings.size(),
              .pBindings = bindings.data(),
            };
            VK_CHK(vkCreateDescriptorSetLayout(k_Device.GetVkDevice(),
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
              .descriptorPool = k_Device.GetVkDescriptorPool(),
              .descriptorSetCount = 1,
              .pSetLayouts = &s_VkDescriptorSetLayout,
            };
            VK_CHK(vkAllocateDescriptorSets(k_Device.GetVkDevice(),
                                            &allocInfo,
                                            &m_VkDescriptorSet));
            return MXC_SUCCESS;
        }

        template<uint32_t N>
        void WriteDescriptors(StaticArray<VkWriteDescriptorSet, N>& writes) const
        {
            for (int i = 0; i < writes.size(); ++i) {
                writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[i].dstSet = m_VkDescriptorSet;
                writes[i].dstBinding = i;
                writes[i].descriptorCount = writes[i].descriptorCount == 0
                                              ? 1
                                              : writes[i].descriptorCount;
            }
            VK_CHK_VOID(vkUpdateDescriptorSets(k_Device.GetVkDevice(),
                                               writes.size(),
                                               writes.data(),
                                               0,
                                               nullptr));
        }
    };
}// namespace Moxaic::Vulkan
