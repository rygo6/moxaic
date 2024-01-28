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

        explicit Semaphore(const Vulkan::Device* device);
        virtual ~Semaphore();

        MXC_RESULT Init(bool readOnly,
                        Locality locality);

        MXC_RESULT InitFromImport(bool readOnly,
                                  HANDLE externalHandle);

        MXC_RESULT SyncLocalWaitValue();

        MXC_RESULT Wait() const;

        HANDLE ClonedExternalHandle(HANDLE const& hTargetProcessHandle) const;

        uint64_t localWaitValue{0};

        const auto& GetVkSemaphore() const { return vkSemaphore; }

    private:
        const Device* const Device;

        VkSemaphore vkSemaphore{VK_NULL_HANDLE};

#ifdef WIN32
        HANDLE externalHandle{nullptr};
#endif
    };
}
