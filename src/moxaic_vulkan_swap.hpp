#pragma once

#include "moxaic_logging.hpp"
#include "moxaic_vulkan.hpp"
#include "static_array.hpp"

#include <vulkan/vulkan.h>

namespace Moxaic::Vulkan
{
    class Device;
    class Texture;

    class Swap final
    {
    public:
        MXC_NO_VALUE_PASS(Swap);

        explicit Swap(const Device* pDevice);
        ~Swap();

        MXC_RESULT Init(PipelineType pipelineType,
                        VkExtent2D dimensions);
        MXC_RESULT Acquire(uint32_t* pSwapIndex);
        void Transition(VkCommandBuffer commandBuffer,
                        uint32_t swapIndex,
                        const Barrier2& src,
                        const Barrier2& dst) const;
        void BlitToSwap(VkCommandBuffer commandBuffer,
                        uint32_t swapIndex,
                        const Texture& srcTexture) const;
        MXC_RESULT QueuePresent(const VkQueue& queue,
                                uint32_t swapIndex) const;

        constexpr VkImageSubresourceRange GetSubresourceRange() const
        {
            return {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
            };
        }

        constexpr static uint32_t SwapCount = 3;

        const auto& GetVkSwapchain() const { return vkSwapchain; }
        const auto& GetVkAcquireCompleteSemaphore() const { return vkAcquireCompleteSemaphore; }
        const auto& GetVkRenderCompleteSemaphore() const { return vkRenderCompleteSemaphore; }
        const auto& GetVkSwapImage(const int i) const { return vkSwapImages[i]; }
        const auto& GetVkSwapImageView(const int i) const { return vkSwapImageViews[i]; }

    private:
        const Device* const Device;

        VkSwapchainKHR vkSwapchain{VK_NULL_HANDLE};
        VkSemaphore vkAcquireCompleteSemaphore{VK_NULL_HANDLE};
        VkSemaphore vkRenderCompleteSemaphore{VK_NULL_HANDLE};

        StaticArray<VkImage, SwapCount> vkSwapImages{VK_NULL_HANDLE};
        StaticArray<VkImageView, SwapCount> vkSwapImageViews{VK_NULL_HANDLE};

        VkFormat format{};
        VkExtent2D dimensions{};
    };
}// namespace Moxaic::Vulkan
