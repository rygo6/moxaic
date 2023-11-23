#pragma once

#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_texture.hpp"

#include <memory>
#include <vulkan/vulkan.h>

namespace Moxaic::Vulkan
{
    class Texture;
    class Device;

    class Framebuffer
    {
    public:
        MXC_NO_VALUE_PASS(Framebuffer);

        explicit Framebuffer(Device const& device);
        virtual ~Framebuffer();

        MXC_RESULT Init(VkExtent2D const& extents,
                        Locality const& locality);

        MXC_RESULT InitFromImport(VkExtent2D const& extents,
                                  HANDLE& const colorExternalHandle,
                                  HANDLE& const normalExternalHandle,
                                  HANDLE& const gBufferExternalHandle,
                                  HANDLE& const depthExternalHandle);

        void AcquireFramebufferFromExternalToGraphicsAttach();
        void Transition(BarrierSrc const& src,
                        BarrierDst const& dst) const;

        auto const& vkFramebuffer() const { return m_VkFramebuffer; }
        auto const& vkRenderCompleteSemaphore() const { return m_VkRenderCompleteSemaphore; }

        auto const& colorTexture() const { return m_ColorTexture; }
        auto const& normalTexture() const { return m_NormalTexture; }
        auto const& gBufferTexture() const { return m_GBufferTexture; }
        auto const& depthTexture() const { return m_DepthTexture; }

        auto const& extents() const { return m_Extents; }

    private:
        Device const& k_Device;

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
}// namespace Moxaic::Vulkan
