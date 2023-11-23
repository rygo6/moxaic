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

        explicit Semaphore(Device const& device);
        virtual ~Semaphore();

        MXC_RESULT Init(bool const& readOnly,
                        Locality const& locality);

        MXC_RESULT InitFromImport(bool const& readOnly,
                                  HANDLE const& externalHandle);

        MXC_RESULT SyncWaitValue();

        MXC_RESULT Wait() const;

        HANDLE ClonedExternalHandle(HANDLE const& hTargetProcessHandle) const;

        void IncrementWaitValue() { m_WaitValue++; }
        void IncrementWaitValue(uint64_t const step) { m_WaitValue += step; }

        MXC_GET(VkSemaphore);
        MXC_GET(WaitValue)

    private:
        Device const* const k_pDevice;;

        uint64_t m_WaitValue{0};
        VkSemaphore m_VkSemaphore{VK_NULL_HANDLE};

#ifdef WIN32
        HANDLE m_ExternalHandle{nullptr};
#endif
    };
}// namespace Moxaic::Vulkan
