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
        case PipelineType::Graphics:
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_SAMPLED_BIT |
                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        case PipelineType::Compute:
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_STORAGE_BIT;
    }
}

static VkImageUsageFlags NormalBufferUsage(PipelineType pipeline)
{
    switch (pipeline) {
        case PipelineType::Graphics:
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_SAMPLED_BIT;
        case PipelineType::Compute:
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_STORAGE_BIT;
    }
}

static VkImageUsageFlags GBufferBufferUsage(PipelineType pipeline)
{
    switch (pipeline) {
        case PipelineType::Graphics:
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                   VK_IMAGE_USAGE_SAMPLED_BIT;
        case PipelineType::Compute:
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                   VK_IMAGE_USAGE_STORAGE_BIT;
    }
}

constexpr VkImageUsageFlags kDepthBufferUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                                VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                                VK_IMAGE_USAGE_SAMPLED_BIT;

Framebuffer::Framebuffer(const Device& device)
    : k_pDevice(&device) {}

Framebuffer::~Framebuffer() = default;

bool Framebuffer::Init(const PipelineType pipelineType,
                       const VkExtent2D extents,
                       const Locality locality)
{
    m_Extents = extents;
    MXC_CHK(m_ColorTexture.Init(kColorBufferFormat,
                                extents,
                                ColorBufferUsage(pipelineType),
                                VK_IMAGE_ASPECT_COLOR_BIT,
                                locality));
    MXC_CHK(m_ColorTexture.TransitionInitialImmediate(pipelineType));
    MXC_CHK(m_NormalTexture.Init(kNormalBufferFormat,
                                 extents,
                                 NormalBufferUsage(pipelineType),
                                 VK_IMAGE_ASPECT_COLOR_BIT,
                                 locality));
    MXC_CHK(m_NormalTexture.TransitionInitialImmediate(pipelineType));
    MXC_CHK(m_GBufferTexture.Init(kGBufferFormat,
                                  extents,
                                  GBufferBufferUsage(pipelineType),
                                  VK_IMAGE_ASPECT_COLOR_BIT,
                                  locality));
    MXC_CHK(m_GBufferTexture.TransitionInitialImmediate(pipelineType));
    if (locality == Locality::Local) {
        // Make depth buffer always local because depth can't be bound
        // to compute, so in compute pipe it needs to be blitted to gbuffer
        // and we aren't using gbuffer right now anyways... so just use gbuffer
        MXC_CHK(m_DepthTexture.Init(kDepthBufferFormat,
                                    extents,
                                    kDepthBufferUsage,
                                    VK_IMAGE_ASPECT_DEPTH_BIT,
                                    Locality::Local));
        MXC_CHK(m_DepthTexture.TransitionInitialImmediate(PipelineType::Graphics));
        // Don't make framebuffer for external because compositor isn't going to render to it, the node will
        MXC_CHK(InitFramebuffer());
    }
    MXC_CHK(InitSemaphore());
    return true;
}

MXC_RESULT Framebuffer::InitFromImport(const PipelineType pipelineType,
                                       const VkExtent2D extents,
                                       const HANDLE colorExternalHandle,
                                       const HANDLE normalExternalHandle,
                                       const HANDLE gBufferExternalHandle)
{
    m_Extents = extents;
    MXC_CHK(m_ColorTexture.InitFromImport(kColorBufferFormat,
                                          extents,
                                          ColorBufferUsage(pipelineType),
                                          VK_IMAGE_ASPECT_COLOR_BIT,
                                          colorExternalHandle));
    MXC_CHK(m_ColorTexture.TransitionInitialImmediate(pipelineType));
    MXC_CHK(m_NormalTexture.InitFromImport(kNormalBufferFormat,
                                           extents,
                                           NormalBufferUsage(pipelineType),
                                           VK_IMAGE_ASPECT_COLOR_BIT,
                                           normalExternalHandle));
    MXC_CHK(m_NormalTexture.TransitionInitialImmediate(pipelineType));
    MXC_CHK(m_GBufferTexture.InitFromImport(kGBufferFormat,
                                            extents,
                                            GBufferBufferUsage(pipelineType),
                                            VK_IMAGE_ASPECT_COLOR_BIT,
                                            gBufferExternalHandle));
    {
        // Init depth as if local because depth is not externally shared
        MXC_CHK(m_DepthTexture.Init(kDepthBufferFormat,
                                    extents,
                                    kDepthBufferUsage,
                                    VK_IMAGE_ASPECT_DEPTH_BIT,
                                    Locality::Local));
        MXC_CHK(m_DepthTexture.TransitionInitialImmediate(PipelineType::Graphics));
    }
    MXC_CHK(InitFramebuffer());
    MXC_CHK(InitSemaphore());
    return MXC_SUCCESS;
}

