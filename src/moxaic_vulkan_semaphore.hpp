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

        uint64_t localWaitValue_{0};

        const auto& GetVkSemaphore() const { return vkSemaphore; }

    private:
        const Device* const Device;

        VkSemaphore vkSemaphore{VK_NULL_HANDLE};

#ifdef WIN32
        HANDLE externalHandle{nullptr};
#endif
    };
}
