#define MXC_DISABLE_LOG

#include "moxaic_vulkan_swap.hpp"

#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_texture.hpp"

#include <vulkan/vk_enum_string_helper.h>

using namespace Moxaic;
using namespace Moxaic::Vulkan;

static MXC_RESULT ChooseSwapPresentMode(const VkPhysicalDevice physicalDevice,
                                        VkPresentModeKHR* pPresentMode)
{
    uint32_t presentModeCount;
    VK_CHK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice,
                                                     Vulkan::GetVkSurface(),
                                                     &presentModeCount,
                                                     nullptr));
    VkPresentModeKHR presentModes[presentModeCount];
    VK_CHK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice,
                                                     Vulkan::GetVkSurface(),
                                                     &presentModeCount,
                                                     (VkPresentModeKHR*) &presentModes));

    // https://www.intel.com/content/www/us/en/developer/articles/training/api-without-secrets-introduction-to-vulkan-part-2.html?language=en#_Toc445674479
    // This logic taken from OVR Vulkan Example
    // VK_PRESENT_MODE_FIFO_KHR - equivalent of eglSwapInterval(1).  The presentation engine waits for the next vertical blanking period to update
    // the current image. Tearing cannot be observed. This mode must be supported by all implementations.
    *pPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (int i = 0; i < presentModeCount; ++i) {
        // Order of preference for no vsync:
        // 1. VK_PRESENT_MODE_IMMEDIATE_KHR - The presentation engine does not wait for a vertical blanking period to update the current image,
        //                                    meaning this mode may result in visible tearing
        // 2. VK_PRESENT_MODE_MAILBOX_KHR - The presentation engine waits for the next vertical blanking period to update the current image. Tearing cannot be observed.
        //                                  An internal single-entry graphicsQueue is used to hold pending presentation requests.
        // 3. VK_PRESENT_MODE_FIFO_RELAXED_KHR - equivalent of eglSwapInterval(-1).
        if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            // The presentation engine does not wait for a vertical blanking period to update the
            // current image, meaning this mode may result in visible tearing
            *pPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            break;
        }

        if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            *pPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        } else if ((*pPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) &&
                   (presentModes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR)) {
            // VK_PRESENT_MODE_FIFO_RELAXED_KHR - equivalent of eglSwapInterval(-1)
            *pPresentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        }
    }

    // So in VR you probably want to use VK_PRESENT_MODE_IMMEDIATE_KHR and rely on the OVR/OXR synchronization
    // but not in VR we probably want to use FIFO to go in hz of monitor... force FIFO for testing
    *pPresentMode = VK_PRESENT_MODE_FIFO_KHR;

    MXC_LOG_NAMED(string_VkPresentModeKHR(*pPresentMode));

    return MXC_SUCCESS;
}

MXC_RESULT ChooseSwapSurfaceFormat(const VkPhysicalDevice physicalDevice,
                                   const PipelineType pipelineType,
                                   VkSurfaceFormatKHR* pSurfaceFormat)
{
    uint32_t formatCount;
    VK_CHK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice,
                                                Vulkan::GetVkSurface(),
                                                &formatCount,
                                                nullptr));
    VkSurfaceFormatKHR formats[formatCount];
    VK_CHK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice,
                                                Vulkan::GetVkSurface(),
                                                &formatCount,
                                                (VkSurfaceFormatKHR*) &formats));

    for (int i = 0; i < formatCount; ++i) {
        if (pipelineType == PipelineType::Compute &&
            formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
            *pSurfaceFormat = formats[i];
            return MXC_SUCCESS;
        }
        if (pipelineType == PipelineType::Graphics &&
              formats[i].format == VK_FORMAT_B8G8R8A8_SRGB ||
            formats[i].format == VK_FORMAT_R8G8B8A8_SRGB) {
            *pSurfaceFormat = formats[i];
            return MXC_SUCCESS;
        }
    }

    // Default to the first one if no sRGB
    *pSurfaceFormat = formats[0];

    return MXC_SUCCESS;
}

Swap::Swap(const Vulkan::Device* const pDevice)
    : Device(pDevice) {}

Swap::~Swap()
{
    vkDestroySemaphore(Device->GetVkDevice(), vkAcquireCompleteSemaphore, VK_ALLOC);
    vkDestroySemaphore(Device->GetVkDevice(), vkRenderCompleteSemaphore, VK_ALLOC);
    for (int i = 0; i < SwapCount; ++i) {
        vkDestroyImageView(Device->GetVkDevice(), vkSwapImageViews[i], VK_ALLOC);
        vkDestroyImage(Device->GetVkDevice(), vkSwapImages[i], VK_ALLOC);
    }
}

