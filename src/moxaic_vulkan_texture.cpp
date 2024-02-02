#define MXC_DISABLE_LOG

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

Texture::Texture(const Vulkan::Device* const device)
    : Device(device)
{}
Texture::Texture(const Vulkan::Device* device, const Texture::Info& info)
    : Device(device),
      format(info.format),
      usage(info.usage),
      aspectMask(info.aspectMask),
      sampleCount(info.sampleCount),
      extents(info.extents),
      mipLevels(info.mipLevels),
      locality(info.locality)
{}

Texture::~Texture()
{
    vkDestroyImageView(Device->GetVkDevice(), vkImageViewHandle, VK_ALLOC);
    vkFreeMemory(Device->GetVkDevice(), vkDeviceMemory, VK_ALLOC);
    vkDestroyImage(Device->GetVkDevice(), vkImageHandle, VK_ALLOC);
    if (externalHandle != nullptr)
        CloseHandle(externalHandle);
}

MXC_RESULT Texture::InitFromFile(const char* file)
{
    MXC_LOG("Texture::InitFromFile",
            file,
            string_Locality(locality));

    int texChannels;
    int width;
    int height;
    stbi_uc* pixels = stbi_load(file,
                                &width,
                                &height,
                                &texChannels,
                                STBI_rgb_alpha);

    MXC_LOG("Loading texture from file.", file, width, height, texChannels);
    if (!pixels) {
        MXC_LOG_ERROR("Failed to load image!");
        return MXC_FAIL;
    }

    const VkDeviceSize imageBufferSize = width * height * 4;
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    Device->CreateAndPopulateStagingBuffer(pixels,
                                           imageBufferSize,
                                           &stagingBuffer,
                                           &stagingBufferMemory);
    stbi_image_free(pixels);

    this->format = VK_FORMAT_R8G8B8A8_SRGB;
    this->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    this->aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    this->sampleCount = VK_SAMPLE_COUNT_1_BIT;
    this->extents = {(uint32_t) width, (uint32_t) height};
    this->mipLevels = 1;
    this->externalHandle = externalHandle;
    this->locality = Locality::Local;

    MXC_CHK(InternalInit());

    MXC_CHK(TransitionImmediateInitialToTransferDst());
    MXC_CHK(Device->CopyBufferToImage(extents,
                                      stagingBuffer,
                                      vkImageHandle));
    MXC_CHK(TransitionImmediateTransferDstToGraphicsRead());

    vkDestroyBuffer(Device->GetVkDevice(),
                    stagingBuffer,
                    VK_ALLOC);
    vkFreeMemory(Device->GetVkDevice(),
                 stagingBufferMemory,
                 VK_ALLOC);

    return MXC_SUCCESS;
}

MXC_RESULT Texture::InitFromImport(const HANDLE externalHandle,
                                   const VkExtent2D& extents)
{
    this->externalHandle = externalHandle;
    this->locality = Locality::Imported;
    this->extents = extents;

    MXC_LOG_MULTILINE("Texture::InitFromImport",
                      externalHandle,
                      string_VkFormat(format),
                      string_VkImageUsageFlags(usage),
                      string_VkImageAspectFlags(aspectMask));

    return InternalInit();
}

bool Texture::Init(const VkExtent2D& extents)
{
    this->extents = extents;

    MXC_LOG_MULTILINE("Texture::InitFromImport",
                  externalHandle,
                  string_VkFormat(format),
                  string_VkImageUsageFlags(usage),
                  string_VkImageAspectFlags(aspectMask));

    return InternalInit();
}

bool Texture::Init(const Info& info)
{
    this->format = info.format;
    this->usage = info.usage;
    this->aspectMask = info.aspectMask;
    this->sampleCount = info.sampleCount;
    this->extents = info.extents;
    this->mipLevels = info.mipLevels;
    this->locality = info.locality;

    MXC_LOG_MULTILINE("Texture::InitFromImport",
                  externalHandle,
                  string_VkFormat(format),
                  string_VkImageUsageFlags(usage),
                  string_VkImageAspectFlags(aspectMask));

    return InternalInit();
}

MXC_RESULT Texture::InternalInit()
{
    MXC_LOG_MULTILINE("Texture::Init",
                      string_VkFormat(format),
                      string_VkImageUsageFlags(usage),
                      string_VkImageAspectFlags(aspectMask),
                      string_Locality(locality));

    assert(format != VK_FORMAT_UNDEFINED);
    assert(extents.width != 0 && extents.height != 0);
    assert(usage != 0);
    assert(aspectMask != VK_IMAGE_ASPECT_NONE);
    assert(sampleCount > 0);

    MXC_CHK(InitImage());

    switch (locality) {
        case Locality::Local:
            MXC_CHK(Device->AllocateBindImage(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                              vkImageHandle,
                                              &vkDeviceMemory));
            break;
        case Locality::External:
            MXC_CHK(Device->AllocateBindImageExport(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                    vkImageHandle,
                                                    MXC_EXTERNAL_HANDLE_TYPE,
                                                    &vkDeviceMemory));
            break;
        case Locality::Imported:
            assert(externalHandle != nullptr);
            MXC_CHK(Device->AllocateBindImageImport(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                                    vkImageHandle,
                                                    MXC_EXTERNAL_HANDLE_TYPE,
                                                    externalHandle,
                                                    &vkDeviceMemory));
            break;
        case Locality::Undefined:
            assert(false);
    }

    MXC_CHK(InitImageView());

    if (locality == Locality::External) {
#if WIN32
        const VkMemoryGetWin32HandleInfoKHR getWin32HandleInfo = {
          .sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR,
          .pNext = nullptr,
          .memory = vkDeviceMemory,
          .handleType = MXC_EXTERNAL_HANDLE_TYPE};
        VK_CHK(VkFunc.GetMemoryWin32HandleKHR(Device->GetVkDevice(),
                                              &getWin32HandleInfo,
                                              &externalHandle));
#endif
    }

    return MXC_SUCCESS;
}

