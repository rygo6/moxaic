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
                        const Barrier& src,
                        const Barrier& dst) const;
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

        MXC_GET(VkAcquireCompleteSemaphore);
        MXC_GET(VkRenderCompleteSemaphore);
        MXC_GET(Format);

        MXC_GETARR(VkSwapImageViews);
        MXC_GETARR(VkSwapImages);

    private:
        const Device* const Device;

        VkSwapchainKHR m_VkSwapchain{VK_NULL_HANDLE};
        VkSemaphore m_VkAcquireCompleteSemaphore{VK_NULL_HANDLE};
        VkSemaphore m_VkRenderCompleteSemaphore{VK_NULL_HANDLE};

        StaticArray<VkImage, SwapCount> m_VkSwapImages{VK_NULL_HANDLE};
        StaticArray<VkImageView, SwapCount> m_VkSwapImageViews{VK_NULL_HANDLE};

        VkFormat m_Format{};
        VkExtent2D m_Dimensions{};
    };
}// namespace Moxaic::Vulkan
