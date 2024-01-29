#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_texture.hpp"
#include "static_array.hpp"

#include <glm/ext/scalar_constants.hpp>
#include <vulkan/vulkan.h>

using namespace Moxaic;
using namespace Moxaic::Vulkan;

static VkImageUsageFlags ColorBufferUsage(PipelineType pipeline)
{
    switch (pipeline) {
        default:
        case PipelineType::Graphics:
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_SAMPLED_BIT |
                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        case PipelineType::Compute:
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_SAMPLED_BIT;
    }
}

static VkImageUsageFlags NormalBufferUsage(PipelineType pipeline)
{
    switch (pipeline) {
        default:
        case PipelineType::Graphics:
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_SAMPLED_BIT;
        case PipelineType::Compute:
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_SAMPLED_BIT;
    }
}

static VkImageUsageFlags GBufferBufferUsage(PipelineType pipeline)
{
    switch (pipeline) {
        default:
        case PipelineType::Graphics:
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                   VK_IMAGE_USAGE_SAMPLED_BIT;
        case PipelineType::Compute:
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                   VK_IMAGE_USAGE_SAMPLED_BIT;
    }
}

constexpr VkImageUsageFlags DepthBufferUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                                VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                                VK_IMAGE_USAGE_SAMPLED_BIT;

Framebuffer::Framebuffer(const Vulkan::Device* const pDevice)
    : Device(pDevice) {}

Framebuffer::~Framebuffer() = default;

bool Framebuffer::Init(const PipelineType pipelineType,
                       const VkExtent2D extents,
                       const Locality locality)
{
    this->extents = extents;
    MXC_CHK(colorTexture.Init(kColorBufferFormat,
                                extents,
                                ColorBufferUsage(pipelineType),
                                VK_IMAGE_ASPECT_COLOR_BIT,
                                locality));
    MXC_CHK(colorTexture.TransitionInitialImmediate(pipelineType));
    MXC_CHK(normalTexture.Init(kNormalBufferFormat,
                                 extents,
                                 NormalBufferUsage(pipelineType),
                                 VK_IMAGE_ASPECT_COLOR_BIT,
                                 locality));
    MXC_CHK(normalTexture.TransitionInitialImmediate(pipelineType));
    MXC_CHK(gbufferTexture.Init(kGBufferFormat,
                                  extents,
                                  GBufferBufferUsage(pipelineType),
                                  VK_IMAGE_ASPECT_COLOR_BIT,
                                  locality));
    MXC_CHK(gbufferTexture.TransitionInitialImmediate(pipelineType));
    MXC_CHK(depthTexture.Init(kDepthBufferFormat,
                                extents,
                                DepthBufferUsage,
                                VK_IMAGE_ASPECT_DEPTH_BIT,
                                locality));
    MXC_CHK(depthTexture.TransitionInitialImmediate(pipelineType));
    MXC_CHK(InitFramebuffer());
    MXC_CHK(InitSemaphore());
    return true;
}

MXC_RESULT Framebuffer::InitFromImport(const PipelineType pipelineType,
                                       const VkExtent2D extents,
                                       const HANDLE colorExternalHandle,
                                       const HANDLE normalExternalHandle,
                                       const HANDLE gBufferExternalHandle,
                                       const HANDLE depthExternalHandle)
{
    this->extents = extents;
    MXC_CHK(colorTexture.InitFromImport(kColorBufferFormat,
                                          extents,
                                          ColorBufferUsage(pipelineType),
                                          VK_IMAGE_ASPECT_COLOR_BIT,
                                          colorExternalHandle));
    MXC_CHK(colorTexture.TransitionInitialImmediate(pipelineType));
    MXC_CHK(normalTexture.InitFromImport(kNormalBufferFormat,
                                           extents,
                                           NormalBufferUsage(pipelineType),
                                           VK_IMAGE_ASPECT_COLOR_BIT,
                                           normalExternalHandle));
    MXC_CHK(normalTexture.TransitionInitialImmediate(pipelineType));
    MXC_CHK(gbufferTexture.InitFromImport(kGBufferFormat,
                                            extents,
                                            GBufferBufferUsage(pipelineType),
                                            VK_IMAGE_ASPECT_COLOR_BIT,
                                            gBufferExternalHandle));
    MXC_CHK(gbufferTexture.TransitionInitialImmediate(pipelineType));
    MXC_CHK(depthTexture.InitFromImport(kDepthBufferFormat,
                                extents,
                                DepthBufferUsage,
                                VK_IMAGE_ASPECT_DEPTH_BIT,
                                depthExternalHandle));
    MXC_CHK(depthTexture.TransitionInitialImmediate(pipelineType));
    MXC_CHK(InitFramebuffer());
    MXC_CHK(InitSemaphore());
    return MXC_SUCCESS;
}

