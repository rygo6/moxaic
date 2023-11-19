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
        MXC_NO_VALUE_PASS(Framebuffer);

    public:
        explicit Framebuffer(const Device& device);
        virtual ~Framebuffer();


        MXC_RESULT Init(VkExtent2D extents,
                        Locality locality);

        MXC_RESULT InitFromImport(VkExtent2D extents,
                                  HANDLE colorExternalHandle,
                                  HANDLE normalExternalHandle,
                                  HANDLE gBufferExternalHandle,
                                  HANDLE depthExternalHandle);

        const auto& vkFramebuffer() const { return m_VkFramebuffer; }
        const auto& vkRenderCompleteSemaphore() const { return m_VkRenderCompleteSemaphore; }

        const auto& colorTexture() const { return m_ColorTexture; }
        const auto& normalTexture() const { return m_NormalTexture; }
        const auto& gBufferTexture() const { return m_GBufferTexture; }
        const auto& depthTexture() const { return m_DepthTexture; }

        const auto& extents() const { return m_Extents; }

    private:
        const Device& k_Device;

        VkFramebuffer m_VkFramebuffer{VK_NULL_HANDLE};
        VkSemaphore m_VkRenderCompleteSemaphore{VK_NULL_HANDLE};

        Texture m_ColorTexture{k_Device};
        Texture m_NormalTexture{k_Device};
        Texture m_GBufferTexture{k_Device};
        Texture m_DepthTexture{k_Device};

        VkExtent2D m_Extents{};

        MXC_RESULT InitFramebuffer();
        MXC_RESULT InitSemaphore();
    };
}
