#include "moxaic_vulkan_swap.hpp"

#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_texture.hpp"

#include <vulkan/vk_enum_string_helper.h>

using namespace Moxaic;
using namespace Moxaic::Vulkan;

static MXC_RESULT ChooseSwapPresentMode(const VkPhysicalDevice physicalDevice, VkPresentModeKHR &outPresentMode)
{
    uint32_t presentModeCount;
    VK_CHK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice,
                                                     Vulkan::vkSurface(),
                                                     &presentModeCount,
                                                     nullptr));
    VkPresentModeKHR presentModes[presentModeCount];
    VK_CHK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice,
                                                     Vulkan::vkSurface(),
                                                     &presentModeCount,
                                                     (VkPresentModeKHR *) &presentModes));

    // https://www.intel.com/content/www/us/en/developer/articles/training/api-without-secrets-introduction-to-vulkan-part-2.html?language=en#_Toc445674479
    // This logic taken from OVR Vulkan Example
    // VK_PRESENT_MODE_FIFO_KHR - equivalent of eglSwapInterval(1).  The presentation engine waits for the next vertical blanking period to update
    // the current image. Tearing cannot be observed. This mode must be supported by all implementations.
    outPresentMode = VK_PRESENT_MODE_FIFO_KHR;
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
            outPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            break;
        } else if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            outPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        } else if ((outPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) &&
                   (presentModes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR)) {
            // VK_PRESENT_MODE_FIFO_RELAXED_KHR - equivalent of eglSwapInterval(-1)
            outPresentMode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        }
    }

    // So in VR you probably want to use VK_PRESENT_MODE_IMMEDIATE_KHR and rely on the OVR/OXR synchronization
    // but not in VR we probably want to use FIFO to go in hz of monitor... force FIFO for testing
    outPresentMode = VK_PRESENT_MODE_FIFO_KHR;

    MXC_LOG_NAMED(string_VkPresentModeKHR(outPresentMode));

    return MXC_SUCCESS;
}

MXC_RESULT ChooseSwapSurfaceFormat(const VkPhysicalDevice physicalDevice,
                                   bool computeStorage,
                                   VkSurfaceFormatKHR &outSurfaceFormat)
{
    uint32_t formatCount;
    VK_CHK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice,
                                                Vulkan::vkSurface(),
                                                &formatCount,
                                                nullptr));
    VkSurfaceFormatKHR formats[formatCount];
    VK_CHK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice,
                                                Vulkan::vkSurface(),
                                                &formatCount,
                                                (VkSurfaceFormatKHR *) &formats));

    for (int i = 0; i < formatCount; ++i) {
        if (computeStorage &&
            formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
            outSurfaceFormat = formats[i];
            return MXC_SUCCESS;
        } else if (!computeStorage &&
                   formats[i].format == VK_FORMAT_B8G8R8A8_SRGB ||
                   formats[i].format == VK_FORMAT_R8G8B8A8_SRGB) {
            outSurfaceFormat = formats[i];
            return MXC_SUCCESS;
        }
    }

    // Default to the first one if no sRGB
    outSurfaceFormat = formats[0];

    return MXC_SUCCESS;
}

Swap::Swap(const Vulkan::Device &device)
        : k_Device(device) {}

Swap::~Swap()
{
    vkDestroySemaphore(k_Device.vkDevice(), m_VkAcquireCompleteSemaphore, VK_ALLOC);
    vkDestroySemaphore(k_Device.vkDevice(), m_VkRenderCompleteSemaphore, VK_ALLOC);
    for (int i = 0; i < SwapCount; ++i) {
        vkDestroyImageView(k_Device.vkDevice(), m_VkSwapImageViews[i], VK_ALLOC);
        vkDestroyImage(k_Device.vkDevice(), m_VkSwapImages[i], VK_ALLOC);
    }
}