MXC_RESULT Swap::Init(const PipelineType pipelineType,
                      const VkExtent2D dimensions)
{
    // Logic from OVR Vulkan example
    VkSurfaceCapabilitiesKHR capabilities;
    VK_CHK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Device->GetVkPhysicalDevice(),
                                                     Vulkan::GetVkSurface(),
                                                     &capabilities));

    // I am setting this to 2 on the premise you get the least latency in VR.
    if (SwapCount < capabilities.minImageCount) {
        MXC_LOG_ERROR("SwapCount is less than minImageCount", SwapCount, capabilities.minImageCount);
        return MXC_FAIL;
    }

    VkPresentModeKHR presentMode;
    MXC_CHK(ChooseSwapPresentMode(Device->GetVkPhysicalDevice(),
                                  &presentMode));
    VkSurfaceFormatKHR surfaceFormat;
    MXC_CHK(ChooseSwapSurfaceFormat(Device->GetVkPhysicalDevice(),
                                    pipelineType,
                                    &surfaceFormat));

    VkSwapchainCreateInfoKHR createInfo = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .pNext = VK_NULL_HANDLE,
      .surface = GetVkSurface(),
      .minImageCount = SwapCount,
      .imageFormat = surfaceFormat.format,
      .imageColorSpace = surfaceFormat.colorSpace,
      .imageExtent = dimensions,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
      .presentMode = presentMode,
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE,
    };

    // OBS is adding VK_IMAGE_USAGE_TRANSFER_SRC_BIT is there a way to detect that!?
    // Let's just add it anyway...
    if (pipelineType == PipelineType::Graphics &&
        (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
        MXC_LOG("Swap support VK_IMAGE_USAGE_TRANSFER_DST_BIT adding!");
        createInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    } else if (pipelineType == PipelineType::Graphics) {
        MXC_LOG_ERROR("Vulkan swapchain does not support VK_IMAGE_USAGE_TRANSFER_DST_BIT.");
    }

    if (pipelineType == PipelineType::Compute &&
        (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT)) {
        MXC_LOG("Swap support VK_IMAGE_USAGE_STORAGE_BIT adding!");
        createInfo.imageUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
    } else if (pipelineType == PipelineType::Compute) {
        MXC_LOG_ERROR("Vulkan Swap does not support VK_IMAGE_USAGE_STORAGE_BIT!");
    }

    if (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        createInfo.preTransform = capabilities.currentTransform;
    }

    if ((capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0) {
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    } else if ((capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) != 0) {
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    } else {
        MXC_LOG_ERROR("Unexpected value for VkSurfaceCapabilitiesKHR.compositeAlpha:",
                      capabilities.supportedCompositeAlpha);
        return MXC_FAIL;
    }

    VK_CHK(vkCreateSwapchainKHR(Device->GetVkDevice(),
                                &createInfo,
                                VK_ALLOC,
                                &vkSwapchain));

    uint32_t swapCount;
    VK_CHK(vkGetSwapchainImagesKHR(Device->GetVkDevice(),
                                   vkSwapchain,
                                   &swapCount,
                                   nullptr));
    if (swapCount != vkSwapImages.size()) {
        MXC_LOG_ERROR("Unexpected swap count!", swapCount, vkSwapImages.size());
        return MXC_FAIL;
    }
    VK_CHK(vkGetSwapchainImagesKHR(Device->GetVkDevice(),
                                   vkSwapchain,
                                   &swapCount,
                                   vkSwapImages.data()));

    for (int i = 0; i < vkSwapImages.size(); ++i) {
        Device->TransitionImageLayoutImmediate(vkSwapImages[i],
                                               VK_IMAGE_LAYOUT_UNDEFINED,
                                               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                               VK_ACCESS_NONE,
                                               VK_ACCESS_NONE,
                                               VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                               VK_IMAGE_ASPECT_COLOR_BIT);
        const VkImageViewCreateInfo viewInfo{
          .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          .image = vkSwapImages[i],
          .viewType = VK_IMAGE_VIEW_TYPE_2D,
          .format = surfaceFormat.format,
          .components{
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
          },
          .subresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
          }};
        VK_CHK(vkCreateImageView(Device->GetVkDevice(),
                                 &viewInfo,
                                 VK_ALLOC,
                                 &vkSwapImageViews[i]));
    }

    constexpr VkSemaphoreCreateInfo swapchainSemaphoreCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
    };
    VK_CHK(vkCreateSemaphore(Device->GetVkDevice(),
                             &swapchainSemaphoreCreateInfo,
                             VK_ALLOC,
                             &vkAcquireCompleteSemaphore));
    VK_CHK(vkCreateSemaphore(Device->GetVkDevice(),
                             &swapchainSemaphoreCreateInfo,
                             VK_ALLOC,
                             &vkRenderCompleteSemaphore));

    MXC_LOG_NAMED(string_VkImageUsageFlags(createInfo.imageUsage));
    MXC_LOG_NAMED(string_VkFormat(createInfo.imageFormat));

    this->dimensions = dimensions;
    this->format = surfaceFormat.format;

    return MXC_SUCCESS;
}

