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

        explicit Framebuffer(const Device& device);
        virtual ~Framebuffer();

        MXC_RESULT Init(const VkExtent2D extents,
                        const PipelineType pipeline, const Locality locality);

        MXC_RESULT InitFromImport(const PipelineType pipeline,
                                  const VkExtent2D extents,
                                  const HANDLE colorExternalHandle,
                                  const HANDLE normalExternalHandle,
                                  const HANDLE gBufferExternalHandle,
                                  const HANDLE depthExternalHandle);

        void AcquireFramebufferFromExternalToGraphicsAttach();
        void Transition(const VkCommandBuffer commandbuffer,
                        const Barrier& src,
                        const Barrier& dst) const;

        const auto& vkFramebuffer() const
        {
            return m_VkFramebuffer;
        }
        const auto& vkRenderCompleteSemaphore() const { return m_VkRenderCompleteSemaphore; }

        const auto& colorTexture() const { return m_ColorTexture; }
        const auto& normalTexture() const { return m_NormalTexture; }
        const auto& gBufferTexture() const { return m_GBufferTexture; }
        const auto& depthTexture() const { return m_DepthTexture; }

        const auto& extents() const { return m_Extents; }

    private:
        const Device* const k_pDevice;

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