MXC_RESULT Swap::Init(VkExtent2D dimensions, bool computeStorage)
{
    // Logic from OVR Vulkan example
    VkSurfaceCapabilitiesKHR capabilities;
    VK_CHK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(k_Device.vkPhysicalDevice(),
                                                     Vulkan::vkSurface(),
                                                     &capabilities));

    // I am setting this to 2 on the premise you get the least latency in VR.
    if (SwapCount < capabilities.minImageCount) {
        MXC_LOG_ERROR("FBR_SWAP_COUNT is less than minImageCount", SwapCount, capabilities.minImageCount);
        return MXC_FAIL;
    }

    VkPresentModeKHR presentMode;
    MXC_CHK(ChooseSwapPresentMode(k_Device.vkPhysicalDevice(),
                                  presentMode));
    VkSurfaceFormatKHR surfaceFormat;
    MXC_CHK(ChooseSwapSurfaceFormat(k_Device.vkPhysicalDevice(),
                                    computeStorage,
                                    surfaceFormat));

    VkSwapchainCreateInfoKHR createInfo = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = VK_NULL_HANDLE,
            .surface = Vulkan::vkSurface(),
            .minImageCount = SwapCount,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = dimensions,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = NULL,
            .presentMode = presentMode,
            .clipped = VK_TRUE,
            .oldSwapchain = VK_NULL_HANDLE,
    };

    // OBS is adding VK_IMAGE_USAGE_TRANSFER_SRC_BIT is there a way to detect that!?
    // Let's just add it anyway...
    if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
        MXC_LOG("Swap support VK_IMAGE_USAGE_TRANSFER_DST_BIT adding!");
        createInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    } else {
        MXC_LOG_ERROR("Vulkan swapchain does not support VK_IMAGE_USAGE_TRANSFER_DST_BIT.");
    }

    if (computeStorage && (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT)) {
        MXC_LOG("Swap support VK_IMAGE_USAGE_STORAGE_BIT adding!");
        createInfo.imageUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
    } else if (computeStorage) {
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
        MXC_LOG_ERROR("Unexpected value for VkSurfaceCapabilitiesKHR.compositeAlpha:", capabilities.supportedCompositeAlpha);
        return MXC_FAIL;
    }

    VK_CHK(vkCreateSwapchainKHR(k_Device.vkDevice(),
                                &createInfo,
                                VK_ALLOC,
                                &m_VkSwapchain));

    uint32_t swapCount;
    VK_CHK(vkGetSwapchainImagesKHR(k_Device.vkDevice(),
                                   m_VkSwapchain,
                                   &swapCount,
                                   nullptr));
    if (swapCount != m_VkSwapImages.size()) {
        MXC_LOG_ERROR("Unexpected swap count!", swapCount, m_VkSwapImages.size());
        return MXC_FAIL;
    }
    VK_CHK(vkGetSwapchainImagesKHR(k_Device.vkDevice(),
                                   m_VkSwapchain,
                                   &swapCount,
                                   m_VkSwapImages.data()));

    for (int i = 0; i < m_VkSwapImages.size(); ++i) {
        k_Device.TransitionImageLayoutImmediate(m_VkSwapImages[i],
                                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                VK_ACCESS_NONE, VK_ACCESS_NONE,
                                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                VK_IMAGE_ASPECT_COLOR_BIT);
        auto swapImage = m_VkSwapImages[i];
        const VkImageViewCreateInfo viewInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = swapImage,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = surfaceFormat.format,
                .components {
                        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
                .subresourceRange {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                }
        };
        auto swapImageView = m_VkSwapImageViews[i];
        VK_CHK(vkCreateImageView(k_Device.vkDevice(),
                                 &viewInfo,
                                 VK_ALLOC,
                                 &swapImageView));
    }

    const VkSemaphoreCreateInfo swapchainSemaphoreCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
    };
    VK_CHK(vkCreateSemaphore(k_Device.vkDevice(),
                             &swapchainSemaphoreCreateInfo,
                             VK_ALLOC,
                             &m_VkAcquireCompleteSemaphore));
    VK_CHK(vkCreateSemaphore(k_Device.vkDevice(),
                             &swapchainSemaphoreCreateInfo,
                             VK_ALLOC,
                             &m_VkRenderCompleteSemaphore));

    MXC_LOG_NAMED(string_VkImageUsageFlags(createInfo.imageUsage));
    MXC_LOG_NAMED(string_VkFormat(createInfo.imageFormat));

    m_Dimensions = dimensions;
    m_Format = surfaceFormat.format;

    if (m_Format != VK_FORMAT_B8G8R8A8_SRGB) {
        MXC_LOG_ERROR("Format is not VK_FORMAT_B8G8R8A8_SRGB! Was expecting VK_FORMAT_B8G8R8A8_SRGB!");
    }

    return MXC_SUCCESS;
}

