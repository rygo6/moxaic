#include "moxaic_vulkan_framebuffer.hpp"
#include "moxaic_vulkan_device.hpp"
#include "moxaic_vulkan_texture.hpp"
#include "static_array.hpp"

#include <vulkan/vulkan.h>

using namespace Moxaic;
using namespace Moxaic::Vulkan;

constinit VkImageUsageFlags kColorBufferUsage =
  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
  VK_IMAGE_USAGE_SAMPLED_BIT |
  VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
  VK_IMAGE_USAGE_STORAGE_BIT;
constinit VkImageUsageFlags kNormalBufferUsage =
  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
  VK_IMAGE_USAGE_SAMPLED_BIT |
  VK_IMAGE_USAGE_STORAGE_BIT;
constinit VkImageUsageFlags kGBufferUsage =
  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
  VK_IMAGE_USAGE_SAMPLED_BIT |
  VK_IMAGE_USAGE_STORAGE_BIT;
constinit VkImageUsageFlags kDepthBufferUsage =
  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
  VK_IMAGE_USAGE_SAMPLED_BIT;
// constinit VkImageUsageFlags kDepthBufferUsage =
//   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
//   VK_IMAGE_USAGE_SAMPLED_BIT |
//   VK_IMAGE_USAGE_STORAGE_BIT;

Framebuffer::Framebuffer(const Device& device)
    : k_pDevice(&device) {}

Framebuffer::~Framebuffer() = default;

bool Framebuffer::Init(const VkExtent2D extents,
                       const Locality locality)
{
    m_Extents = extents;
    MXC_CHK(m_ColorTexture.Init(k_ColorBufferFormat,
                                extents,
                                kColorBufferUsage,
                                VK_IMAGE_ASPECT_COLOR_BIT,
                                locality));
    MXC_CHK(m_ColorTexture.TransitionImmediateInitialToGraphicsRead());
    MXC_CHK(m_NormalTexture.Init(k_NormalBufferFormat,
                                 extents,
                                 kNormalBufferUsage,
                                 VK_IMAGE_ASPECT_COLOR_BIT,
                                 locality));
    MXC_CHK(m_NormalTexture.TransitionImmediateInitialToGraphicsRead());
    MXC_CHK(m_GBufferTexture.Init(k_GBufferFormat,
                                  extents,
                                  kGBufferUsage,
                                  VK_IMAGE_ASPECT_COLOR_BIT,
                                  locality));
    MXC_CHK(m_GBufferTexture.TransitionImmediateInitialToGraphicsRead());
    MXC_CHK(m_DepthTexture.Init(k_DepthBufferFormat,
                                extents,
                                kDepthBufferUsage,
                                VK_IMAGE_ASPECT_DEPTH_BIT,
                                locality));
    MXC_CHK(m_DepthTexture.TransitionImmediateInitialToGraphicsRead());
    MXC_CHK(InitFramebuffer());
    MXC_CHK(InitSemaphore());
    return true;
}

MXC_RESULT Framebuffer::InitFromImport(const VkExtent2D extents,
                                       const HANDLE colorExternalHandle,
                                       const HANDLE normalExternalHandle,
                                       const HANDLE gBufferExternalHandle,
                                       const HANDLE depthExternalHandle)
{
    m_Extents = extents;
    MXC_CHK(m_ColorTexture.InitFromImport(k_ColorBufferFormat,
                                          extents,
                                          kColorBufferUsage,
                                          VK_IMAGE_ASPECT_COLOR_BIT,
                                          colorExternalHandle));
    MXC_CHK(m_ColorTexture.TransitionImmediateInitialToGraphicsRead());
    MXC_CHK(m_NormalTexture.InitFromImport(k_NormalBufferFormat,
                                           extents,
                                           kNormalBufferUsage,
                                           VK_IMAGE_ASPECT_COLOR_BIT,
                                           normalExternalHandle));
    MXC_CHK(m_NormalTexture.TransitionImmediateInitialToGraphicsRead());
    MXC_CHK(m_GBufferTexture.InitFromImport(k_GBufferFormat,
                                            extents,
                                            kGBufferUsage,
                                            VK_IMAGE_ASPECT_COLOR_BIT,
                                            gBufferExternalHandle));
    MXC_CHK(m_GBufferTexture.TransitionImmediateInitialToGraphicsRead());
    MXC_CHK(m_DepthTexture.InitFromImport(k_DepthBufferFormat,
                                          extents,
                                          kDepthBufferUsage,
                                          VK_IMAGE_ASPECT_DEPTH_BIT,
                                          depthExternalHandle));
    MXC_CHK(m_DepthTexture.TransitionImmediateInitialToGraphicsRead());
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
    const StaticArray acquireDepthImageMemoryBarriers{
      (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = src.depthAccessMask,
        .dstAccessMask = dst.depthAccessMask,
        .oldLayout = src.depthLayout,
        .newLayout = dst.depthLayout,
        .srcQueueFamilyIndex = k_pDevice->GetSrcQueue(src),
        .dstQueueFamilyIndex = k_pDevice->GetDstQueue(src, dst),
        .image = m_DepthTexture.GetVkImage(),
        .subresourceRange = DefaultDepthSubresourceRange,
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
