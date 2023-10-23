#include "moxaic_vulkan_texture.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_logging.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>

#if WIN32
#include <vulkan/vulkan_win32.h>
#endif

Moxaic::VulkanTexture::VulkanTexture(const VulkanDevice &device)
        : m_Device(device)
{}

Moxaic::VulkanTexture::~VulkanTexture() = default;

bool Moxaic::VulkanTexture::InitFromImage(VkFormat format,
                                          VkExtent2D extent,
                                          VkImage image)
{

    return true;
}

bool Moxaic::VulkanTexture::InitFromFile(bool external,
                                         const char *filename)
{

    return true;
}

bool Moxaic::VulkanTexture::InitFromImport(VkFormat format,
                                           VkExtent2D extent,
                                           VkImageUsageFlags usage,
                                           VkImageAspectFlags aspectMask,
                                           HANDLE externalMemory)
{

    return true;
}

bool Moxaic::VulkanTexture::Init(VkFormat format,
                                 VkExtent3D extent,
                                 VkImageUsageFlags usage,
                                 VkImageAspectFlags aspectMask)
{
    MXC_LOG("CreateTexture: ", format, usage, aspectMask);

    const VkImageCreateInfo imageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = extent,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    MXC_CHK(m_Device.CreateImage(imageCreateInfo, m_Image));
    MXC_CHK(m_Device.AllocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_Image, m_DeviceMemory));
    MXC_CHK(m_Device.BindImageMemory(m_Image, m_DeviceMemory));

    const VkImageViewCreateInfo imageViewCreateInfo {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = m_Image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .components {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange {
                    .aspectMask = aspectMask,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
            }
    };
    MXC_CHK(m_Device.CreateImageView(imageViewCreateInfo, m_ImageView));

    m_Extent = extent;

    return true;
}

bool Moxaic::VulkanTexture::InitExternal(VkFormat format,
                                         VkExtent3D extent,
                                         VkImageUsageFlags usage,
                                         VkImageAspectFlags aspectMask)
{
    MXC_LOG("CreateExternalTexture: ", format, usage, aspectMask);

    const VkExternalMemoryHandleTypeFlagBits externalHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    const VkExternalMemoryImageCreateInfo externalImageInfo = {
            .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .handleTypes = externalHandleType,
    };
    const VkImageCreateInfo imageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = &externalImageInfo,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = extent,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    MXC_CHK(m_Device.CreateImage(imageCreateInfo, m_Image));
    // todo your supposed check if it wants dedicated memory
//    VkMemoryDedicatedAllocateInfoKHR dedicatedAllocInfo = {
//            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR,
//            .image = pTestTexture->image,
//            .buffer = VK_NULL_HANDLE,
//    };
    MXC_CHK(m_Device.AllocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_Image, m_DeviceMemory));
    MXC_CHK(m_Device.BindImageMemory(m_Image, m_DeviceMemory));

    const VkImageViewCreateInfo imageViewCreateInfo {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = m_Image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .components {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange {
                    .aspectMask = aspectMask,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
            }
    };
    MXC_CHK(m_Device.CreateImageView(imageViewCreateInfo, m_ImageView));

#if WIN32
    const VkMemoryGetWin32HandleInfoKHR memoryInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
            .pNext = nullptr,
            .memory = m_DeviceMemory,
            .handleType = externalHandleType
    };
    m_Device.GetMemoryHandle(memoryInfo, m_ExternalMemory);
#endif

    m_Extent = extent;

    return true;
}

void Moxaic::VulkanTexture::Cleanup()
{

}