MXC_RESULT Framebuffer::InitFramebuffer()
{
    const StaticArray attachments{
      m_ColorTexture.GetVkImageView(),
      m_NormalTexture.GetVkImageView(),
      m_GBufferTexture.GetVkImageView(),
      m_DepthTexture.GetVkImageView(),
    };
    const VkFramebufferCreateInfo framebufferCreateInfo{
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .renderPass = k_pDevice->GetVkRenderPass(),
      .attachmentCount = attachments.size(),
      .pAttachments = attachments.data(),
      .width = m_Extents.width,
      .height = m_Extents.height,
      .layers = 1,
    };
    VK_CHK(vkCreateFramebuffer(k_pDevice->GetVkDevice(),
                               &framebufferCreateInfo,
                               VK_ALLOC,
                               &m_VkFramebuffer));
    return MXC_SUCCESS;
}

MXC_RESULT Framebuffer::InitSemaphore()
{
    constexpr VkSemaphoreCreateInfo renderCompleteCreateInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
    };
    VK_CHK(vkCreateSemaphore(k_pDevice->GetVkDevice(),
                             &renderCompleteCreateInfo,
                             VK_ALLOC,
                             &m_VkRenderCompleteSemaphore));
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
        .srcQueueFamilyIndex = k_pDevice->GetSrcQueue(src),
        .dstQueueFamilyIndex = k_pDevice->GetDstQueue(src, dst),
        .image = m_ColorTexture.GetVkImage(),
        .subresourceRange = DefaultColorSubresourceRange,
      },
      (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = src.colorAccessMask,
        .dstAccessMask = dst.colorAccessMask,
        .oldLayout = src.colorLayout,
        .newLayout = dst.colorLayout,
        .srcQueueFamilyIndex = k_pDevice->GetSrcQueue(src),
        .dstQueueFamilyIndex = k_pDevice->GetDstQueue(src, dst),
        .image = m_NormalTexture.GetVkImage(),
        .subresourceRange = DefaultColorSubresourceRange,
      },
      (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = src.colorAccessMask,
        .dstAccessMask = dst.colorAccessMask,
        .oldLayout = src.colorLayout,
        .newLayout = dst.colorLayout,
        .srcQueueFamilyIndex = k_pDevice->GetSrcQueue(src),
        .dstQueueFamilyIndex = k_pDevice->GetDstQueue(src, dst),
        .image = m_GBufferTexture.GetVkImage(),
        .subresourceRange = DefaultColorSubresourceRange,
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
    // const StaticArray acquireDepthImageMemoryBarriers{
    //   (VkImageMemoryBarrier){
    //     .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
    //     .srcAccessMask = src.depthAccessMask,
    //     .dstAccessMask = dst.depthAccessMask,
    //     .oldLayout = src.depthLayout,
    //     .newLayout = dst.depthLayout,
    //     .srcQueueFamilyIndex = k_pDevice->GetSrcQueue(src),
    //     .dstQueueFamilyIndex = k_pDevice->GetDstQueue(src, dst),
    //     .image = m_DepthTexture.GetVkImage(),
    //     .subresourceRange = DefaultDepthSubresourceRange,
    //   }};
    // VK_CHK_VOID(vkCmdPipelineBarrier(commandbuffer,
    //                                  src.depthStageMask,
    //                                  dst.depthStageMask,
    //                                  0,
    //                                  0,
    //                                  nullptr,
    //                                  0,
    //                                  nullptr,
    //                                  acquireDepthImageMemoryBarriers.size(),
    //                                  acquireDepthImageMemoryBarriers.data()));
}
