#pragma once

#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"


#include <vulkan/vulkan.h>
#include <windows.h>

namespace Moxaic
{
    class VulkanDevice;

    template<typename T>
    class VulkanUniform
    {
    public:
        VulkanUniform(const VulkanDevice &device)
                : k_Device(device) {}

        virtual ~VulkanUniform()
        {
            vkDestroyBuffer(k_Device.vkDevice(), m_VkBuffer, VK_ALLOC);

            if (m_pMappedBuffer != nullptr)
                vkUnmapMemory(k_Device.vkDevice(), m_VkDeviceMemory);

            vkFreeMemory(k_Device.vkDevice(), m_VkDeviceMemory, VK_ALLOC);

            if (m_ExternalMemory != nullptr)
                CloseHandle(m_ExternalMemory);
        }

        bool InitFromImport()
        {
            return MXC_SUCCESS;
        }

        bool Init(const VkMemoryPropertyFlags properties,
                  const VkBufferUsageFlags usage,
                  const Locality locality)
        {
            MXC_LOG("Init Uniform:",
                    string_VkMemoryPropertyFlags(properties),
                    string_VkBufferUsageFlags(usage),
                    BufferSize(),
                    string_BufferLocality(locality));
            VkMemoryPropertyFlags supportedProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            SDL_assert(((supportedProperties & properties) == supportedProperties) &&
                       "Uniform needs to be VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT!");
            MXC_CHK(k_Device.CreateAllocateBindBuffer(usage,
                                                      properties,
                                                      BufferSize(),
                                                      locality,
                                                      m_VkBuffer,
                                                      m_VkDeviceMemory,
                                                      m_ExternalMemory));
            VK_CHK(vkMapMemory(k_Device.vkDevice(),
                               m_VkDeviceMemory,
                               0,
                               BufferSize(),
                               0,
                               &m_pMappedBuffer));
            return MXC_SUCCESS;
        }

        inline T &Mapped() { return *(T*)(m_pMappedBuffer); }
        inline VkDeviceSize BufferSize() const { return sizeof(T); }

        inline auto vkBuffer() const { return m_VkBuffer; }

    private:
        const VulkanDevice &k_Device;
        VkBuffer m_VkBuffer{VK_NULL_HANDLE};
        VkDeviceMemory m_VkDeviceMemory{VK_NULL_HANDLE};
        void *m_pMappedBuffer{nullptr};
#ifdef WIN32
        HANDLE m_ExternalMemory{nullptr};
#endif
    };
}
