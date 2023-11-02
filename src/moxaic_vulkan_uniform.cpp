#include "moxaic_vulkan_uniform.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_descriptor.hpp"

#include "SDL2/SDL.h"

template<typename T>
Moxaic::VulkanUniform<T>::VulkanUniform(const VulkanDevice &device)
        : k_Device(device) {}

template<typename T>
Moxaic::VulkanUniform<T>::~VulkanUniform()
{
    vkDestroyBuffer(k_Device.vkDevice(), m_VkBuffer, VK_ALLOC);

    if (m_pMappedBuffer != nullptr)
        vkUnmapMemory(k_Device.vkDevice(), m_VkDeviceMemory);

    vkFreeMemory(k_Device.vkDevice(), m_VkDeviceMemory, VK_ALLOC);

    if (m_ExternalMemory != nullptr)
        CloseHandle(m_ExternalMemory);
}

template<typename T>
bool Moxaic::VulkanUniform<T>::InitFromImport()
{
    return true;
}

template<typename T>
bool Moxaic::VulkanUniform<T>::Init(const VkMemoryPropertyFlags properties,
                                    const VkBufferUsageFlags usage,
                                    const Locality locality)
{
    MXC_LOG("Init Uniform:",
            string_VkMemoryPropertyFlags(properties),
            string_VkBufferUsageFlags(usage),
            BufferSize(),
            string_BufferLocality(locality));
    VkMemoryPropertyFlags supportedProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    assert(((supportedProperties & properties) == supportedProperties) && "Uniform needs to be VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT!");
    MXC_CHK(k_Device.AllocateBindBuffer(usage,
                                        properties,
                                        BufferSize(),
                                        locality,
                                        m_VkBuffer,
                                        m_VkDeviceMemory,
                                        m_ExternalMemory));
    VK_CHK(vkMapMemory(k_Device.vkDevice(),
                       m_VkDeviceMemory,
                       0,
                       BufferSize(),
                       0,
                       (void **) &m_pMappedBuffer));
    return true;
}

template
class Moxaic::VulkanUniform<Moxaic::GlobalDescriptor::Buffer>;