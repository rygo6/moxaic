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

        MXC_RESULT Init(const PipelineType pipelineType,
                        const VkExtent2D extents, const Locality locality);

        MXC_RESULT InitFromImport(const PipelineType pipelineType,
                                  const VkExtent2D extents,
                                  const HANDLE colorExternalHandle,
                                  const HANDLE normalExternalHandle,
                                  const HANDLE gBufferExternalHandle);

        void AcquireFramebufferFromExternalToGraphicsAttach();
        void Transition(const VkCommandBuffer commandbuffer,
                        const Barrier& src,
                        const Barrier& dst) const;

        MXC_GET(VkFramebuffer);
        MXC_GET(VkRenderCompleteSemaphore);
        MXC_GET(ColorTexture);
        MXC_GET(NormalTexture);
        MXC_GET(GBufferTexture);
        MXC_GET(DepthTexture);
        MXC_GET(Extents);

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
