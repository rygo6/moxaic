#include "moxaic_vulkan_texture.hpp"
#include "moxaic_logging.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#if WIN32
#include <vulkan/vulkan_win32.h>
#define MXC_EXTERNAL_HANDLE_TYPE VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT
#endif

using namespace Moxaic;
using namespace Moxaic::Vulkan;

Texture::Texture(const Vulkan::Device* const pDevice)
    : k_pDevice(pDevice) {}

Texture::~Texture()
{
    vkDestroyImageView(k_pDevice->GetVkDevice(), m_VkImageView, VK_ALLOC);
    vkFreeMemory(k_pDevice->GetVkDevice(), m_VkDeviceMemory, VK_ALLOC);
    vkDestroyImage(k_pDevice->GetVkDevice(), m_VkImage, VK_ALLOC);
    if (m_ExternalHandle != nullptr)
        CloseHandle(m_ExternalHandle);
}

MXC_RESULT Texture::InitFromFile(const std::string& file,
                                 const Locality locality)
{
    MXC_LOG("Texture::InitFromFile",
            file,
            string_Locality(locality));
    int texChannels;
    int width;
    int height;
    stbi_uc* pixels = stbi_load(file.c_str(),
                                &width,
                                &height,
                                &texChannels,
                                STBI_rgb_alpha);
    const VkDeviceSize imageBufferSize = width * height * 4;

    MXC_LOG("Loading texture from file.", file, width, height, texChannels);

    if (!pixels) {
        MXC_LOG_ERROR("Failed to load image!");
        return MXC_FAIL;
    }

    const VkExtent2D extents = {
      static_cast<uint32_t>(width),
      static_cast<uint32_t>(height)};

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    k_pDevice->CreateStagingBuffer(pixels,
                                   imageBufferSize,
                                   &stagingBuffer,
                                   &stagingBufferMemory);

    stbi_image_free(pixels);

    Init(VK_FORMAT_R8G8B8A8_SRGB,
         extents,
         VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
         VK_IMAGE_ASPECT_COLOR_BIT,
         locality);

    MXC_CHK(TransitionImmediateInitialToTransferDst());
    k_pDevice->CopyBufferToImage(extents,
                                 stagingBuffer,
                                 m_VkImage);
    MXC_CHK(TransitionImmediateTransferDstToGraphicsRead());

    vkDestroyBuffer(k_pDevice->GetVkDevice(),
                    stagingBuffer,
                    VK_ALLOC);
    vkFreeMemory(k_pDevice->GetVkDevice(),
                 stagingBufferMemory,
                 VK_ALLOC);

    return MXC_SUCCESS;
}

MXC_RESULT Texture::InitFromImport(const VkFormat format,
                                   const VkExtent2D extents,
                                   const VkImageUsageFlags usage,
                                   const VkImageAspectFlags aspectMask,
                                   const HANDLE externalMemory)
{
    MXC_LOG_MULTILINE("Texture::InitFromImport",
                      string_VkFormat(format),
                      string_VkImageUsageFlags(usage),
                      string_VkImageAspectFlags(aspectMask),
                      externalMemory);
    MXC_CHK(InitImage(format,
                      extents,
                      usage,
                      Locality::External));
    MXC_CHK(k_pDevice->AllocateBindImageImport(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                               m_VkImage,
                                               MXC_EXTERNAL_HANDLE_TYPE,
                                               externalMemory,
                                               &m_VkDeviceMemory));
    MXC_CHK(InitImageView(format, aspectMask));
    m_Extents = extents;
    m_AspectMask = aspectMask;
    m_Format = format;
    return MXC_SUCCESS;
}

MXC_RESULT Texture::Init(const VkFormat format,
                         const VkExtent2D extents,
                         const VkImageUsageFlags usage,
                         const VkImageAspectFlags aspectMask,
                         const Locality locality)
{
    MXC_LOG_MULTILINE("Texture::Init",
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
            MXC_CHK(k_pDevice->AllocateBindImage(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                 m_VkImage,
                                                 &m_VkDeviceMemory));
            break;
        case Locality::External:
            MXC_CHK(k_pDevice->AllocateBindImageExport(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                       m_VkImage,
                                                       MXC_EXTERNAL_HANDLE_TYPE,
                                                       &m_VkDeviceMemory));
            break;
    }
    MXC_CHK(InitImageView(format, aspectMask));
    if (locality == Locality::External) {
#if WIN32
        const VkMemoryGetWin32HandleInfoKHR getWin32HandleInfo = {
          .sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
          .pNext = nullptr,
          .memory = m_VkDeviceMemory,
          .handleType = MXC_EXTERNAL_HANDLE_TYPE};
        VK_CHK(VkFunc.GetMemoryWin32HandleKHR(k_pDevice->GetVkDevice(),
                                              &getWin32HandleInfo,
                                              &m_ExternalHandle));
#endif
    }
    m_Extents = extents;
    m_AspectMask = aspectMask;
    m_Format = format;
    return MXC_SUCCESS;
}

MXC_RESULT Texture::TransitionInitialImmediate(const PipelineType pipelineType) const
{
    switch (pipelineType) {
        case PipelineType::Graphics:
            return k_pDevice->TransitionImageLayoutImmediate(m_VkImage,
                                                             Vulkan::FromInitial,
                                                             Vulkan::ToGraphicsRead,
                                                             m_AspectMask);
        case PipelineType::Compute:
            return k_pDevice->TransitionImageLayoutImmediate(m_VkImage,
                                                             Vulkan::FromInitial,
                                                             Vulkan::ToComputeRead,
                                                             m_AspectMask);
        default:
            SDL_assert(false);
            return MXC_FAIL;
    }
}