MXC_RESULT Framebuffer::InitFramebuffer()
{
    const StaticArray attachments{
      colorTexture.GetVkImageView(),
      normalTexture.GetVkImageView(),
      // gbufferTexture.GetVkImageView(),
      depthTexture.GetVkImageView(),
    };
    const VkFramebufferCreateInfo framebufferCreateInfo{
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .renderPass = Device->GetVkRenderPass(),
      .attachmentCount = attachments.size(),
      .pAttachments = attachments.data(),
      .width = extents.width,
      .height = extents.height,
      .layers = 1,
    };
    VK_CHK(vkCreateFramebuffer(Device->  GetVkDevice(),
                               &framebufferCreateInfo,
                               VK_ALLOC,
                               &vkFramebuffer));
    return MXC_SUCCESS;
}

MXC_RESULT Framebuffer::InitSemaphore()
{
    constexpr VkSemaphoreCreateInfo renderCompleteCreateInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
    };
    VK_CHK(vkCreateSemaphore(Device->  GetVkDevice(),
                             &renderCompleteCreateInfo,
                             VK_ALLOC,
                             &vkRenderCompleteSemaphore));
    return MXC_SUCCESS;
}

void Framebuffer::Transition(const VkCommandBuffer commandbuffer,
                             const Barrier& src,
                             const Barrier& dst) const
{
    const StaticArray acquireColorImageMemoryBarriers{
      (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = src.colorAccessMask,
        .dstAccessMask = dst.colorAccessMask,
        .oldLayout = src.colorLayout,
        .newLayout = dst.colorLayout,
        .srcQueueFamilyIndex = Device->GetSrcQueue(src),
        .dstQueueFamilyIndex = Device->GetDstQueue(src, dst),
        .image = colorTexture.GetVkImage(),
        .subresourceRange = colorTexture.GetSubresourceRange(),
      },
      (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = src.colorAccessMask,
        .dstAccessMask = dst.colorAccessMask,
        .oldLayout = src.colorLayout,
        .newLayout = dst.colorLayout,
        .srcQueueFamilyIndex = Device->GetSrcQueue(src),
        .dstQueueFamilyIndex = Device->GetDstQueue(src, dst),
        .image = normalTexture.GetVkImage(),
        .subresourceRange = normalTexture.GetSubresourceRange(),
      },
      (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = src.colorAccessMask,
        .dstAccessMask = dst.colorAccessMask,
        .oldLayout = src.colorLayout,
        .newLayout = dst.colorLayout,
        .srcQueueFamilyIndex = Device->GetSrcQueue(src),
        .dstQueueFamilyIndex = Device->GetDstQueue(src, dst),
        .image = gbufferTexture.GetVkImage(),
        .subresourceRange = gbufferTexture.GetSubresourceRange(),
      }};
    VK_CHK_VOID(vkCmdPipelineBarrier(commandbuffer,
                                     src.colorStageMask,
                                     dst.colorStageMask,
                                     0,
                                     0,
                                     nullptr,
                                     0,
                                     nullptr,
                                     acquireColorImageMemoryBarriers.size(),
                                     acquireColorImageMemoryBarriers.data()));
    const StaticArray acquireDepthImageMemoryBarriers{
      (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = src.depthAccessMask,
        .dstAccessMask = dst.depthAccessMask,
        .oldLayout = src.depthLayout,
        .newLayout = dst.depthLayout,
        .srcQueueFamilyIndex = Device->GetSrcQueue(src),
        .dstQueueFamilyIndex = Device->GetDstQueue(src, dst),
        .image = depthTexture.GetVkImage(),
        .subresourceRange = depthTexture.GetSubresourceRange(),
      }};
    VK_CHK_VOID(vkCmdPipelineBarrier(commandbuffer,
                                     src.depthStageMask,
                                     dst.depthStageMask,
                                     0,
                                     0,
                                     nullptr,
                                     0,
                                     nullptr,
                                     acquireDepthImageMemoryBarriers.size(),
                                     acquireDepthImageMemoryBarriers.data()));
}