MXC_RESULT Swap::Acquire()
{
    VK_CHK(vkAcquireNextImageKHR(k_Device.vkDevice(),
                                 m_VkSwapchain,
                                 UINT64_MAX,
                                 m_VkAcquireCompleteSemaphore,
                                 VK_NULL_HANDLE,
                                 &m_LastAcquiredSwapIndex));
    return MXC_SUCCESS;
}

void Swap::BlitToSwap(const Texture &srcTexture) const
{
    const std::array transitionBlitBarrier{
            (VkImageMemoryBarrier) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .srcAccessMask = 0,
                    .dstAccessMask = 0,
                    .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .srcQueueFamilyIndex = k_Device.graphicsQueueFamilyIndex(),
                    .dstQueueFamilyIndex = k_Device.graphicsQueueFamilyIndex(),
                    .image = srcTexture.vkImage(),
                    .subresourceRange = Vulkan::defaultColorSubresourceRange,
            },
            (VkImageMemoryBarrier) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .srcAccessMask = 0,
                    .dstAccessMask = 0,
                    .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .srcQueueFamilyIndex = k_Device.graphicsQueueFamilyIndex(),
                    .dstQueueFamilyIndex = k_Device.graphicsQueueFamilyIndex(),
                    .image = m_VkSwapImages[m_LastAcquiredSwapIndex],
                    .subresourceRange = Vulkan::defaultColorSubresourceRange,
            },
    };
    vkCmdPipelineBarrier(k_Device.vkGraphicsCommandBuffer(),
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0, nullptr,
                         0, nullptr,
                         transitionBlitBarrier.size(), transitionBlitBarrier.data());
    constexpr VkImageSubresourceLayers imageSubresourceLayers{
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
    };
    const VkOffset3D offsets[2]{
            (VkOffset3D) {
                    .x = 0,
                    .y = 0,
                    .z = 0,
            },
            (VkOffset3D) {
                    .x = static_cast<int32_t>(m_Dimensions.width),
                    .y = static_cast<int32_t>(m_Dimensions.height),
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
    vkCmdBlitImage(k_Device.vkGraphicsCommandBuffer(),
                   srcTexture.vkImage(),
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   m_VkSwapImages[m_LastAcquiredSwapIndex],
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1,
                   &imageBlit,
                   VK_FILTER_NEAREST);
    const std::array transitionPresentBarrier{
            (VkImageMemoryBarrier) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .srcAccessMask = 0,
                    .dstAccessMask = 0,
                    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .srcQueueFamilyIndex = k_Device.graphicsQueueFamilyIndex(),
                    .dstQueueFamilyIndex = k_Device.graphicsQueueFamilyIndex(),
                    .image = srcTexture.vkImage(),
                    .subresourceRange = Vulkan::defaultColorSubresourceRange,
            },
            (VkImageMemoryBarrier) {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .srcAccessMask = 0,
                    .dstAccessMask = 0,
                    .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    .srcQueueFamilyIndex = k_Device.graphicsQueueFamilyIndex(),
                    .dstQueueFamilyIndex = k_Device.graphicsQueueFamilyIndex(),
                    .image = m_VkSwapImages[m_LastAcquiredSwapIndex],
                    .subresourceRange = Vulkan::defaultColorSubresourceRange,
            },
    };
    vkCmdPipelineBarrier(k_Device.vkGraphicsCommandBuffer(),
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0,
                         0, nullptr,
                         0, nullptr,
                         transitionPresentBarrier.size(), transitionPresentBarrier.data());
}

MXC_RESULT Swap::QueuePresent() const
{
    // TODO want to use this?? https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_present_id.html
    const VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &m_VkRenderCompleteSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &m_VkSwapchain,
            .pImageIndices = &m_LastAcquiredSwapIndex,
    };
    VK_CHK(vkQueuePresentKHR(k_Device.vkGraphicsQueue(),
                             &presentInfo));
    return MXC_SUCCESS;
}