MXC_RESULT Texture::TransitionImmediateInitialToTransferDst() const
{
    return k_pDevice->TransitionImageLayoutImmediate(m_VkImage,
                                                     VK_IMAGE_LAYOUT_UNDEFINED,
                                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                     VK_ACCESS_NONE,
                                                     VK_ACCESS_MEMORY_WRITE_BIT,
                                                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                     m_AspectMask);
}

MXC_RESULT Texture::TransitionImmediateTransferDstToGraphicsRead() const
{
    return k_pDevice->TransitionImageLayoutImmediate(m_VkImage,
                                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                     VK_ACCESS_MEMORY_WRITE_BIT,
                                                     VK_ACCESS_SHADER_READ_BIT,
                                                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                                     m_AspectMask);
}

HANDLE Texture::ClonedExternalHandle(const HANDLE& hTargetProcessHandle) const
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

MXC_RESULT Texture::InitImageView(const VkFormat& format,
                                  const VkImageAspectFlags& aspectMask)
{
    const VkImageViewCreateInfo imageViewCreateInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .image = m_VkImage,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .components{
        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
      },
      .subresourceRange{
        .aspectMask = aspectMask,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
    };
    VK_CHK(vkCreateImageView(k_pDevice->GetVkDevice(),
                             &imageViewCreateInfo,
                             VK_ALLOC,
                             &m_VkImageView));
    return MXC_SUCCESS;
}

MXC_RESULT Texture::InitImage(const VkFormat format,
                              const VkExtent2D extents,
                              const VkImageUsageFlags usage,
                              const Locality locality)
{
    constexpr VkExternalMemoryImageCreateInfo externalImageInfo{
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
      .pNext = nullptr,
      .handleTypes = MXC_EXTERNAL_HANDLE_TYPE,
    };
    const VkImageCreateInfo imageCreateInfo{
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
    VK_CHK(vkCreateImage(k_pDevice->GetVkDevice(),
                         &imageCreateInfo,
                         VK_ALLOC,
                         &m_VkImage));
    return MXC_SUCCESS;
}


void Texture::BlitTo(const VkCommandBuffer commandBuffer,
                     const Texture& dstTexture) const
{
    MXC_LOG_NAMED(IsDepth());
    const StaticArray transitionBlitBarrier{
      (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_NONE,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = IsDepth() ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = k_pDevice->GetGraphicsQueueFamilyIndex(),
        .dstQueueFamilyIndex = k_pDevice->GetGraphicsQueueFamilyIndex(),
        .image = m_VkImage,
        .subresourceRange = IsDepth() ? Vulkan::DefaultDepthSubresourceRange :
                                        Vulkan::DefaultColorSubresourceRange,
      },
      (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_NONE,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = k_pDevice->GetGraphicsQueueFamilyIndex(),
        .dstQueueFamilyIndex = k_pDevice->GetGraphicsQueueFamilyIndex(),
        .image = dstTexture.GetVkImage(),
        .subresourceRange = Vulkan::DefaultColorSubresourceRange,
      },
    };
    VK_CHK_VOID(vkCmdPipelineBarrier(commandBuffer,
                                     IsDepth() ?
                                       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT :
                                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     0,
                                     0,
                                     nullptr,
                                     0,
                                     nullptr,
                                     transitionBlitBarrier.size(),
                                     transitionBlitBarrier.data()));
    constexpr VkImageSubresourceLayers imageSubresourceLayers{
      .mipLevel = 0,
      .baseArrayLayer = 0,
      .layerCount = 1,
    };
    const VkOffset3D offsets[2]{
      (VkOffset3D){
        .x = 0,
        .y = 0,
        .z = 0,
      },
      (VkOffset3D){
        .x = int32_t(m_Extents.width),
        .y = int32_t(m_Extents.height),
        .z = 1,
      },
    };
    VkImageBlit imageBlit{};
    imageBlit.srcSubresource = imageSubresourceLayers;
    imageBlit.srcSubresource.aspectMask = IsDepth() ?
                                            VK_IMAGE_ASPECT_DEPTH_BIT :
                                            VK_IMAGE_ASPECT_COLOR_BIT;
    imageBlit.srcOffsets[0] = offsets[0];
    imageBlit.srcOffsets[1] = offsets[1];
    imageBlit.dstSubresource = imageSubresourceLayers;
    imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBlit.dstOffsets[0] = offsets[0];
    imageBlit.dstOffsets[1] = offsets[1];
    VK_CHK_VOID(vkCmdBlitImage(commandBuffer,
                               m_VkImage,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               dstTexture.GetVkImage(),
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1,
                               &imageBlit,
                               VK_FILTER_NEAREST));
    const StaticArray transitionPresentBarrier{
      (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .dstAccessMask = VK_ACCESS_NONE,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .newLayout = IsDepth() ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = k_pDevice->GetGraphicsQueueFamilyIndex(),
        .dstQueueFamilyIndex = k_pDevice->GetGraphicsQueueFamilyIndex(),
        .image = m_VkImage,
        .subresourceRange = Vulkan::DefaultColorSubresourceRange,
      },
      (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_NONE,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = k_pDevice->GetGraphicsQueueFamilyIndex(),
        .dstQueueFamilyIndex = k_pDevice->GetGraphicsQueueFamilyIndex(),
        .image = dstTexture.GetVkImage(),
        .subresourceRange = Vulkan::DefaultColorSubresourceRange,
      },
    };
    VK_CHK_VOID(vkCmdPipelineBarrier(commandBuffer,
                                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                                     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                     0,
                                     0,
                                     nullptr,
                                     0,
                                     nullptr,
                                     transitionPresentBarrier.size(),
                                     transitionPresentBarrier.data()));
}