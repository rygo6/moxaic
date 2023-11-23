#pragma once

#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"

#include <vulkan/vulkan.h>
#include <windows.h>

namespace Moxaic::Vulkan
{
    class Device;

    template<typename T>
    class Uniform
    {
    public:
        MXC_NO_VALUE_PASS(Uniform);

        explicit Uniform(Device const& device)
            : k_Device(device) {}

        virtual ~Uniform()
        {
            vkDestroyBuffer(k_Device.GetVkDevice(), m_VkBuffer, VK_ALLOC);

            if (m_pMappedBuffer != nullptr)
                vkUnmapMemory(k_Device.GetVkDevice(), m_VkDeviceMemory);

            vkFreeMemory(k_Device.GetVkDevice(), m_VkDeviceMemory, VK_ALLOC);

            if (m_ExternalMemory != nullptr)
                CloseHandle(m_ExternalMemory);
        }

        bool Init(VkMemoryPropertyFlags const properties,
                  VkBufferUsageFlags const usage,
                  Locality const locality)
        {
            MXC_LOG_MULTILINE("Init Uniform:",
                              string_VkMemoryPropertyFlags(properties),
                              string_VkBufferUsageFlags(usage),
                              Size(),
                              string_Locality(locality));
            constexpr VkMemoryPropertyFlags supportedProperties =
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            SDL_assert(((supportedProperties & properties) == supportedProperties) &&
                       "Uniform needs to be VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT!");
            MXC_CHK(k_Device.CreateAllocateBindBuffer(usage,
                                                      properties,
                                                      Size(),
                                                      locality,
                                                      &m_VkBuffer,
                                                      &m_VkDeviceMemory,
                                                      &m_ExternalMemory));
            VK_CHK(vkMapMemory(k_Device.GetVkDevice(),
                               m_VkDeviceMemory,
                               0,
                               Size(),
                               0,
                               &m_pMappedBuffer));
            return MXC_SUCCESS;
        }

        void CopyBuffer(T const& srcBuffer)
        {
            memcpy(m_pMappedBuffer, &srcBuffer, Size());
        }

        T& mapped() { return *static_cast<T*>(m_pMappedBuffer); }
        auto const& vkBuffer() const { return m_VkBuffer; }

        static constexpr VkDeviceSize Size() { return sizeof(T); }

    private:
        Device const& k_Device;
        VkBuffer m_VkBuffer{VK_NULL_HANDLE};
        VkDeviceMemory m_VkDeviceMemory{VK_NULL_HANDLE};
        void* m_pMappedBuffer{nullptr};
#ifdef WIN32
        HANDLE m_ExternalMemory{nullptr};
#endif
    };
}// namespace Moxaic::Vulkan
