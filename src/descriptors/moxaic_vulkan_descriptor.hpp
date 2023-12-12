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

    template<typename Derived>
    class VulkanDescriptorBase
    {
    public:
        MXC_NO_VALUE_PASS(VulkanDescriptorBase)

        explicit VulkanDescriptorBase(const Vulkan::Device* const pDevice)
            : k_pDevice(pDevice) {}

        virtual ~VulkanDescriptorBase()
        {
            vkFreeDescriptorSets(k_pDevice->GetVkDevice(),
                                 k_pDevice->GetVkDescriptorPool(),
                                 1,
                                 &m_VkDescriptorSet);
        }

        MXC_GET(VkDescriptorSet);

        static const VkDescriptorSetLayout& GetOrInitVkDescriptorSetLayout(const Vulkan::Device& device)
        {
            CheckLayoutInitialized(device);
            return s_VkDescriptorSetLayout;
        }

    protected:
        const Device* const k_pDevice;
        // at some point layout will need to be a map on the device to support multiple devices
        inline static VkDescriptorSetLayout s_VkDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSet m_VkDescriptorSet{VK_NULL_HANDLE};

        static void CheckLayoutInitialized(const Vulkan::Device& device)
        {
            if (s_VkDescriptorSetLayout == VK_NULL_HANDLE)
                Derived::InitLayout(device);
        }

        template<uint32_t N>
        static MXC_RESULT CreateDescriptorSetLayout(const Vulkan::Device& device,
                                                    StaticArray<VkDescriptorSetLayoutBinding, N>& bindings)
        {
            SDL_assert(s_VkDescriptorSetLayout == VK_NULL_HANDLE);
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
            VK_CHK(vkCreateDescriptorSetLayout(device.GetVkDevice(),
                                               &layoutInfo,
                                               VK_ALLOC,
                                               &s_VkDescriptorSetLayout));
            return MXC_SUCCESS;
        }

        MXC_RESULT AllocateDescriptorSet()
        {
            SDL_assert(m_VkDescriptorSet == nullptr);
            CheckLayoutInitialized(*k_pDevice);
            const VkDescriptorSetAllocateInfo allocInfo{
              .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
              .pNext = nullptr,
              .descriptorPool = k_pDevice->GetVkDescriptorPool(),
              .descriptorSetCount = 1,
              .pSetLayouts = &s_VkDescriptorSetLayout,
            };
            VK_CHK(vkAllocateDescriptorSets(k_pDevice->GetVkDevice(),
                                            &allocInfo,
                                            &m_VkDescriptorSet));
            return MXC_SUCCESS;
        }

        template<uint32_t N>
        void WriteDescriptors(StaticArray<VkWriteDescriptorSet, N>& writes) const
        {
            SDL_assert(s_VkDescriptorSetLayout != VK_NULL_HANDLE);
            SDL_assert(m_VkDescriptorSet != nullptr);
            for (int i = 0; i < writes.size(); ++i) {
                writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[i].dstSet = m_VkDescriptorSet;
                writes[i].dstBinding = writes[i].dstBinding == 0 ? i : writes[i].dstBinding;
                writes[i].descriptorCount = writes[i].descriptorCount == 0 ? 1 : writes[i].descriptorCount;
            }
            VK_CHK_VOID(vkUpdateDescriptorSets(k_pDevice->GetVkDevice(),
                                               writes.size(),
                                               writes.data(),
                                               0,
                                               nullptr));
        }
    };
}// namespace Moxaic::Vulkan
