#pragma once

#include "moxaic_node_process_descriptor.hpp"
#include "moxaic_vulkan.hpp"
#include "moxaic_vulkan_texture.hpp"

#include <memory>
#include <vulkan/vulkan.h>

namespace Moxaic::Vulkan
{
    class Device;

    class Framebuffer
    {
        MXC_NO_VALUE_PASS(Framebuffer);
    public:
        constexpr static VkImageUsageFlags ColorBufferUsage{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                            VK_IMAGE_USAGE_SAMPLED_BIT};
        constexpr static VkImageUsageFlags NormalBufferUsage{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                             VK_IMAGE_USAGE_SAMPLED_BIT};
        constexpr static VkImageUsageFlags GBufferUsage{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                        VK_IMAGE_USAGE_SAMPLED_BIT |
                                                        VK_IMAGE_USAGE_STORAGE_BIT};
        constexpr static VkImageUsageFlags DepthBufferUsage{VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                                            VK_IMAGE_USAGE_SAMPLED_BIT};
        constexpr static uint32_t GBufferMipLevelCount{10};

    private:
        const Vulkan::Device* const Device;
        const Vulkan::Locality Locality;

        VkExtent2D extents{};

        VkFramebuffer vkFramebuffer{VK_NULL_HANDLE};
        VkSemaphore vkRenderCompleteSemaphore{VK_NULL_HANDLE};
        VkImageView vkGbufferImageViewMipHandles[GBufferMipLevelCount]{VK_NULL_HANDLE};
        // NodeProcessDescriptor nodeProcessDescriptors[GBufferMipLevelCount]{NodeProcessDescriptor(Device)};

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
            .mipLevels = GBufferMipLevelCount,
            .locality = Locality,
          }};

    public:
        const Texture& ColorTexture{colorTexture};
        const Texture& NormalTexture{normalTexture};
        const Texture& DepthTexture{depthTexture};
        const Texture& GbufferTexture{gbufferTexture};

        const VkImageView (&VkGbufferImageViewMipHandles)[GBufferMipLevelCount]{vkGbufferImageViewMipHandles};

        const auto& GetVkFramebuffer() const { return vkFramebuffer; }
        const auto& GetVkRenderCompleteSemaphore() const { return vkRenderCompleteSemaphore; }
        const auto& GetColorTexture() const { return colorTexture; }
        const auto& GetNormalTexture() const { return normalTexture; }
        const auto& GetGBufferTexture() const { return gbufferTexture; }
        const auto& GetDepthTexture() const { return depthTexture; }
        const auto& GetExtents() const { return extents; }

        explicit Framebuffer(const Vulkan::Device* device,
                             Vulkan::Locality locality = Locality::Local);
        virtual ~Framebuffer();

        MXC_RESULT Init(PipelineType pipelineType,
                        VkExtent2D extents);
        MXC_RESULT InitFromImport(PipelineType pipelineType,
                                  VkExtent2D extents,
                                  HANDLE colorExternalHandle,
                                  HANDLE normalExternalHandle,
                                  HANDLE gBufferExternalHandle,
                                  HANDLE depthExternalHandle);
        void TransitionAttachmentBuffers(VkCommandBuffer commandBuffer,
                                         const Barrier2& src,
                                         const Barrier2& dst) const;
        // void TransitionGBuffers(VkCommandBuffer commandbuffer,
        //                         const Barrier& src,
        //                         const Barrier& dst) const;

    private:
        MXC_RESULT InitFramebuffer();
        MXC_RESULT InitSemaphore();
    };
}// namespace Moxaic::Vulkan
