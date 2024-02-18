#define MXC_DISABLE_LOG

#include "moxaic_vulkan_semaphore.hpp"
#include "moxaic_vulkan_device.hpp"

#include <vulkan/vulkan.h>

#ifdef WIN32
#include <vulkan/vulkan_win32.h>
#define MXC_EXTERNAL_SEMAPHORE_HANDLE_TYPE VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT
#endif

using namespace Moxaic;
using namespace Moxaic::Vulkan;

Semaphore::Semaphore(const Vulkan::Device* const device)
    : Device(device) {}

Semaphore::~Semaphore()
{
    switch (locality) {
        case Locality::Local:
            MXC_LOG("Destroy Semaphore Local");
            vkDestroySemaphore(Device->GetVkDevice(), vkSemaphore, VK_ALLOC);
            break;
        case Locality::External:
            MXC_LOG("Destroy Semaphore External");
            vkDestroySemaphore(Device->GetVkDevice(), vkSemaphore, VK_ALLOC);
            if (externalHandle != nullptr)
                CloseHandle(externalHandle);
            break;
        case Locality::Imported:
            MXC_LOG("Destroy Semaphore Imported");
            if (externalHandle != nullptr)
                CloseHandle(externalHandle);
            break;
    }
}

MXC_RESULT Semaphore::Init(const bool readOnly, const Locality locality)
{
    MXC_LOG("Vulkan::Semaphore::Init", string_Locality(locality));
    this->locality = locality;
    const VkExportSemaphoreWin32HandleInfoKHR exportSemaphoreWin32HandleInfo{
      .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
      .pNext = nullptr,
      .pAttributes = nullptr,
      // TODO are these the best security options? Read seems to affect it and solves issue of
      // child corrupting semaphore on crash... but not 100%
      .dwAccess = readOnly ? GENERIC_READ : GENERIC_ALL,
      //            .name = L"FBR_SEMAPHORE"
    };
    const VkExportSemaphoreCreateInfo exportSemaphoreCreateInfo{
      .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
      .pNext = locality == Locality::External || locality == Locality::Imported ?
                 &exportSemaphoreWin32HandleInfo :
                 nullptr,
      .handleTypes = MXC_EXTERNAL_SEMAPHORE_HANDLE_TYPE,
    };
    const VkSemaphoreTypeCreateInfo timelineSemaphoreTypeCreateInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
      .pNext = &exportSemaphoreCreateInfo,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
      .initialValue = 0,
    };
    const VkSemaphoreCreateInfo timelineSemaphoreCreateInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = &timelineSemaphoreTypeCreateInfo,
      .flags = 0,
    };
    VK_CHK(vkCreateSemaphore(Device->GetVkDevice(),
                             &timelineSemaphoreCreateInfo,
                             VK_ALLOC,
                             &vkSemaphore));
    if (locality == Locality::External || locality == Locality::Imported) {
#ifdef WIN32
        const VkSemaphoreGetWin32HandleInfoKHR semaphoreGetWin32HandleInfo{
          .sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR,
          .pNext = nullptr,
          .semaphore = vkSemaphore,
          .handleType = MXC_EXTERNAL_SEMAPHORE_HANDLE_TYPE,
        };
        VK_CHK(VkFunc.GetSemaphoreWin32HandleKHR(Device->GetVkDevice(),
                                                 &semaphoreGetWin32HandleInfo,
                                                 &externalHandle));
#endif
    }
    return MXC_SUCCESS;
}

MXC_RESULT Semaphore::Wait() const
{
    const VkSemaphoreWaitInfo waitInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
      .pNext = nullptr,
      .flags = 0,
      .semaphoreCount = 1,
      .pSemaphores = &vkSemaphore,
      .pValues = &localWaitValue,
    };
    VK_CHK(vkWaitSemaphores(Device->GetVkDevice(),
                            &waitInfo,
                            UINT64_MAX));
    return MXC_SUCCESS;
}

HANDLE Semaphore::ClonedExternalHandle(const HANDLE& hTargetProcessHandle) const
{
    HANDLE duplicateHandle;
    DuplicateHandle(GetCurrentProcess(),
                    externalHandle,
                    hTargetProcessHandle,
                    &duplicateHandle,
                    0,
                    false,
                    DUPLICATE_SAME_ACCESS);
    return duplicateHandle;
}

MXC_RESULT Semaphore::InitFromImport(const bool readOnly, const HANDLE externalHandle)
{
    MXC_CHK(Init(readOnly, Locality::Imported));
#ifdef WIN32
    const VkImportSemaphoreWin32HandleInfoKHR importSemaphoreWin32HandleInfo{
      .sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
      .pNext = nullptr,
      .semaphore = vkSemaphore,
      .handleType = MXC_EXTERNAL_SEMAPHORE_HANDLE_TYPE,
      .handle = externalHandle,
      .name = nullptr,
    };
    VK_CHK(VkFunc.ImportSemaphoreWin32HandleKHR(Device->GetVkDevice(),
                                                &importSemaphoreWin32HandleInfo));
#endif
    return MXC_SUCCESS;
}

MXC_RESULT Semaphore::SyncLocalWaitValue()
{
    VK_CHK(vkGetSemaphoreCounterValue(Device->GetVkDevice(), vkSemaphore, &localWaitValue));
    return MXC_SUCCESS;
}
