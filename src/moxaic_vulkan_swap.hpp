#pragma once

#include "moxaic_logging.hpp"
#include "moxaic_vulkan.hpp"
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

        Swap(const Device& device);
        virtual ~Swap();

        MXC_RESULT Init(const VkExtent2D& dimensions,
                        const bool& computeStorage);

        MXC_RESULT Acquire(uint32_t* pSwapIndex);
        void Transition(const VkCommandBuffer commandBuffer,
                        const uint32_t swapIndex,
                        const Barrier& src,
                        const Barrier& dst) const;
        void BlitToSwap(const VkCommandBuffer commandBuffer,
                        const uint32_t swapIndex,
                        const Texture& srcTexture) const;
        MXC_RESULT QueuePresent(const VkQueue& queue,
                                const uint32_t swapIndex) const;

        MXC_GET(VkAcquireCompleteSemaphore);
        MXC_GET(VkRenderCompleteSemaphore);
        MXC_GET(Format);

        MXC_GETARR(VkSwapImageViews);

    private:
        const Device* const k_pDevice;

        VkSwapchainKHR m_VkSwapchain{VK_NULL_HANDLE};
        VkSemaphore m_VkAcquireCompleteSemaphore{VK_NULL_HANDLE};
        VkSemaphore m_VkRenderCompleteSemaphore{VK_NULL_HANDLE};

        StaticArray<VkImage, SwapCount> m_VkSwapImages{VK_NULL_HANDLE};
        StaticArray<VkImageView, SwapCount> m_VkSwapImageViews{VK_NULL_HANDLE};

        VkFormat m_Format{};
        VkExtent2D m_Dimensions{};
    };
}// namespace Moxaic::Vulkan
