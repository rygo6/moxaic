#include "moxaic_vulkan_texture.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_logging.hpp"

#include <vulkan/vulkan.h>

Moxaic::VulkanTexture::VulkanTexture(const VulkanDevice &device)
        : m_Device(device)
{}

Moxaic::VulkanTexture::~VulkanTexture() = default;

bool Moxaic::VulkanTexture::CreateFromImage(VkFormat format,
                                            VkExtent2D extent,
                                            VkImage image)
{

    return true;
}

bool Moxaic::VulkanTexture::CreateFromFile(bool external,
                                           const char *filename)
{

    return true;
}

bool Moxaic::VulkanTexture::Import(VkFormat format,
                                   VkExtent2D extent,
                                   VkImageUsageFlags usage,
                                   VkImageAspectFlags aspectMask,
                                   HANDLE externalMemory)
{

    return true;
}

bool Moxaic::VulkanTexture::Create(VkFormat format,
                                   VkExtent3D extent,
                                   VkImageUsageFlags usage,
                                   VkImageAspectFlags aspectMask,
                                   bool external)
{
    MXC_LOG("CreateTexture: ", format, usage, aspectMask);

    const VkImageCreateInfo imageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = extent,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    MXC_CHK(m_Device.CreateImage(imageCreateInfo, m_Image));
    MXC_CHK(m_Device.AllocateMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_Image, m_DeviceMemory));
    MXC_CHK(m_Device.BindImageMemory(m_Image, m_DeviceMemory));

    const VkImageViewCreateInfo imageViewCreateInfo {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
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

    return true;
}

void Moxaic::VulkanTexture::Destroy()
{

}
