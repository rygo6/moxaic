#pragma once

#include "moxaic_vulkan.hpp"
#include "moxaic_logging.hpp"

#include <vulkan/vulkan.h>
#include <array>

#ifdef WIN32
#include <windows.h>
#endif

namespace Moxaic::Vulkan
{
    class Device;

    class Semaphore
    {
    public:
        Semaphore(const Device &device);
        virtual ~Semaphore();

        MXC_RESULT Init(const bool readOnly,
                        const Vulkan::Locality locality);

        MXC_RESULT InitFromImport(const bool readOnly,
                                  const HANDLE externalHandle);

        MXC_RESULT Wait();

        inline void IncrementWaitValue() { m_WaitValue++; }

        const HANDLE ClonedExternalHandle(HANDLE hTargetProcessHandle) const;

        inline auto vkSemaphore() const { return m_vkSemaphore; }
        inline auto waitValue() const { return m_WaitValue; }

    private:
        const Device &k_Device;

        uint64_t m_WaitValue{0};
        VkSemaphore m_vkSemaphore{VK_NULL_HANDLE};

#ifdef WIN32
        HANDLE m_ExternalHandle{nullptr};
#endif
    };
}
