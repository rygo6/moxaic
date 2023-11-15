#pragma once

#include "moxaic_vulkan.hpp"
#include "moxaic_logging.hpp"

#include <vulkan/vulkan.h>
#include <array>

#ifdef WIN32
#include <windows.h>
#endif

namespace Moxaic
{
    class VulkanDevice;

    class VulkanTimelineSemaphore
    {
    public:
        VulkanTimelineSemaphore(const VulkanDevice &device);
        virtual ~VulkanTimelineSemaphore();

        MXC_RESULT Init(bool readOnly, Vulkan::Locality locality);

        MXC_RESULT Wait();

        inline void IncrementWaitValue() { m_WaitValue++; }

        inline auto vkSemaphore() const { return m_vkSemaphore; }
        inline auto waitValue() const { return m_WaitValue; }

    private:
        const VulkanDevice &k_Device;

        uint64_t m_WaitValue{0};
        VkSemaphore m_vkSemaphore{VK_NULL_HANDLE};

#ifdef WIN32
        HANDLE m_ExternalHandle{nullptr};
#endif
    };
}
