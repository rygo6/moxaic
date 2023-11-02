#pragma once

#include "moxaic_vulkan.hpp"

#include <vulkan/vulkan.h>
#include <windows.h>

namespace Moxaic
{
    class VulkanDevice;

    template<typename T>
    class VulkanUniform
    {
    public:
        VulkanUniform(const VulkanDevice &device);
        virtual ~VulkanUniform();

        bool InitFromImport();

        bool Init(const VkMemoryPropertyFlags properties,
                  const VkBufferUsageFlags usage,
                  const Locality external);

        inline T& Mapped() { return *m_pMappedBuffer; }
        inline VkDeviceSize BufferSize() const { return sizeof(T); }

        inline auto vkBuffer() const { return m_VkBuffer; }

    private:
        const VulkanDevice &k_Device;
        VkBuffer m_VkBuffer{VK_NULL_HANDLE};
        VkDeviceMemory m_VkDeviceMemory{VK_NULL_HANDLE};
        T *m_pMappedBuffer{nullptr};
#ifdef WIN32
        HANDLE m_ExternalMemory{nullptr};
#endif
    };
}
