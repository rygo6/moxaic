#include "moxaic_vulkan_semaphore.hpp"
#include "moxaic_vulkan_device.hpp"

#include <vulkan/vulkan.h>

#ifdef WIN32
#include <vulkan/vulkan_win32.h>
#define MXC_EXTERNAL_SEMAPHORE_HANDLE_TYPE VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT
#endif

using namespace Moxaic;
using namespace Moxaic::Vulkan;

Semaphore::Semaphore(const Device& device)
    : k_Device(device) {}

Semaphore::~Semaphore()
{
    if (m_ExternalHandle != nullptr)
        CloseHandle(m_ExternalHandle);

    vkDestroySemaphore(k_Device.vkDevice(), m_vkSemaphore, VK_ALLOC);
}

MXC_RESULT Semaphore::Init(const bool readOnly, const Locality locality)
{
    MXC_LOG("Vulkan::Semaphore::Init");
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
      .pNext = locality == Locality::External ? &exportSemaphoreWin32HandleInfo : nullptr,
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
    VK_CHK(vkCreateSemaphore(k_Device.vkDevice(),
                             &timelineSemaphoreCreateInfo,
                             VK_ALLOC,
                             &m_vkSemaphore));
    if (locality == Locality::External) {
#ifdef WIN32
        const VkSemaphoreGetWin32HandleInfoKHR semaphoreGetWin32HandleInfo{
          .sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR,
          .pNext = nullptr,
          .semaphore = m_vkSemaphore,
          .handleType = MXC_EXTERNAL_SEMAPHORE_HANDLE_TYPE,
        };
        VK_CHK(VkFunc.GetSemaphoreWin32HandleKHR(k_Device.vkDevice(),
                                                 &semaphoreGetWin32HandleInfo,
                                                 &m_ExternalHandle));
#endif
    }
    return MXC_SUCCESS;
}

MXC_RESULT Semaphore::Wait()
{
    const VkSemaphoreWaitInfo waitInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
      .pNext = nullptr,
      .flags = 0,
      .semaphoreCount = 1,
      .pSemaphores = &m_vkSemaphore,
      .pValues = &m_WaitValue,
    };
    VK_CHK(vkWaitSemaphores(k_Device.vkDevice(),
                            &waitInfo,
                            UINT64_MAX));
    return MXC_SUCCESS;
}

HANDLE Semaphore::ClonedExternalHandle(const HANDLE hTargetProcessHandle) const
{
    HANDLE duplicateHandle;
    DuplicateHandle(GetCurrentProcess(),
                    m_ExternalHandle,
                    hTargetProcessHandle,
                    &duplicateHandle,
                    0,
                    false,
                    DUPLICATE_SAME_ACCESS);
    return duplicateHandle;
}

MXC_RESULT Semaphore::InitFromImport(const bool readOnly, const HANDLE externalHandle)
{
    MXC_LOG("Importing Semaphore", externalHandle);
    MXC_CHK(Init(readOnly, Locality::External));
#ifdef WIN32
    const VkImportSemaphoreWin32HandleInfoKHR importSemaphoreWin32HandleInfo{
      .sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
      .pNext = nullptr,
      .semaphore = m_vkSemaphore,
      .handleType = MXC_EXTERNAL_SEMAPHORE_HANDLE_TYPE,
      .handle = externalHandle,
      .name = nullptr,
    };
    VK_CHK(VkFunc.ImportSemaphoreWin32HandleKHR(k_Device.vkDevice(),
                                                &importSemaphoreWin32HandleInfo));
#endif
    return MXC_SUCCESS;
}

MXC_RESULT Semaphore::SyncWaitValue()
{
    VK_CHK(vkGetSemaphoreCounterValue(k_Device.vkDevice(), m_vkSemaphore, &m_WaitValue));
    return MXC_SUCCESS;
}
