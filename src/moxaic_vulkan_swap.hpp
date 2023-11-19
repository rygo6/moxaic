#pragma once

#include "moxaic_logging.hpp"

#include <vulkan/vulkan.h>
#include <array>

namespace Moxaic::Vulkan
{
    class Device;
    class Texture;

    inline constexpr uint32_t SwapCount = 2;

    class Swap
    {
        MXC_NO_VALUE_PASS(Swap);
    public:
        explicit Swap(const Device &device);
        virtual ~Swap();

        MXC_RESULT Init(VkExtent2D dimensions, bool computeStorage);

        MXC_RESULT Acquire();
        void BlitToSwap(const Texture &srcTexture) const;
        MXC_RESULT QueuePresent() const;

        const auto& vkAcquireCompleteSemaphore() const { return m_VkAcquireCompleteSemaphore; }
        const auto& vkRenderCompleteSemaphore() const { return m_VkRenderCompleteSemaphore; }
        const auto& format() const { return m_Format; }

    private:
        const Device &k_Device;

        VkSwapchainKHR m_VkSwapchain{VK_NULL_HANDLE};
        VkSemaphore m_VkAcquireCompleteSemaphore{VK_NULL_HANDLE};
        VkSemaphore m_VkRenderCompleteSemaphore{VK_NULL_HANDLE};

        std::array<VkImage, SwapCount> m_VkSwapImages{VK_NULL_HANDLE};
        std::array<VkImageView, SwapCount> m_VkSwapImageViews{VK_NULL_HANDLE};

        VkFormat m_Format;
        VkExtent2D m_Dimensions;

        uint32_t m_LastAcquiredSwapIndex;
    };
}
