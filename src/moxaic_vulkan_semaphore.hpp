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

    class Semaphore final
    {
        MXC_NO_VALUE_PASS(Semaphore);

        const Device* const Device;

        VkSemaphore vkSemaphore{VK_NULL_HANDLE};

        Locality locality{Locality::Local};

        HANDLE externalHandle{nullptr};

    public:
        const VkSemaphore& VkSemaphoreHandle{vkSemaphore};

        explicit Semaphore(const Vulkan::Device* device);
        ~Semaphore();

        MXC_RESULT Init(bool readOnly,
                        Locality locality);

        MXC_RESULT InitFromImport(bool readOnly,
                                  HANDLE externalHandle);

        MXC_RESULT SyncLocalWaitValue();

        MXC_RESULT Wait() const;

        HANDLE ClonedExternalHandle(HANDLE const& hTargetProcessHandle) const;

        bool IsExternalHandleValid() {
            DWORD dwFlags;
            return GetHandleInformation(externalHandle, &dwFlags) != 0;
        }

        uint64_t localWaitValue{0};

        const auto& GetVkSemaphore() const { return vkSemaphore; }
    };
}// namespace Moxaic::Vulkan
