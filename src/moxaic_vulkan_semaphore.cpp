// #define MXC_DISABLE_LOG

#include "moxaic_vulkan_semaphore.hpp"
#include "moxaic_vulkan_device.hpp"

#include <vulkan/vulkan.h>

#ifdef WIN32
#include <vulkan/vulkan_win32.h>
#define MXC_EXTERNAL_SEMAPHORE_HANDLE_TYPE VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT
#endif

using namespace Moxaic;
using namespace Moxaic::Vulkan;

Semaphore::Semaphore(const Vulkan::Device* const pDevice)
    : device_(pDevice) {}

Semaphore::~Semaphore()
{
    if (externalHandle_ != nullptr)
        CloseHandle(externalHandle_);

    vkDestroySemaphore(device_->  GetVkDevice(), vkSemaphore_, VK_ALLOC);
}

MXC_RESULT Semaphore::Init(bool const& readOnly, Locality const& locality)
{
    MXC_LOG("Vulkan::Semaphore::Init");
    VkExportSemaphoreWin32HandleInfoKHR const exportSemaphoreWin32HandleInfo{
      .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
      .pNext = nullptr,
      .pAttributes = nullptr,
      // TODO are these the best security options? Read seems to affect it and solves issue of
      // child corrupting semaphore on crash... but not 100%
      .dwAccess = readOnly ? GENERIC_READ : GENERIC_ALL,
      //            .name = L"FBR_SEMAPHORE"
    };
    VkExportSemaphoreCreateInfo const exportSemaphoreCreateInfo{
      .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
      .pNext = locality == Locality::External ? &exportSemaphoreWin32HandleInfo : nullptr,
      .handleTypes = MXC_EXTERNAL_SEMAPHORE_HANDLE_TYPE,
    };
    VkSemaphoreTypeCreateInfo const timelineSemaphoreTypeCreateInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
      .pNext = &exportSemaphoreCreateInfo,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
      .initialValue = 0,
    };
    VkSemaphoreCreateInfo const timelineSemaphoreCreateInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = &timelineSemaphoreTypeCreateInfo,
      .flags = 0,
    };
    VK_CHK(vkCreateSemaphore(device_->  GetVkDevice(),
                             &timelineSemaphoreCreateInfo,
                             VK_ALLOC,
                             &vkSemaphore_));
    if (locality == Locality::External) {
#ifdef WIN32
        VkSemaphoreGetWin32HandleInfoKHR const semaphoreGetWin32HandleInfo{
          .sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR,
          .pNext = nullptr,
          .semaphore = vkSemaphore_,
          .handleType = MXC_EXTERNAL_SEMAPHORE_HANDLE_TYPE,
        };
        VK_CHK(VkFunc.GetSemaphoreWin32HandleKHR(device_->  GetVkDevice(),
                                                 &semaphoreGetWin32HandleInfo,
                                                 &externalHandle_));
#endif
    }
    return MXC_SUCCESS;
}

MXC_RESULT Semaphore::Wait() const
{
    VkSemaphoreWaitInfo const waitInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
      .pNext = nullptr,
      .flags = 0,
      .semaphoreCount = 1,
      .pSemaphores = &vkSemaphore_,
      .pValues = &localWaitValue_,
    };
    VK_CHK(vkWaitSemaphores(device_->  GetVkDevice(),
                            &waitInfo,
                            UINT64_MAX));
    return MXC_SUCCESS;
}

HANDLE Semaphore::ClonedExternalHandle(const HANDLE& hTargetProcessHandle) const
{
    HANDLE duplicateHandle;
    DuplicateHandle(GetCurrentProcess(),
                    externalHandle_,
                    hTargetProcessHandle,
                    &duplicateHandle,
                    0,
                    false,
                    DUPLICATE_SAME_ACCESS);
    return duplicateHandle;
}

MXC_RESULT Semaphore::InitFromImport(bool const& readOnly, const HANDLE& externalHandle)
{
    MXC_LOG("Importing Semaphore", externalHandle);
    MXC_CHK(Init(readOnly, Locality::External));
#ifdef WIN32
    VkImportSemaphoreWin32HandleInfoKHR const importSemaphoreWin32HandleInfo{
      .sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
      .pNext = nullptr,
      .semaphore = vkSemaphore_,
      .handleType = MXC_EXTERNAL_SEMAPHORE_HANDLE_TYPE,
      .handle = externalHandle,
      .name = nullptr,
    };
    VK_CHK(VkFunc.ImportSemaphoreWin32HandleKHR(device_->  GetVkDevice(),
                                                &importSemaphoreWin32HandleInfo));
#endif
    return MXC_SUCCESS;
}

MXC_RESULT Semaphore::SyncLocalWaitValue()
{
    VK_CHK(vkGetSemaphoreCounterValue(device_->  GetVkDevice(), vkSemaphore_, &localWaitValue_));
    return MXC_SUCCESS;
}
