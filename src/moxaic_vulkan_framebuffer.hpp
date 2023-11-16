#pragma once

#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_texture.hpp"

#include <vulkan/vulkan.h>
#include <memory>

namespace Moxaic::Vulkan
{
    class Texture;
    class Device;

    class Framebuffer
    {
    public:
        Framebuffer(const Device &device);
        virtual ~Framebuffer();

        bool Init(const VkExtent2D extents,
                  const Vulkan::Locality locality);

        inline auto vkFramebuffer() const { return m_VkFramebuffer; }
        inline auto vkRenderCompleteSemaphore() const { return m_VkRenderCompleteSemaphore; }

        inline const auto &colorTexture() const { return m_ColorTexture; }
        inline const auto &normalTexture() const { return m_NormalTexture; }
        inline const auto &gBufferTexture() const { return m_GBufferTexture; }
        inline const auto &depthTexture() const { return m_DepthTexture; }

        inline const auto &extents() const { return m_Extents; }

    private:
        const Device &k_Device;

        VkFramebuffer m_VkFramebuffer{VK_NULL_HANDLE};
        VkSemaphore m_VkRenderCompleteSemaphore{VK_NULL_HANDLE};

        Texture m_ColorTexture{k_Device};
        Texture m_NormalTexture{k_Device};
        Texture m_GBufferTexture{k_Device};
        Texture m_DepthTexture{k_Device};

        VkExtent2D m_Extents;
    };
}
