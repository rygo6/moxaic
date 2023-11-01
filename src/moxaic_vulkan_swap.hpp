#pragma once

#include "moxaic_logging.hpp"

#include <vulkan/vulkan.h>
#include <array>

namespace Moxaic
{
    class VulkanDevice;

    const uint32_t k_SwapCount = 2;

    class VulkanSwap
    {
    public:
        VulkanSwap(const VulkanDevice &device);
        virtual ~VulkanSwap();

        MXC_RESULT Init(VkExtent2D dimensions, bool computeStorage);

//        MXC_RESULT AcquireSwap(uint32_t& outSwapIndex);

        MXC_RESULT BlitToSwap(VkImage srcImage);

        inline auto vkSwapchain() const { return m_Swapchain; }
        inline auto acquireCompleteSemaphore() const { return m_AcquireCompleteSemaphore; }
        inline auto renderCompleteSemaphore() const { return m_RenderCompleteSemaphore; }
        inline auto swapImages(int i) const { return m_SwapImages[i]; }
        inline auto swapImageViews(int i) const { return m_SwapImageViews[i]; }
        inline auto blitIndex() const { return m_BlitIndex; }

    private:
        const VulkanDevice &k_Device;

        VkSwapchainKHR m_Swapchain{VK_NULL_HANDLE};
        VkSemaphore m_AcquireCompleteSemaphore{VK_NULL_HANDLE};
        VkSemaphore m_RenderCompleteSemaphore{VK_NULL_HANDLE};

        std::array<VkImage, k_SwapCount> m_SwapImages{VK_NULL_HANDLE};
        std::array<VkImageView, k_SwapCount> m_SwapImageViews{VK_NULL_HANDLE};

        VkFormat m_Format;
        VkExtent2D m_Dimensions;

        uint32_t m_BlitIndex;
    };
}
