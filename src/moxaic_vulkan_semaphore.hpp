#pragma once

#include "moxaic_logging.hpp"
#include "moxaic_vulkan.hpp"

#include <vulkan/vulkan.h>

#ifdef WIN32
#include <windows.h>
#endif

namespace Moxaic::Vulkan
{
    class Device;

    class Semaphore
    {
    public:
        MXC_NO_VALUE_PASS(Semaphore);

        explicit Semaphore(const Device& device);
        virtual ~Semaphore();

        MXC_RESULT Init(bool readOnly,
                        Locality locality);

        MXC_RESULT InitFromImport(bool readOnly,
                                  HANDLE externalHandle);

        MXC_RESULT SyncWaitValue();

        MXC_RESULT Wait();

        HANDLE ClonedExternalHandle(HANDLE hTargetProcessHandle) const;

        void IncrementWaitValue() { m_WaitValue++; }

        void IncrementWaitValue(const uint64_t step) { m_WaitValue += step; }

        const auto& vkSemaphore() const { return m_vkSemaphore; }
        auto waitValue() const { return m_WaitValue; }

    private:
        const Device& k_Device;

        uint64_t m_WaitValue{0};
        VkSemaphore m_vkSemaphore{VK_NULL_HANDLE};

#ifdef WIN32
        HANDLE m_ExternalHandle{nullptr};
#endif
    };
}// namespace Moxaic::Vulkan
