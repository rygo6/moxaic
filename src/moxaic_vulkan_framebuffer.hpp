#pragma once

#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_texture.hpp"

#include <memory>
#include <vulkan/vulkan.h>

namespace Moxaic::Vulkan
{
    class Device;

    class Framebuffer
    {
    public:
        MXC_NO_VALUE_PASS(Framebuffer);

        explicit Framebuffer(const Vulkan::Device* device,
                             Locality locality = Locality::Local);
        virtual ~Framebuffer();

        MXC_RESULT Init(PipelineType pipelineType,
                        VkExtent2D extents);
        MXC_RESULT InitFromImport(PipelineType pipelineType,
                                  VkExtent2D extents,
                                  HANDLE colorExternalHandle,
                                  HANDLE normalExternalHandle,
                                  HANDLE gBufferExternalHandle,
                                  HANDLE depthExternalHandle);
        void Transition(VkCommandBuffer commandbuffer,
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
        constexpr static VkImageUsageFlags ColorBufferUsage{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                            VK_IMAGE_USAGE_SAMPLED_BIT};
        constexpr static VkImageUsageFlags NormalBufferUsage{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                             VK_IMAGE_USAGE_SAMPLED_BIT};
        constexpr static VkImageUsageFlags GBufferUsage{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                        VK_IMAGE_USAGE_SAMPLED_BIT};
        constexpr static VkImageUsageFlags DepthBufferUsage{VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                                            VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                                            VK_IMAGE_USAGE_SAMPLED_BIT};

        const Vulkan::Device* const Device;
        const Vulkan::Locality Locality;

        VkExtent2D extents{};

        VkFramebuffer vkFramebuffer{VK_NULL_HANDLE};
        VkSemaphore vkRenderCompleteSemaphore{VK_NULL_HANDLE};

        Texture colorTexture{
          Device,
          {
            .format = kColorBufferFormat,
            .usage = ColorBufferUsage,
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .locality = Locality,
          }};
        Texture normalTexture{
          Device,
          {
            .format = kNormalBufferFormat,
            .usage = NormalBufferUsage,
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .locality = Locality,
          }};
        Texture depthTexture{
          Device,
          {
            .format = kDepthBufferFormat,
            .usage = DepthBufferUsage,
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .locality = Locality,
          }};
        Texture gbufferTexture{
          Device,
          {
            .format = kGBufferFormat,
            .usage = GBufferUsage,
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevels = 4,
            .locality = Locality,
          }};

        MXC_RESULT InitFramebuffer();
        MXC_RESULT InitSemaphore();
    };
}// namespace Moxaic::Vulkan
