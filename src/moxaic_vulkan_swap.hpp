#pragma once

#include "moxaic_logging.hpp"

#include <vulkan/vulkan.h>
#include <array>

namespace Moxaic
{
    class VulkanDevice;
    class VulkanTexture;

    const uint32_t k_SwapCount = 2;

    class VulkanSwap
    {
    public:
        VulkanSwap(const VulkanDevice &device);
        virtual ~VulkanSwap();

        MXC_RESULT Init(VkExtent2D dimensions, bool computeStorage);

        MXC_RESULT BlitToSwap(const VulkanTexture &srcTexture);

        inline auto vkSwapchain() const { return m_VkSwapchain; }
        inline auto vkAcquireCompleteSemaphore() const { return m_VkAcquireCompleteSemaphore; }
        inline auto vkRenderCompleteSemaphore() const { return m_VkRenderCompleteSemaphore; }
        inline auto vkSwapImages(int i) const { return m_VkSwapImages[i]; }
        inline auto vkSwapImageViews(int i) const { return m_VkSwapImageViews[i]; }
        inline auto blitIndex() const { return m_BlitIndex; }
        inline auto format() const { return m_Format; }

    private:
        const VulkanDevice &k_Device;

        VkSwapchainKHR m_VkSwapchain{VK_NULL_HANDLE};
        VkSemaphore m_VkAcquireCompleteSemaphore{VK_NULL_HANDLE};
        VkSemaphore m_VkRenderCompleteSemaphore{VK_NULL_HANDLE};

        std::array<VkImage, k_SwapCount> m_VkSwapImages{VK_NULL_HANDLE};
        std::array<VkImageView, k_SwapCount> m_VkSwapImageViews{VK_NULL_HANDLE};

        VkFormat m_Format;
        VkExtent2D m_Dimensions;

        uint32_t m_BlitIndex;
    };
}
