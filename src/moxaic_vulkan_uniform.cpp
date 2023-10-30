#include "moxaic_vulkan_uniform.hpp"
#include "moxaic_vulkan_device.hpp"

template<typename T>
Moxaic::VulkanUniform<T>::VulkanUniform(const VulkanDevice &device)
{

}

template<typename T>
Moxaic::VulkanUniform<T>::~VulkanUniform()
{
    vkDestroyBuffer(k_Device.vkDevice(), m_VkBuffer, VK_ALLOC);

    if (m_pMappedBuffer != nullptr)
        vkUnmapMemory(k_Device.vkDevice(), m_pMappedBuffer);

    vkFreeMemory(k_Device.vkDevice(), m_pMappedBuffer, VK_ALLOC);

    if (m_ExternalMemory != nullptr)
        CloseHandle(m_ExternalMemory);
}

template<typename T>
bool Moxaic::VulkanUniform<T>::InitFromImport()
{
    return true;
}

template<typename T>
bool Moxaic::VulkanUniform<T>::Init(const VkMemoryPropertyFlags &properties,
                                    const VkBufferUsageFlags &usage,
                                    const BufferLocality &locality)
{
    MXC_LOG("Init Uniform:",
            string_VkMemoryPropertyFlags(properties),
            string_VkBufferUsageFlags(usage),
            BufferSize(),
            string_BufferLocality(locality));
    const VkExternalMemoryHandleTypeFlagBits externalHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
    const VkExternalMemoryBufferCreateInfoKHR externalBufferInfo = {
            .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO_KHR,
            .pNext = nullptr,
            .handleTypes = externalHandleType
    };
    const VkBufferCreateInfo bufferCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = locality ? &externalBufferInfo : nullptr,
            .flags = 0,
            .size = BufferSize(),
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
    };
    VK_CHK(vkCreateBuffer(k_Device.vkDevice(),
                          &bufferCreateInfo,
                          nullptr,
                          &m_VkBuffer));
    MXC_CHK(k_Device.AllocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                    m_VkBuffer,
                                    locality == External ? externalHandleType : 0,
                                    m_VkDeviceMemory));
    VK_CHK(vkBindBufferMemory(k_Device.vkDevice(),
                              m_VkBuffer,
                              m_VkDeviceMemory,
                              0));
    VK_CHK(vkMapMemory(k_Device.vkDevice(),
                       m_VkDeviceMemory,
                       0,
                       BufferSize(),
                       0,
                       &m_pMappedBuffer));
    if (locality == BufferLocality::External) {
#if WIN32
        const VkMemoryGetWin32HandleInfoKHR getWin32HandleInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
                .pNext = nullptr,
                .memory = m_VkDeviceMemory,
                .handleType = externalHandleType
        };
        VK_CHK(VkFunc.GetMemoryWin32HandleKHR(k_Device.vkDevice(),
                                              &getWin32HandleInfo,
                                              &m_ExternalMemory));
#endif
    }
    return true;
}