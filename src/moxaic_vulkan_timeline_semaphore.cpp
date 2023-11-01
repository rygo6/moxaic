#include "moxaic_vulkan_timeline_semaphore.hpp"
#include "moxaic_vulkan_device.hpp"

#include <vulkan/vulkan.h>
#ifdef WIN32
#include <vulkan/vulkan_win32.h>
#endif

Moxaic::VulkanTimelineSemaphore::VulkanTimelineSemaphore(const VulkanDevice &device)
        : k_Device(device)
{}

Moxaic::VulkanTimelineSemaphore::~VulkanTimelineSemaphore()
{

}

MXC_RESULT Moxaic::VulkanTimelineSemaphore::Init(bool readOnly, Locality locality)
{
    const VkExportSemaphoreWin32HandleInfoKHR exportSemaphoreWin32HandleInfo = {
            .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
            .pNext = NULL,
            .pAttributes = NULL,
            // TODO are these the best security options? Read seems to affect it and solves issue of
            // child corrupting semaphore on crash... but not 100%
            .dwAccess = readOnly ? GENERIC_READ : GENERIC_ALL,
//            .name = L"FBR_SEMAPHORE"
    };
    const VkExportSemaphoreCreateInfo exportSemaphoreCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
            .pNext = locality == Locality::External ? &exportSemaphoreWin32HandleInfo : nullptr,
            .handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT,
    };
    const VkSemaphoreTypeCreateInfo timelineSemaphoreTypeCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .pNext = &exportSemaphoreCreateInfo,
            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
            .initialValue = 0,
    };
    const VkSemaphoreCreateInfo timelineSemaphoreCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = &timelineSemaphoreTypeCreateInfo,
            .flags = 0,
    };
    VK_CHK(vkCreateSemaphore(k_Device.vkDevice(), &timelineSemaphoreCreateInfo, NULL, &m_vkSemaphore));
    return MXC_SUCCESS;
}

MXC_RESULT Moxaic::VulkanTimelineSemaphore::Wait()
{
    const VkSemaphoreWaitInfo waitInfo {
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