MXC_RESULT Swap::Acquire(uint32_t* pSwapIndex)
{
    VK_CHK(vkAcquireNextImageKHR(Device->GetVkDevice(),
                                 vkSwapchain,
                                 UINT64_MAX,
                                 vkAcquireCompleteSemaphore,
                                 VK_NULL_HANDLE,
                                 pSwapIndex));
    return MXC_SUCCESS;
}

void Swap::Transition(const VkCommandBuffer commandBuffer,
                      const uint32_t swapIndex,
                      const Barrier2& src,
                      const Barrier2& dst) const
{
    const VkImageMemoryBarrier2 barrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .srcStageMask = src.colorStageMask,
      .srcAccessMask = src.colorAccessMask,
      .dstStageMask = dst.colorStageMask,
      .dstAccessMask = dst.colorAccessMask,
      .oldLayout = src.layout,
      .newLayout = dst.layout,
      .srcQueueFamilyIndex = Device->GetSrcQueue(src),
      .dstQueueFamilyIndex = Device->GetDstQueue(src, dst),
      .image = vkSwapImages[swapIndex],
      .subresourceRange = GetSubresourceRange(),
    };
    const VkDependencyInfo dependencyInfo{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &barrier,
    };
    VK_CHK_VOID(vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo));
}

void Swap::BlitToSwap(const VkCommandBuffer commandBuffer,
                      const uint32_t swapIndex,
                      const Texture& srcTexture) const
{
    const StaticArray transitionBlitBarrier{
      VkImageMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = Device->GetGraphicsQueueFamilyIndex(),
        .dstQueueFamilyIndex = Device->GetGraphicsQueueFamilyIndex(),
        .image = srcTexture.VkImageHandle,
        .subresourceRange = srcTexture.GetSubresourceRange(),
      },
      VkImageMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = Device->GetGraphicsQueueFamilyIndex(),
        .dstQueueFamilyIndex = Device->GetGraphicsQueueFamilyIndex(),
        .image = vkSwapImages[swapIndex],
        .subresourceRange = GetSubresourceRange(),
      },
    };
    VK_CHK_VOID(vkCmdPipelineBarrier(commandBuffer,
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
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel = 0,
      .baseArrayLayer = 0,
      .layerCount = 1,
    };
    const VkOffset3D offsets[2]{
      VkOffset3D{
        .x = 0,
        .y = 0,
        .z = 0,
      },
      VkOffset3D{
        .x = (int32_t)dimensions.width,
        .y = (int32_t)dimensions.height,
        .z = 1,
      },
    };
    VkImageBlit imageBlit{};
    imageBlit.srcSubresource = imageSubresourceLayers;
    imageBlit.srcOffsets[0] = offsets[0];
    imageBlit.srcOffsets[1] = offsets[1];
    imageBlit.dstSubresource = imageSubresourceLayers;
    imageBlit.dstOffsets[0] = offsets[0];
    imageBlit.dstOffsets[1] = offsets[1];
    VK_CHK_VOID(vkCmdBlitImage(commandBuffer,
                               srcTexture.VkImageHandle,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               vkSwapImages[swapIndex],
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1,
                               &imageBlit,
                               VK_FILTER_NEAREST));
    const StaticArray transitionPresentBarrier{
      VkImageMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .dstAccessMask = VK_ACCESS_NONE,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = Device->GetGraphicsQueueFamilyIndex(),
        .dstQueueFamilyIndex = Device->GetGraphicsQueueFamilyIndex(),
        .image = srcTexture.VkImageHandle,
        .subresourceRange = srcTexture.GetSubresourceRange(),
      },
      VkImageMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_NONE,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = Device->GetGraphicsQueueFamilyIndex(),
        .dstQueueFamilyIndex = Device->GetGraphicsQueueFamilyIndex(),
        .image = vkSwapImages[swapIndex],
        .subresourceRange = GetSubresourceRange(),
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

MXC_RESULT Swap::QueuePresent(const VkQueue& queue,
                              const uint32_t swapIndex) const
{
    // TODO want to use this?? https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_present_id.html
    const VkPresentInfoKHR presentInfo{
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &vkRenderCompleteSemaphore,
      .swapchainCount = 1,
      .pSwapchains = &vkSwapchain,
      .pImageIndices = &swapIndex,
    };
    VK_CHK(vkQueuePresentKHR(queue,
                             &presentInfo));
    return MXC_SUCCESS;
}
