#include "moxaic_vulkan_texture.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_logging.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#if WIN32
#include <vulkan/vulkan_win32.h>
#endif

Moxaic::VulkanTexture::VulkanTexture(const VulkanDevice &device)
        : k_Device(device) {}

Moxaic::VulkanTexture::~VulkanTexture()
{
    vkDestroyImageView(k_Device.vkDevice(), m_VkImageView, VK_ALLOC);
    vkFreeMemory(k_Device.vkDevice(), m_VkDeviceMemory, VK_ALLOC);
    vkDestroyImage(k_Device.vkDevice(), m_VkImage, VK_ALLOC);
    if (m_ExternalMemory != nullptr)
        CloseHandle(m_ExternalMemory);
}

MXC_RESULT Moxaic::VulkanTexture::InitFromImage(VkFormat format,
                                          VkExtent2D extent,
                                          VkImage image)
{
    return MXC_SUCCESS;
}

MXC_RESULT Moxaic::VulkanTexture::InitFromFile(const std::string file,
                                               const Moxaic::Locality locality)
{
    int texChannels;
    int width;
    int height;
    stbi_uc *pixels = stbi_load(file.c_str(),
                                &width,
                                &height,
                                &texChannels,
                                STBI_rgb_alpha);
    VkDeviceSize imageBufferSize = width * height * 4;

    MXC_LOG("Loading texture from file.", file, width, height, texChannels);

    if (!pixels) {
        MXC_LOG_ERROR("Failed to load image!");
        return MXC_FAIL;
    }

    VkExtent2D extents = {static_cast<uint32_t>(width),
                          static_cast<uint32_t>(height)};

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    k_Device.CreateStagingBuffer(pixels,
                                 imageBufferSize,
                                 stagingBuffer,
                                 stagingBufferMemory);

    stbi_image_free(pixels);

    Init(VK_FORMAT_R8G8B8A8_SRGB,
         extents,
         VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
         VK_IMAGE_ASPECT_COLOR_BIT,
         locality);

    TransitionImediateInitialToTransferDst();
    k_Device.CopyBufferToImage(extents,
                               stagingBuffer,
                               m_VkImage);
    TransitionImmediateTransferDstToGraphicsRead();

    vkDestroyBuffer(k_Device.vkDevice(),
                    stagingBuffer, VK_ALLOC);
    vkFreeMemory(k_Device.vkDevice(),
                 stagingBufferMemory, VK_ALLOC);

    return MXC_SUCCESS;
}

bool Moxaic::VulkanTexture::InitFromImport(VkFormat format,
                                           VkExtent2D extent,
                                           VkImageUsageFlags usage,
                                           VkImageAspectFlags aspectMask,
                                           HANDLE externalMemory)
{
    return MXC_SUCCESS;
}

bool Moxaic::VulkanTexture::Init(const VkFormat format,
                                 const VkExtent2D extents,
                                 const VkImageUsageFlags usage,
                                 const VkImageAspectFlags aspectMask,
                                 const Locality locality)
{
    MXC_LOG("CreateTexture:",
            string_VkFormat(format),
            string_VkImageUsageFlags(usage),
            string_VkImageAspectFlags(aspectMask),
            string_BufferLocality(locality));
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
            .extent = {extents.width, extents.height, 1},
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
    m_Dimensions = extents;
    m_AspectMask = aspectMask;
    return MXC_SUCCESS;
}

MXC_RESULT Moxaic::VulkanTexture::TransitionImmediateInitialToGraphicsRead()
{
    return k_Device.TransitionImageLayoutImmediate(m_VkImage,
                                                   VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                   VK_ACCESS_NONE, VK_ACCESS_NONE,
                                                   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                                   m_AspectMask);
}

MXC_RESULT Moxaic::VulkanTexture::TransitionImediateInitialToTransferDst()
{
    return k_Device.TransitionImageLayoutImmediate(m_VkImage,
                                                   VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                   VK_ACCESS_NONE, VK_ACCESS_MEMORY_WRITE_BIT,
                                                   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                   m_AspectMask);
}

MXC_RESULT Moxaic::VulkanTexture::TransitionImmediateTransferDstToGraphicsRead()
{
    return k_Device.TransitionImageLayoutImmediate(m_VkImage,
                                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                   VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                                   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                                   m_AspectMask);
}