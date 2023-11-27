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

        MXC_RESULT Init(const VkExtent2D extents,
                        const Locality locality);

        MXC_RESULT InitFromImport(const VkExtent2D extents,
                                  const HANDLE colorExternalHandle,
                                  const HANDLE normalExternalHandle,
                                  const HANDLE gBufferExternalHandle,
                                  const HANDLE depthExternalHandle);

        void AcquireFramebufferFromExternalToGraphicsAttach();
        void Transition(VkCommandBuffer const commandbuffer,
                        Barrier const& src,
                        Barrier const& dst) const;

        auto const& vkFramebuffer() const
        {
            return m_VkFramebuffer;
        }
        auto const& vkRenderCompleteSemaphore() const { return m_VkRenderCompleteSemaphore; }

        auto const& colorTexture() const { return m_ColorTexture; }
        auto const& normalTexture() const { return m_NormalTexture; }
        auto const& gBufferTexture() const { return m_GBufferTexture; }
        auto const& depthTexture() const { return m_DepthTexture; }

        auto const& extents() const { return m_Extents; }

    private:
        Device const* const k_pDevice;

        VkFramebuffer m_VkFramebuffer{VK_NULL_HANDLE};
        VkSemaphore m_VkRenderCompleteSemaphore{VK_NULL_HANDLE};

        Texture m_ColorTexture{*k_pDevice};
        Texture m_NormalTexture{*k_pDevice};
        Texture m_GBufferTexture{*k_pDevice};
        Texture m_DepthTexture{*k_pDevice};

        VkExtent2D m_Extents{};

        MXC_RESULT InitFramebuffer();
        MXC_RESULT InitSemaphore();
    };
}// namespace Moxaic::Vulkan
