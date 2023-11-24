#pragma once

#include "moxaic_logging.hpp"
#include "static_array.hpp"

#include <vulkan/vulkan.h>

namespace Moxaic::Vulkan
{
    class Device;
    class Texture;

    inline constexpr uint32_t SwapCount = 2;

    class Swap
    {
    public:
        MXC_NO_VALUE_PASS(Swap);

        Swap(Device const& device);
        virtual ~Swap();

        MXC_RESULT Init(VkExtent2D const& dimensions,
                        bool const& computeStorage);

        MXC_RESULT Acquire();
        void BlitToSwap(Texture const& srcTexture) const;
        MXC_RESULT QueuePresent() const;

        MXC_GET(VkAcquireCompleteSemaphore);
        MXC_GET(VkRenderCompleteSemaphore);
        MXC_GET(Format);

        MXC_GETARR(VkSwapImageViews);

    private:
        Device const* const k_pDevice;

        VkSwapchainKHR m_VkSwapchain{VK_NULL_HANDLE};
        VkSemaphore m_VkAcquireCompleteSemaphore{VK_NULL_HANDLE};
        VkSemaphore m_VkRenderCompleteSemaphore{VK_NULL_HANDLE};

        StaticArray<VkImage, SwapCount> m_VkSwapImages{VK_NULL_HANDLE};
        StaticArray<VkImageView, SwapCount> m_VkSwapImageViews{VK_NULL_HANDLE};

        VkFormat m_Format{};
        VkExtent2D m_Dimensions{};

        uint32_t m_LastAcquiredSwapIndex{0};
    };
}// namespace Moxaic::Vulkan
