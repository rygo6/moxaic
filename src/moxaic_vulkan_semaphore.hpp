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

        explicit Semaphore(const Vulkan::Device* const pDevice);
        virtual ~Semaphore();

        MXC_RESULT Init(const bool& readOnly,
                        const Locality& locality);

        MXC_RESULT InitFromImport(const bool& readOnly,
                                  HANDLE const& externalHandle);

        MXC_RESULT SyncLocalWaitValue();

        MXC_RESULT Wait() const;

        HANDLE ClonedExternalHandle(HANDLE const& hTargetProcessHandle) const;

        void IncrementLocalWaitValue() { m_LocalWaitValue++; }
        void IncrementLocalWaitValue(const uint64_t step) { m_LocalWaitValue += step; }

        MXC_GET(VkSemaphore);
        MXC_GET(LocalWaitValue)

    private:
        const Device* const k_pDevice;
        ;

        uint64_t m_LocalWaitValue{0};
        VkSemaphore m_VkSemaphore{VK_NULL_HANDLE};

#ifdef WIN32
        HANDLE m_ExternalHandle{nullptr};
#endif
    };
}// namespace Moxaic::Vulkan
