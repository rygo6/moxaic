#pragma once

#include "main.hpp"
#include "moxaic_logging.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"
#include "vulkan_medium.hpp"

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
        const char* name;

    public:
        const VkDescriptorSet& VkDescriptorSetHandle{vkDescriptorSetHandle};

        MXC_NO_VALUE_PASS(VulkanDescriptorBase2)
        VulkanDescriptorBase2() = default;
        ~VulkanDescriptorBase2()
        {
            if (vkDescriptorSetHandle != VK_NULL_HANDLE)
                vkFreeDescriptorSets(device->GetVkDevice(),
                                     device->GetVkDescriptorPool(),
                                     1,
                                     &vkDescriptorSetHandle);
        }

        static VkResult InitLayout(const Vulkan::Device& device)
        {
            VulkanDescriptorBase2::device = &device;
            const MVk::DescriptorSetLayoutCreateInfo createInfo{
              .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
              .bindingCount = Derived::LayoutBindings.size(),
              .pBindings = Derived::LayoutBindings.data(),
            };
            return MVk::CreateDescriptorSetLayout(device.GetVkDevice(),
                                                  &createInfo,
                                                  &vkSharedDescriptorSetLayoutHandle);
        }

        static const VkDescriptorSetLayout& GetOrInitSharedVkDescriptorSetLayout(const Device& device)
        {
            if (vkSharedDescriptorSetLayoutHandle == VK_NULL_HANDLE)
                InitLayout(device);
            return vkSharedDescriptorSetLayoutHandle;
        }

        MXC_RESULT Init()
        {
            const MVk::DescriptorSetAllocateInfo allocInfo{
              .descriptorPool = device->GetVkDescriptorPool(),
              .pSetLayouts = &vkSharedDescriptorSetLayoutHandle,
            };
            VK_CHK(MVk::AllocateDescriptorSets(device->GetVkDevice(),
                                               &allocInfo,
                                               &vkDescriptorSetHandle));
            const VkDebugUtilsObjectNameInfoEXT debugInfo{
              .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
              .objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET,
              .objectHandle = (uint64_t) vkDescriptorSetHandle,
              .pObjectName = name};
            VK_CHK(VkFunc.SetDebugUtilsObjectNameEXT(device->VkDeviceHandle,
                                                     &debugInfo));
            return MXC_SUCCESS;
        }

        static void EmplaceDescriptorWrite(const uint32_t bindingIndex,
                                           const VkDescriptorSet dstSet,
                                           const MVk::DescriptorImageInfo* pImageInfo,
                                           MVk::WriteDescriptorSet* pWriteDescriptorSet)
        {
            new (pWriteDescriptorSet) MVk::WriteDescriptorSet{
              .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              .dstSet = dstSet,
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
        const char* name;

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

            const VkDebugUtilsObjectNameInfoEXT debugInfo{
              .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
              .objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET,
              .objectHandle = (uint64_t) vkDescriptorSet,
              .pObjectName = name};
            VkFunc.SetDebugUtilsObjectNameEXT(device->VkDeviceHandle, &debugInfo);

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
            vkUpdateDescriptorSets(device->GetVkDevice(),
                                   writes.size(),
                                   writes.data(),
                                   0,
                                   nullptr);
        }
    };
}// namespace Moxaic::Vulkan
