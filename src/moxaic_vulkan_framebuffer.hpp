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
        explicit VulkanFramebuffer(const VulkanDevice &device);
        VulkanFramebuffer(const VulkanDevice &&) = delete;  // prevents rvalue binding?

        virtual ~VulkanFramebuffer();

        bool Init(const VkExtent2D &extent,
                  const BufferLocality &locality);

    private:
        const VulkanDevice &k_Device;
        std::unique_ptr<VulkanTexture> m_ColorTexture{};
        std::unique_ptr<VulkanTexture> m_NormalTexture{};
        std::unique_ptr<VulkanTexture> m_GBufferTexture{};
        std::unique_ptr<VulkanTexture> m_DepthTexture{};
        VkSampleCountFlagBits m_Samples{VK_SAMPLE_COUNT_1_BIT};


    public:
#define VK_HANDLES \
        VK_HANDLE(,Framebuffer) \
        VK_HANDLE(RenderComplete,Semaphore)

#define VK_HANDLE(p, h) VK_GETTERS(p, h)
        VK_HANDLES
#undef VK_HANDLE
    private:
#define VK_HANDLE(p, h) VK_MEMBERS(p, h)
        VK_HANDLES
#undef VK_HANDLE
#undef VK_HANDLES
    };
}
