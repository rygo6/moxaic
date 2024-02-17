#pragma once

#include "main.hpp"
#include "moxaic_logging.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"
#include "vulkan_mid.hpp"

#include <vulkan/vulkan.h>

#include "static_array.hpp"

namespace Moxaic::Vulkan
{
    class Device;

    template<typename Derived>
    class VulkanDescriptorBase2
    {
    protected:
        inline static const Device* device{VK_NULL_HANDLE};
        inline static VkDescriptorSetLayout vkSharedDescriptorSetLayoutHandle{VK_NULL_HANDLE};
        VkDescriptorSet vkDescriptorSetHandle{VK_NULL_HANDLE};

    public:
        const VkDescriptorSet& VkDescriptorSetHandle{vkDescriptorSetHandle};

        static VkResult InitLayout(const Vulkan::Device& device)
        {
            VulkanDescriptorBase2::device = &device;
            return Vkm::CreateDescriptorSetLayout(device.GetVkDevice(),
                                                  Derived::LayoutBindings.size(),
                                                  Derived::LayoutBindings.data(),
                                                  &vkSharedDescriptorSetLayoutHandle);
        }

        static const VkDescriptorSetLayout& GetOrInitSharedVkDescriptorSetLayout(const Device& device)
        {
            if (vkSharedDescriptorSetLayoutHandle == VK_NULL_HANDLE)
                InitLayout(device);
            return vkSharedDescriptorSetLayoutHandle;
        }

        VkResult Init()
        {
            const Vkm::DescriptorSetAllocateInfo allocInfo{
                .descriptorPool = device->GetVkDescriptorPool(),
                .pSetLayouts = &vkSharedDescriptorSetLayoutHandle,
              };
            return Vkm::AllocateDescriptorSets(device->GetVkDevice(),
                                                &allocInfo,
                                                &vkDescriptorSetHandle);
        }

        void EmplaceDescriptorWrite(const uint32_t bindingIndex,
                        const Vkm::DescriptorImageInfo* pImageInfo,
                        Vkm::WriteDescriptorSet* pWriteDescriptorSet) const
        {
            new (pWriteDescriptorSet) Vkm::WriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = vkDescriptorSetHandle,
                .dstBinding = Derived::LayoutBindings[bindingIndex].binding,
                .descriptorCount = Derived::LayoutBindings[bindingIndex].descriptorCount,
                .descriptorType = Derived::LayoutBindings[bindingIndex].descriptorType,
                .pImageInfo = pImageInfo,
              };
        }
    };

    template<typename Derived>
    class VulkanDescriptorBase
    {
    public:
        MXC_NO_VALUE_PASS(VulkanDescriptorBase)

        explicit VulkanDescriptorBase() = default;
        explicit VulkanDescriptorBase(const Device* const device)
            : device(device) {}

        ~VulkanDescriptorBase()
        {
            vkFreeDescriptorSets(device->GetVkDevice(),
                                 device->GetVkDescriptorPool(),
                                 1,
                                 &vkDescriptorSet);
        }

        const auto& GetVkDescriptorSet() const { return vkDescriptorSet; }

        static const VkDescriptorSetLayout& GetOrInitSharedVkDescriptorSetLayout(const Device& device)
        {
            CheckLayoutInitialized(device);
            return sharedVkDescriptorSetLayout;
        }

    protected:
        const Device* device;
        // at some point layout will need to be a map on the device to support multiple devices
        inline static VkDescriptorSetLayout sharedVkDescriptorSetLayout{VK_NULL_HANDLE};
        VkDescriptorSet vkDescriptorSet{VK_NULL_HANDLE};

        static void CheckLayoutInitialized(const Vulkan::Device& device)
        {
            if (sharedVkDescriptorSetLayout == VK_NULL_HANDLE)
                Derived::InitLayout(device);
        }

        template<uint32_t N>
        static MXC_RESULT CreateDescriptorSetLayout(const Vulkan::Device& device,
                                                    StaticArray<VkDescriptorSetLayoutBinding, N>& bindings)
        {
            assert(sharedVkDescriptorSetLayout == VK_NULL_HANDLE);
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
                                               &sharedVkDescriptorSetLayout));
            return MXC_SUCCESS;
        }

        MXC_RESULT AllocateDescriptorSet()
        {
            assert(vkDescriptorSet == nullptr);
            CheckLayoutInitialized(*device);
            const VkDescriptorSetAllocateInfo allocInfo{
              .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
              .pNext = nullptr,
              .descriptorPool = device->GetVkDescriptorPool(),
              .descriptorSetCount = 1,
              .pSetLayouts = &sharedVkDescriptorSetLayout,
            };
            VK_CHK(vkAllocateDescriptorSets(device->GetVkDevice(),
                                            &allocInfo,
                                            &vkDescriptorSet));
            return MXC_SUCCESS;
        }

        template<uint32_t N>
        void WriteDescriptors(StaticArray<VkWriteDescriptorSet, N>& writes) const
        {
            assert(sharedVkDescriptorSetLayout != VK_NULL_HANDLE);
            assert(vkDescriptorSet != nullptr);
            for (int i = 0; i < writes.size(); ++i) {
                writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[i].dstSet = vkDescriptorSet;
                writes[i].dstBinding = writes[i].dstBinding == 0 ? i : writes[i].dstBinding;
                writes[i].descriptorCount = writes[i].descriptorCount == 0 ? 1 : writes[i].descriptorCount;
            }
            VK_CHK_VOID(vkUpdateDescriptorSets(device->GetVkDevice(),
                                               writes.size(),
                                               writes.data(),
                                               0,
                                               nullptr));
        }
    };
}// namespace Moxaic::Vulkan
