#pragma once

#include "moxaic_logging.hpp"

#include <vulkan/vulkan.h>
#include <array>

namespace Moxaic::Vulkan
{
    class Device;
    class Texture;

    const uint32_t k_SwapCount = 2;

    class Swap
    {
    public:
        Swap(const Device &device);
        virtual ~Swap();

        MXC_RESULT Init(VkExtent2D dimensions, bool computeStorage);

        MXC_RESULT Acquire();
        MXC_RESULT BlitToSwap(const Texture &srcTexture) const;
        MXC_RESULT QueuePresent() const;

        inline auto vkSwapchain() const { return m_VkSwapchain; }
        inline auto vkAcquireCompleteSemaphore() const { return m_VkAcquireCompleteSemaphore; }
        inline auto vkRenderCompleteSemaphore() const { return m_VkRenderCompleteSemaphore; }
        inline auto vkSwapImages(int i) const { return m_VkSwapImages[i]; }
        inline auto vkSwapImageViews(int i) const { return m_VkSwapImageViews[i]; }
        inline auto format() const { return m_Format; }

    private:
        const Device &k_Device;

        VkSwapchainKHR m_VkSwapchain{VK_NULL_HANDLE};
        VkSemaphore m_VkAcquireCompleteSemaphore{VK_NULL_HANDLE};
        VkSemaphore m_VkRenderCompleteSemaphore{VK_NULL_HANDLE};

        std::array<VkImage, k_SwapCount> m_VkSwapImages{VK_NULL_HANDLE};
        std::array<VkImageView, k_SwapCount> m_VkSwapImageViews{VK_NULL_HANDLE};

        VkFormat m_Format;
        VkExtent2D m_Dimensions;

        uint32_t m_LastAcquiredSwapIndex;
    };
}