MXC_RESULT Texture::TransitionInitialImmediate(const PipelineType pipelineType) const
{
    switch (pipelineType) {
        case PipelineType::Graphics:
            return Device->TransitionImageLayoutImmediate(vkImageHandle,
                                                          Vulkan::FromInitial,
                                                          Vulkan::ToGraphicsRead,
                                                          aspectMask);
        case PipelineType::Compute:
            return Device->TransitionImageLayoutImmediate(vkImageHandle,
                                                          Vulkan::FromInitial,
                                                          Vulkan::ToComputeRead,
                                                          aspectMask);
        default:
            SDL_assert(false);
            return MXC_FAIL;
    }
}

MXC_RESULT Texture::TransitionImmediateInitialToTransferDst() const
{
    return Device->TransitionImageLayoutImmediate(vkImageHandle,
                                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                  VK_ACCESS_NONE,
                                                  VK_ACCESS_MEMORY_WRITE_BIT,
                                                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                                  VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                  aspectMask);
}

MXC_RESULT Texture::TransitionImmediateTransferDstToGraphicsRead() const
{
    return Device->TransitionImageLayoutImmediate(vkImageHandle,
                                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                  VK_ACCESS_MEMORY_WRITE_BIT,
                                                  VK_ACCESS_SHADER_READ_BIT,
                                                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                                  aspectMask);
}

HANDLE Texture::ClonedExternalHandle(const HANDLE& hTargetProcessHandle) const
{
    HANDLE duplicateHandle;
    DuplicateHandle(GetCurrentProcess(),
                    externalHandle,
                    hTargetProcessHandle,
                    &duplicateHandle,
                    0,
                    false,
                    DUPLICATE_SAME_ACCESS);
    return duplicateHandle;
}

MXC_RESULT Texture::InitImageView()
{
    const VkImageViewCreateInfo imageViewCreateInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .image = vkImageHandle,
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
        .levelCount = mipLevels,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
    };
    VK_CHK(vkCreateImageView(Device->GetVkDevice(),
                             &imageViewCreateInfo,
                             VK_ALLOC,
                             &vkImageViewHandle));
    return MXC_SUCCESS;
}

MXC_RESULT Texture::InitImage()
{
    constexpr VkExternalMemoryImageCreateInfo externalImageInfo{
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
      .pNext = nullptr,
      .handleTypes = MXC_EXTERNAL_HANDLE_TYPE,
    };
    const VkImageCreateInfo imageCreateInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = locality == Locality::External || locality == Locality::Imported ?
                 &externalImageInfo :
                 nullptr,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = format,
      .extent = {extents.width, extents.height, 1},
      .mipLevels = mipLevels,
      .arrayLayers = 1,
      .samples = sampleCount,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VK_CHK(vkCreateImage(Device->GetVkDevice(),
                         &imageCreateInfo,
                         VK_ALLOC,
                         &vkImageHandle));
    return MXC_SUCCESS;
}

void Texture::BlitTo(const VkCommandBuffer commandBuffer,
                     const Texture& dstTexture) const
{
    const StaticArray transitionBlitBarrier{
      (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_NONE,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = IsDepth() ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = Device->GetGraphicsQueueFamilyIndex(),
        .dstQueueFamilyIndex = Device->GetGraphicsQueueFamilyIndex(),
        .image = vkImageHandle,
        .subresourceRange = GetSubresourceRange(),
      },
      (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_NONE,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = Device->GetGraphicsQueueFamilyIndex(),
        .dstQueueFamilyIndex = Device->GetGraphicsQueueFamilyIndex(),
        .image = dstTexture.VkImageHandle,
        .subresourceRange = dstTexture.GetSubresourceRange(),
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
        .x = int32_t(extents.width),
        .y = int32_t(extents.height),
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
                               vkImageHandle,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               dstTexture.VkImageHandle,
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
        .srcQueueFamilyIndex = Device->GetGraphicsQueueFamilyIndex(),
        .dstQueueFamilyIndex = Device->GetGraphicsQueueFamilyIndex(),
        .image = vkImageHandle,
        .subresourceRange = GetSubresourceRange(),
      },
      (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_NONE,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = Device->GetGraphicsQueueFamilyIndex(),
        .dstQueueFamilyIndex = Device->GetGraphicsQueueFamilyIndex(),
        .image = dstTexture.VkImageHandle,
        .subresourceRange = dstTexture.GetSubresourceRange(),
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
