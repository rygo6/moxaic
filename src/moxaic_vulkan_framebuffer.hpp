#pragma once

#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_texture.hpp"

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

        bool Init(const VkExtent2D extents,
                  const Locality locality);

        inline auto vkFramebuffer() const { return m_VkFramebuffer; }
        inline auto vkRenderCompleteSemaphore() const { return m_VkRenderCompleteSemaphore; }

        inline const auto &colorTexture() const { return m_ColorTexture; }
        inline const auto &normalTexture() const { return m_NormalTexture; }
        inline const auto &gBufferTexture() const { return m_GBufferTexture; }
        inline const auto &depthTexture() const { return m_DepthTexture; }

        inline const auto &dimensions() const { return m_Dimensions; }

    private:
        const VulkanDevice &k_Device;

        VkFramebuffer m_VkFramebuffer{VK_NULL_HANDLE};
        VkSemaphore m_VkRenderCompleteSemaphore{VK_NULL_HANDLE};

        VulkanTexture m_ColorTexture{k_Device};
        VulkanTexture m_NormalTexture{k_Device};
        VulkanTexture m_GBufferTexture{k_Device};
        VulkanTexture m_DepthTexture{k_Device};

        VkExtent2D m_Dimensions;
    };
}
