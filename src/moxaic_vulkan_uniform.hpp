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
        explicit VulkanUniform(const VulkanDevice &device);
        virtual ~VulkanUniform();

        bool InitFromImport();

        bool Init(const VkMemoryPropertyFlags &properties,
                  const VkBufferUsageFlags &usage,
                  const BufferLocality &external);

        inline T Mapped() const { return m_pMappedBuffer; }
        inline VkDeviceSize BufferSize() const { return sizeof(T); }

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
