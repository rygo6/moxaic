#pragma once

#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"

#include <vulkan/vulkan.h>
#include <windows.h>

namespace Moxaic::Vulkan
{
    class Device;

    template<typename T>
    class Buffer
    {
    public:
        MXC_NO_VALUE_PASS(Buffer);

        explicit Buffer(const Vulkan::Device* const pDevice)
            : Device(pDevice) {}

        virtual ~Buffer()
        {
            vkDestroyBuffer(Device->GetVkDevice(), vkBuffer, VK_ALLOC);
            if (mappedBuffer != nullptr) {
                vkUnmapMemory(Device->GetVkDevice(), vkDeviceMemory);
            }
            vkFreeMemory(Device->GetVkDevice(), vkDeviceMemory, VK_ALLOC);
            if (externalMemory != nullptr) {
                CloseHandle(externalMemory);
            }
        }

        bool Init(const VkMemoryPropertyFlags properties,
                  const VkBufferUsageFlags usage,
                  const Locality locality)
        {
            MXC_CHK(Device->CreateAllocateBindBuffer(usage,
                                                     properties,
                                                     Size(),
                                                     locality,
                                                     &vkBuffer,
                                                     &vkDeviceMemory,
                                                     &externalMemory));
            if ((VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT & properties) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
                VK_CHK(vkMapMemory(Device->GetVkDevice(),
                                   vkDeviceMemory,
                                   0,
                                   Size(),
                                   0,
                                   (void**)&mappedBuffer));
            }
            return MXC_SUCCESS;
        }

        void CopyBuffer(const T& srcBuffer)
        {
            memcpy(mappedBuffer, &srcBuffer, Size());
        }

        static constexpr VkDeviceSize Size() { return sizeof(T); }

        const auto& GetVkBuffer() const { return vkBuffer; }

        T* mappedBuffer{nullptr};

    private:
        const Device* const Device;
        VkBuffer vkBuffer{VK_NULL_HANDLE};
        VkDeviceMemory vkDeviceMemory{VK_NULL_HANDLE};
#ifdef WIN32
        HANDLE externalMemory{nullptr};
#endif
    };
}
