#pragma once

#include "moxaic_vulkan.hpp"

#include <vulkan/vulkan.h>
#include <memory>

namespace Moxaic
{
    class VulkanTexture;

    class VulkanDevice;

    class VulkanFramebuffer
    {
    public:
#define VK_HANDLES \
        VK_HANDLE(,Framebuffer) \
        VK_HANDLE(RenderComplete,Semaphore)

        explicit VulkanFramebuffer(const VulkanDevice &device);
        VulkanFramebuffer(const VulkanDevice &&) = delete;  // prevents rvalue binding?
        virtual ~VulkanFramebuffer();

        bool Init(const VkExtent2D &extent,
                  const BufferLocality &locality);

        inline auto vkFramebuffer() const { return m_VkFramebuffer; }
        inline auto vkRenderCompleteSemaphore() const { return m_VkRenderCompleteSemaphore; }

    private:
        const VulkanDevice &k_Device;

        VkFramebuffer m_VkFramebuffer{VK_NULL_HANDLE};
        VkSemaphore m_VkRenderCompleteSemaphore{VK_NULL_HANDLE};

        std::unique_ptr<VulkanTexture> m_ColorTexture{};
        std::unique_ptr<VulkanTexture> m_NormalTexture{};
        std::unique_ptr<VulkanTexture> m_GBufferTexture{};
        std::unique_ptr<VulkanTexture> m_DepthTexture{};
        VkSampleCountFlagBits m_Samples{VK_SAMPLE_COUNT_1_BIT};

    };
}
