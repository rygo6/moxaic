#pragma once

#include "moxaic_vulkan.hpp"
#include "moxaic_logging.hpp"

#include <vulkan/vulkan.h>

#ifdef WIN32
#include <windows.h>
#endif

namespace Moxaic::Vulkan
{
    class Device;

    class Semaphore
    {
        MXC_NO_VALUE_PASS(Semaphore);

    public:
        explicit Semaphore(const Device& device);
        virtual ~Semaphore();

        MXC_RESULT Init(bool readOnly,
                        Locality locality);

        MXC_RESULT InitFromImport(bool readOnly,
                                  HANDLE externalHandle);

        MXC_RESULT Wait();

        void IncrementWaitValue() { m_WaitValue++; }

        HANDLE ClonedExternalHandle(HANDLE hTargetProcessHandle) const;

        const auto& vkSemaphore() const { return m_vkSemaphore; }
        const auto& waitValue() const { return m_WaitValue; }

    private:
        const Device& k_Device;

        uint64_t m_WaitValue{0};
        VkSemaphore m_vkSemaphore{VK_NULL_HANDLE};

#ifdef WIN32
        HANDLE m_ExternalHandle{nullptr};
#endif
    };
}
