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
#define MXC_EXTERNAL_HANDLE_TYPE VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT
#endif

using namespace Moxaic;
using namespace Moxaic::Vulkan;

Texture::Texture(const Device &device)
        : k_Device(device) {}

Texture::~Texture()
{
    vkDestroyImageView(k_Device.vkDevice(), vkImageView, VK_ALLOC);
    vkFreeMemory(k_Device.vkDevice(), m_VkDeviceMemory, VK_ALLOC);
    vkDestroyImage(k_Device.vkDevice(), vkImage, VK_ALLOC);
    if (m_ExternalHandle != nullptr)
        CloseHandle(m_ExternalHandle);
}

MXC_RESULT Texture::InitFromFile(const std::string file,
                                 const Locality locality)
{
    MXC_LOG("Texture::InitFromFile",
            file,
            string_Locality(locality));
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

    TransitionImmediateInitialToTransferDst();
    k_Device.CopyBufferToImage(extents,
                               stagingBuffer,
                               vkImage);
    TransitionImmediateTransferDstToGraphicsRead();

    vkDestroyBuffer(k_Device.vkDevice(),
                    stagingBuffer, VK_ALLOC);
    vkFreeMemory(k_Device.vkDevice(),
                 stagingBufferMemory, VK_ALLOC);

    return MXC_SUCCESS;
}

MXC_RESULT Texture::InitFromImport(const VkFormat format,
                                   const VkExtent2D extents,
                                   const VkImageUsageFlags usage,
                                   const VkImageAspectFlags aspectMask,
                                   const HANDLE externalMemory)
{
    MXC_LOG("Texture::InitFromImport",
            string_VkFormat(format),
            string_VkImageUsageFlags(usage),
            string_VkImageAspectFlags(aspectMask),
            externalMemory);
    MXC_CHK(InitImage(format,
                      extents,
                      usage,
                      Locality::External));
    MXC_CHK(k_Device.AllocateBindImageImport(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                             vkImage,
                                             MXC_EXTERNAL_HANDLE_TYPE,
                                             externalMemory,
                                             m_VkDeviceMemory));
    MXC_CHK(InitImageView(format, aspectMask));
    m_Extents = extents;
    m_AspectMask = aspectMask;
    return MXC_SUCCESS;
}

MXC_RESULT Texture::Init(const VkFormat format,
                         const VkExtent2D extents,
                         const VkImageUsageFlags usage,
                         const VkImageAspectFlags aspectMask,
                         const Locality locality)
{
    MXC_LOG("Texture::Init",
            string_VkFormat(format),
            string_VkImageUsageFlags(usage),
            string_VkImageAspectFlags(aspectMask),
            string_Locality(locality));
    MXC_CHK(InitImage(format,
                      extents,
                      usage,
                      locality));
    switch (locality) {
        case Locality::Local:
            MXC_CHK(k_Device.AllocateBindImage(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                               vkImage,
                                               m_VkDeviceMemory));
            break;
        case Locality::External:
            MXC_CHK(k_Device.AllocateBindImageExport(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                     vkImage,
                                                     MXC_EXTERNAL_HANDLE_TYPE,
                                                     m_VkDeviceMemory));
            break;
    }
    MXC_CHK(InitImageView(format, aspectMask));
    if (locality == Vulkan::Locality::External) {
#if WIN32
        const VkMemoryGetWin32HandleInfoKHR getWin32HandleInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
                .pNext = nullptr,
                .memory = m_VkDeviceMemory,
                .handleType = MXC_EXTERNAL_HANDLE_TYPE
        };
        VK_CHK(Vulkan::VkFunc.GetMemoryWin32HandleKHR(k_Device.vkDevice(),
                                                      &getWin32HandleInfo,
                                                      &m_ExternalHandle));
#endif
    }
    m_Extents = extents;
    m_AspectMask = aspectMask;
    return MXC_SUCCESS;
}

MXC_RESULT Texture::TransitionImmediateInitialToGraphicsRead()
{
    return k_Device.TransitionImageLayoutImmediate(vkImage,
                                                   VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                   VK_ACCESS_NONE, VK_ACCESS_NONE,
                                                   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                                   m_AspectMask);
}

MXC_RESULT Texture::TransitionImmediateInitialToTransferDst()
{
    return k_Device.TransitionImageLayoutImmediate(vkImage,
                                                   VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                   VK_ACCESS_NONE, VK_ACCESS_MEMORY_WRITE_BIT,
                                                   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                   m_AspectMask);
}

MXC_RESULT Texture::TransitionImmediateTransferDstToGraphicsRead()
{
    return k_Device.TransitionImageLayoutImmediate(vkImage,
                                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                   VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                                   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                                   m_AspectMask);
}

const HANDLE Texture::ClonedExternalHandle(HANDLE hTargetProcessHandle) const
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

MXC_RESULT Texture::InitImageView(const VkFormat format,
                                  const VkImageAspectFlags aspectMask)
{
    const VkImageViewCreateInfo imageViewCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = vkImage,
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
                             &vkImageView));
    return MXC_SUCCESS;
}

MXC_RESULT Texture::InitImage(const VkFormat format,
                              const VkExtent2D extents,
                              const VkImageUsageFlags usage,
                              const Locality locality)
{
    const VkExternalMemoryImageCreateInfo externalImageInfo = {
            .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .handleTypes = MXC_EXTERNAL_HANDLE_TYPE,
    };
    const VkImageCreateInfo imageCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = locality == Locality::External ? &externalImageInfo : nullptr,
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
                         &vkImage));
    return MXC_SUCCESS;
}
