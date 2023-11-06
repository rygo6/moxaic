#include "moxaic_vulkan_texture.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_logging.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#if WIN32
#include <vulkan/vulkan_win32.h>
#endif

Moxaic::VulkanTexture::VulkanTexture(const VulkanDevice &device)
        : k_Device(device) {}

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

bool Moxaic::VulkanTexture::Init(const VkFormat format,
                                 const VkExtent3D dimensions,
                                 const VkImageUsageFlags usage,
                                 const VkImageAspectFlags aspectMask,
                                 const Locality locality)
{
    MXC_LOG("CreateTexture:",
            string_VkFormat(format),
            string_VkImageUsageFlags(usage),
            string_VkImageAspectFlags(aspectMask),
            string_BufferLocality(locality));
    const auto checkedDimensions = dimensions.depth == 0 ?
                                   (VkExtent3D) {dimensions.width, dimensions.height, 1} :
                                   dimensions;
    const VkExternalMemoryHandleTypeFlagBits externalHandleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    const VkExternalMemoryImageCreateInfo externalImageInfo = {
            .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .handleTypes = externalHandleType,
    };
    const VkImageCreateInfo imageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = locality == External ? &externalImageInfo : nullptr,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = checkedDimensions,
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
    VK_CHK(vkCreateImage(k_Device.vkDevice(),
                         &imageCreateInfo,
                         VK_ALLOC,
                         &m_VkImage));
    MXC_CHK(k_Device.AllocateBindImage(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                       m_VkImage,
                                       locality == External ? externalHandleType : 0,
                                       m_VkDeviceMemory));
    const VkImageViewCreateInfo imageViewCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = m_VkImage,
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
    VK_CHK(vkCreateImageView(k_Device.vkDevice(),
                             &imageViewCreateInfo,
                             VK_ALLOC,
                             &m_VkImageView));
    if (locality == External) {
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
    m_Dimensions = checkedDimensions;
    m_AspectMask = aspectMask;
    return true;
}
bool Moxaic::VulkanTexture::InitialReadTransition()
{
    return k_Device.TransitionImageLayoutImmediate(m_VkImage,
                                                   VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                   0, 0,
                                                   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                                   m_AspectMask);
}
