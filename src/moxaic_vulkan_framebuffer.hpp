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

        explicit Framebuffer(const Vulkan::Device* const pDevice);
        virtual ~Framebuffer();

        MXC_RESULT Init(const PipelineType pipelineType,
                        const VkExtent2D extents, const Locality locality);

        MXC_RESULT InitFromImport(const PipelineType pipelineType,
                                  const VkExtent2D extents,
                                  const HANDLE colorExternalHandle,
                                  const HANDLE normalExternalHandle,
                                  const HANDLE gBufferExternalHandle,
                                  const HANDLE depthExternalHandle);

        void AcquireFramebufferFromExternalToGraphicsAttach();
        void Transition(const VkCommandBuffer commandbuffer,
                        const Barrier& src,
                        const Barrier& dst) const;

        const auto& GetVkFramebuffer() const { return vkFramebuffer; }
        const auto& GetVkRenderCompleteSemaphore() const { return vkRenderCompleteSemaphore; }
        const auto& GetColorTexture() const { return colorTexture; }
        const auto& GetNormalTexture() const { return normalTexture; }
        const auto& GetGBufferTexture() const { return gbufferTexture; }
        const auto& GetDepthTexture() const { return depthTexture; }
        const auto& GetExtents() const { return extents; }

    private:
        const Vulkan::Device* const Device;

        VkFramebuffer vkFramebuffer{VK_NULL_HANDLE};
        VkSemaphore vkRenderCompleteSemaphore{VK_NULL_HANDLE};

        Texture colorTexture{Device};
        Texture normalTexture{Device};
        Texture gbufferTexture{Device};
        Texture depthTexture{Device};

        VkExtent2D extents{};

        MXC_RESULT InitFramebuffer();
        MXC_RESULT InitSemaphore();
    };
}// namespace Moxaic::Vulkan
